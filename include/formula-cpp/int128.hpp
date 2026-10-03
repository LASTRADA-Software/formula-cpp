// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// `formula::Int128`, the signed 128-bit integer `Rational` stores its
/// numerator and denominator in.
///
/// One class, with one API on every compiler. It is held as two 64-bit words
/// in two's complement everywhere. Where the compiler has a 128-bit integer
/// -- GCC, Clang and AppleClang -- multiplication, division and the overflow
/// check are carried out in it; elsewhere, in portable `constexpr` code on the
/// two words. That is cl, and clang-cl too: it accepts `__int128`, but
/// dividing one calls compiler-rt's `__divti3`, which the MSVC linker does
/// not supply. Both routes give the same bits for every operation, at compile
/// time and at run time, so a number is the same on every compiler.
///
/// Division, the remainder and the greatest common divisor take a 64-bit
/// route whenever their operands fit 64 bits, which nearly every number a
/// formula forms does.
///
/// **No conversion to a built-in integer type**, implicit or explicit.
/// Narrowing is spelled `to_int64()` and `to_uint64()`, which answer
/// `std::nullopt` when the value does not fit, so that no code can cut a
/// 128-bit value down to 64 bits without saying what happens when it does
/// not fit.
///
/// **Overflow is a precondition violation**, as for a built-in signed
/// integer. An arithmetic operator -- `+ - * / %` or unary `-` -- whose
/// exact result does not fit has broken its precondition: a sum, difference
/// or product past the range, -2^127 / -1, and -(-2^127). So has division
/// or remainder by zero. The shifts are outside it: `<<` loses the bits
/// shifted out, as a built-in `<<` does since C++20. A caller that needs to
/// know whether a result fits uses the checked forms in
/// `detail/checked_int.hpp`, as `Rational` does. `%` by -1 is 0 for every
/// dividend, the minimum included, since that result fits.
///
/// Its `std::formatter` lives in `format.hpp`, with the library's others.

#include <bit>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <type_traits>
#include <utility>

#if defined(__SIZEOF_INT128__) && !defined(_MSC_VER)
    /// 1 where the compiler's own 128-bit integer carries `Int128`'s
    /// multiplication and division, 0 where the portable code does. Internal:
    /// not part of the library's contract.
    #define FORMULA_NATIVE_INT128 1
#else
    /// 0: the portable code carries `Int128`'s multiplication and division.
    #define FORMULA_NATIVE_INT128 0
#endif

namespace formula::detail
{

/// An unsigned 128-bit integer, as two 64-bit words, the more significant
/// first so that the defaulted comparisons order it numerically. It holds
/// magnitudes: 2^127, the magnitude of `Int128`'s minimum, has no signed
/// counterpart.
struct UInt128
{
    /// Bits 64 to 127.
    std::uint64_t highWord = 0;
    /// Bits 0 to 63.
    std::uint64_t lowWord = 0;

    /// @p whole, widened.
    [[nodiscard]] static constexpr UInt128 from_u64(std::uint64_t whole) noexcept { return UInt128 { 0, whole }; }

    /// Whether it fits 64 bits.
    [[nodiscard]] constexpr bool fits_u64() const noexcept { return highWord == 0; }

    /// Whether it is zero.
    [[nodiscard]] constexpr bool is_zero() const noexcept { return highWord == 0 && lowWord == 0; }

    /// How many bits writing it takes: 0 for zero, 128 at most.
    [[nodiscard]] constexpr int bit_width() const noexcept
    {
        return highWord != 0 ? 64 + static_cast<int>(std::bit_width(highWord)) : static_cast<int>(std::bit_width(lowWord));
    }

    /// Numeric equality.
    [[nodiscard]] constexpr bool operator==(UInt128 const&) const noexcept = default;
    /// Numeric order: the more significant word first.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(UInt128 const&) const noexcept = default;
};

/// A quotient and a remainder.
struct UInt128Division
{
    /// The quotient, rounded toward zero.
    UInt128 quotient {};
    /// What is left over: below the divisor.
    UInt128 remainder {};
};

/// The portable arithmetic on two words. Every compiler builds it and the
/// tests check it; `Int128` uses it where the compiler has no 128-bit
/// integer of its own.
namespace portable
{
    /// The sum, wrapping past 2^128.
    [[nodiscard]] constexpr UInt128 add(UInt128 leftOperand, UInt128 rightOperand) noexcept
    {
        std::uint64_t const lowSum = leftOperand.lowWord + rightOperand.lowWord;
        std::uint64_t const carried = lowSum < leftOperand.lowWord ? 1U : 0U;
        return UInt128 { leftOperand.highWord + rightOperand.highWord + carried, lowSum };
    }

    /// The difference, wrapping below zero.
    [[nodiscard]] constexpr UInt128 subtract(UInt128 leftOperand, UInt128 rightOperand) noexcept
    {
        std::uint64_t const borrowed = leftOperand.lowWord < rightOperand.lowWord ? 1U : 0U;
        return UInt128 { leftOperand.highWord - rightOperand.highWord - borrowed, leftOperand.lowWord - rightOperand.lowWord };
    }

    /// The full product of two words, from their 32-bit halves.
    [[nodiscard]] constexpr UInt128 multiply_words(std::uint64_t leftWord, std::uint64_t rightWord) noexcept
    {
        constexpr std::uint64_t HalfMask = 0xFFFF'FFFFU;
        std::uint64_t const lowLow = (leftWord & HalfMask) * (rightWord & HalfMask);
        std::uint64_t const highLow = (leftWord >> 32) * (rightWord & HalfMask);
        std::uint64_t const lowHigh = (leftWord & HalfMask) * (rightWord >> 32);
        std::uint64_t const highHigh = (leftWord >> 32) * (rightWord >> 32);
        std::uint64_t const middle = (lowLow >> 32) + (highLow & HalfMask) + (lowHigh & HalfMask);
        return UInt128 { highHigh + (highLow >> 32) + (lowHigh >> 32) + (middle >> 32),
                         (middle << 32) | (lowLow & HalfMask) };
    }

    /// The product's low 128 bits.
    [[nodiscard]] constexpr UInt128 multiply(UInt128 leftOperand, UInt128 rightOperand) noexcept
    {
        UInt128 product = multiply_words(leftOperand.lowWord, rightOperand.lowWord);
        // Each cross term lands in the high word; what passes 2^128 wraps away.
        product.highWord += leftOperand.highWord * rightOperand.lowWord + leftOperand.lowWord * rightOperand.highWord;
        return product;
    }

    /// The product, or nothing when it needs more than 128 bits.
    [[nodiscard]] constexpr std::optional<UInt128> multiply_checked(UInt128 leftOperand, UInt128 rightOperand) noexcept
    {
        if (leftOperand.highWord != 0 && rightOperand.highWord != 0)
            return std::nullopt;
        UInt128 product = multiply_words(leftOperand.lowWord, rightOperand.lowWord);
        // At most one of the two cross terms is not zero.
        UInt128 const crossTerm = leftOperand.highWord != 0 ? multiply_words(leftOperand.highWord, rightOperand.lowWord)
                                                            : multiply_words(leftOperand.lowWord, rightOperand.highWord);
        if (crossTerm.highWord != 0)
            return std::nullopt;
        std::uint64_t const raisedHigh = product.highWord + crossTerm.lowWord;
        if (raisedHigh < product.highWord)
            return std::nullopt;
        product.highWord = raisedHigh;
        return product;
    }

    /// Shifted left by @p places, below 128; the bits shifted out are lost.
    [[nodiscard]] constexpr UInt128 shift_left(UInt128 operandValue, int places) noexcept
    {
        if (places == 0)
            return operandValue;
        if (places >= 64)
            return UInt128 { operandValue.lowWord << (places - 64), 0 };
        return UInt128 { (operandValue.highWord << places) | (operandValue.lowWord >> (64 - places)),
                         operandValue.lowWord << places };
    }

    /// Shifted right by @p places, below 128, with zeros shifted in.
    [[nodiscard]] constexpr UInt128 shift_right(UInt128 operandValue, int places) noexcept
    {
        if (places == 0)
            return operandValue;
        if (places >= 64)
            return UInt128 { 0, operandValue.highWord >> (places - 64) };
        return UInt128 { operandValue.highWord >> places,
                         (operandValue.lowWord >> places) | (operandValue.highWord << (64 - places)) };
    }

    /// Long division, one bit of the quotient a step. @pre @p divisor is not zero.
    [[nodiscard]] constexpr UInt128Division divide(UInt128 dividend, UInt128 divisor) noexcept
    {
        if (dividend < divisor)
            return UInt128Division { UInt128 {}, dividend };
        int const shiftedBy = dividend.bit_width() - divisor.bit_width();
        UInt128 shiftedDivisor = shift_left(divisor, shiftedBy);
        UInt128 quotientSoFar {};
        UInt128 remaining = dividend;
        for (int place = shiftedBy; place >= 0; --place)
        {
            quotientSoFar = shift_left(quotientSoFar, 1);
            if (!(remaining < shiftedDivisor))
            {
                remaining = subtract(remaining, shiftedDivisor);
                quotientSoFar.lowWord |= 1U;
            }
            shiftedDivisor = shift_right(shiftedDivisor, 1);
        }
        return UInt128Division { quotientSoFar, remaining };
    }
} // namespace portable

#if FORMULA_NATIVE_INT128
/// The compiler's own unsigned 128-bit integer. `__extension__` keeps
/// `-Wpedantic` quiet under `-std=c++23`; nothing outside this header and its
/// tests names it, and it is never handed to the standard library, whose
/// type traits do not count it as an integer in that mode.
__extension__ typedef unsigned __int128 NativeUInt128;

/// @p operandValue as the compiler's own integer.
[[nodiscard]] constexpr NativeUInt128 to_native(UInt128 operandValue) noexcept
{
    return (static_cast<NativeUInt128>(operandValue.highWord) << 64) | operandValue.lowWord;
}

/// The compiler's own integer @p operandValue as two words.
[[nodiscard]] constexpr UInt128 from_native(NativeUInt128 operandValue) noexcept
{
    return UInt128 { static_cast<std::uint64_t>(operandValue >> 64), static_cast<std::uint64_t>(operandValue) };
}
#endif

/// The sum, wrapping past 2^128.
[[nodiscard]] constexpr UInt128 u128_add(UInt128 leftOperand, UInt128 rightOperand) noexcept
{
    return portable::add(leftOperand, rightOperand);
}

/// The difference, wrapping below zero.
[[nodiscard]] constexpr UInt128 u128_sub(UInt128 leftOperand, UInt128 rightOperand) noexcept
{
    return portable::subtract(leftOperand, rightOperand);
}

/// The full product of two words.
[[nodiscard]] constexpr UInt128 u128_mul_words(std::uint64_t leftWord, std::uint64_t rightWord) noexcept
{
#if FORMULA_NATIVE_INT128
    return from_native(static_cast<NativeUInt128>(leftWord) * rightWord);
#else
    return portable::multiply_words(leftWord, rightWord);
#endif
}

/// The product's low 128 bits.
[[nodiscard]] constexpr UInt128 u128_mul(UInt128 leftOperand, UInt128 rightOperand) noexcept
{
#if FORMULA_NATIVE_INT128
    return from_native(to_native(leftOperand) * to_native(rightOperand));
#else
    return portable::multiply(leftOperand, rightOperand);
#endif
}

/// The product, or nothing when it needs more than 128 bits.
[[nodiscard]] constexpr std::optional<UInt128> u128_mul_checked(UInt128 leftOperand, UInt128 rightOperand) noexcept
{
#if FORMULA_NATIVE_INT128
    NativeUInt128 product = 0;
    if (__builtin_mul_overflow(to_native(leftOperand), to_native(rightOperand), &product))
        return std::nullopt;
    return from_native(product);
#else
    return portable::multiply_checked(leftOperand, rightOperand);
#endif
}

/// Quotient and remainder. @pre @p divisor is not zero.
[[nodiscard]] constexpr UInt128Division u128_divmod(UInt128 dividend, UInt128 divisor) noexcept
{
    if (dividend.fits_u64() && divisor.fits_u64())
        return UInt128Division { UInt128::from_u64(dividend.lowWord / divisor.lowWord),
                                 UInt128::from_u64(dividend.lowWord % divisor.lowWord) };
#if FORMULA_NATIVE_INT128
    NativeUInt128 const nativeDividend = to_native(dividend);
    NativeUInt128 const nativeDivisor = to_native(divisor);
    return UInt128Division { from_native(nativeDividend / nativeDivisor), from_native(nativeDividend % nativeDivisor) };
#else
    return portable::divide(dividend, divisor);
#endif
}

/// How many zero bits end @p operandValue. @pre it is not zero.
[[nodiscard]] constexpr int u128_countr_zero(UInt128 operandValue) noexcept
{
    return operandValue.lowWord != 0 ? std::countr_zero(operandValue.lowWord) : 64 + std::countr_zero(operandValue.highWord);
}

/// The greatest common divisor; that of 0 and n is n. Binary: no step
/// divides, where each of Euclid's would be a 128-bit division. Both
/// operands of 64 bits, at the start or on the way, finish in 64 bits.
[[nodiscard]] constexpr UInt128 u128_gcd(UInt128 leftOperand, UInt128 rightOperand) noexcept
{
    if (leftOperand.fits_u64() && rightOperand.fits_u64())
        return UInt128::from_u64(std::gcd(leftOperand.lowWord, rightOperand.lowWord));
    if (leftOperand.is_zero())
        return rightOperand;
    if (rightOperand.is_zero())
        return leftOperand;
    int const sharedTwos = u128_countr_zero(
        UInt128 { leftOperand.highWord | rightOperand.highWord, leftOperand.lowWord | rightOperand.lowWord });
    leftOperand = portable::shift_right(leftOperand, u128_countr_zero(leftOperand));
    for (;;)
    {
        rightOperand = portable::shift_right(rightOperand, u128_countr_zero(rightOperand));
        if (rightOperand < leftOperand)
            std::swap(leftOperand, rightOperand);
        rightOperand = portable::subtract(rightOperand, leftOperand);
        if (rightOperand.is_zero())
            break;
        if (leftOperand.fits_u64() && rightOperand.fits_u64())
        {
            leftOperand = UInt128::from_u64(std::gcd(leftOperand.lowWord, rightOperand.lowWord));
            break;
        }
    }
    return portable::shift_left(leftOperand, sharedTwos);
}

/// The largest r with r * r <= @p radicand, which is below 2^64 for every
/// radicand: one bit of the root a step, from the top.
[[nodiscard]] constexpr std::uint64_t u128_isqrt(UInt128 radicand) noexcept
{
    std::uint64_t rootSoFar = 0;
    for (int bitAt = 63; bitAt >= 0; --bitAt)
    {
        std::uint64_t const candidate = rootSoFar | (std::uint64_t { 1 } << bitAt);
        if (!(radicand < u128_mul_words(candidate, candidate)))
            rootSoFar = candidate;
    }
    return rootSoFar;
}

/// 10^@p exponent for 0 to 38, and nothing otherwise: 10^38 is the largest
/// power of ten below 2^127.
[[nodiscard]] constexpr std::optional<UInt128> u128_pow10(int exponent) noexcept
{
    if (exponent < 0 || exponent > 38)
        return std::nullopt;
    UInt128 power = UInt128::from_u64(1);
    for (int multiplied = 0; multiplied < exponent; ++multiplied)
        power = u128_mul(power, UInt128::from_u64(10));
    return power;
}

/// The decimal digits of a 128-bit magnitude: at most 39.
struct DecimalSpelling
{
    /// The digits, most significant first; the first `length` are written.
    char characters[39] {};
    /// How many digits there are: 1 for zero.
    int length = 0;
};

/// @p magnitudeShown's decimal digits.
[[nodiscard]] constexpr DecimalSpelling u128_decimal(UInt128 magnitudeShown) noexcept
{
    char reversed[39] {};
    int produced = 0;
    do
    {
        UInt128Division const split = u128_divmod(magnitudeShown, UInt128::from_u64(10));
        reversed[produced] = static_cast<char>('0' + split.remainder.lowWord);
        ++produced;
        magnitudeShown = split.quotient;
    } while (!magnitudeShown.is_zero());
    DecimalSpelling written {};
    written.length = produced;
    for (int at = 0; at < produced; ++at)
        written.characters[at] = reversed[produced - 1 - at];
    return written;
}

/// The high word of @p whole's 128-bit two's complement form: all ones when
/// it is negative, zero otherwise. Outside `Int128`, so that it is defined
/// before any of that class's member bodies converts an `int` to it.
template <typename T>
[[nodiscard]] constexpr std::uint64_t sign_extension_word(T whole) noexcept
{
    if constexpr (std::is_signed_v<T>)
        return whole < 0 ? ~std::uint64_t { 0 } : std::uint64_t { 0 };
    else
        return 0;
}

} // namespace formula::detail

namespace formula
{

/// A signed 128-bit integer: `Rational`'s numerator and denominator. See
/// this header's file comment for how it computes, and why it converts to no
/// built-in integer type.
class Int128
{
  public:
    /// Zero.
    constexpr Int128() noexcept = default;

    /// @p whole, exactly: every built-in integer of up to 64 bits fits. Not
    /// `bool`, which is no number.
    template <typename T>
        requires std::is_integral_v<T> && (!std::is_same_v<std::remove_cv_t<T>, bool>) && (sizeof(T) <= 8)
    constexpr Int128(T whole) noexcept:
        _highWord { detail::sign_extension_word(whole) },
        _lowWord { static_cast<std::uint64_t>(whole) }
    {
    }

    /// The value whose two's complement words are @p highWord and @p lowWord.
    [[nodiscard]] static constexpr Int128 from_words(std::uint64_t highWord, std::uint64_t lowWord) noexcept
    {
        Int128 made {};
        made._highWord = highWord;
        made._lowWord = lowWord;
        return made;
    }

    /// Bits 64 to 127 of the two's complement form.
    [[nodiscard]] constexpr std::uint64_t high_word() const noexcept { return _highWord; }
    /// Bits 0 to 63 of the two's complement form.
    [[nodiscard]] constexpr std::uint64_t low_word() const noexcept { return _lowWord; }

    /// Whether it is below zero.
    [[nodiscard]] constexpr bool is_negative() const noexcept { return (_highWord >> 63) != 0U; }

    /// Whether `std::int64_t` holds it.
    [[nodiscard]] constexpr bool fits_int64() const noexcept
    {
        return _highWord == ((_lowWord >> 63) != 0U ? ~std::uint64_t { 0 } : std::uint64_t { 0 });
    }

    /// It as a `std::int64_t`, or nothing when it does not fit.
    [[nodiscard]] constexpr std::optional<std::int64_t> to_int64() const noexcept
    {
        if (!fits_int64())
            return std::nullopt;
        // Well defined since C++20: conversion to a signed type is modular.
        return static_cast<std::int64_t>(_lowWord);
    }

    /// It as a `std::uint64_t`, or nothing when it is negative or does not fit.
    [[nodiscard]] constexpr std::optional<std::uint64_t> to_uint64() const noexcept
    {
        if (_highWord != 0U)
            return std::nullopt;
        return _lowWord;
    }

    /// The nearest `double`, ties to even. Named, as `Rational::to_double` is,
    /// so that every loss of exactness is visible where it happens.
    [[nodiscard]] constexpr double to_double() const noexcept
    {
        if (fits_int64())
            return static_cast<double>(static_cast<std::int64_t>(_lowWord));
        detail::UInt128 const magnitudeOf = magnitude_pattern();
        int const dropped = magnitudeOf.bit_width() - 64;
        detail::UInt128 const kept = detail::portable::shift_right(magnitudeOf, dropped);
        // A sticky bit below the 53 a double keeps: rounding to nearest then
        // ties only when the bits dropped are exactly half a unit.
        bool const inexact = !(detail::portable::shift_left(kept, dropped) == magnitudeOf);
        double widened = static_cast<double>(kept.lowWord | (inexact ? 1U : 0U));
        for (int doubled = 0; doubled < dropped; ++doubled)
            widened *= 2.0;
        return is_negative() ? -widened : widened;
    }

    /// Numeric equality.
    [[nodiscard]] constexpr bool operator==(Int128 const&) const noexcept = default;

    /// Numeric order.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(Int128 const& compared) const noexcept
    {
        if (_highWord != compared._highWord)
            return static_cast<std::int64_t>(_highWord) <=> static_cast<std::int64_t>(compared._highWord);
        return _lowWord <=> compared._lowWord;
    }

    /// The sum. @pre it fits.
    [[nodiscard]] friend constexpr Int128 operator+(Int128 leftOperand, Int128 rightOperand) noexcept
    {
        return from_pattern(detail::portable::add(leftOperand.as_pattern(), rightOperand.as_pattern()));
    }

    /// The difference. @pre it fits.
    [[nodiscard]] friend constexpr Int128 operator-(Int128 leftOperand, Int128 rightOperand) noexcept
    {
        return from_pattern(detail::portable::subtract(leftOperand.as_pattern(), rightOperand.as_pattern()));
    }

    /// The product. @pre it fits.
    [[nodiscard]] friend constexpr Int128 operator*(Int128 leftOperand, Int128 rightOperand) noexcept
    {
        return from_pattern(detail::u128_mul(leftOperand.as_pattern(), rightOperand.as_pattern()));
    }

    /// The quotient, rounded toward zero. @pre @p divisor is not zero, and
    /// the quotient fits: -2^127 / -1 does not.
    [[nodiscard]] friend constexpr Int128 operator/(Int128 dividend, Int128 divisor) noexcept
    {
        detail::UInt128 const quotientMagnitude =
            detail::u128_divmod(dividend.magnitude_pattern(), divisor.magnitude_pattern()).quotient;
        // Negated through the bit pattern: a quotient of -2^127 fits, and its
        // magnitude has no signed counterpart to apply unary minus to.
        return from_pattern(dividend.is_negative() != divisor.is_negative()
                                ? detail::portable::subtract(detail::UInt128 {}, quotientMagnitude)
                                : quotientMagnitude);
    }

    /// The remainder, of the dividend's sign; by -1 it is 0 for every
    /// dividend, -2^127 included. @pre @p divisor is not zero.
    [[nodiscard]] friend constexpr Int128 operator%(Int128 dividend, Int128 divisor) noexcept
    {
        Int128 const remainderMagnitude =
            from_pattern(detail::u128_divmod(dividend.magnitude_pattern(), divisor.magnitude_pattern()).remainder);
        return dividend.is_negative() ? -remainderMagnitude : remainderMagnitude;
    }

    /// Shifted left by @p places, below 128. The bits shifted out are lost,
    /// as with a built-in `<<` since C++20: `Int128 { 1 } << 127` is -2^127.
    [[nodiscard]] friend constexpr Int128 operator<<(Int128 operandValue, int places) noexcept
    {
        return from_pattern(detail::portable::shift_left(operandValue.as_pattern(), places));
    }

    /// Shifted right by @p places, below 128: arithmetic, filling the bits
    /// vacated with the sign, so -8 >> 1 is -4.
    [[nodiscard]] friend constexpr Int128 operator>>(Int128 operandValue, int places) noexcept
    {
        detail::UInt128 const shifted = detail::portable::shift_right(operandValue.as_pattern(), places);
        if (!operandValue.is_negative() || places == 0)
            return from_pattern(shifted);
        detail::UInt128 const signFill =
            detail::portable::shift_left(detail::UInt128 { ~std::uint64_t { 0 }, ~std::uint64_t { 0 } }, 128 - places);
        return from_pattern(detail::UInt128 { shifted.highWord | signFill.highWord, shifted.lowWord | signFill.lowWord });
    }

    /// The negation. @pre @p operandValue is not -2^127, whose negation does
    /// not fit.
    [[nodiscard]] friend constexpr Int128 operator-(Int128 operandValue) noexcept
    {
        return from_pattern(detail::portable::subtract(detail::UInt128 {}, operandValue.as_pattern()));
    }

    /// Itself.
    [[nodiscard]] friend constexpr Int128 operator+(Int128 operandValue) noexcept { return operandValue; }

    /// `*this = *this + rightOperand`.
    constexpr Int128& operator+=(Int128 rightOperand) noexcept { return *this = *this + rightOperand; }
    /// `*this = *this - rightOperand`.
    constexpr Int128& operator-=(Int128 rightOperand) noexcept { return *this = *this - rightOperand; }
    /// `*this = *this * rightOperand`.
    constexpr Int128& operator*=(Int128 rightOperand) noexcept { return *this = *this * rightOperand; }
    /// `*this = *this / rightOperand`.
    constexpr Int128& operator/=(Int128 rightOperand) noexcept { return *this = *this / rightOperand; }
    /// `*this = *this % rightOperand`.
    constexpr Int128& operator%=(Int128 rightOperand) noexcept { return *this = *this % rightOperand; }
    /// `*this = *this << places`.
    constexpr Int128& operator<<=(int places) noexcept { return *this = *this << places; }
    /// `*this = *this >> places`.
    constexpr Int128& operator>>=(int places) noexcept { return *this = *this >> places; }
    /// Adds one.
    constexpr Int128& operator++() noexcept { return *this += 1; }
    /// Subtracts one.
    constexpr Int128& operator--() noexcept { return *this -= 1; }
    /// Adds one, answering the value before.
    constexpr Int128 operator++(int) noexcept
    {
        Int128 const before = *this;
        ++*this;
        return before;
    }
    /// Subtracts one, answering the value before.
    constexpr Int128 operator--(int) noexcept
    {
        Int128 const before = *this;
        --*this;
        return before;
    }

  private:
    [[nodiscard]] constexpr detail::UInt128 as_pattern() const noexcept { return detail::UInt128 { _highWord, _lowWord }; }

    [[nodiscard]] static constexpr Int128 from_pattern(detail::UInt128 bitPattern) noexcept
    {
        return from_words(bitPattern.highWord, bitPattern.lowWord);
    }

    [[nodiscard]] constexpr detail::UInt128 magnitude_pattern() const noexcept
    {
        return is_negative() ? detail::portable::subtract(detail::UInt128 {}, as_pattern()) : as_pattern();
    }

    std::uint64_t _highWord = 0;
    std::uint64_t _lowWord = 0;
};

namespace detail
{
    /// The magnitude of @p operandValue: 2^127 for the minimum, which has no
    /// signed counterpart.
    [[nodiscard]] constexpr UInt128 magnitude(Int128 operandValue) noexcept
    {
        UInt128 const wordPattern { operandValue.high_word(), operandValue.low_word() };
        return operandValue.is_negative() ? portable::subtract(UInt128 {}, wordPattern) : wordPattern;
    }

    /// The `Int128` of magnitude @p magnitudeOf, negative when @p negative.
    /// @pre it fits: @p magnitudeOf is below 2^127, or is 2^127 and @p negative.
    [[nodiscard]] constexpr Int128 signed_from_magnitude(UInt128 magnitudeOf, bool negative) noexcept
    {
        // Negated through the bit pattern: 2^127 has no signed counterpart to
        // apply unary minus to.
        UInt128 const signedPattern = negative ? portable::subtract(UInt128 {}, magnitudeOf) : magnitudeOf;
        return Int128::from_words(signedPattern.highWord, signedPattern.lowWord);
    }
} // namespace detail

} // namespace formula

namespace std
{
/// `formula::Int128`'s limits, stated as a built-in signed integer's are.
template <>
class numeric_limits<formula::Int128>
{
  public:
    static constexpr bool is_specialized = true; ///< Specialized here.
    static constexpr bool is_signed = true; ///< Signed.
    static constexpr bool is_integer = true; ///< An integer.
    static constexpr bool is_exact = true; ///< Exact.
    static constexpr bool has_infinity = false; ///< No infinity.
    static constexpr bool has_quiet_NaN = false; ///< No quiet NaN.
    static constexpr bool has_signaling_NaN = false; ///< No signaling NaN.
    static constexpr std::float_round_style round_style = std::round_toward_zero; ///< Division truncates toward zero.
    static constexpr bool is_iec559 = false; ///< Not a floating-point type.
    static constexpr bool is_bounded = true; ///< Holds a finite range.
    static constexpr bool is_modulo = false; ///< Overflow is a precondition violation, not a wrap.
    static constexpr int digits = 127; ///< The bits besides the sign.
    static constexpr int digits10 = 38; ///< Every 38-digit decimal fits.
    static constexpr int max_digits10 = 0; ///< Zero: an integer has no rounding to undo.
    static constexpr int radix = 2; ///< Binary.
    static constexpr int min_exponent = 0; ///< Zero: an integer has no exponent.
    static constexpr int min_exponent10 = 0; ///< Zero: an integer has no exponent.
    static constexpr int max_exponent = 0; ///< Zero: an integer has no exponent.
    static constexpr int max_exponent10 = 0; ///< Zero: an integer has no exponent.
    static constexpr bool traps = false; ///< No arithmetic traps.
    static constexpr bool tinyness_before = false; ///< False: an integer has no tininess.

    /// -2^127.
    [[nodiscard]] static constexpr formula::Int128 min() noexcept
    {
        return formula::Int128::from_words(std::uint64_t { 1 } << 63, 0);
    }
    /// -2^127.
    [[nodiscard]] static constexpr formula::Int128 lowest() noexcept { return min(); }
    /// 2^127 - 1.
    [[nodiscard]] static constexpr formula::Int128 max() noexcept
    {
        return formula::Int128::from_words(~(std::uint64_t { 1 } << 63), ~std::uint64_t { 0 });
    }
    /// Zero: an integer has none.
    [[nodiscard]] static constexpr formula::Int128 epsilon() noexcept { return 0; }
    /// Zero: an integer has none.
    [[nodiscard]] static constexpr formula::Int128 round_error() noexcept { return 0; }
    /// Zero: an integer has none.
    [[nodiscard]] static constexpr formula::Int128 infinity() noexcept { return 0; }
    /// Zero: an integer has none.
    [[nodiscard]] static constexpr formula::Int128 quiet_NaN() noexcept { return 0; }
    /// Zero: an integer has none.
    [[nodiscard]] static constexpr formula::Int128 signaling_NaN() noexcept { return 0; }
    /// Zero: an integer has none.
    [[nodiscard]] static constexpr formula::Int128 denorm_min() noexcept { return 0; }
};
} // namespace std
