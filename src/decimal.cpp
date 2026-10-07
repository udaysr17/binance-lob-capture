#include "decimal.h"

#include "types.h"

namespace bc {

namespace {

inline bool is_digit(char c) { return c >= '0' && c <= '9'; }

// v = v * 10 + d, with overflow detection.
inline bool mul10_add(std::int64_t& v, std::int64_t d) {
    std::int64_t t = 0;
    if (__builtin_mul_overflow(v, std::int64_t{10}, &t)) return false;
    if (__builtin_add_overflow(t, d, &t)) return false;
    v = t;
    return true;
}

}  // namespace

const char* to_string(DecimalError e) {
    switch (e) {
        case DecimalError::Ok: return "ok";
        case DecimalError::Empty: return "empty";
        case DecimalError::BadChar: return "bad_char";
        case DecimalError::TooManyDecimals: return "too_many_decimals";
        case DecimalError::Overflow: return "overflow";
    }
    return "unknown";
}

DecimalError parse_scaled_1e8(std::string_view s, std::int64_t& out) {
    if (s.empty()) return DecimalError::Empty;
    const std::size_t n = s.size();
    std::size_t i = 0;
    std::int64_t v = 0;
    bool overflow = false;

    while (i < n && is_digit(s[i])) {
        if (!overflow && !mul10_add(v, static_cast<std::int64_t>(s[i] - '0'))) overflow = true;
        ++i;
    }
    if (i == 0) return DecimalError::BadChar;  // must start with a digit

    int frac = 0;
    if (i < n) {
        if (s[i] != '.') return DecimalError::BadChar;
        ++i;
        if (i == n) return DecimalError::BadChar;  // "1." is not accepted
        bool too_many = false;
        for (; i < n; ++i) {
            if (!is_digit(s[i])) return DecimalError::BadChar;
            if (frac == kScaleDecimals) {
                too_many = true;  // keep scanning so bad chars still win
                continue;
            }
            if (!overflow && !mul10_add(v, static_cast<std::int64_t>(s[i] - '0'))) overflow = true;
            ++frac;
        }
        if (too_many) return DecimalError::TooManyDecimals;
    }
    for (; frac < kScaleDecimals; ++frac) {
        if (!overflow && !mul10_add(v, 0)) overflow = true;
    }
    if (overflow) return DecimalError::Overflow;
    out = v;
    return DecimalError::Ok;
}

DecimalError parse_uint64(std::string_view s, std::uint64_t& out) {
    if (s.empty()) return DecimalError::Empty;
    std::uint64_t v = 0;
    for (char c : s) {
        if (!is_digit(c)) return DecimalError::BadChar;
        std::uint64_t t = 0;
        if (__builtin_mul_overflow(v, std::uint64_t{10}, &t)) return DecimalError::Overflow;
        if (__builtin_add_overflow(t, static_cast<std::uint64_t>(c - '0'), &t)) {
            return DecimalError::Overflow;
        }
        v = t;
    }
    out = v;
    return DecimalError::Ok;
}

DecimalError parse_int64(std::string_view s, std::int64_t& out) {
    if (s.empty()) return DecimalError::Empty;
    bool neg = false;
    if (s[0] == '-') {
        neg = true;
        s.remove_prefix(1);
        if (s.empty()) return DecimalError::BadChar;
    }
    std::int64_t v = 0;
    for (char c : s) {
        if (!is_digit(c)) return DecimalError::BadChar;
        // Accumulate negatively so INT64_MIN is representable.
        std::int64_t t = 0;
        if (__builtin_mul_overflow(v, std::int64_t{10}, &t)) return DecimalError::Overflow;
        if (__builtin_sub_overflow(t, static_cast<std::int64_t>(c - '0'), &t)) {
            return DecimalError::Overflow;
        }
        v = t;
    }
    if (!neg) {
        if (v == INT64_MIN) return DecimalError::Overflow;
        v = -v;
    }
    out = v;
    return DecimalError::Ok;
}

}  // namespace bc
