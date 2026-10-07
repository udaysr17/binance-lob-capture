#pragma once

#include <cstdint>
#include <string_view>

namespace bc {

enum class DecimalError : std::uint8_t {
    Ok,
    Empty,            // empty input
    BadChar,          // anything other than [0-9]+(\.[0-9]+)?
    TooManyDecimals,  // more than 8 fractional digits (never truncated)
    Overflow,         // does not fit int64 after scaling by 1e8
};

const char* to_string(DecimalError e);

// Parses a non-negative Binance decimal string ("123.45000000") into an int64
// scaled by 1e8. Pure integer arithmetic, no floating point, no locale.
// Grammar: DIGITS [ '.' DIGITS ], at most 8 fractional digits.
// On error, `out` is left unchanged.
DecimalError parse_scaled_1e8(std::string_view s, std::int64_t& out);

// Parses a non-negative integer made only of digits (update ids).
DecimalError parse_uint64(std::string_view s, std::uint64_t& out);

// Parses an optionally '-'-prefixed integer (CSV int64 columns).
DecimalError parse_int64(std::string_view s, std::int64_t& out);

}  // namespace bc
