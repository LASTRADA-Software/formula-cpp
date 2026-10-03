// SPDX-License-Identifier: Apache-2.0
//
// formula::Int128: two's complement arithmetic on 128 bits, the same at
// compile time and at run time, and the same through the compiler's own
// 128-bit integer and through the portable code. Expected values were
// computed with Python's integers.
#include <formula-cpp/format.hpp>
#include <formula-cpp/int128.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <type_traits>

namespace
{
using formula::Int128;
using formula::detail::UInt128;

constexpr Int128 words(std::uint64_t highWord, std::uint64_t lowWord) noexcept
{
    return Int128::from_words(highWord, lowWord);
}

constexpr UInt128 unsigned_words(std::uint64_t highWord, std::uint64_t lowWord) noexcept
{
    return UInt128 { highWord, lowWord };
}

struct ArithmeticCase
{
    Int128 leftOperand;
    Int128 rightOperand;
    Int128 added;
    Int128 subtracted;
    Int128 multiplied;
    Int128 divided;
    Int128 remaining;
};

// Sums, differences and products wrap; quotients round toward zero; a
// remainder takes the dividend's sign.
constexpr std::array<ArithmeticCase, 9> arithmeticCases { {
    { words(0x7fffffffffffffff, 0xffffffffffffffff), words(0x0000000000000000, 0x0000000000000003),
      words(0x8000000000000000, 0x0000000000000002), words(0x7fffffffffffffff, 0xfffffffffffffffc),
      words(0x7fffffffffffffff, 0xfffffffffffffffd), words(0x2aaaaaaaaaaaaaaa, 0xaaaaaaaaaaaaaaaa),
      words(0x0000000000000000, 0x0000000000000001) },
    { words(0x8000000000000000, 0x0000000000000000), words(0x0000000000000000, 0x0000000000000007),
      words(0x8000000000000000, 0x0000000000000007), words(0x7fffffffffffffff, 0xfffffffffffffff9),
      words(0x8000000000000000, 0x0000000000000000), words(0xedb6db6db6db6db6, 0xdb6db6db6db6db6e),
      words(0xffffffffffffffff, 0xfffffffffffffffe) },
    { words(0x0000000000000001, 0x0000000000003039), words(0x0000000000000000, 0xffffffffffffffff),
      words(0x0000000000000002, 0x0000000000003038), words(0x0000000000000000, 0x000000000000303a),
      words(0x0000000000003037, 0xffffffffffffcfc7), words(0x0000000000000000, 0x0000000000000001),
      words(0x0000000000000000, 0x000000000000303a) },
    { words(0xfffffffe7116f009, 0x3c8c1f11b1c0f52e), words(0x0000000000000000, 0x0db4da5f49f8b478),
      words(0xfffffffe7116f009, 0x4a40f970fbb9a9a6), words(0xfffffffe7116f009, 0x2ed744b267c840b6),
      words(0x4b38a08ad7aa0090, 0x0e176230a1674590), words(0xffffffffffffffff, 0xffffffe2e56b6274),
      words(0xffffffffffffffff, 0xf326734031d13ece) },
    { words(0x7fffffffffffffff, 0xffffffffffffffff), words(0x8000000000000000, 0x0000000000000001),
      words(0x0000000000000000, 0x0000000000000000), words(0xffffffffffffffff, 0xfffffffffffffffe),
      words(0xffffffffffffffff, 0xffffffffffffffff), words(0xffffffffffffffff, 0xffffffffffffffff),
      words(0x0000000000000000, 0x0000000000000000) },
    { words(0x0000001000000000, 0x0000000000000001), words(0xffffffffffffffff, 0xffffffeffffffffb),
      words(0x0000000fffffffff, 0xffffffeffffffffc), words(0x0000001000000000, 0x0000001000000006),
      words(0xffffffafffffffff, 0xffffffeffffffffb), words(0xffffffffffffffff, 0x0000000050000000),
      words(0x0000000000000000, 0x0000000190000001) },
    { words(0xffffffffffffffff, 0xfffffffffffffffb), words(0x0000000000000000, 0x0000000000000003),
      words(0xffffffffffffffff, 0xfffffffffffffffe), words(0xffffffffffffffff, 0xfffffffffffffff8),
      words(0xffffffffffffffff, 0xfffffffffffffff1), words(0xffffffffffffffff, 0xffffffffffffffff),
      words(0xffffffffffffffff, 0xfffffffffffffffe) },
    { words(0x0000000000000000, 0x0000000000000005), words(0xffffffffffffffff, 0xfffffffffffffffd),
      words(0x0000000000000000, 0x0000000000000002), words(0x0000000000000000, 0x0000000000000008),
      words(0xffffffffffffffff, 0xfffffffffffffff1), words(0xffffffffffffffff, 0xffffffffffffffff),
      words(0x0000000000000000, 0x0000000000000002) },
    { words(0xffffffffffffffff, 0x0000000000000000), words(0xffffffffffffffff, 0x8000000000000000),
      words(0xfffffffffffffffe, 0x8000000000000000), words(0xffffffffffffffff, 0x8000000000000000),
      words(0x8000000000000000, 0x0000000000000000), words(0x0000000000000000, 0x0000000000000002),
      words(0x0000000000000000, 0x0000000000000000) },
} };

constexpr bool every_arithmetic_case_holds() noexcept
{
    for (ArithmeticCase const& checked: arithmeticCases)
    {
        if (!(checked.leftOperand + checked.rightOperand == checked.added)
            || !(checked.leftOperand - checked.rightOperand == checked.subtracted)
            || !(checked.leftOperand * checked.rightOperand == checked.multiplied)
            || !(checked.leftOperand / checked.rightOperand == checked.divided)
            || !(checked.leftOperand % checked.rightOperand == checked.remaining))
            return false;
    }
    return true;
}

struct GcdCase
{
    UInt128 leftOperand;
    UInt128 rightOperand;
    UInt128 common;
};

constexpr std::array<GcdCase, 6> gcdCases { {
    // 2^127 - 1 and 2^64 - 1: 2^gcd(127, 64) - 1 = 1.
    { unsigned_words(0x7fffffffffffffff, 0xffffffffffffffff), unsigned_words(0, 0xffffffffffffffff), unsigned_words(0, 1) },
    // 2^90 3^20 and 2^80 3^25 5: 2^80 3^20.
    { unsigned_words(0x033f506e44000000, 0), unsigned_words(0x03da5faed52f0000, 0), unsigned_words(0x0000cfd41b910000, 0) },
    { unsigned_words(0x7fffffffffffffff, 0xffffffffffffffff), unsigned_words(0x7fffffffffffffff, 0xffffffffffffffff),
      unsigned_words(0x7fffffffffffffff, 0xffffffffffffffff) },
    { unsigned_words(0, 0), unsigned_words(0x0000001000000000, 0), unsigned_words(0x0000001000000000, 0) },
    // 6 * 10^30 and 4 * 10^25 + 2.
    { unsigned_words(0x0000004bbb0bace1, 0xa6bd937d80000000), unsigned_words(0x0000000000211654, 0x5850052128000002),
      unsigned_words(0, 6) },
    { unsigned_words(7, 0), unsigned_words(0x4d, 0), unsigned_words(7, 0) },
} };
} // namespace

TEST_CASE("Int128 adds, subtracts, multiplies and divides as a 128-bit two's complement integer", "[int128]")
{
    for (ArithmeticCase const& checked: arithmeticCases)
    {
        CHECK(checked.leftOperand + checked.rightOperand == checked.added);
        CHECK(checked.leftOperand - checked.rightOperand == checked.subtracted);
        CHECK(checked.leftOperand * checked.rightOperand == checked.multiplied);
        CHECK(checked.leftOperand / checked.rightOperand == checked.divided);
        CHECK(checked.leftOperand % checked.rightOperand == checked.remaining);
    }
}

TEST_CASE("Int128 computes the same at compile time", "[int128]")
{
    STATIC_REQUIRE(every_arithmetic_case_holds());
}

TEST_CASE("Int128 converts from every built-in integer exactly, and to none without asking", "[int128]")
{
    STATIC_REQUIRE(Int128 { -5 } == words(~std::uint64_t { 0 }, ~std::uint64_t { 0 } - 4));
    STATIC_REQUIRE(Int128 { std::numeric_limits<std::uint64_t>::max() } == words(0, ~std::uint64_t { 0 }));
    STATIC_REQUIRE(Int128 { std::numeric_limits<std::int64_t>::min() } == words(~std::uint64_t { 0 }, std::uint64_t { 1 } << 63));
    STATIC_REQUIRE_FALSE(std::is_constructible_v<Int128, bool>);
    // No conversion to a built-in integer, implicit or explicit: narrowing is
    // to_int64() or to_uint64(), which say when the value does not fit.
    STATIC_REQUIRE_FALSE(std::is_convertible_v<Int128, std::int64_t>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<std::int64_t, Int128>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<std::uint64_t, Int128>);
    STATIC_REQUIRE(Int128 { -1 }.to_int64() == std::optional<std::int64_t> { -1 });
    STATIC_REQUIRE(Int128 { std::numeric_limits<std::uint64_t>::max() }.to_int64() == std::nullopt);
    STATIC_REQUIRE(Int128 { std::numeric_limits<std::uint64_t>::max() }.to_uint64()
                   == std::optional<std::uint64_t> { std::numeric_limits<std::uint64_t>::max() });
    STATIC_REQUIRE(Int128 { -1 }.to_uint64() == std::nullopt);
    STATIC_REQUIRE(words(1, 0).to_int64() == std::nullopt);
}

TEST_CASE("Int128 orders as a signed integer and shifts arithmetically", "[int128]")
{
    constexpr Int128 smallest = std::numeric_limits<Int128>::min();
    constexpr Int128 largest = std::numeric_limits<Int128>::max();
    STATIC_REQUIRE(smallest == words(std::uint64_t { 1 } << 63, 0));
    STATIC_REQUIRE(largest == words(~(std::uint64_t { 1 } << 63), ~std::uint64_t { 0 }));
    STATIC_REQUIRE(smallest < Int128 { -1 });
    STATIC_REQUIRE(Int128 { -1 } < Int128 { 0 });
    STATIC_REQUIRE(words(0, ~std::uint64_t { 0 }) < words(1, 0));
    STATIC_REQUIRE(0 < largest);
    STATIC_REQUIRE((Int128 { -8 } >> 1) == Int128 { -4 });
    STATIC_REQUIRE((Int128 { -1 } >> 127) == Int128 { -1 });
    STATIC_REQUIRE((smallest >> 64) == words(~std::uint64_t { 0 }, std::uint64_t { 1 } << 63));
    STATIC_REQUIRE((Int128 { 1 } << 127) == smallest);
    STATIC_REQUIRE((Int128 { 3 } << 64) == words(3, 0));
    STATIC_REQUIRE(-smallest == smallest);
    STATIC_REQUIRE(std::numeric_limits<Int128>::digits == 127);
    STATIC_REQUIRE(std::numeric_limits<Int128>::is_signed);
}

TEST_CASE("Int128 converts to the nearest double, ties to even", "[int128]")
{
    CHECK(words(0x7fffffffffffffff, 0xffffffffffffffff).to_double() == 0x1p+127);
    CHECK(words(0x8000000000000000, 0).to_double() == -0x1p+127);
    // 2^64 + 2^11 is half way between two doubles: to even, 2^64.
    CHECK(words(1, 0x800).to_double() == 0x1p+64);
    // One more and it is past half way.
    CHECK(words(1, 0x801).to_double() == 0x1.0000000000001p+64);
    // 2^64 + 3 * 2^11 is half way again: to even, upwards this time.
    CHECK(words(1, 0x1800).to_double() == 0x1.0000000000002p+64);
    CHECK(words(0xffffffefffffffff, 0xffff800000000000).to_double() == -0x1p+100);
    CHECK(words(0xffffffefffffffff, 0xffff7fffffffffff).to_double() == -0x1.0000000000001p+100);
    CHECK(Int128 { -7 }.to_double() == -7.0);
}

TEST_CASE("the 128-bit greatest common divisor, square root and powers of ten", "[int128]")
{
    for (GcdCase const& checked: gcdCases)
    {
        CHECK(formula::detail::u128_gcd(checked.leftOperand, checked.rightOperand) == checked.common);
        CHECK(formula::detail::u128_gcd(checked.rightOperand, checked.leftOperand) == checked.common);
    }
    CHECK(formula::detail::u128_isqrt(unsigned_words(~std::uint64_t { 0 }, ~std::uint64_t { 0 })) == ~std::uint64_t { 0 });
    CHECK(formula::detail::u128_isqrt(unsigned_words(std::uint64_t { 1 } << 63, 0)) == 0xb504f333f9de6484);
    CHECK(formula::detail::u128_isqrt(unsigned_words(1, 0)) == std::uint64_t { 1 } << 32);
    // (2^64 - 1)^2 and one below it.
    CHECK(formula::detail::u128_isqrt(unsigned_words(0xfffffffffffffffe, 1)) == ~std::uint64_t { 0 });
    CHECK(formula::detail::u128_isqrt(unsigned_words(0xfffffffffffffffe, 0)) == 0xfffffffffffffffe);
    CHECK(formula::detail::u128_pow10(38) == unsigned_words(0x4b3b4ca85a86c47a, 0x098a224000000000));
    CHECK(formula::detail::u128_pow10(39) == std::nullopt);
    CHECK(formula::detail::u128_pow10(-1) == std::nullopt);
}

TEST_CASE("Int128 formats as its decimal digits", "[int128][format]")
{
    CHECK(std::format("{}", Int128 { 0 }) == "0");
    CHECK(std::format("{}", Int128 { -1 }) == "-1");
    CHECK(std::format("{}", std::numeric_limits<Int128>::max()) == "170141183460469231731687303715884105727");
    CHECK(std::format("{}", std::numeric_limits<Int128>::min()) == "-170141183460469231731687303715884105728");
    CHECK(std::format("{}", words(1, 0)) == "18446744073709551616");
    CHECK(std::format("{}", words(0x4b3b4ca85a86c47a, 0x098a224000000000)) == "100000000000000000000000000000000000000");
}

TEST_CASE("the 128-bit division, product and greatest common divisor keep their defining identities", "[int128]")
{
    // splitmix64, seeded: a mix of 64-bit, 128-bit and boundary operands.
    std::uint64_t state = 20261003;
    auto const nextWord = [&state] {
        state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t mixed = state;
        mixed = (mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9ULL;
        mixed = (mixed ^ (mixed >> 27)) * 0x94D049BB133111EBULL;
        return mixed ^ (mixed >> 31);
    };
    for (int drawn = 0; drawn < 4000; ++drawn)
    {
        std::uint64_t const shape = nextWord() % 4;
        UInt128 const dividend { shape == 0 ? std::uint64_t { 0 } : nextWord(), nextWord() };
        UInt128 const divisor { shape < 2 ? std::uint64_t { 0 } : nextWord() >> (nextWord() % 64), nextWord() | 1U };
        formula::detail::UInt128Division const split = formula::detail::u128_divmod(dividend, divisor);
        CHECK(split.remainder < divisor);
        std::optional<UInt128> const recombined = formula::detail::u128_mul_checked(split.quotient, divisor);
        REQUIRE(recombined.has_value());
        CHECK(formula::detail::u128_add(*recombined, split.remainder) == dividend);
        UInt128 const common = formula::detail::u128_gcd(dividend, divisor);
        CHECK(formula::detail::u128_divmod(dividend, common).remainder.is_zero());
        CHECK(formula::detail::u128_divmod(divisor, common).remainder.is_zero());
        CHECK(formula::detail::u128_gcd(formula::detail::u128_divmod(dividend, common).quotient,
                                        formula::detail::u128_divmod(divisor, common).quotient)
              == UInt128::from_u64(1));
    }
}

#if FORMULA_NATIVE_INT128
TEST_CASE("the portable 128-bit arithmetic agrees with the compiler's own", "[int128]")
{
    using namespace formula::detail;
    std::uint64_t state = 1272026;
    auto const nextWord = [&state] {
        state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t mixed = state;
        mixed = (mixed ^ (mixed >> 30)) * 0xBF58476D1CE4E5B9ULL;
        mixed = (mixed ^ (mixed >> 27)) * 0x94D049BB133111EBULL;
        return mixed ^ (mixed >> 31);
    };
    for (int drawn = 0; drawn < 4000; ++drawn)
    {
        UInt128 const leftOperand { nextWord() % 3 == 0 ? std::uint64_t { 0 } : nextWord(), nextWord() };
        UInt128 const rightOperand { nextWord() % 3 == 0 ? std::uint64_t { 0 } : nextWord() >> (nextWord() % 64),
                                     nextWord() | 1U };
        NativeUInt128 const nativeLeft = to_native(leftOperand);
        NativeUInt128 const nativeRight = to_native(rightOperand);
        CHECK(portable::multiply(leftOperand, rightOperand) == from_native(nativeLeft * nativeRight));
        CHECK(portable::divide(leftOperand, rightOperand).quotient == from_native(nativeLeft / nativeRight));
        CHECK(portable::divide(leftOperand, rightOperand).remainder == from_native(nativeLeft % nativeRight));
        NativeUInt128 nativeProduct = 0;
        bool const nativeOverflowed = __builtin_mul_overflow(nativeLeft, nativeRight, &nativeProduct);
        std::optional<UInt128> const portableProduct = portable::multiply_checked(leftOperand, rightOperand);
        CHECK(portableProduct.has_value() == !nativeOverflowed);
        if (portableProduct.has_value())
            CHECK(*portableProduct == from_native(nativeProduct));
        CHECK(portable::multiply_words(leftOperand.lowWord, rightOperand.lowWord)
              == from_native(static_cast<NativeUInt128>(leftOperand.lowWord) * rightOperand.lowWord));
    }
}
#endif
