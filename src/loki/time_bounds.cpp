#include "loki/time_bounds.hpp"

#include <limits>
#include <stdexcept>

namespace duckdb {
namespace loki {

bool TryAddNanos(int64_t left, int64_t right, int64_t &result) {
	if (right > 0 && left > std::numeric_limits<int64_t>::max() - right) {
		return false;
	}
	if (right < 0 && left < std::numeric_limits<int64_t>::min() - right) {
		return false;
	}
	result = left + right;
	return true;
}

bool TryMultiplyNanos(int64_t left, int64_t right, int64_t &result) {
	constexpr int64_t MAX = std::numeric_limits<int64_t>::max();
	constexpr int64_t MIN = std::numeric_limits<int64_t>::min();
	if (left == 0 || right == 0) {
		result = 0;
		return true;
	}
	// Negation of MIN is the one case the division tests below cannot express.
	if (left == -1) {
		if (right == MIN) {
			return false;
		}
		result = -right;
		return true;
	}
	if (right == -1) {
		if (left == MIN) {
			return false;
		}
		result = -left;
		return true;
	}
	if (left > 0) {
		if (right > 0 ? left > MAX / right : right < MIN / left) {
			return false;
		}
	} else {
		if (right > 0 ? left < MIN / right : left < MAX / right) {
			return false;
		}
	}
	result = left * right;
	return true;
}

int64_t IntervalToNanos(int32_t months, int32_t days, int64_t micros) {
	if (months != 0) {
		throw std::invalid_argument("loki_scan: month/year INTERVAL bounds are calendar-dependent and not supported; "
		                            "pass an absolute TIMESTAMP (e.g. now() - INTERVAL 30 DAY) instead");
	}
	constexpr int64_t NANOS_PER_DAY = 86400LL * 1000LL * 1000LL * 1000LL;
	constexpr int64_t NANOS_PER_MICRO = 1000LL;
	// Checked throughout: signed overflow is undefined behavior, and unchecked wrapping would
	// silently turn a huge INTERVAL into a nonsense (often inverted) query window.
	int64_t day_nanos = 0;
	int64_t micro_nanos = 0;
	int64_t total = 0;
	if (!TryMultiplyNanos(static_cast<int64_t>(days), NANOS_PER_DAY, day_nanos) ||
	    !TryMultiplyNanos(micros, NANOS_PER_MICRO, micro_nanos) || !TryAddNanos(day_nanos, micro_nanos, total)) {
		throw std::out_of_range("loki_scan: INTERVAL bound is too large to express as a nanosecond offset; "
		                        "pass an absolute TIMESTAMP instead");
	}
	return total;
}

int64_t ParseDurationToNanos(const std::string &text) {
	const std::string invalid = "loki: invalid duration '" + text +
	                            "'; expected a non-negative number with an optional unit suffix "
	                            "(ms, s, m, h, d) — e.g. '0', '30s', '5m', '1h'";

	size_t begin = text.find_first_not_of(" \t");
	size_t end = text.find_last_not_of(" \t");
	if (begin == std::string::npos) {
		throw std::invalid_argument(invalid);
	}
	const std::string trimmed = text.substr(begin, end - begin + 1);

	size_t digits = 0;
	while (digits < trimmed.size() && trimmed[digits] >= '0' && trimmed[digits] <= '9') {
		digits++;
	}
	if (digits == 0) {
		throw std::invalid_argument(invalid);
	}

	std::string suffix = trimmed.substr(digits);
	for (auto &c : suffix) {
		if (c >= 'A' && c <= 'Z') {
			c = static_cast<char>(c - 'A' + 'a');
		}
	}

	constexpr int64_t NANOS_PER_MILLI = 1000LL * 1000LL;
	int64_t unit_nanos = 0;
	if (suffix.empty() || suffix == "s") {
		unit_nanos = 1000LL * NANOS_PER_MILLI;
	} else if (suffix == "ms") {
		unit_nanos = NANOS_PER_MILLI;
	} else if (suffix == "m") {
		unit_nanos = 60LL * 1000LL * NANOS_PER_MILLI;
	} else if (suffix == "h") {
		unit_nanos = 3600LL * 1000LL * NANOS_PER_MILLI;
	} else if (suffix == "d") {
		unit_nanos = 86400LL * 1000LL * NANOS_PER_MILLI;
	} else {
		throw std::invalid_argument(invalid);
	}

	// Parse the digits by hand rather than via stoll: a value far past int64 must be an
	// out_of_range for the caller to report, not an unspecified wrap.
	int64_t value = 0;
	for (size_t i = 0; i < digits; i++) {
		int64_t digit = trimmed[i] - '0';
		if (!TryMultiplyNanos(value, 10, value) || !TryAddNanos(value, digit, value)) {
			throw std::out_of_range("loki: duration '" + text + "' is too large");
		}
	}
	int64_t total = 0;
	if (!TryMultiplyNanos(value, unit_nanos, total)) {
		throw std::out_of_range("loki: duration '" + text + "' is too large to express in nanoseconds");
	}
	return total;
}

} // namespace loki
} // namespace duckdb
