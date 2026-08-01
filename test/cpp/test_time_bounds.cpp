#include "test_util.hpp"

#include "loki/time_bounds.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

using duckdb::loki::IntervalToNanos;
using duckdb::loki::ParseDurationToNanos;
using duckdb::loki::TryAddNanos;
using duckdb::loki::TryMultiplyNanos;

static constexpr int64_t INT64_MAX_V = std::numeric_limits<int64_t>::max();
static constexpr int64_t INT64_MIN_V = std::numeric_limits<int64_t>::min();

TEST_CASE("interval micros to nanos") {
	CHECK_EQ(IntervalToNanos(0, 0, 0), int64_t(0));
	CHECK_EQ(IntervalToNanos(0, 0, 1), int64_t(1000));               // 1 micro = 1000 ns
	CHECK_EQ(IntervalToNanos(0, 0, 1000000), int64_t(1000000000LL)); // 1 s
}

TEST_CASE("interval days to nanos") {
	CHECK_EQ(IntervalToNanos(0, 1, 0), int64_t(86400LL * 1000000000LL));
	CHECK_EQ(IntervalToNanos(0, 2, 500000), int64_t(2LL * 86400LL * 1000000000LL + 500000LL * 1000LL));
}

TEST_CASE("negative interval offsets into the past") {
	CHECK_EQ(IntervalToNanos(0, 0, -1000000), int64_t(-1000000000LL));
}

TEST_CASE("month or year interval is rejected") {
	bool threw = false;
	try {
		IntervalToNanos(1, 0, 0);
	} catch (const std::invalid_argument &) {
		threw = true;
	}
	CHECK(threw);
}

TEST_CASE("checked addition rejects overflow instead of wrapping") {
	int64_t out = 0;
	CHECK(TryAddNanos(1, 2, out));
	CHECK_EQ(out, int64_t(3));
	CHECK_FALSE(TryAddNanos(INT64_MAX_V, 1, out));
	CHECK_FALSE(TryAddNanos(INT64_MIN_V, -1, out));
	CHECK(TryAddNanos(INT64_MAX_V, -1, out));
	CHECK_EQ(out, int64_t(INT64_MAX_V - 1));
}

TEST_CASE("checked multiplication rejects overflow instead of wrapping") {
	int64_t out = 0;
	CHECK(TryMultiplyNanos(0, INT64_MIN_V, out));
	CHECK_EQ(out, int64_t(0));
	CHECK(TryMultiplyNanos(-3, -4, out));
	CHECK_EQ(out, int64_t(12));
	CHECK_FALSE(TryMultiplyNanos(INT64_MAX_V, 2, out));
	CHECK_FALSE(TryMultiplyNanos(INT64_MIN_V, -1, out));
	CHECK_FALSE(TryMultiplyNanos(-1, INT64_MIN_V, out));
	CHECK_FALSE(TryMultiplyNanos(INT64_MIN_V / 2, 3, out));
}

TEST_CASE("an interval too large for the nanosecond range is rejected") {
	// 200,000,000 days overflows int64 nanoseconds; it used to wrap to a nonsense window.
	bool threw = false;
	try {
		IntervalToNanos(0, 200000000, 0);
	} catch (const std::out_of_range &) {
		threw = true;
	}
	CHECK(threw);
}

//===--------------------------------------------------------------------===//
// ParseDurationToNanos — the ATTACH `label_cache_ttl` option (DESIGN.md §11.8)
//===--------------------------------------------------------------------===//

static constexpr int64_t SEC = 1000000000LL;

TEST_CASE("duration suffixes scale correctly") {
	CHECK_EQ(ParseDurationToNanos("0"), int64_t(0));
	CHECK_EQ(ParseDurationToNanos("300"), int64_t(300 * SEC)); // bare number = seconds
	CHECK_EQ(ParseDurationToNanos("30s"), int64_t(30 * SEC));
	CHECK_EQ(ParseDurationToNanos("500ms"), int64_t(SEC / 2));
	CHECK_EQ(ParseDurationToNanos("5m"), int64_t(300 * SEC));
	CHECK_EQ(ParseDurationToNanos("1h"), int64_t(3600 * SEC));
	CHECK_EQ(ParseDurationToNanos("2d"), int64_t(2 * 86400 * SEC));
}

TEST_CASE("duration parsing tolerates whitespace and suffix case") {
	CHECK_EQ(ParseDurationToNanos("  5m "), int64_t(300 * SEC));
	CHECK_EQ(ParseDurationToNanos("1H"), int64_t(3600 * SEC));
	CHECK_EQ(ParseDurationToNanos("250MS"), int64_t(SEC / 4));
}

static bool DurationRejected(const char *text) {
	try {
		ParseDurationToNanos(text);
	} catch (const std::invalid_argument &) {
		return true;
	} catch (const std::out_of_range &) {
		return true;
	}
	return false;
}

TEST_CASE("malformed durations are rejected, not silently coerced") {
	CHECK(DurationRejected(""));
	CHECK(DurationRejected("   "));
	CHECK(DurationRejected("m"));    // no number
	CHECK(DurationRejected("-5s"));  // negative: a TTL cannot run backwards
	CHECK(DurationRejected("5 m"));  // inner whitespace
	CHECK(DurationRejected("5min")); // not one of the accepted suffixes
	CHECK(DurationRejected("1.5h")); // fractions unsupported — use 90m
	CHECK(DurationRejected("5s junk"));
}

TEST_CASE("a duration past the nanosecond range is rejected instead of wrapping") {
	CHECK(DurationRejected("999999999999999999999d"));
	CHECK(DurationRejected("9223372036854775807d"));
}
