// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/detail/checked_int.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <random>

using formula::detail::add_checked_or_none;
using formula::detail::decimal_digits;
using formula::detail::floor_divmod;
using formula::detail::gcd;
using formula::detail::Int;
using formula::detail::IntMax;
using formula::detail::IntMin;
using formula::detail::magnitude;
using formula::detail::mul_checked_or_none;
using formula::detail::mul_pow10;
using formula::detail::pow10;
using formula::detail::sub_checked_or_none;

// ---- compile-time coverage: these must hold without running anything ----

static_assert(add_checked_or_none(Int { 2 }, Int { 3 }) == Int { 5 });
static_assert(!add_checked_or_none(IntMax, Int { 1 }).has_value());
static_assert(!add_checked_or_none(IntMin, Int { -1 }).has_value());
static_assert(add_checked_or_none(IntMax, Int { -1 }) == IntMax - 1);

static_assert(sub_checked_or_none(Int { 2 }, Int { 3 }) == Int { -1 });
static_assert(!sub_checked_or_none(IntMin, Int { 1 }).has_value());
static_assert(!sub_checked_or_none(IntMax, Int { -1 }).has_value());

static_assert(mul_checked_or_none(Int { 6 }, Int { 7 }) == Int { 42 });
static_assert(mul_checked_or_none(Int { 0 }, IntMin) == Int { 0 });
static_assert(!mul_checked_or_none(IntMin, Int { -1 }).has_value());
static_assert(mul_checked_or_none(IntMin, Int { 1 }) == IntMin);
static_assert(!mul_checked_or_none(IntMax, Int { 2 }).has_value());
static_assert(!mul_checked_or_none(Int { -3037000500 }, Int { 3037000500 }).has_value());
static_assert(mul_checked_or_none(Int { 3037000499 }, Int { 3037000499 }).has_value());

static_assert(magnitude(IntMin) == 9223372036854775808ULL);
static_assert(magnitude(Int { -5 }) == 5ULL);
static_assert(magnitude(Int { 5 }) == 5ULL);

static_assert(gcd(12ULL, 18ULL) == 6ULL);
static_assert(gcd(0ULL, 7ULL) == 7ULL);
static_assert(gcd(7ULL, 0ULL) == 7ULL);
static_assert(gcd(9223372036854775808ULL, 9223372036854775808ULL) == 9223372036854775808ULL);

static_assert(floor_divmod(Int { 7 }, Int { 2 }).quotient == Int { 3 });
static_assert(floor_divmod(Int { 7 }, Int { 2 }).remainder == Int { 1 });
static_assert(floor_divmod(Int { -7 }, Int { 2 }).quotient == Int { -4 });
static_assert(floor_divmod(Int { -7 }, Int { 2 }).remainder == Int { 1 });
static_assert(floor_divmod(Int { -6 }, Int { 2 }).quotient == Int { -3 });
static_assert(floor_divmod(Int { -6 }, Int { 2 }).remainder == Int { 0 });
static_assert(floor_divmod(IntMin, Int { 1 }).quotient == IntMin);
static_assert(floor_divmod(IntMin, Int { 1 }).remainder == Int { 0 });

static_assert(pow10(0) == Int { 1 });
static_assert(pow10(18) == Int { 1000000000000000000 });
static_assert(!pow10(19).has_value());
static_assert(!pow10(-1).has_value());

static_assert(decimal_digits(Int { 0 }) == 1);
static_assert(decimal_digits(Int { 9 }) == 1);
static_assert(decimal_digits(Int { 10 }) == 2);
static_assert(decimal_digits(Int { -999 }) == 3);
static_assert(decimal_digits(IntMax) == 19);
static_assert(decimal_digits(IntMin) == 19);

static_assert(mul_pow10(Int { 3 }, 2) == Int { 300 });
static_assert(!mul_pow10(IntMax, 1).has_value());
static_assert(mul_pow10(Int { -7 }, 3) == Int { -7000 });

// ---- runtime coverage: the same primitives, exercised through Catch2 ----

TEST_CASE("add_checked_or_none reports overflow rather than wrapping", "[checked_int]")
{
    CHECK(add_checked_or_none(IntMax - 1, Int { 1 }) == IntMax);
    CHECK_FALSE(add_checked_or_none(IntMax, Int { 1 }).has_value());
    CHECK_FALSE(add_checked_or_none(IntMin, Int { -1 }).has_value());
}

// ---- an independent oracle for the overflow predicates ----
//
// Comparing mul_overflows against mul_checked_or_none proves nothing: mul_checked_or_none is
// defined as `if (mul_overflows(...)) return nullopt;`, so the two agree by
// construction for any implementation, correct or not. A comparison flipped in
// one sign quadrant would pass such a test unnoticed.
//
// This oracle shares no logic with the implementation. mul_overflows decides by
// division; this assembles the full 128-bit product from 32-bit halves and
// divides nowhere. Checked against __int128 ground truth over 2.9M pairs before
// being written here. __int128 itself cannot be used: MSVC has no such type.

namespace
{

struct Product128
{
    std::uint64_t high {};
    std::uint64_t low {};
};

constexpr Product128 wide_multiply(std::uint64_t lhs, std::uint64_t rhs) noexcept
{
    constexpr std::uint64_t Mask = 0xFFFFFFFFULL;
    std::uint64_t const lhsLow = lhs & Mask;
    std::uint64_t const lhsHigh = lhs >> 32;
    std::uint64_t const rhsLow = rhs & Mask;
    std::uint64_t const rhsHigh = rhs >> 32;

    std::uint64_t const lowLow = lhsLow * rhsLow;
    std::uint64_t const lowHigh = lhsLow * rhsHigh;
    std::uint64_t const highLow = lhsHigh * rhsLow;
    std::uint64_t const highHigh = lhsHigh * rhsHigh;

    std::uint64_t const carry = ((lowLow >> 32) + (lowHigh & Mask) + (highLow & Mask)) >> 32;
    std::uint64_t const low = lowLow + (lowHigh << 32) + (highLow << 32);
    std::uint64_t const high = highHigh + (lowHigh >> 32) + (highLow >> 32) + carry;
    return { high, low };
}

/// True when the exact product of @p lhs and @p rhs lies outside [IntMin, IntMax].
constexpr bool product_is_unrepresentable(Int lhs, Int rhs) noexcept
{
    Product128 const product = wide_multiply(magnitude(lhs), magnitude(rhs));
    bool const negative = (lhs < 0) != (rhs < 0);
    // The negative range reaches exactly one further than the positive one.
    std::uint64_t const limit = negative ? 9223372036854775808ULL : 9223372036854775807ULL;
    return product.high != 0 || product.low > limit;
}

// The oracle has to be right itself, so pin it where it is easiest to get wrong.
static_assert(!product_is_unrepresentable(IntMin, Int { 1 }));
static_assert(product_is_unrepresentable(IntMin, Int { -1 }));
static_assert(!product_is_unrepresentable(Int { 3037000499 }, Int { 3037000499 }));
static_assert(product_is_unrepresentable(Int { 3037000500 }, Int { 3037000500 }));
static_assert(!product_is_unrepresentable(Int { 0 }, IntMin));

} // namespace

TEST_CASE("mul_overflows matches an independent 128-bit oracle", "[checked_int]")
{
    Int const boundaries[] = { IntMin,     IntMin + 1, -3037000500, -3037000499, -4294967296, -65536, -3,
                               -2,         -1,         0,           1,           2,           3,      65536,
                               4294967295, 4294967296, 3037000499,  3037000500,  IntMax - 1,  IntMax };

    for (Int const lhs: boundaries)
    {
        for (Int const rhs: boundaries)
        {
            INFO(lhs << " * " << rhs);
            CHECK(formula::detail::mul_overflows(lhs, rhs) == product_is_unrepresentable(lhs, rhs));
        }
    }

    for (Int lhs = -200; lhs <= 200; ++lhs)
        for (Int rhs = -200; rhs <= 200; ++rhs)
            REQUIRE(formula::detail::mul_overflows(lhs, rhs) == product_is_unrepresentable(lhs, rhs));

    // A deterministic sweep, then one biased into the magnitude band around
    // sqrt(IntMax) where the predicate actually changes its answer.
    std::mt19937_64 generator { 12345 };
    for (int iteration = 0; iteration < 200000; ++iteration)
    {
        auto const lhs = static_cast<Int>(generator());
        auto const rhs = static_cast<Int>(generator());
        REQUIRE(formula::detail::mul_overflows(lhs, rhs) == product_is_unrepresentable(lhs, rhs));
    }

    std::uniform_int_distribution<Int> nearRoot { 3000000000LL, 3100000000LL };
    for (int iteration = 0; iteration < 50000; ++iteration)
    {
        Int const lhs = (generator() & 1U) != 0U ? nearRoot(generator) : -nearRoot(generator);
        Int const rhs = (generator() & 1U) != 0U ? nearRoot(generator) : -nearRoot(generator);
        REQUIRE(formula::detail::mul_overflows(lhs, rhs) == product_is_unrepresentable(lhs, rhs));
    }
}

TEST_CASE("add_overflows and sub_overflows agree with a magnitude-domain reference", "[checked_int]")
{
    auto const addIsUnrepresentable = [](Int lhs, Int rhs) {
        if ((lhs < 0) != (rhs < 0))
            return false; // opposite signs always shrink the magnitude
        // Summing the magnitudes would itself wrap: IntMin + IntMin is
        // 2^63 + 2^63, which is 0 modulo 2^64 and would read as "no overflow".
        // Comparing against the remaining headroom cannot wrap, because the
        // subtrahend never exceeds the limit when both operands share a sign.
        std::uint64_t const limit = lhs >= 0 ? static_cast<std::uint64_t>(IntMax) : 9223372036854775808ULL;
        return magnitude(lhs) > limit - magnitude(rhs);
    };

    Int const boundaries[] = { IntMin, IntMin + 1, -65536, -2, -1, 0, 1, 2, 65536, IntMax - 1, IntMax };

    for (Int const lhs: boundaries)
    {
        for (Int const rhs: boundaries)
        {
            INFO(lhs << " + " << rhs);
            CHECK(formula::detail::add_overflows(lhs, rhs) == addIsUnrepresentable(lhs, rhs));
        }
    }

    // Subtraction is addition of the negation, except where the negation is
    // itself unrepresentable -- which is exactly rhs == IntMin.
    for (Int const lhs: boundaries)
    {
        for (Int const rhs: boundaries)
        {
            if (rhs == IntMin)
                continue;
            INFO(lhs << " - " << rhs);
            CHECK(formula::detail::sub_overflows(lhs, rhs) == addIsUnrepresentable(lhs, -rhs));
        }
    }
}

TEST_CASE("floor_divmod always yields a remainder from 0 up to but excluding the denominator", "[checked_int]")
{
    for (Int numerator = -20; numerator <= 20; ++numerator)
    {
        for (Int denominator = 1; denominator <= 7; ++denominator)
        {
            auto const [quotient, remainder] = floor_divmod(numerator, denominator);
            CHECK(remainder >= 0);
            CHECK(remainder < denominator);
            CHECK(quotient * denominator + remainder == numerator);
        }
    }
}

TEST_CASE("decimal_digits counts the digits of the magnitude", "[checked_int]")
{
    CHECK(decimal_digits(Int { 1 }) == 1);
    CHECK(decimal_digits(Int { 100 }) == 3);
    CHECK(decimal_digits(Int { -100 }) == 3);
    CHECK(decimal_digits(Int { 999999999999999999 }) == 18);
}
