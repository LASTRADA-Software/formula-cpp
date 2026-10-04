// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Fixed-width unsigned integers wider than 64 bits, for the exact arithmetic
/// behind a declared precision (`rounded_output`, `opaque.hpp`): a value the
/// 128-bit `Rational` cannot hold is computed here exactly, and only its
/// rounding is ever written.
///
/// `WideUnsigned<Limbs>` holds `Limbs` limbs of 32 bits, least significant
/// first. Every product of two limbs is formed in `std::uint64_t`: cl has no
/// 128-bit integer (`int128.hpp` gives the same reason), and nothing here
/// uses an intrinsic or floating point, so a result depends on its operands
/// alone.
///
/// **Overflow is reported, never wrapped.** Every operation that can leave the
/// width answers `std::optional`, empty when it would, and is named
/// `*_checked_or_none` as the `Int` helpers of `checked_int.hpp` are;
/// `sub_checked_or_none` is empty for a negative difference too. Unlike
/// `checked_add(Rational, Rational)` and its siblings, which answer
/// `std::expected<Rational, ArithmeticError>`, each of these has one way to
/// fail, and its caller names the error -- `ArithmeticError::Overflow`
/// throughout this library.
///
/// No shift here is by 32 or more on a 32-bit limb, which would be undefined
/// behaviour: shift counts are split into whole limbs and a part below 32.
///
/// A signed integer is a sign beside a magnitude (`WideSigned`), and a fraction
/// a sign beside two magnitudes (`WideRatio`). Zero is never negative.

#include <formula-cpp/int128.hpp>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace formula::detail
{

/// An unsigned integer of `32 * Limbs` bits, zero when default-constructed. A
/// value type with no arithmetic operators: every operation that can overflow
/// is a free function below, answering `std::optional`, so that no expression
/// wraps silently.
template <std::size_t Limbs>
    requires(Limbs >= 2)
class WideUnsigned
{
  public:
    /// How many bits it holds.
    static constexpr std::size_t bits = 32 * Limbs;

    /// Zero.
    constexpr WideUnsigned() noexcept = default;

    /// @p narrow, exactly: two limbs always hold it.
    [[nodiscard]] static constexpr WideUnsigned from_u64(std::uint64_t narrow) noexcept
    {
        std::array<std::uint32_t, Limbs> held {};
        held[0] = static_cast<std::uint32_t>(narrow & 0xFFFF'FFFFU);
        held[1] = static_cast<std::uint32_t>(narrow >> 32U);
        return from_limbs(held);
    }

    /// @p narrow, exactly. Four limbs hold it.
    [[nodiscard]] static constexpr WideUnsigned from_u128(UInt128 narrow) noexcept
        requires(Limbs >= 4)
    {
        std::array<std::uint32_t, Limbs> held {};
        held[0] = static_cast<std::uint32_t>(narrow.lowWord & 0xFFFF'FFFFU);
        held[1] = static_cast<std::uint32_t>(narrow.lowWord >> 32U);
        held[2] = static_cast<std::uint32_t>(narrow.highWord & 0xFFFF'FFFFU);
        held[3] = static_cast<std::uint32_t>(narrow.highWord >> 32U);
        return from_limbs(held);
    }

    /// The integer whose limbs are @p held, least significant first.
    [[nodiscard]] static constexpr WideUnsigned from_limbs(std::array<std::uint32_t, Limbs> const& held) noexcept
    {
        WideUnsigned made;
        made._limbs = held;
        return made;
    }

    /// Every limb, least significant first.
    [[nodiscard]] constexpr std::array<std::uint32_t, Limbs> const& limbs() const noexcept
    {
        return _limbs;
    }

    /// This value as a `std::uint64_t`, or nothing when it needs more than 64 bits.
    [[nodiscard]] constexpr std::optional<std::uint64_t> to_u64() const noexcept
    {
        for (std::size_t at = 2; at < Limbs; ++at)
            if (_limbs[at] != 0)
                return std::nullopt;
        return (static_cast<std::uint64_t>(_limbs[1]) << 32U) | _limbs[0];
    }

    /// This value as 128 bits, or nothing when it needs more.
    [[nodiscard]] constexpr std::optional<UInt128> to_u128() const noexcept
    {
        // Every limb is read before anything is decided, with no early return
        // in between. An early return left g++ 14 at -O3 a tail that reads only
        // the low four limbs, which it split out of each width and then merged
        // across widths; the merged copy, typed for the widest, made
        // -Warray-bounds report a read past a narrower value that never
        // happens.
        std::uint32_t aboveLow = 0;
        for (std::size_t limbAt = 4; limbAt < Limbs; ++limbAt)
            aboveLow |= _limbs[limbAt];
        // The low four limbs, zero past the top of a narrower value.
        std::array<std::uint64_t, 4> lowLimbs {};
        for (std::size_t limbAt = 0; limbAt < lowLimbs.size() && limbAt < Limbs; ++limbAt)
            lowLimbs[limbAt] = _limbs[limbAt];
        if (aboveLow != 0U)
            return std::nullopt;
        return UInt128 { (lowLimbs[3] << 32U) | lowLimbs[2], (lowLimbs[1] << 32U) | lowLimbs[0] };
    }

    /// Whether this is zero.
    [[nodiscard]] constexpr bool is_zero() const noexcept
    {
        for (std::uint32_t const each: _limbs)
            if (each != 0)
                return false;
        return true;
    }

    /// How many bits writing it takes: 0 for zero, 1 for one, 65 for 2^64.
    [[nodiscard]] constexpr std::size_t bit_length() const noexcept
    {
        for (std::size_t at = Limbs; at > 0; --at)
        {
            std::uint32_t topLimb = _limbs[at - 1];
            if (topLimb == 0)
                continue;
            std::size_t topWidth = 0;
            while (topLimb != 0)
            {
                topLimb >>= 1U;
                ++topWidth;
            }
            return 32 * (at - 1) + topWidth;
        }
        return 0;
    }

    /// Limb @p at, counted from the least significant; zero past the top, so
    /// that a caller reading one limb beyond needs no bounds check of its own.
    [[nodiscard]] constexpr std::uint32_t limb(std::size_t at) const noexcept
    {
        return at < Limbs ? _limbs[at] : 0U;
    }

    /// Limb-wise equality, which for this representation is value equality.
    friend constexpr bool operator==(WideUnsigned const&, WideUnsigned const&) noexcept = default;

    /// The values' order, read from the most significant limb down.
    friend constexpr std::strong_ordering operator<=>(WideUnsigned const& leftOperand,
                                                      WideUnsigned const& rightOperand) noexcept
    {
        for (std::size_t at = Limbs; at > 0; --at)
            if (leftOperand._limbs[at - 1] != rightOperand._limbs[at - 1])
                return leftOperand._limbs[at - 1] <=> rightOperand._limbs[at - 1];
        return std::strong_ordering::equal;
    }

  private:
    std::array<std::uint32_t, Limbs> _limbs {};
};

/// @p augend plus @p addend, or nothing when the sum leaves the width.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> add_checked_or_none(WideUnsigned<L> const& augend,
                                                                           WideUnsigned<L> const& addend) noexcept
{
    std::array<std::uint32_t, L> held {};
    std::uint64_t carry = 0;
    for (std::size_t at = 0; at < L; ++at)
    {
        std::uint64_t const limbSum = std::uint64_t { augend.limb(at) } + addend.limb(at) + carry;
        held[at] = static_cast<std::uint32_t>(limbSum & 0xFFFF'FFFFU);
        carry = limbSum >> 32U;
    }
    if (carry != 0)
        return std::nullopt;
    return WideUnsigned<L>::from_limbs(held);
}

/// @p minuend less @p subtrahend, or nothing when that is negative.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> sub_checked_or_none(WideUnsigned<L> const& minuend,
                                                                           WideUnsigned<L> const& subtrahend) noexcept
{
    std::array<std::uint32_t, L> held {};
    std::uint64_t borrow = 0;
    for (std::size_t at = 0; at < L; ++at)
    {
        std::uint64_t const taken = std::uint64_t { subtrahend.limb(at) } + borrow;
        std::uint64_t const fromLimb = minuend.limb(at);
        // Modular, so well defined; masked to the limb.
        held[at] = static_cast<std::uint32_t>((fromLimb - taken) & 0xFFFF'FFFFU);
        borrow = fromLimb < taken ? 1U : 0U;
    }
    if (borrow != 0)
        return std::nullopt;
    return WideUnsigned<L>::from_limbs(held);
}

/// @p multiplicand times @p multiplier, schoolbook, or nothing when the
/// product leaves the width. A limb product plus a limb plus a carry is at
/// most 2^64 - 1, so each step fits `std::uint64_t`. A zero limb of
/// @p multiplicand is skipped whole, which is what keeps a product with a
/// small or sparse left operand cheap in a constant evaluation; the answer
/// is the same in either operand order.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> mul_checked_or_none(WideUnsigned<L> const& multiplicand,
                                                                           WideUnsigned<L> const& multiplier) noexcept
{
    std::array<std::uint32_t, L> held {};
    for (std::size_t outer = 0; outer < L; ++outer)
    {
        std::uint64_t const multiplicandLimb = multiplicand.limb(outer);
        if (multiplicandLimb == 0)
            continue;
        std::uint64_t carry = 0;
        for (std::size_t inner = 0; inner + outer < L; ++inner)
        {
            std::uint64_t const term = multiplicandLimb * multiplier.limb(inner) + held[inner + outer] + carry;
            held[inner + outer] = static_cast<std::uint32_t>(term & 0xFFFF'FFFFU);
            carry = term >> 32U;
        }
        if (carry != 0)
            return std::nullopt;
        // A non-zero limb of the multiplier that would land past the top.
        for (std::size_t inner = L - outer; inner < L; ++inner)
            if (multiplier.limb(inner) != 0)
                return std::nullopt;
    }
    return WideUnsigned<L>::from_limbs(held);
}

/// @p multiplicand times the 32-bit @p smallFactor, in one carry chain, or
/// nothing when the product leaves the width.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> mul_small_checked_or_none(WideUnsigned<L> const& multiplicand,
                                                                                 std::uint32_t smallFactor) noexcept
{
    std::array<std::uint32_t, L> held {};
    std::uint64_t carry = 0;
    for (std::size_t at = 0; at < L; ++at)
    {
        std::uint64_t const term = std::uint64_t { multiplicand.limb(at) } * smallFactor + carry;
        held[at] = static_cast<std::uint32_t>(term & 0xFFFF'FFFFU);
        carry = term >> 32U;
    }
    if (carry != 0)
        return std::nullopt;
    return WideUnsigned<L>::from_limbs(held);
}

/// @p augend plus the 32-bit @p smallAddend, in one carry chain, or nothing
/// when the sum leaves the width.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> add_small_checked_or_none(WideUnsigned<L> const& augend,
                                                                                 std::uint32_t smallAddend) noexcept
{
    std::array<std::uint32_t, L> held {};
    std::uint64_t carry = smallAddend;
    for (std::size_t at = 0; at < L; ++at)
    {
        std::uint64_t const limbSum = std::uint64_t { augend.limb(at) } + carry;
        held[at] = static_cast<std::uint32_t>(limbSum & 0xFFFF'FFFFU);
        carry = limbSum >> 32U;
    }
    if (carry != 0)
        return std::nullopt;
    return WideUnsigned<L>::from_limbs(held);
}

/// @p shifted times 2^@p shiftCount, or nothing when a set bit would leave the
/// width. Zero shifts to zero by any count.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> shift_left_checked_or_none(WideUnsigned<L> const& shifted,
                                                                                  std::size_t shiftCount) noexcept
{
    if (shifted.is_zero())
        return shifted;
    if (shiftCount >= WideUnsigned<L>::bits || shifted.bit_length() + shiftCount > WideUnsigned<L>::bits)
        return std::nullopt;
    std::array<std::uint32_t, L> held {};
    std::size_t const wholeLimbs = shiftCount / 32;
    auto const bitShift = static_cast<unsigned>(shiftCount % 32);
    for (std::size_t at = L; at > wholeLimbs; --at)
    {
        std::size_t const fromAt = at - 1 - wholeLimbs;
        std::uint32_t moved = shifted.limb(fromAt) << bitShift;
        if (bitShift != 0 && fromAt > 0)
            moved |= shifted.limb(fromAt - 1) >> (32U - bitShift);
        held[at - 1] = moved;
    }
    return WideUnsigned<L>::from_limbs(held);
}

/// @p shifted divided by 2^@p shiftCount, truncated: zero from the width on.
template <std::size_t L>
[[nodiscard]] constexpr WideUnsigned<L> shift_right(WideUnsigned<L> const& shifted, std::size_t shiftCount) noexcept
{
    std::array<std::uint32_t, L> held {};
    if (shiftCount >= WideUnsigned<L>::bits)
        return WideUnsigned<L>::from_limbs(held);
    std::size_t const wholeLimbs = shiftCount / 32;
    auto const bitShift = static_cast<unsigned>(shiftCount % 32);
    for (std::size_t at = 0; at + wholeLimbs < L; ++at)
    {
        std::uint32_t moved = shifted.limb(at + wholeLimbs) >> bitShift;
        if (bitShift != 0)
            moved |= static_cast<std::uint32_t>(shifted.limb(at + wholeLimbs + 1) << (32U - bitShift));
        held[at] = moved;
    }
    return WideUnsigned<L>::from_limbs(held);
}

/// A quotient and its remainder: `divmod`'s answer.
template <std::size_t L>
struct WideDivision
{
    /// The quotient, truncated.
    WideUnsigned<L> quotient;
    /// What is left, always below the divisor.
    WideUnsigned<L> remainder;
};

/// @p dividend divided by @p divisor, by binary long division: one bit of the
/// dividend at a time into the remainder, the divisor subtracted whenever it
/// fits. Simple, and obviously right: `remainder < divisor` holds before each
/// step, and the remainder never exceeds the dividend's leading bits read so
/// far, so shifting it left never loses a set bit -- a divisor with its top
/// bit set needs no special case.
///
/// @pre @p divisor is not zero; every caller establishes it. A zero divisor is
/// not detected: the answer is then a quotient with every bit set up to the
/// dividend's leading bit, and the dividend as the remainder.
template <std::size_t L>
[[nodiscard]] constexpr WideDivision<L> divmod(WideUnsigned<L> const& dividend, WideUnsigned<L> const& divisor) noexcept
{
    std::array<std::uint32_t, L> quotientLimbs {};
    std::array<std::uint32_t, L> remainderLimbs {};
    for (std::size_t bitsLeft = dividend.bit_length(); bitsLeft > 0; --bitsLeft)
    {
        std::size_t const bitAt = bitsLeft - 1;
        for (std::size_t at = L; at > 1; --at)
            remainderLimbs[at - 1] = (remainderLimbs[at - 1] << 1U) | (remainderLimbs[at - 2] >> 31U);
        remainderLimbs[0] = (remainderLimbs[0] << 1U) | ((dividend.limb(bitAt / 32) >> (bitAt % 32)) & 1U);
        if (!(WideUnsigned<L>::from_limbs(remainderLimbs) < divisor))
        {
            std::uint64_t borrow = 0;
            for (std::size_t at = 0; at < L; ++at)
            {
                std::uint64_t const taken = std::uint64_t { divisor.limb(at) } + borrow;
                std::uint64_t const fromLimb = remainderLimbs[at];
                remainderLimbs[at] = static_cast<std::uint32_t>((fromLimb - taken) & 0xFFFF'FFFFU);
                borrow = fromLimb < taken ? 1U : 0U;
            }
            quotientLimbs[bitAt / 32] |= std::uint32_t { 1 } << (bitAt % 32);
        }
    }
    return WideDivision<L> { WideUnsigned<L>::from_limbs(quotientLimbs), WideUnsigned<L>::from_limbs(remainderLimbs) };
}

/// A quotient and its remainder below a 32-bit divisor: `divmod_small`'s answer.
template <std::size_t L>
struct WideSmallDivision
{
    /// The quotient, truncated.
    WideUnsigned<L> quotient;
    /// What is left, below the divisor.
    std::uint32_t remainder;
};

/// @p dividend divided by the 32-bit @p divisor, one limb at a time from the
/// top: each step divides `carried * 2^32 + limb`, which is below
/// `divisor * 2^32` and so fits `std::uint64_t`. Far cheaper than `divmod`
/// in a constant evaluation -- one step per limb, not per bit.
///
/// @pre @p divisor is not zero; every caller establishes it. A zero divisor
/// divides by zero: undefined behaviour at run time, and not a constant
/// expression at compile time.
template <std::size_t L>
[[nodiscard]] constexpr WideSmallDivision<L> divmod_small(WideUnsigned<L> const& dividend, std::uint32_t divisor) noexcept
{
    std::array<std::uint32_t, L> quotientLimbs {};
    std::uint64_t carried = 0;
    for (std::size_t at = L; at > 0; --at)
    {
        std::uint64_t const partial = (carried << 32U) | dividend.limb(at - 1);
        quotientLimbs[at - 1] = static_cast<std::uint32_t>(partial / divisor);
        carried = partial % divisor;
    }
    return WideSmallDivision<L> { WideUnsigned<L>::from_limbs(quotientLimbs), static_cast<std::uint32_t>(carried) };
}

/// The greatest common divisor, by the binary algorithm: `gcd(0, n) == n`.
template <std::size_t L>
[[nodiscard]] constexpr WideUnsigned<L> gcd(WideUnsigned<L> leftOperand, WideUnsigned<L> rightOperand) noexcept
{
    if (leftOperand.is_zero())
        return rightOperand;
    if (rightOperand.is_zero())
        return leftOperand;
    std::size_t sharedTwos = 0;
    while (((leftOperand.limb(0) | rightOperand.limb(0)) & 1U) == 0)
    {
        leftOperand = shift_right(leftOperand, 1);
        rightOperand = shift_right(rightOperand, 1);
        ++sharedTwos;
    }
    while ((leftOperand.limb(0) & 1U) == 0)
        leftOperand = shift_right(leftOperand, 1);
    while (!rightOperand.is_zero())
    {
        while ((rightOperand.limb(0) & 1U) == 0)
            rightOperand = shift_right(rightOperand, 1);
        if (rightOperand < leftOperand)
        {
            WideUnsigned<L> const smaller = rightOperand;
            rightOperand = leftOperand;
            leftOperand = smaller;
        }
        rightOperand = *sub_checked_or_none(rightOperand, leftOperand);
    }
    // The common factor of two was taken out of both, so it fits back in.
    return *shift_left_checked_or_none(leftOperand, sharedTwos);
}

/// The least common multiple, zero when either operand is zero, or nothing
/// when it leaves the width.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> lcm_checked_or_none(WideUnsigned<L> const& leftOperand,
                                                                           WideUnsigned<L> const& rightOperand) noexcept
{
    if (leftOperand.is_zero() || rightOperand.is_zero())
        return WideUnsigned<L> {};
    return mul_checked_or_none(divmod(leftOperand, gcd(leftOperand, rightOperand)).quotient, rightOperand);
}

/// 10 to the power @p exponent, or nothing when it leaves the width.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> pow10(std::size_t exponent) noexcept
{
    WideUnsigned<L> power = WideUnsigned<L>::from_u64(1);
    for (std::size_t times = 0; times < exponent; ++times)
    {
        std::optional<WideUnsigned<L>> const grown = mul_small_checked_or_none(power, 10U);
        if (!grown)
            return std::nullopt;
        power = *grown;
    }
    return power;
}

/// A fraction: a sign beside a numerator and a denominator.
template <std::size_t L>
struct WideRatio
{
    /// Whether the fraction is below zero; never set for a zero numerator.
    bool negative = false;
    /// The numerator's magnitude.
    WideUnsigned<L> numerator;
    /// The denominator; not zero. The fraction need not be in lowest terms.
    WideUnsigned<L> denominator;
};

/// A signed integer: a sign beside a magnitude.
template <std::size_t L>
struct WideSigned
{
    /// Whether the integer is below zero; never set for a zero magnitude.
    bool negative = false;
    /// The magnitude.
    WideUnsigned<L> magnitude;
};

/// @p leftOperand plus @p rightOperand, signs included; nothing on overflow.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideSigned<L>> add_checked_or_none(WideSigned<L> const& leftOperand,
                                                                         WideSigned<L> const& rightOperand) noexcept
{
    if (leftOperand.negative == rightOperand.negative)
    {
        std::optional<WideUnsigned<L>> const together = add_checked_or_none(leftOperand.magnitude, rightOperand.magnitude);
        if (!together)
            return std::nullopt;
        return WideSigned<L> { leftOperand.negative && !together->is_zero(), *together };
    }
    bool const leftLarger = !(leftOperand.magnitude < rightOperand.magnitude);
    WideUnsigned<L> const apart = leftLarger ? *sub_checked_or_none(leftOperand.magnitude, rightOperand.magnitude)
                                             : *sub_checked_or_none(rightOperand.magnitude, leftOperand.magnitude);
    return WideSigned<L> { (leftLarger ? leftOperand.negative : rightOperand.negative) && !apart.is_zero(), apart };
}

/// @p leftOperand less @p rightOperand, signs included; nothing on overflow.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideSigned<L>> sub_checked_or_none(WideSigned<L> const& leftOperand,
                                                                         WideSigned<L> const& rightOperand) noexcept
{
    return add_checked_or_none(
        leftOperand, WideSigned<L> { !rightOperand.negative && !rightOperand.magnitude.is_zero(), rightOperand.magnitude });
}

/// @p leftOperand times @p rightOperand, negative when the signs differ and the
/// product is not zero; nothing on overflow.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideSigned<L>> mul_checked_or_none(WideSigned<L> const& leftOperand,
                                                                         WideSigned<L> const& rightOperand) noexcept
{
    std::optional<WideUnsigned<L>> const product = mul_checked_or_none(leftOperand.magnitude, rightOperand.magnitude);
    if (!product)
        return std::nullopt;
    return WideSigned<L> { leftOperand.negative != rightOperand.negative && !product->is_zero(), *product };
}

} // namespace formula::detail
