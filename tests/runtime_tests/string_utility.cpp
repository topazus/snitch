#include "testing.hpp"

#include <cmath>

using namespace std::literals;

namespace {
constexpr std::size_t max_length = 20u;

using string_type = snitch::small_string<max_length>;

enum class enum_type { value1 = 0, value2 = 12 };

using function_ptr_type1 = void (*)();
using function_ptr_type2 = void (*)(int);

void foo() {}
} // namespace

TEMPLATE_TEST_CASE(
    "append",
    "[utility]",
    int,
    unsigned int,
    std::ptrdiff_t,
    std::size_t,
    float,
    double,
    bool,
    void*,
    const void*,
    std::nullptr_t,
    std::string_view,
    enum_type,
    function_ptr_type1,
    function_ptr_type2) {

    auto create_value = []() -> std::pair<TestType, std::string_view> {
        if constexpr (std::is_same_v<TestType, int>) {
            return {-112, "-112"sv};
        } else if constexpr (std::is_same_v<TestType, unsigned int>) {
            return {203u, "203"sv};
        } else if constexpr (std::is_same_v<TestType, std::ptrdiff_t>) {
            return {-546876, "-546876"sv};
        } else if constexpr (std::is_same_v<TestType, std::size_t>) {
            return {26545u, "26545"sv};
        } else if constexpr (std::is_same_v<TestType, float>) {
            return {3.1415f, "3.141500e+00"sv};
        } else if constexpr (std::is_same_v<TestType, double>) {
            return {-0.0001, "-1.000000e-04"sv};
        } else if constexpr (std::is_same_v<TestType, bool>) {
            return {true, "true"sv};
        } else if constexpr (std::is_same_v<TestType, void*>) {
            static int i = 0;
            return {&i, "0x"sv};
        } else if constexpr (std::is_same_v<TestType, const void*>) {
            static const int i = 0;
            return {&i, "0x"sv};
        } else if constexpr (std::is_same_v<TestType, std::nullptr_t>) {
            return {{}, "nullptr"};
        } else if constexpr (std::is_same_v<TestType, std::string_view>) {
            return {"hello"sv, "hello"sv};
        } else if constexpr (std::is_same_v<TestType, enum_type>) {
            return {enum_type::value2, "12"sv};
        } else if constexpr (std::is_same_v<TestType, function_ptr_type1>) {
            return {&foo, "0x????????"sv};
        } else if constexpr (std::is_same_v<TestType, function_ptr_type2>) {
            return {nullptr, "nullptr"sv};
        }
    };

    SECTION("on empty") {
        string_type s;

        auto [value, expected] = create_value();
        CHECK(append(s, value));

        if constexpr (
            !std::is_pointer_v<TestType> || std::is_function_v<std::remove_pointer_t<TestType>>) {
            CHECK(std::string_view(s) == expected);
        } else {
#if defined(SNITCH_COMPILER_MSVC)
            CHECK(std::string_view(s).size() > 0u);
#else
            CHECK(std::string_view(s).starts_with(expected));
#endif
        }
    }

    SECTION("on partially full") {
        std::string_view initial = "abcdefghijklmnopqr"sv;
        string_type      s       = initial;

        auto [value, expected] = create_value();
        CHECK(!append(s, value));
        CHECK(std::string_view(s).starts_with(initial));
        if constexpr (
            (std::is_arithmetic_v<TestType> && !std::is_same_v<TestType, bool>) ||
            (std::is_pointer_v<TestType> && !std::is_function_v<std::remove_pointer_t<TestType>>) ||
            std::is_enum_v<TestType>) {
            // We are stuck with snprintf, which insists on writing a null-terminator character,
            // therefore we loose one character at the end.
            CHECK(s.size() == max_length - 1u);
            CHECK(expected.substr(0, 1) == std::string_view(s).substr(s.size() - 1, 1));
        } else {
            CHECK(s.size() == max_length);
            CHECK(expected.substr(0, 2) == std::string_view(s).substr(s.size() - 2, 2));
        }
    }

    SECTION("on full") {
        std::string_view initial = "abcdefghijklmnopqrst"sv;
        string_type      s       = initial;

        auto [value, expected] = create_value();
        CHECK(!append(s, value));
        CHECK(std::string_view(s) == initial);
    }
}

namespace append_test {
struct append_expected {
    std::string_view str;
    bool             success    = true;
    bool             start_with = false;
};

struct append_expected_diff {
    append_expected str_constexpr;
    append_expected str_fast;
};

template<std::size_t N>
struct append_result {
    snitch::small_string<N> str;
    bool                    success = true;

    constexpr bool operator==(const append_expected& o) const {
        return success == o.success &&
               (o.start_with ? std::string_view(str.begin(), str.end()).starts_with(o.str)
                             : str == o.str);
    }
};

template<std::size_t N>
struct append_result2 {
    std::optional<append_result<N>> str_constexpr;
    std::optional<append_result<N>> str_fast;

    constexpr bool operator==(const append_expected& o) const {
        return (str_constexpr.has_value() ? str_constexpr.value() == o : true) &&
               (str_fast.has_value() ? str_fast.value() == o : true);
    }

    constexpr bool operator==(const append_expected_diff& o) const {
        return (str_constexpr.has_value() ? str_constexpr.value() == o.str_constexpr : true) &&
               (str_fast.has_value() ? str_fast.value() == o.str_fast : true);
    }
};

template<std::size_t N, bool TestConstexpr, typename T>
constexpr append_result2<N> to_string(const T& value) {
    if (std::is_constant_evaluated()) {
        snitch::small_string<N> str;
        bool                    success = append(str, value);
        return {{{str, success}}, {}};
    } else {
        if constexpr (TestConstexpr) {
            snitch::small_string<N> str1, str2;
            bool                    success1 = append(str1, value);
            bool                    success2 = snitch::impl::append_constexpr(str2, value);
            return {{{str2, success2}}, {{str1, success1}}};
        } else {
            snitch::small_string<N> str;
            bool                    success = append(str, value);
            return {{}, {{str, success}}};
        }
    }
}

#if defined(SNITCH_TEST_WITH_SNITCH)
template<std::size_t N>
constexpr bool append(snitch::small_string_span s, const append_result<N>& r) {
    return append(s, "{", r.str, ",", r.success, "}");
}

template<std::size_t N>
constexpr bool append(snitch::small_string_span s, const append_result2<N>& r) {
    if (r.str_constexpr.has_value() && r.str_fast.has_value()) {
        return append(s, "{", r.str_constexpr.value(), ",", r.str_fast.value(), "}");
    } else if (r.str_constexpr.has_value()) {
        return append(s, r.str_constexpr.value());
    } else if (r.str_fast.has_value()) {
        return append(s, r.str_fast.value());
    } else {
        return append(s, "{}");
    }
}

constexpr bool append(snitch::small_string_span s, const append_expected& r) {
    return append(s, "{", r.str, ",", r.success, "}");
}

constexpr bool append(snitch::small_string_span s, const append_expected_diff& r) {
    return append(s, "{", r.str_constexpr, ",", r.str_fast, "}");
}
#endif
} // namespace append_test

TEST_CASE("constexpr append", "[utility]") {
    using ae  = append_test::append_expected;
    using aed = append_test::append_expected_diff;

    SECTION("strings do fit") {
        constexpr auto a = [](const auto& value) constexpr {
            return append_test::to_string<21, true>(value);
        };

        CONSTEXPR_CHECK(a(""sv) == ae{""sv, true});
        CONSTEXPR_CHECK(a("a"sv) == ae{"a"sv, true});
        CONSTEXPR_CHECK(a("abcd"sv) == ae{"abcd"sv, true});
    }

    SECTION("strings don't fit") {
        constexpr auto a = [](const auto& value) constexpr {
            return append_test::to_string<5, true>(value);
        };

        CONSTEXPR_CHECK(a("abcdefghijklmnopqrst"sv) == ae{"abcde"sv, false});
    }

    SECTION("booleans do fit") {
        constexpr auto a = [](const auto& value) constexpr {
            return append_test::to_string<21, false>(value);
        };

        CONSTEXPR_CHECK(a(true) == ae{"true"sv, true});
        CONSTEXPR_CHECK(a(false) == ae{"false"sv, true});
    }

    SECTION("booleans don't fit") {
        constexpr auto a = [](const auto& value) constexpr {
            return append_test::to_string<3, false>(value);
        };

        CONSTEXPR_CHECK(a(true) == ae{"tru"sv, false});
        CONSTEXPR_CHECK(a(false) == ae{"fal"sv, false});
    }

    SECTION("nullptr do fit") {
        constexpr auto a = [](const auto& value) constexpr {
            return append_test::to_string<21, true>(value);
        };

        CONSTEXPR_CHECK(a(nullptr) == ae{"nullptr"sv, true});
    }

    SECTION("nullptr don't fit") {
        constexpr auto a = [](const auto& value) constexpr {
            return append_test::to_string<3, true>(value);
        };

        CONSTEXPR_CHECK(a(nullptr) == ae{"nul"sv, false});
    }

    SECTION("pointers do fit") {
        constexpr auto a = [](const auto& value) constexpr {
            return append_test::to_string<21, true>(static_cast<void*>(value));
        };

        struct b {
            int            i = 0;
            constexpr int* get() {
                return &i;
            }
        };

        CONSTEXPR_CHECK(a(nullptr) == ae{"nullptr"sv, true});
        CONSTEXPR_CHECK(a(b{}.get()) == aed{{"0x????????"sv, true}, {"0x"sv, true, true}});
    }

    SECTION("pointers don't fit") {
        constexpr auto a = [](const auto& value) constexpr {
            return append_test::to_string<5, true>(value);
        };

        struct b {
            int            i = 0;
            constexpr int* get() {
                return &i;
            }
        };

        CONSTEXPR_CHECK(a(nullptr) == ae{"nullp"sv, false});
        CONSTEXPR_CHECK(a(b{}.get()) == aed{{"0x???"sv, false}, {"0x"sv, false, true}});
    }

    SECTION("integers do fit") {
        constexpr auto a = [](const auto& value) constexpr {
            if constexpr (std::is_signed_v<std::decay_t<decltype(value)>>) {
                return append_test::to_string<21, true>(static_cast<std::ptrdiff_t>(value));
            } else {
                return append_test::to_string<21, true>(static_cast<std::size_t>(value));
            }
        };

        CONSTEXPR_CHECK(a(0) == ae{"0"sv, true});
        CONSTEXPR_CHECK(a(0u) == ae{"0"sv, true});
        CONSTEXPR_CHECK(a(1) == ae{"1"sv, true});
        CONSTEXPR_CHECK(a(-1) == ae{"-1"sv, true});
        CONSTEXPR_CHECK(a(1u) == ae{"1"sv, true});
        CONSTEXPR_CHECK(a(9) == ae{"9"sv, true});
        CONSTEXPR_CHECK(a(-9) == ae{"-9"sv, true});
        CONSTEXPR_CHECK(a(9u) == ae{"9"sv, true});
        CONSTEXPR_CHECK(a(10) == ae{"10"sv, true});
        CONSTEXPR_CHECK(a(-10) == ae{"-10"sv, true});
        CONSTEXPR_CHECK(a(10u) == ae{"10"sv, true});
        CONSTEXPR_CHECK(a(15) == ae{"15"sv, true});
        CONSTEXPR_CHECK(a(-15) == ae{"-15"sv, true});
        CONSTEXPR_CHECK(a(15u) == ae{"15"sv, true});
        CONSTEXPR_CHECK(a(115) == ae{"115"sv, true});
        CONSTEXPR_CHECK(a(-115) == ae{"-115"sv, true});
        CONSTEXPR_CHECK(a(115u) == ae{"115"sv, true});
        CONSTEXPR_CHECK(a(10005) == ae{"10005"sv, true});
        CONSTEXPR_CHECK(a(-10005) == ae{"-10005"sv, true});
        CONSTEXPR_CHECK(a(10005u) == ae{"10005"sv, true});

        // Limits 32bit
        if constexpr (sizeof(std::size_t) >= 4) {
            CONSTEXPR_CHECK(a(4294967295u) == ae{"4294967295"sv, true});
            CONSTEXPR_CHECK(a(2147483647) == ae{"2147483647"sv, true});
            // NB: "-2147483648" does not work as an integer literal even though it is
            // representable, because "-" and "2147483648" are treated as two tokens,
            // and the latter (as positive integer) isn't representable. Hence the trick below.
            // https://stackoverflow.com/a/65008288/1565581
            CONSTEXPR_CHECK(a(-2147483647 - 1) == ae{"-2147483648"sv, true});
        }

        // Limits 64bit
        if constexpr (sizeof(std::size_t) >= 8) {
            CONSTEXPR_CHECK(a(18446744073709551615u) == ae{"18446744073709551615"sv, true});
            CONSTEXPR_CHECK(a(9223372036854775807) == ae{"9223372036854775807"sv, true});
            // NB: "-9223372036854775808" does not work as an integer literal even though it is
            // representable, because "-" and "9223372036854775808" are treated as two tokens,
            // and the latter (as positive integer) isn't representable. Hence the trick below.
            // https://stackoverflow.com/a/65008288/1565581
            CONSTEXPR_CHECK(a(-9223372036854775807 - 1) == ae{"-9223372036854775808"sv, true});
        }
    }

    SECTION("integers don't fit") {
        constexpr auto a = [](const auto& value) constexpr {
            if constexpr (std::is_signed_v<std::decay_t<decltype(value)>>) {
                return append_test::to_string<5, true>(static_cast<std::ptrdiff_t>(value));
            } else {
                return append_test::to_string<5, true>(static_cast<std::size_t>(value));
            }
        };

        // Different expectation at runtime and compile-time. At runtime,
        // we are stuck with snprintf, which insists on writing a null-terminator character,
        // therefore we loose one character at the end.
        CONSTEXPR_CHECK(a(123456) == aed{{"12345"sv, false}, {"1234"sv, false}});
        CONSTEXPR_CHECK(a(1234567) == aed{{"12345"sv, false}, {"1234"sv, false}});
        CONSTEXPR_CHECK(a(12345678) == aed{{"12345"sv, false}, {"1234"sv, false}});
        CONSTEXPR_CHECK(a(-12345) == aed{{"-1234"sv, false}, {"-123"sv, false}});
        CONSTEXPR_CHECK(a(-123456) == aed{{"-1234"sv, false}, {"-123"sv, false}});
        CONSTEXPR_CHECK(a(-1234567) == aed{{"-1234"sv, false}, {"-123"sv, false}});
        CONSTEXPR_CHECK(a(123456u) == aed{{"12345"sv, false}, {"1234"sv, false}});
        CONSTEXPR_CHECK(a(1234567u) == aed{{"12345"sv, false}, {"1234"sv, false}});
        CONSTEXPR_CHECK(a(12345678u) == aed{{"12345"sv, false}, {"1234"sv, false}});
    }

    SECTION("floats do fit") {
        constexpr auto a = [](const auto& value) constexpr {
            return append_test::to_string<21, true>(value);
        };

        CONSTEXPR_CHECK(a(0.0f) == ae{"0.000000e+00"sv, true});
#if SNITCH_CONSTEXPR_FLOAT_USE_BITCAST
        CONSTEXPR_CHECK(a(-0.0f) == ae{"-0.000000e+00"sv, true});
#else
        // Without std::bit_cast (or C++23), we are unable to tell the difference between -0.0f and
        // +0.0f in constexpr expressions. Therefore -0.0f in constexpr gets displayed as +0.0f.
        CONSTEXPR_CHECK(a(-0.0f) == aed{{"0.000000e+00"sv, true}, {"-0.000000e+00"sv, true}});
#endif
        CONSTEXPR_CHECK(a(1.0f) == ae{"1.000000e+00"sv, true});
        CONSTEXPR_CHECK(a(1.5f) == ae{"1.500000e+00"sv, true});
        CONSTEXPR_CHECK(a(1.51f) == ae{"1.510000e+00"sv, true});
        CONSTEXPR_CHECK(a(1.501f) == ae{"1.501000e+00"sv, true});
        CONSTEXPR_CHECK(a(1.5001f) == ae{"1.500100e+00"sv, true});
        CONSTEXPR_CHECK(a(1.50001f) == ae{"1.500010e+00"sv, true});
        CONSTEXPR_CHECK(a(1.500001f) == ae{"1.500001e+00"sv, true});
        CONSTEXPR_CHECK(a(-1.0f) == ae{"-1.000000e+00"sv, true});
        CONSTEXPR_CHECK(a(10.0f) == ae{"1.000000e+01"sv, true});
        CONSTEXPR_CHECK(a(1e4f) == ae{"1.000000e+04"sv, true});
        CONSTEXPR_CHECK(a(2.3456e28f) == ae{"2.345600e+28"sv, true});
        CONSTEXPR_CHECK(a(-2.3456e28f) == ae{"-2.345600e+28"sv, true});
        CONSTEXPR_CHECK(a(2.3456e-28f) == ae{"2.345600e-28"sv, true});
        CONSTEXPR_CHECK(a(-2.3456e-28f) == ae{"-2.345600e-28"sv, true});
        CONSTEXPR_CHECK(a(2.3456e-42f) == ae{"2.345774e-42"sv, true});
        CONSTEXPR_CHECK(a(-2.3456e-42f) == ae{"-2.345774e-42"sv, true});
        CONSTEXPR_CHECK(a(std::numeric_limits<float>::infinity()) == ae{"inf"sv, true});
        CONSTEXPR_CHECK(a(-std::numeric_limits<float>::infinity()) == ae{"-inf"sv, true});
        CONSTEXPR_CHECK(a(std::numeric_limits<float>::quiet_NaN()) == ae{"nan"sv, true});

        // Test that the rounding mode is the same as std::printf.
        CONSTEXPR_CHECK(a(1.0000001f) == ae{"1.000000e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000002f) == ae{"1.000000e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000003f) == ae{"1.000000e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000004f) == ae{"1.000000e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000005f) == ae{"1.000000e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000006f) == ae{"1.000001e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000007f) == ae{"1.000001e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000008f) == ae{"1.000001e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000009f) == ae{"1.000001e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000010f) == ae{"1.000001e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000011f) == ae{"1.000001e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000012f) == ae{"1.000001e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000013f) == ae{"1.000001e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000014f) == ae{"1.000001e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000015f) == ae{"1.000002e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000016f) == ae{"1.000002e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000017f) == ae{"1.000002e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000018f) == ae{"1.000002e+00"sv, true});
        CONSTEXPR_CHECK(a(1.0000019f) == ae{"1.000002e+00"sv, true});
    }

    SECTION("floats don't fit") {
        constexpr auto a = [](const auto& value) constexpr {
            return append_test::to_string<5, true>(value);
        };

        // Different expectation at runtime and compile-time. At runtime,
        // we are stuck with snprintf, which insists on writing a null-terminator character,
        // therefore we loose one character at the end.
        CONSTEXPR_CHECK(a(0.0f) == aed{{"0.000"sv, false}, {"0.00"sv, false}});
        CONSTEXPR_CHECK(a(-1.0f) == aed{{"-1.00"sv, false}, {"-1.0"sv, false}});
    }

#if 0
    // This takes a long time, and a few floats (0.05%) don't match exactly.
    SECTION("constexpr floats match printf(%e)") {
        const float mi = -std::numeric_limits<float>::max();
        const float ma = std::numeric_limits<float>::max();

        snitch::small_string<21> ssc;
        snitch::small_string<21> ssr;

        // Iterate over all float point values between 'mi' and 'ma'
        std::size_t k = 0, b = 0;
        for (float f = mi; f < ma; f = std::nextafter(f, ma), ++k) {
            ssc.clear();
            ssr.clear();

            (void)snitch::impl::append_constexpr(ssc, f);
            (void)snitch::impl::append_fast(ssr, f);

            auto svc = std::string_view(ssc.begin(), ssc.end());
            auto svr = std::string_view(ssr.begin(), ssr.end());
            if (svc != svr) {
                ++b;
            }
        }

        std::cout << static_cast<double>(b) / static_cast<double>(k) << std::endl;
    }
#endif
}

TEST_CASE("append multiple", "[utility]") {
    string_type s;

    SECTION("nothing") {
        CHECK(append(s, "", "", "", ""));
        CHECK(std::string_view(s) == ""sv);
    }

    SECTION("enough space") {
        CHECK(append(s, "int=", 123456));
        CHECK(std::string_view(s) == "int=123456"sv);
    }

    SECTION("just enough space") {
        CHECK(append(s, "int=", 123456, " bool=", true));
        CHECK(std::string_view(s) == "int=123456 bool=true"sv);
    }

    SECTION("not enough space between arguments") {
        CHECK(!append(s, "int=", 123456, " bool=", true, " float=", 3.1415));
        CHECK(std::string_view(s) == "int=123456 bool=true"sv);
    }

    SECTION("not enough space in middle of argument") {
        CHECK(!append(s, "int=", 123456, ", bool=", true));
        CHECK(std::string_view(s) == "int=123456, bool=tru"sv);
    }
}

TEMPLATE_TEST_CASE(
    "truncate_end",
    "[utility]",
    snitch::small_string<1>,
    snitch::small_string<2>,
    snitch::small_string<3>,
    snitch::small_string<4>,
    snitch::small_string<5>) {

    TestType s;

    SECTION("on empty") {
        truncate_end(s);

        CHECK(s.size() == std::min<std::size_t>(s.capacity(), 3u));
        CHECK(std::string_view(s) == "..."sv.substr(0, s.size()));
    }

    SECTION("on non-empty") {
        s = "a"sv;
        truncate_end(s);

        CHECK(s.size() == std::min<std::size_t>(s.capacity(), 4u));
        if (s.capacity() > 3) {
            CHECK(std::string_view(s) == "a...");
        } else {
            CHECK(std::string_view(s) == "..."sv.substr(0, s.size()));
        }
    }

    SECTION("on full") {
        s = "abcde"sv.substr(0, s.capacity());
        truncate_end(s);

        CHECK(s.size() == s.capacity());
        if (s.capacity() > 3) {
            CAPTURE(s);
            CHECK(std::string_view(s).starts_with("abcde"sv.substr(0, s.capacity() - 3u)));
            CHECK(std::string_view(s).ends_with("..."));
        } else {
            CHECK(std::string_view(s) == "..."sv.substr(0, s.size()));
        }
    }
}

TEMPLATE_TEST_CASE(
    "append_or_truncate",
    "[utility]",
    snitch::small_string<1>,
    snitch::small_string<2>,
    snitch::small_string<3>,
    snitch::small_string<4>,
    snitch::small_string<5>,
    snitch::small_string<6>) {

    TestType s;
    append_or_truncate(s, "i=", "1", "+", "2");

    if (s.capacity() >= 5) {
        CHECK(std::string_view(s) == "i=1+2");
    } else if (s.capacity() > 3) {
        CAPTURE(s);
        CHECK(std::string_view(s).starts_with("i=1+2"sv.substr(0, s.capacity() - 3u)));
        CHECK(std::string_view(s).ends_with("..."));
    } else {
        CHECK(std::string_view(s) == "..."sv.substr(0, s.capacity()));
    }
}

TEMPLATE_TEST_CASE(
    "replace_all",
    "[utility]",
    snitch::small_string<5>,
    snitch::small_string<6>,
    snitch::small_string<7>,
    snitch::small_string<8>,
    snitch::small_string<9>,
    snitch::small_string<10>) {
    TestType s;

    SECTION("same size different value") {
        s = "abaca"sv;
        CHECK(replace_all(s, "a", "b"));
        CHECK(std::string_view(s) == "bbbcb");
    }

    SECTION("same size same value") {
        s = "abaca"sv;
        CHECK(replace_all(s, "a", "a"));
        CHECK(std::string_view(s) == "abaca");
    }

    SECTION("same size no match") {
        s = "abaca"sv;
        CHECK(replace_all(s, "t", "a"));
        CHECK(std::string_view(s) == "abaca");
    }

    SECTION("same size with pattern bigger than capacity") {
        s = "abaca"sv;
        CHECK(replace_all(s, "abacaabcdefghijklmqrst", "tsrqmlkjihgfedcbaacaba"));
        CAPTURE(std::string_view(s));
        CHECK(std::string_view(s) == "abaca");
    }

    SECTION("smaller different value") {
        s = "atata"sv;
        CHECK(replace_all(s, "ta", "c"));
        CHECK(std::string_view(s) == "acc");
        s = "atata"sv;
        CHECK(replace_all(s, "at", "c"));
        CHECK(std::string_view(s) == "cca");
    }

    SECTION("smaller same value") {
        s = "atata"sv;
        CHECK(replace_all(s, "ta", "t"));
        CHECK(std::string_view(s) == "att");
        s = "taata"sv;
        CHECK(replace_all(s, "ta", "t"));
        CHECK(std::string_view(s) == "tat");
        s = "atata"sv;
        CHECK(replace_all(s, "at", "a"));
        CHECK(std::string_view(s) == "aaa");
    }

    SECTION("smaller no match") {
        s = "abaca"sv;
        CHECK(replace_all(s, "ta", "a"));
        CHECK(std::string_view(s) == "abaca");
    }

    SECTION("smaller with pattern bigger than capacity") {
        s = "abaca"sv;
        CHECK(replace_all(s, "abacaabcdefghijklmqrst", "a"));
        CHECK(std::string_view(s) == "abaca");
    }

    SECTION("smaller with replacement bigger than capacity") {
        s = "abaca"sv;
        CHECK(replace_all(s, "abcdefghijklmnopqrstabcdefghijklmnopqrst", "abcdefghijklmnopqrst"));
        CHECK(std::string_view(s) == "abaca");
    }

    SECTION("bigger different value") {
        s = "abaca"sv;

        bool success = replace_all(s, "a", "bb");
        if (s.capacity() >= 8u) {
            CHECK(success);
            CHECK(std::string_view(s) == "bbbbbcbb");
        } else {
            CHECK(!success);
            CHECK(std::string_view(s) == "bbbbbcbb"sv.substr(0, s.capacity()));
        }

        s = "ababa"sv;

        success = replace_all(s, "b", "aa");
        if (s.capacity() >= 7u) {
            CHECK(success);
            CHECK(std::string_view(s) == "aaaaaaa");
        } else {
            CHECK(!success);
            CHECK(std::string_view(s) == "aaaaaaa"sv.substr(0, s.capacity()));
        }
    }

    SECTION("bigger same value") {
        s = "abaca"sv;

        bool success = replace_all(s, "a", "aa");
        if (s.capacity() >= 8u) {
            CHECK(success);
            CHECK(std::string_view(s) == "aabaacaa");
        } else {
            CHECK(!success);
            CHECK(std::string_view(s) == "aabaacaa"sv.substr(0, s.capacity()));
        }

        s = "ababa"sv;

        success = replace_all(s, "b", "bb");
        if (s.capacity() >= 7u) {
            CHECK(success);
            CHECK(std::string_view(s) == "abbabba");
        } else {
            CHECK(!success);
            CHECK(std::string_view(s) == "abbabba"sv.substr(0, s.capacity()));
        }
    }

    SECTION("bigger no match") {
        s = "abaca"sv;
        CHECK(replace_all(s, "t", "aa"));
        CHECK(std::string_view(s) == "abaca");
    }

    SECTION("bigger with replacement bigger than capacity") {
        s = "abaca"sv;
        CHECK(!replace_all(s, "a", "abcdefghijklmnopqrst"));
        CHECK(std::string_view(s) == "abcdefghijklmnopqrst"sv.substr(0, s.capacity()));
    }

    SECTION("bigger with pattern bigger than capacity") {
        s = "abaca"sv;
        CHECK(replace_all(s, "abacaabcdefghijklmqrst", "abcdefghijklmnopqrstabcdefghijklmnopqrst"));
        CHECK(std::string_view(s) == "abaca");
    }
}

TEST_CASE("is_match", "[utility]") {
    SECTION("empty") {
        CHECK(snitch::is_match(""sv, ""sv));
    }

    SECTION("empty regex") {
        CHECK(snitch::is_match("abc"sv, ""sv));
    }

    SECTION("empty string") {
        CHECK(!snitch::is_match(""sv, "abc"sv));
    }

    SECTION("no wildcard match") {
        CHECK(snitch::is_match("abc"sv, "abc"sv));
    }

    SECTION("no wildcard not match") {
        CHECK(!snitch::is_match("abc"sv, "cba"sv));
    }

    SECTION("no wildcard not match smaller regex") {
        CHECK(!snitch::is_match("abc"sv, "ab"sv));
        CHECK(!snitch::is_match("abc"sv, "bc"sv));
        CHECK(!snitch::is_match("abc"sv, "a"sv));
        CHECK(!snitch::is_match("abc"sv, "b"sv));
        CHECK(!snitch::is_match("abc"sv, "c"sv));
    }

    SECTION("no wildcard not match larger regex") {
        CHECK(!snitch::is_match("abc"sv, "abcd"sv));
        CHECK(!snitch::is_match("abc"sv, "zabc"sv));
        CHECK(!snitch::is_match("abc"sv, "abcdefghijkl"sv));
    }

    SECTION("single wildcard match") {
        CHECK(snitch::is_match("abc"sv, "*"sv));
        CHECK(snitch::is_match("azzzzzzzzzzbc"sv, "*"sv));
        CHECK(snitch::is_match(""sv, "*"sv));
    }

    SECTION("start wildcard match") {
        CHECK(snitch::is_match("abc"sv, "*bc"sv));
        CHECK(snitch::is_match("azzzzzzzzzzbc"sv, "*bc"sv));
        CHECK(snitch::is_match("bc"sv, "*bc"sv));
    }

    SECTION("start wildcard not match") {
        CHECK(!snitch::is_match("abd"sv, "*bc"sv));
        CHECK(!snitch::is_match("azzzzzzzzzzbd"sv, "*bc"sv));
        CHECK(!snitch::is_match("bd"sv, "*bc"sv));
    }

    SECTION("end wildcard match") {
        CHECK(snitch::is_match("abc"sv, "ab*"sv));
        CHECK(snitch::is_match("abccccccccccc"sv, "ab*"sv));
        CHECK(snitch::is_match("ab"sv, "ab*"sv));
    }

    SECTION("end wildcard not match") {
        CHECK(!snitch::is_match("adc"sv, "ab*"sv));
        CHECK(!snitch::is_match("adccccccccccc"sv, "ab*"sv));
        CHECK(!snitch::is_match("ad"sv, "ab*"sv));
    }

    SECTION("mid wildcard match") {
        CHECK(snitch::is_match("ab_cd"sv, "ab*cd"sv));
        CHECK(snitch::is_match("abasdasdasdcd"sv, "ab*cd"sv));
        CHECK(snitch::is_match("abcd"sv, "ab*cd"sv));
    }

    SECTION("mid wildcard not match") {
        CHECK(!snitch::is_match("adcd"sv, "ab*cd"sv));
        CHECK(!snitch::is_match("abcc"sv, "ab*cd"sv));
        CHECK(!snitch::is_match("accccccccccd"sv, "ab*cd"sv));
        CHECK(!snitch::is_match("ab"sv, "ab*cd"sv));
        CHECK(!snitch::is_match("abc"sv, "ab*cd"sv));
        CHECK(!snitch::is_match("abd"sv, "ab*cd"sv));
        CHECK(!snitch::is_match("cd"sv, "ab*cd"sv));
        CHECK(!snitch::is_match("bcd"sv, "ab*cd"sv));
        CHECK(!snitch::is_match("acd"sv, "ab*cd"sv));
    }

    SECTION("multi wildcard match") {
        CHECK(snitch::is_match("zab_cdw"sv, "*ab*cd*"sv));
        CHECK(snitch::is_match("zzzzzzabcccccccccccdwwwwwww"sv, "*ab*cd*"sv));
        CHECK(snitch::is_match("abcd"sv, "*ab*cd*"sv));
        CHECK(snitch::is_match("ab_cdw"sv, "*ab*cd*"sv));
        CHECK(snitch::is_match("zabcdw"sv, "*ab*cd*"sv));
        CHECK(snitch::is_match("zab_cd"sv, "*ab*cd*"sv));
        CHECK(snitch::is_match("abcd"sv, "*ab*cd*"sv));
        CHECK(snitch::is_match("ababcd"sv, "*ab*cd*"sv));
        CHECK(snitch::is_match("abcdabcd"sv, "*ab*cd*"sv));
        CHECK(snitch::is_match("abcdabcc"sv, "*ab*cd*"sv));
    }

    SECTION("multi wildcard not match") {
        CHECK(!snitch::is_match("zad_cdw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zac_cdw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zaa_cdw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zdb_cdw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zcb_cdw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zbb_cdw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zab_ddw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zab_bdw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zab_adw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zab_ccw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zab_cbw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zab_caw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zab_"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("zab"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("ab_"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("ab"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("_cdw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("cdw"sv, "*ab*cd*"sv));
        CHECK(!snitch::is_match("cd"sv, "*ab*cd*"sv));
    }

    SECTION("double wildcard match") {
        CHECK(snitch::is_match("abc"sv, "**"sv));
        CHECK(snitch::is_match("azzzzzzzzzzbc"sv, "**"sv));
        CHECK(snitch::is_match(""sv, "**"sv));
        CHECK(snitch::is_match("abcdefg"sv, "*g*******"sv));
        CHECK(snitch::is_match("abc"sv, "abc**"sv));
        CHECK(snitch::is_match("abc"sv, "ab**"sv));
        CHECK(snitch::is_match("abc"sv, "a**"sv));
        CHECK(snitch::is_match("abc"sv, "**abc"sv));
        CHECK(snitch::is_match("abc"sv, "**bc"sv));
        CHECK(snitch::is_match("abc"sv, "**c"sv));
        CHECK(snitch::is_match("abc"sv, "ab**c"sv));
        CHECK(snitch::is_match("abc"sv, "a**bc"sv));
        CHECK(snitch::is_match("abc"sv, "a**c"sv));
    }

    SECTION("double wildcard not match") {
        CHECK(!snitch::is_match("abc"sv, "abd**"sv));
        CHECK(!snitch::is_match("abc"sv, "ad**"sv));
        CHECK(!snitch::is_match("abc"sv, "d**"sv));
        CHECK(!snitch::is_match("abc"sv, "**abd"sv));
        CHECK(!snitch::is_match("abc"sv, "**bd"sv));
        CHECK(!snitch::is_match("abc"sv, "**d"sv));
        CHECK(!snitch::is_match("abc"sv, "ab**d"sv));
        CHECK(!snitch::is_match("abc"sv, "a**d"sv));
        CHECK(!snitch::is_match("abc"sv, "abc**abc"sv));
        CHECK(!snitch::is_match("abc"sv, "abc**ab"sv));
        CHECK(!snitch::is_match("abc"sv, "abc**a"sv));
        CHECK(!snitch::is_match("abc"sv, "abc**def"sv));
    }

    SECTION("string contains wildcard & escaped wildcard") {
        CHECK(snitch::is_match("a*c"sv, "a\\*c"sv));
        CHECK(snitch::is_match("a*"sv, "a\\*"sv));
        CHECK(snitch::is_match("*a"sv, "\\*a"sv));
        CHECK(snitch::is_match("a*"sv, "a*"sv));
        CHECK(snitch::is_match("a\\b"sv, "a\\\\b"sv));
        CHECK(snitch::is_match("a"sv, "\\a"sv));
        CHECK(!snitch::is_match("a"sv, "a\\"sv));
        CHECK(!snitch::is_match("a"sv, "a\\\\"sv));
        CHECK(!snitch::is_match("a"sv, "\\\\a"sv));
    }
}

using snitch::filter_result;
using snitch::is_filter_match_id;
using snitch::is_filter_match_name;
using snitch::is_filter_match_tags;

TEST_CASE("is_filter_match", "[utility]") {
    CHECK(is_filter_match_name("abc"sv, "abc"sv) == filter_result::included);
    CHECK(is_filter_match_name("abc"sv, "ab*"sv) == filter_result::included);
    CHECK(is_filter_match_name("abc"sv, "*bc"sv) == filter_result::included);
    CHECK(is_filter_match_name("abc"sv, "*"sv) == filter_result::included);
    CHECK(is_filter_match_name("abc"sv, "def"sv) == filter_result::not_included);
    CHECK(is_filter_match_name("abc"sv, "~abc"sv) == filter_result::excluded);
    CHECK(is_filter_match_name("abc"sv, "~ab*"sv) == filter_result::excluded);
    CHECK(is_filter_match_name("abc"sv, "~*bc"sv) == filter_result::excluded);
    CHECK(is_filter_match_name("abc"sv, "~*"sv) == filter_result::excluded);
    CHECK(is_filter_match_name("abc"sv, "~def"sv) == filter_result::not_excluded);
}

TEST_CASE("is_filter_match_tag", "[utility]") {
    CHECK(is_filter_match_tags("[tag1]"sv, "[tag1]"sv) == filter_result::included);
    CHECK(is_filter_match_tags("[tag1][tag2]"sv, "[tag1]"sv) == filter_result::included);
    CHECK(is_filter_match_tags("[tag1][tag2]"sv, "[tag2]"sv) == filter_result::included);
    CHECK(is_filter_match_tags("[tag1][tag2]"sv, "[tag*]"sv) == filter_result::included);
    CHECK(is_filter_match_tags("[tag1][tag2]"sv, "[tag*]"sv) == filter_result::included);
    CHECK(is_filter_match_tags("[tag1][tag2]"sv, "~[tug*]"sv) == filter_result::not_excluded);
    CHECK(is_filter_match_tags("[tag1][tag2][.]"sv, "[.]"sv) == filter_result::included);
    CHECK(is_filter_match_tags("[tag1][.tag2]"sv, "[.]"sv) == filter_result::included);
    CHECK(is_filter_match_tags("[.tag1][tag2]"sv, "[.]"sv) == filter_result::included);
    CHECK(is_filter_match_tags("[tag1][tag2]"sv, "~[.]"sv) == filter_result::not_excluded);
    CHECK(is_filter_match_tags("[tag1][!mayfail]"sv, "[!mayfail]"sv) == filter_result::included);
    CHECK(is_filter_match_tags("[tag1][tag2]"sv, "~[!mayfail]"sv) == filter_result::not_excluded);
    CHECK(
        is_filter_match_tags("[tag1][!shouldfail]"sv, "[!shouldfail]"sv) ==
        filter_result::included);
    CHECK(
        is_filter_match_tags("[tag1][tag2]"sv, "~[!shouldfail]"sv) == filter_result::not_excluded);

    CHECK(is_filter_match_tags("[tag1]"sv, "[tag2]"sv) == filter_result::not_included);
    CHECK(is_filter_match_tags("[tag1][tag2]"sv, "[tag3]"sv) == filter_result::not_included);
    CHECK(is_filter_match_tags("[tag1][tag2]"sv, "[tug*]*"sv) == filter_result::not_included);
    CHECK(is_filter_match_tags("[tag1][tag2]"sv, "[.]"sv) == filter_result::not_included);
    CHECK(is_filter_match_tags("[.tag1][tag2]"sv, "[.tag1]"sv) == filter_result::not_included);
    CHECK(is_filter_match_tags("[tag1][tag2][.]"sv, "[.tag1]"sv) == filter_result::not_included);
    CHECK(is_filter_match_tags("[tag1][tag2][.]"sv, "[.tag2]"sv) == filter_result::not_included);
    CHECK(is_filter_match_tags("[tag1][tag2]"sv, "~[tag1]"sv) == filter_result::excluded);
    CHECK(is_filter_match_tags("[tag1][tag2]"sv, "~[tag2]"sv) == filter_result::excluded);
}

TEST_CASE("is_filter_match_id", "[utility]") {
    CHECK(is_filter_match_id({"abc"sv, "[tag1][tag2]"sv}, "abc"sv) == filter_result::included);
    CHECK(is_filter_match_id({"abc"sv, "[tag1][tag2]"sv}, "~abc"sv) == filter_result::excluded);
    CHECK(is_filter_match_id({"abc"sv, "[tag1][tag2]"sv}, "ab*"sv) == filter_result::included);
    CHECK(is_filter_match_id({"abc"sv, "[tag1][tag2]"sv}, "[tag1]"sv) == filter_result::included);
    CHECK(is_filter_match_id({"abc"sv, "[tag1][tag2]"sv}, "[tag2]"sv) == filter_result::included);
    CHECK(
        is_filter_match_id({"abc"sv, "[tag1][tag2]"sv}, "[tag3]"sv) == filter_result::not_included);
    CHECK(
        is_filter_match_id({"abc"sv, "[tag1][tag2]"sv}, "~[tag3]"sv) ==
        filter_result::not_excluded);
    CHECK(
        is_filter_match_id({"[weird]"sv, "[tag1][tag2]"sv}, "\\[weird]"sv) ==
        filter_result::included);
    CHECK(
        is_filter_match_id({"[weird]"sv, "[tag1][tag2]"sv}, "[weird]"sv) ==
        filter_result::not_included);
}
