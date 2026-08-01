// NOTE: this translation unit is compiled with CPPHTTPLIB_OPENSSL_SUPPORT (see CMakeLists.txt)
// so that https:// endpoints (e.g. Grafana Cloud) go through cpp-httplib's TLS client. OpenSSL
// is already linked into both extension targets.
#include "loki/http_client.hpp"

#include <httplib.h>

#include <string>

namespace duckdb {
namespace loki {

namespace {

// httplib::Client parses `scheme://host[:port]` in its constructor and does NOT tolerate a
// malformed port — `http://host:notaport` crashes the process rather than failing the request.
// An endpoint is user-supplied (a secret, `endpoint :=`, or an ATTACH path), so a typo must
// surface as an ordinary error. Returns "" when the endpoint is usable.
std::string EndpointProblem(const std::string &endpoint) {
	auto scheme_end = endpoint.find("://");
	if (scheme_end == std::string::npos) {
		return "is missing a scheme — expected http:// or https://";
	}
	std::string authority = endpoint.substr(scheme_end + 3);
	// Strip any path/query so only host[:port] is inspected.
	auto authority_end = authority.find_first_of("/?#");
	if (authority_end != std::string::npos) {
		authority = authority.substr(0, authority_end);
	}
	if (authority.empty()) {
		return "is missing a host";
	}
	// Only the last ':' can start a port; an IPv6 literal keeps its colons inside brackets.
	auto colon = authority.rfind(':');
	auto bracket = authority.rfind(']');
	if (colon == std::string::npos || (bracket != std::string::npos && colon < bracket)) {
		return "";
	}
	std::string port = authority.substr(colon + 1);
	if (port.empty()) {
		return "has a ':' but no port number";
	}
	for (char c : port) {
		if (c < '0' || c > '9') {
			return "has a non-numeric port '" + port + "'";
		}
	}
	return "";
}

} // namespace

HttpResponse HttpGet(const std::string &endpoint, const std::string &path, const std::string &query,
                     const std::vector<std::pair<std::string, std::string>> &headers, int connect_timeout_s,
                     int read_timeout_s) {
	HttpResponse out;

	std::string problem = EndpointProblem(endpoint);
	if (!problem.empty()) {
		out.error = "invalid endpoint '" + endpoint + "': it " + problem;
		return out;
	}

	httplib::Client client(endpoint);
	client.set_connection_timeout(connect_timeout_s, 0);
	client.set_read_timeout(read_timeout_s, 0);
	// Redirects are deliberately NOT followed. Loki's read API never needs them, and cpp-httplib
	// re-issues the request with our tenant header (X-Scope-OrgID) and any `headers :=` / secret
	// headers — where Grafana Cloud and reverse-proxy API keys live — still attached, to whatever
	// host the Location says, downgrading https to http if it asks. Only Authorization/Cookie are
	// stripped, and only by recent httplib versions, so following would make credential safety a
	// property of the resolved dependency version rather than of this code.

	httplib::Headers header_map;
	for (const auto &header : headers) {
		header_map.emplace(header.first, header.second);
	}

	std::string target = query.empty() ? path : path + "?" + query;
	auto res = client.Get(target, header_map);
	if (!res) {
		out.error = httplib::to_string(res.error());
		return out;
	}
	out.status = res->status;
	out.body = std::move(res->body);
	return out;
}

} // namespace loki
} // namespace duckdb
