#!/usr/bin/env bash
#
# Mock-HTTP sqllogictest harness (DESIGN.md §8): start the deterministic Loki mock, point the
# sqllogictests at it via ${LOKI_MOCK_URL}, run them through DuckDB's unittest runner, then tear
# the mock down. No Docker, no real Loki — fast and deterministic.
#
# Those test files are gated by `require-env LOKI_MOCK_URL`, so a plain `make test` skips them;
# this script is the only thing that sets the var and thus actually runs them. (The offline ATTACH
# coverage in test/sql/loki_attach_offline.test needs no mock and runs under `make test`.)
#
# Requires: python3, and a built extension + test runner. Override with env vars:
#   UNITTEST   path to the DuckDB test runner   (default: <repo>/build/release/test/unittest)
#   PYTHON     python interpreter               (default: python3)
#
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$HERE/../.." && pwd)"
UNITTEST="${UNITTEST:-$REPO_ROOT/build/release/test/unittest}"
PYTHON="${PYTHON:-python3}"

if [[ ! -x "$UNITTEST" ]]; then
	echo "unittest runner not found at $UNITTEST — run 'make' first (or set UNITTEST=...)." >&2
	exit 1
fi

# Start the mock on an ephemeral port; it prints the chosen port on its first stdout line.
PORT_FILE="$(mktemp)"
"$PYTHON" "$HERE/mock_loki.py" 0 >"$PORT_FILE" &
MOCK_PID=$!
trap 'kill "$MOCK_PID" >/dev/null 2>&1 || true; rm -f "$PORT_FILE"' EXIT

echo "==> Waiting for mock Loki to bind a port"
PORT=
for _ in $(seq 1 50); do
	PORT="$(head -n1 "$PORT_FILE" 2>/dev/null || true)"
	[[ -n "$PORT" ]] && break
	sleep 0.1
done
[[ -n "$PORT" ]] || { echo "mock server did not report a port" >&2; exit 1; }

export LOKI_MOCK_URL="http://127.0.0.1:$PORT"

# Seed the standard logcli env vars so the test can exercise the `env` secret provider
# (CREATE SECRET (TYPE loki, PROVIDER env)) end-to-end: LOKI_ADDR -> endpoint,
# LOKI_BEARER_TOKEN -> token, LOKI_ORG_ID -> tenant. The mock echoes auth/tenant back.
export LOKI_ADDR="$LOKI_MOCK_URL"
export LOKI_BEARER_TOKEN="glc_from_env"
export LOKI_ORG_ID="99"

echo "==> Waiting for mock Loki to be ready at $LOKI_MOCK_URL"
ready=
for _ in $(seq 1 50); do
	if "$PYTHON" -c "import urllib.request,sys; urllib.request.urlopen('$LOKI_MOCK_URL/ready', timeout=1)" >/dev/null 2>&1; then
		ready=1
		break
	fi
	sleep 0.1
done
[[ -n "$ready" ]] || { echo "mock server did not become ready at $LOKI_MOCK_URL" >&2; exit 1; }

# Optional args: one or more test files to run. The default is every mock-gated suite — CI invokes
# this script with no arguments (.github/workflows/Smoke.yml), so anything omitted here runs nowhere.
TESTS=("$@")
if [[ ${#TESTS[@]} -eq 0 ]]; then
	TESTS=(test/sql/loki_mock.test test/sql/loki_attach.test)
fi

# One invocation per file: the unittest runner takes a single filter and silently concatenates
# extra positional args into one unmatchable string.
for test_file in "${TESTS[@]}"; do
	echo "==> Running mock sqllogictest: $test_file"
	"$UNITTEST" --test-dir "$REPO_ROOT" "$test_file"
done
