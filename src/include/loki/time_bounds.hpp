#pragma once

#include <cstdint>
#include <string>

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

// Parse a short duration string into nanoseconds, for the ATTACH `label_cache_ttl` option
// (DESIGN.md §11.8): a non-negative integer with an optional unit suffix — `ms`, `s`, `m`, `h`,
// or `d`. A bare number means seconds, so `'0'` and `'300'` both work. Whitespace around the
// value is ignored; the suffix is case-insensitive. Throws std::invalid_argument on anything
// else and std::out_of_range when the result exceeds the nanosecond range. Pure: no DuckDB
// types, no I/O (unit-testable standalone).
int64_t ParseDurationToNanos(const std::string &text);

} // namespace loki
} // namespace duckdb
