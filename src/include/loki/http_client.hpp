#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace duckdb {
namespace loki {

struct HttpResponse {
	int64_t status = 0; // HTTP status code; 0 if the request never completed
	std::string body;   // response body (on success or error status)
	std::string error;  // transport-level error message; empty if the request completed
};

// Issue an HTTP(S) GET. `endpoint` is scheme://host[:port] (TLS is used for https://).
// `path` is the URL path, `query` an already-encoded query string (no leading '?').
// This is the one impure seam over cpp-httplib; everything above it is unit-testable.
// The timeouts default to the values every scan/discovery call wants; callers that run on the bind
// path (the catalog's TTL label refresh, DESIGN.md §11.8) pass something much shorter, because a
// stall there blocks the connection's transaction start.
HttpResponse HttpGet(const std::string &endpoint, const std::string &path, const std::string &query,
                     const std::vector<std::pair<std::string, std::string>> &headers, int connect_timeout_s = 10,
                     int read_timeout_s = 60);

} // namespace loki
} // namespace duckdb
