#pragma once

#include <cstdint>

namespace duckdb {
namespace loki {

// Checked int64 addition/multiplication: return false instead of overflowing (signed overflow
// is undefined behavior, and a wrapped time bound silently inverts the query window).
bool TryAddNanos(int64_t left, int64_t right, int64_t &result);
bool TryMultiplyNanos(int64_t left, int64_t right, int64_t &result);

// Convert a DuckDB interval (broken into its months/days/micros fields) to a nanosecond
// offset. Days are treated as exactly 24h and micros scaled to ns. Calendar-dependent
// month/year components cannot be resolved without a reference date, so a non-zero
// `months` throws std::invalid_argument — the caller should surface a message asking for
// an absolute TIMESTAMP instead. An interval too large for the nanosecond range throws
// std::out_of_range. Pure: no DuckDB types, no I/O (unit-testable standalone).
int64_t IntervalToNanos(int32_t months, int32_t days, int64_t micros);

} // namespace loki
} // namespace duckdb
