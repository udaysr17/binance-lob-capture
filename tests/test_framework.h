#pragma once

// Minimal self-contained test harness (no third-party test framework is
// available in the allowed dependency set).

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace bt {

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> r;
    return r;
}

inline int& failures() {
    static int f = 0;
    return f;
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};

inline void report_failure(const char* file, int line, const std::string& msg) {
    ++failures();
    std::fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, msg.c_str());
}

template <typename T>
std::string show(const T& v) {
    if constexpr (std::is_convertible_v<T, std::string_view>) {
        return "\"" + std::string(std::string_view(v)) + "\"";
    } else if constexpr (std::is_same_v<T, char>) {
        return std::string("'") + v + "'";
    } else if constexpr (std::is_enum_v<T>) {
        return std::to_string(static_cast<long long>(v));
    } else if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    } else {
        return std::to_string(v);
    }
}

}  // namespace bt

#define BT_CONCAT2(a, b) a##b
#define BT_CONCAT(a, b) BT_CONCAT2(a, b)

#define TEST(name)                                                   \
    static void name();                                              \
    static const ::bt::Registrar BT_CONCAT(bt_reg_, name)(#name, &name); \
    static void name()

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) ::bt::report_failure(__FILE__, __LINE__, "CHECK(" #cond ")"); \
    } while (0)

#define CHECK_EQ(a, b)                                                              \
    do {                                                                            \
        const auto& bt_a = (a);                                                     \
        const auto& bt_b = (b);                                                     \
        if (!(bt_a == bt_b)) {                                                      \
            ::bt::report_failure(__FILE__, __LINE__,                                \
                                 "CHECK_EQ(" #a ", " #b "): " + ::bt::show(bt_a) +  \
                                     " != " + ::bt::show(bt_b));                    \
        }                                                                           \
    } while (0)
