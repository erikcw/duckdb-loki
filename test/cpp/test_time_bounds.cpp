#include "test_util.hpp"

#include "loki/time_bounds.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

using duckdb::loki::IntervalToNanos;
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
