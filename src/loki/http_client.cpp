// NOTE: this translation unit is compiled with CPPHTTPLIB_OPENSSL_SUPPORT (see CMakeLists.txt)
// so that https:// endpoints (e.g. Grafana Cloud) go through cpp-httplib's TLS client. OpenSSL
// is already linked into both extension targets.
#include "loki/http_client.hpp"

#include <httplib.h>

namespace duckdb {
namespace loki {

HttpResponse HttpGet(const std::string &endpoint, const std::string &path, const std::string &query,
                     const std::vector<std::pair<std::string, std::string>> &headers) {
	HttpResponse out;

	httplib::Client client(endpoint);
	client.set_connection_timeout(10, 0); // 10s
	client.set_read_timeout(60, 0);       // 60s
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
