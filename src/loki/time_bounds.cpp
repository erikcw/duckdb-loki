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

} // namespace loki
} // namespace duckdb
