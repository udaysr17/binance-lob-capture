#include <cstdint>
#include <limits>

#include "decimal.h"
#include "test_framework.h"

using bc::DecimalError;
using bc::parse_scaled_1e8;

namespace {
std::int64_t ok(const char* s) {
    std::int64_t v = -1;
    const DecimalError e = parse_scaled_1e8(s, v);
    if (e != DecimalError::Ok) {
        bt::report_failure(__FILE__, __LINE__, std::string("expected ok for ") + s);
    }
    return v;
}
DecimalError err(const char* s) {
    std::int64_t v = 12345;
    const DecimalError e = parse_scaled_1e8(s, v);
    if (v != 12345) bt::report_failure(__FILE__, __LINE__, "output modified on error");
    return e;
}
}  // namespace

TEST(decimal_valid) {
    CHECK_EQ(ok("0"), 0);
    CHECK_EQ(ok("1"), 100000000);
    CHECK_EQ(ok("0.1"), 10000000);
    CHECK_EQ(ok("84156.70000000"), 8415670000000);
    CHECK_EQ(ok("84113.00"), 8411300000000);
    CHECK_EQ(ok("0.097"), 9700000);
    CHECK_EQ(ok("000123.4"), 12340000000);
    CHECK_EQ(ok("0.00000000"), 0);
}

TEST(decimal_eight_decimals) {
    CHECK_EQ(ok("0.00000001"), 1);
    CHECK_EQ(ok("1.23456789"), 123456789);
    CHECK_EQ(ok("4.34397000"), 434397000);
}

TEST(decimal_nine_decimals_rejected) {
    CHECK_EQ(err("0.000000001"), DecimalError::TooManyDecimals);
    CHECK_EQ(err("1.000000000"), DecimalError::TooManyDecimals);  // never truncated
    CHECK_EQ(err("1.123456789x"), DecimalError::BadChar);         // bad char wins
}

TEST(decimal_overflow) {
    // int64 max = 9223372036854775807 -> max integer part 92233720368.
    CHECK_EQ(ok("92233720368.54775807"), std::numeric_limits<std::int64_t>::max());
    CHECK_EQ(err("92233720368.54775808"), DecimalError::Overflow);
    CHECK_EQ(err("92233720369"), DecimalError::Overflow);
    CHECK_EQ(err("99999999999999999999999999"), DecimalError::Overflow);
}

TEST(decimal_empty) { CHECK_EQ(err(""), DecimalError::Empty); }

TEST(decimal_non_digit) {
    CHECK_EQ(err("-1"), DecimalError::BadChar);
    CHECK_EQ(err("+1"), DecimalError::BadChar);
    CHECK_EQ(err("1e5"), DecimalError::BadChar);
    CHECK_EQ(err("1.2.3"), DecimalError::BadChar);
    CHECK_EQ(err(".5"), DecimalError::BadChar);
    CHECK_EQ(err("1."), DecimalError::BadChar);
    CHECK_EQ(err("."), DecimalError::BadChar);
    CHECK_EQ(err(" 1"), DecimalError::BadChar);
    CHECK_EQ(err("1 "), DecimalError::BadChar);
    CHECK_EQ(err("1,5"), DecimalError::BadChar);
    CHECK_EQ(err("abc"), DecimalError::BadChar);
}

TEST(decimal_uint64_and_int64) {
    std::uint64_t u = 0;
    CHECK_EQ(bc::parse_uint64("11753932871878", u), DecimalError::Ok);
    CHECK_EQ(u, std::uint64_t{11753932871878});
    CHECK_EQ(bc::parse_uint64("18446744073709551615", u), DecimalError::Ok);
    CHECK_EQ(bc::parse_uint64("18446744073709551616", u), DecimalError::Overflow);
    CHECK_EQ(bc::parse_uint64("", u), DecimalError::Empty);
    CHECK_EQ(bc::parse_uint64("12a", u), DecimalError::BadChar);
    std::int64_t i = 0;
    CHECK_EQ(bc::parse_int64("-9223372036854775808", i), DecimalError::Ok);
    CHECK_EQ(i, std::numeric_limits<std::int64_t>::min());
    CHECK_EQ(bc::parse_int64("9223372036854775807", i), DecimalError::Ok);
    CHECK_EQ(bc::parse_int64("9223372036854775808", i), DecimalError::Overflow);
    CHECK_EQ(bc::parse_int64("-", i), DecimalError::BadChar);
}
