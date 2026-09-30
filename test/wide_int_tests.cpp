// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/detail/checked_int.hpp>
#include <formula-cpp/detail/wide_int.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>

namespace
{
using W4 = formula::detail::WideUnsigned<4>;
using W8 = formula::detail::WideUnsigned<8>;
using S4 = formula::detail::WideSigned<4>;
using Int = formula::detail::Int;
// Each using-declaration also brings in the non-template `Int` overload of the
// same name from checked_int.hpp, so this file resolves both families together.
using formula::detail::add_checked_or_none;
using formula::detail::add_small_checked_or_none;
using formula::detail::divmod;
using formula::detail::divmod_small;
using formula::detail::gcd;
using formula::detail::lcm_checked_or_none;
using formula::detail::mul_checked_or_none;
using formula::detail::mul_small_checked_or_none;
using formula::detail::pow10;
using formula::detail::shift_left_checked_or_none;
using formula::detail::shift_right;
using formula::detail::sub_checked_or_none;

constexpr std::uint64_t allBits64 = std::numeric_limits<std::uint64_t>::max();
constexpr W4 topBit4 = W4::from_limbs({ 0U, 0U, 0U, 0x8000'0000U });
constexpr W4 allBits4 = W4::from_limbs({ 0xFFFF'FFFFU, 0xFFFF'FFFFU, 0xFFFF'FFFFU, 0xFFFF'FFFFU });

/// A reproducible stream of 64-bit operands (a linear congruential
/// generator with Knuth's MMIX constants), so that a failure names its case.
struct Operands
{
    std::uint64_t state;

    std::uint64_t draw() noexcept
    {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return state;
    }
};
} // namespace

TEST_CASE("wide unsigned: a value round-trips through 64 bits and reports its width", "[wide-int]")
{
    STATIC_REQUIRE(W4::bits == 128);
    STATIC_REQUIRE(W8::bits == 256);
    STATIC_REQUIRE(W4 {}.is_zero());
    STATIC_REQUIRE(W4 {}.bit_length() == 0);
    STATIC_REQUIRE(W4::from_u64(1).bit_length() == 1);
    STATIC_REQUIRE(W4::from_u64(allBits64).to_u64() == allBits64);
    STATIC_REQUIRE(W4::from_u64(allBits64).bit_length() == 64);
    STATIC_REQUIRE(W4::from_limbs({ 0U, 0U, 1U, 0U }).bit_length() == 65);
    STATIC_REQUIRE(topBit4.bit_length() == 128);
    STATIC_REQUIRE(!topBit4.to_u64().has_value());
    STATIC_REQUIRE(W4::from_u64(0x1'0000'0002ULL).limb(0) == 2U);
    STATIC_REQUIRE(W4::from_u64(0x1'0000'0002ULL).limb(1) == 1U);
    STATIC_REQUIRE(W4::from_u64(7).limb(9) == 0U); // past the top
    STATIC_REQUIRE(W4::from_u64(7).limbs()[0] == 7U);
}

TEST_CASE("wide unsigned: carries cross 2^32 and 2^64 and overflow at the top is reported", "[wide-int]")
{
    STATIC_REQUIRE(*add_small_checked_or_none(W4::from_u64(0xFFFF'FFFFULL), 1U) == W4::from_u64(0x1'0000'0000ULL));
    STATIC_REQUIRE(*add_checked_or_none(W4::from_u64(allBits64), W4::from_u64(1)) == W4::from_limbs({ 0U, 0U, 1U, 0U }));
    STATIC_REQUIRE(!add_checked_or_none(topBit4, topBit4).has_value());
    STATIC_REQUIRE(!add_small_checked_or_none(allBits4, 1U).has_value());
    STATIC_REQUIRE(*sub_checked_or_none(W4::from_limbs({ 0U, 0U, 1U, 0U }), W4::from_u64(1)) == W4::from_u64(allBits64));
    STATIC_REQUIRE(!sub_checked_or_none(W4::from_u64(1), W4::from_u64(2)).has_value()); // negative, never wrapped
    STATIC_REQUIRE(sub_checked_or_none(W4::from_u64(2), W4::from_u64(2))->is_zero());

    // Same names, two families, never ambiguous: `Int` operands reach the
    // non-templates of checked_int.hpp (a template on L is never deduced from
    // an Int), and wide operands the templates (no wide type converts to Int).
    STATIC_REQUIRE(std::is_same_v<decltype(add_checked_or_none(Int { 2 }, Int { 3 })), std::optional<Int>>);
    STATIC_REQUIRE(*add_checked_or_none(Int { 2 }, Int { 3 }) == 5);
    STATIC_REQUIRE(!mul_checked_or_none(formula::detail::IntMax, Int { 2 }).has_value());
    STATIC_REQUIRE(*sub_checked_or_none(Int { 2 }, Int { 5 }) == -3); // Int subtracts below zero
    STATIC_REQUIRE(std::is_same_v<decltype(sub_checked_or_none(W4 {}, W4 {})), std::optional<W4>>);
    STATIC_REQUIRE(std::is_same_v<decltype(mul_checked_or_none(S4 {}, S4 {})), std::optional<S4>>);
    STATIC_REQUIRE(gcd(std::uint64_t { 12 }, std::uint64_t { 18 }) == 6U);
}

TEST_CASE("wide unsigned: products are schoolbook and never wrap", "[wide-int]")
{
    // (2^64 - 1)^2 = 2^128 - 2^65 + 1.
    STATIC_REQUIRE(*mul_checked_or_none(W4::from_u64(allBits64), W4::from_u64(allBits64))
                   == W4::from_limbs({ 1U, 0U, 0xFFFF'FFFEU, 0xFFFF'FFFFU }));
    STATIC_REQUIRE(!mul_checked_or_none(W4::from_limbs({ 0U, 0U, 1U, 0U }), W4::from_limbs({ 0U, 0U, 1U, 0U })).has_value());
    STATIC_REQUIRE(!mul_checked_or_none(topBit4, W4::from_u64(2)).has_value());
    STATIC_REQUIRE(mul_checked_or_none(allBits4, W4 {})->is_zero());
    STATIC_REQUIRE(*mul_small_checked_or_none(W4::from_u64(1'000'000'000'000'000'000ULL), 10U)
                   == W4::from_u64(10'000'000'000'000'000'000ULL));
    // 10^20 needs 67 bits: past 64, inside 128.
    STATIC_REQUIRE(!mul_small_checked_or_none(W4::from_u64(1'000'000'000'000'000'000ULL), 100U)->to_u64().has_value());
    STATIC_REQUIRE(!mul_small_checked_or_none(topBit4, 2U).has_value());
    // A zero-heavy operand on either side, since the zero limbs of the left
    // one are skipped: 7 * 2^32 times 2^64 - 1 is 7 * 2^96 - 7 * 2^32.
    constexpr W4 sparse = W4::from_limbs({ 0U, 7U, 0U, 0U });
    constexpr W4 dense = W4::from_u64(allBits64);
    STATIC_REQUIRE(*mul_checked_or_none(sparse, dense) == W4::from_limbs({ 0U, 0xFFFF'FFF9U, 0xFFFF'FFFFU, 6U }));
    STATIC_REQUIRE(*mul_checked_or_none(dense, sparse) == *mul_checked_or_none(sparse, dense));
    // 2^96 times 2^64 overflows whichever operand is on the left.
    STATIC_REQUIRE(!mul_checked_or_none(W4::from_limbs({ 0U, 0U, 0U, 1U }), W4::from_limbs({ 0U, 0U, 1U, 0U })).has_value());
    STATIC_REQUIRE(!mul_checked_or_none(W4::from_limbs({ 0U, 0U, 1U, 0U }), W4::from_limbs({ 0U, 0U, 0U, 1U })).has_value());
}

TEST_CASE("wide unsigned: short division by a 32-bit divisor agrees with long division", "[wide-int]")
{
    constexpr auto byThree = divmod_small(allBits4, 3U);
    STATIC_REQUIRE(byThree.remainder == 0U);
    STATIC_REQUIRE(byThree.quotient == W4::from_limbs({ 0x5555'5555U, 0x5555'5555U, 0x5555'5555U, 0x5555'5555U }));
    // 2^128 - 1 = (2^32 - 1)(2^96 + 2^64 + 2^32 + 1): the largest divisor, no remainder.
    constexpr auto byLargest = divmod_small(allBits4, 0xFFFF'FFFFU);
    STATIC_REQUIRE(byLargest.quotient == W4::from_limbs({ 1U, 1U, 1U, 1U }));
    STATIC_REQUIRE(byLargest.remainder == 0U);
    STATIC_REQUIRE(divmod_small(*pow10<4>(38), 10U).quotient == *pow10<4>(37));
    STATIC_REQUIRE(divmod_small(W4::from_u64(7), 9U).quotient.is_zero());
    STATIC_REQUIRE(divmod_small(W4::from_u64(7), 9U).remainder == 7U);

    // Against the binary divmod, on 20000 wide dividends and 32-bit divisors.
    Operands operands { 0x6A09'E667'F3BC'C909ULL };
    int agreed = 0;
    for (int drawn = 0; drawn < 20000; ++drawn)
    {
        std::array<std::uint32_t, 4> dividendLimbs {};
        for (std::uint32_t& each: dividendLimbs)
            each = static_cast<std::uint32_t>(operands.draw() >> 32U);
        std::uint64_t const shaped = operands.draw();
        auto const divisorValue = static_cast<std::uint32_t>((shaped >> (shaped % 32U + 32U)) | 1U);
        W4 const dividendValue = W4::from_limbs(dividendLimbs);
        auto const shortSplit = divmod_small(dividendValue, divisorValue);
        auto const longSplit = divmod(dividendValue, W4::from_u64(divisorValue));
        if (shortSplit.quotient == longSplit.quotient && longSplit.remainder == W4::from_u64(shortSplit.remainder))
            ++agreed;
    }
    CHECK(agreed == 20000);
}

TEST_CASE("wide unsigned: shifts move whole limbs and parts of limbs", "[wide-int]")
{
    STATIC_REQUIRE(*shift_left_checked_or_none(W4::from_u64(1), 32) == W4::from_u64(0x1'0000'0000ULL));
    STATIC_REQUIRE(*shift_left_checked_or_none(W4::from_u64(1), 127) == topBit4);
    STATIC_REQUIRE(!shift_left_checked_or_none(W4::from_u64(1), 128).has_value());
    STATIC_REQUIRE(!shift_left_checked_or_none(topBit4, 1).has_value());
    STATIC_REQUIRE(shift_left_checked_or_none(W4 {}, 500)->is_zero());
    // 0x8000'0001 * 2^33 = 2^64 + 2^33.
    STATIC_REQUIRE(*shift_left_checked_or_none(W4::from_u64(0x8000'0001ULL), 33) == W4::from_limbs({ 0U, 2U, 1U, 0U }));
    STATIC_REQUIRE(shift_right(W4::from_limbs({ 0U, 2U, 1U, 0U }), 33) == W4::from_u64(0x8000'0001ULL));
    STATIC_REQUIRE(shift_right(topBit4, 127) == W4::from_u64(1));
    STATIC_REQUIRE(shift_right(W4::from_u64(0x1'0000'0000ULL), 32) == W4::from_u64(1));
    STATIC_REQUIRE(shift_right(allBits4, 128).is_zero());
    STATIC_REQUIRE(shift_right(allBits4, 0) == allBits4);
}

TEST_CASE("wide unsigned: division leaves a remainder below the divisor", "[wide-int]")
{
    // A divisor with its top bit set: the subtraction happens at full width.
    constexpr auto atTop = divmod(allBits4, W4::from_limbs({ 1U, 0U, 0U, 0x8000'0000U }));
    STATIC_REQUIRE(atTop.quotient == W4::from_u64(1));
    STATIC_REQUIRE(atTop.remainder == W4::from_limbs({ 0xFFFF'FFFEU, 0xFFFF'FFFFU, 0xFFFF'FFFFU, 0x7FFF'FFFFU }));
    constexpr auto byThree = divmod(allBits4, W4::from_u64(3));
    STATIC_REQUIRE(byThree.remainder.is_zero());
    STATIC_REQUIRE(byThree.quotient == W4::from_limbs({ 0x5555'5555U, 0x5555'5555U, 0x5555'5555U, 0x5555'5555U }));
    constexpr auto belowDivisor = divmod(W4::from_u64(7), W4::from_u64(9));
    STATIC_REQUIRE(belowDivisor.quotient.is_zero());
    STATIC_REQUIRE(belowDivisor.remainder == W4::from_u64(7));
    STATIC_REQUIRE(*pow10<4>(38) == *mul_checked_or_none(*pow10<4>(19), *pow10<4>(19)));
    STATIC_REQUIRE(!pow10<4>(39).has_value()); // 10^39 > 2^128
    STATIC_REQUIRE(divmod(*pow10<4>(38), *pow10<4>(19)).quotient == *pow10<4>(19));

    // Against 64-bit division, and back through the product, on 20000 pairs.
    Operands operands { 0x9E37'79B9'7F4A'7C15ULL };
    int agreed = 0;
    for (int drawn = 0; drawn < 20000; ++drawn)
    {
        std::uint64_t const dividendValue = operands.draw();
        std::uint64_t const shaped = operands.draw();
        std::uint64_t const divisorValue = (shaped >> (shaped % 60U)) | 1U;
        auto const split = divmod(W4::from_u64(dividendValue), W4::from_u64(divisorValue));
        auto const product = mul_checked_or_none(W4::from_u64(dividendValue), W4::from_u64(divisorValue));
        auto const back = divmod(*product, W4::from_u64(divisorValue));
        if (split.quotient.to_u64() == dividendValue / divisorValue && split.remainder.to_u64() == dividendValue % divisorValue
            && back.quotient == W4::from_u64(dividendValue) && back.remainder.is_zero())
            ++agreed;
    }
    CHECK(agreed == 20000);

    // q * d + r == n with r < d, at 256 bits, on 2000 pairs of every size.
    int identities = 0;
    for (int drawn = 0; drawn < 2000; ++drawn)
    {
        std::array<std::uint32_t, 8> dividendLimbs {};
        std::array<std::uint32_t, 8> divisorLimbs {};
        std::size_t const divisorTop = static_cast<std::size_t>(operands.draw() % 8U) + 1U;
        for (std::size_t at = 0; at < 8; ++at)
        {
            dividendLimbs[at] = static_cast<std::uint32_t>(operands.draw() >> 32U);
            divisorLimbs[at] = at < divisorTop ? static_cast<std::uint32_t>(operands.draw() >> 32U) : 0U;
        }
        divisorLimbs[0] |= 1U;
        W8 const dividendValue = W8::from_limbs(dividendLimbs);
        W8 const divisorValue = W8::from_limbs(divisorLimbs);
        auto const split = divmod(dividendValue, divisorValue);
        auto const product = mul_checked_or_none(split.quotient, divisorValue);
        auto const rebuilt = product ? add_checked_or_none(*product, split.remainder) : std::nullopt;
        if (rebuilt && *rebuilt == dividendValue && split.remainder < divisorValue)
            ++identities;
    }
    CHECK(identities == 2000);
}

TEST_CASE("wide unsigned: gcd and lcm agree with 64-bit arithmetic", "[wide-int]")
{
    // 3 * 2^64 and 9 * 2^32: 3 * 2^32.
    STATIC_REQUIRE(gcd(W4::from_limbs({ 0U, 0U, 3U, 0U }), W4::from_limbs({ 0U, 9U, 0U, 0U })) == W4::from_limbs({ 0U, 3U, 0U, 0U }));
    STATIC_REQUIRE(gcd(W4 {}, W4::from_u64(12)) == W4::from_u64(12));
    STATIC_REQUIRE(gcd(W4::from_u64(12), W4 {}) == W4::from_u64(12));
    STATIC_REQUIRE(*lcm_checked_or_none(W4::from_u64(6), W4::from_u64(10)) == W4::from_u64(30));
    STATIC_REQUIRE(lcm_checked_or_none(W4 {}, W4::from_u64(10))->is_zero());
    STATIC_REQUIRE(!lcm_checked_or_none(topBit4, W4::from_u64(3)).has_value());

    Operands operands { 0x2545'F491'4F6C'DD1DULL };
    int agreed = 0;
    for (int drawn = 0; drawn < 20000; ++drawn)
    {
        std::uint64_t const shapedLeft = operands.draw();
        std::uint64_t const shapedRight = operands.draw();
        std::uint64_t const leftValue = shapedLeft >> (shapedLeft % 40U);
        std::uint64_t const rightValue = (shapedRight >> (shapedRight % 40U)) * static_cast<std::uint64_t>(drawn % 7 + 1);
        if (gcd(W4::from_u64(leftValue), W4::from_u64(rightValue)).to_u64() == gcd(leftValue, rightValue))
            ++agreed;
    }
    CHECK(agreed == 20000);
}

TEST_CASE("wide unsigned: comparison reads the top limb first", "[wide-int]")
{
    STATIC_REQUIRE(W4::from_limbs({ 0U, 0U, 1U, 0U }) > W4::from_u64(allBits64));
    STATIC_REQUIRE(W4::from_limbs({ 0xFFFF'FFFFU, 0U, 0U, 0U }) < W4::from_limbs({ 0U, 1U, 0U, 0U }));
    STATIC_REQUIRE((W4::from_u64(5) <=> W4::from_u64(5)) == std::strong_ordering::equal);
}

TEST_CASE("wide unsigned: a signed value adds and multiplies as integers do", "[wide-int]")
{
    constexpr S4 minusSeven { true, W4::from_u64(7) };
    constexpr S4 five { false, W4::from_u64(5) };
    STATIC_REQUIRE(add_checked_or_none(minusSeven, five)->negative);
    STATIC_REQUIRE(add_checked_or_none(minusSeven, five)->magnitude == W4::from_u64(2));
    STATIC_REQUIRE(!add_checked_or_none(S4 { true, W4::from_u64(5) }, five)->negative); // zero is never negative
    STATIC_REQUIRE(add_checked_or_none(S4 { true, W4::from_u64(5) }, five)->magnitude.is_zero());
    STATIC_REQUIRE(sub_checked_or_none(five, minusSeven)->magnitude == W4::from_u64(12));
    STATIC_REQUIRE(!sub_checked_or_none(five, minusSeven)->negative);
    STATIC_REQUIRE(mul_checked_or_none(minusSeven, minusSeven)->magnitude == W4::from_u64(49));
    STATIC_REQUIRE(!mul_checked_or_none(minusSeven, minusSeven)->negative);
    STATIC_REQUIRE(!mul_checked_or_none(minusSeven, S4 {})->negative);
    STATIC_REQUIRE(!add_checked_or_none(S4 { false, topBit4 }, S4 { false, topBit4 }).has_value());

    // Against std::int64_t on operands below 2^30, where nothing overflows.
    Operands operands { 0x0123'4567'89AB'CDEFULL };
    int agreed = 0;
    auto const signedOf = [](S4 const& held) {
        auto const heldMagnitude = static_cast<std::int64_t>(*held.magnitude.to_u64());
        return held.negative ? -heldMagnitude : heldMagnitude;
    };
    for (int drawn = 0; drawn < 20000; ++drawn)
    {
        auto const leftValue = static_cast<std::int64_t>(operands.draw() >> 34U) - (std::int64_t { 1 } << 29);
        auto const rightValue = static_cast<std::int64_t>(operands.draw() >> 34U) - (std::int64_t { 1 } << 29);
        S4 const leftWide { leftValue < 0, W4::from_u64(formula::detail::magnitude(leftValue)) };
        S4 const rightWide { rightValue < 0, W4::from_u64(formula::detail::magnitude(rightValue)) };
        auto const added = add_checked_or_none(leftWide, rightWide);
        auto const subtracted = sub_checked_or_none(leftWide, rightWide);
        auto const multiplied = mul_checked_or_none(leftWide, rightWide);
        if (signedOf(*added) == leftValue + rightValue && signedOf(*subtracted) == leftValue - rightValue
            && signedOf(*multiplied) == leftValue * rightValue && !(added->magnitude.is_zero() && added->negative))
            ++agreed;
    }
    CHECK(agreed == 20000);
}
