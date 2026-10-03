// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Overflow-detecting integer primitives, all constexpr, on two widths.
///
/// - **128 bits**, for `Int128`, which `Rational` stores its numerator and
///   denominator in. The sum and the difference are formed on the two words'
///   bit patterns; the product's overflow check is the compiler's own 128-bit
///   integer where it has one, and portable code on the two words elsewhere
///   (`int128.hpp`).
/// - **64 bits**, `Int`, for what stays 64-bit: the powers of ten up to 10^18
///   that rounding's decimal places and `from_decimal`'s exponent span, the
///   `_r` literal's mantissa, and narrowing a value to the 64-bit fields of
///   `Band` and `Breakpoint`, or to the transcendental kernel's 64-bit
///   words (`narrow_to_int64`). MSVC has no
///   __builtin_*_overflow, and its <intrin.h> equivalents are not constexpr,
///   so these checks are written in portable C++ and used on every compiler.
///   Optimisers recognise these idioms.
///
/// **The overflow census.** This repository's own census programs are compiled
/// with `FORMULA_OVERFLOW_CENSUS` defined. The macro is internal to them: it
/// is not part of the library's contract, no consumer's build defines it,
/// and it must be defined in all of a program's translation units or in
/// none -- one of each gives the arithmetic below two definitions, which the
/// linker would silently merge. cl and clang-cl refuse such a program at
/// link time (`#pragma detect_mismatch`, below: LNK2038); an ELF linker has
/// no such check.
/// Then every integer these primitives form at run time -- and every
/// numerator and denominator `Rational::make` is handed -- is reported to
/// `census_record`, which the census program defines
/// (`support/census_tally.cpp`), so that it can say how many of the bits
/// `Rational::Int` holds real formulas use (`docs/numeric-headroom.md`). A
/// constant evaluation reports nothing. Without the macro -- every build but
/// the census's -- `FORMULA_CENSUS_NOTE` expands to nothing, its arguments
/// are never evaluated, and none of the census's names exist: no call, no
/// symbol, no cost.

#include <formula-cpp/int128.hpp>

#include <cstdint>
#include <optional>
#include <type_traits>

namespace formula::detail
{

using Int = std::int64_t;

inline constexpr Int IntMax = 9223372036854775807LL;
inline constexpr Int IntMin = -IntMax - 1;

/// Absolute value as an unsigned quantity. Exists because `-IntMin` overflows
/// but `magnitude(IntMin)` is an ordinary number.
[[nodiscard]] constexpr std::uint64_t magnitude(Int operandValue) noexcept
{
    return operandValue < 0 ? ~static_cast<std::uint64_t>(operandValue) + 1U : static_cast<std::uint64_t>(operandValue);
}

#if defined(_MSC_VER)
    #if defined(FORMULA_OVERFLOW_CENSUS)
        #pragma detect_mismatch("formula_overflow_census", "on")
    #else
        #pragma detect_mismatch("formula_overflow_census", "off")
    #endif
#endif

#if defined(FORMULA_OVERFLOW_CENSUS)
/// What an integer the overflow census is told of was: a numerator or a
/// denominator handed to `Rational::make`, any other signed intermediate, or
/// an unsigned one (`rounded_sqrt`'s, which has 128 bits to use).
enum class CensusRole : std::uint8_t
{
    Numerator,
    Denominator,
    Intermediate,
    Unsigned,
};

/// Told the magnitude of an integer formed at run time, as 128 bits. Declared
/// here and defined only by the census program, never by the library.
void census_record(CensusRole role, UInt128 magnitudeSeen) noexcept;

/// Tells the overflow census of @p magnitudeSeen, unless this is a constant
/// evaluation.
constexpr void census_note(CensusRole role, UInt128 magnitudeSeen) noexcept
{
    if !consteval
    {
        census_record(role, magnitudeSeen);
    }
}

/// Tells the overflow census of a 64-bit @p magnitudeSeen.
constexpr void census_note(CensusRole role, std::uint64_t magnitudeSeen) noexcept
{
    census_note(role, UInt128::from_u64(magnitudeSeen));
}

    /// Tells the overflow census that an integer of @p magnitudeSeen was formed
    /// in @p role (a `CensusRole` enumerator's name). See the file comment.
    #define FORMULA_CENSUS_NOTE(role, magnitudeSeen) \
        ::formula::detail::census_note(::formula::detail::CensusRole::role, (magnitudeSeen))
#else
    /// Nothing: this is not a census build. The arguments are not evaluated.
    #define FORMULA_CENSUS_NOTE(role, magnitudeSeen) static_cast<void>(0)
#endif

/// True when `leftOperand + rightOperand` is not representable.
[[nodiscard]] constexpr bool add_overflows(Int leftOperand, Int rightOperand) noexcept
{
    return (rightOperand > 0 && leftOperand > IntMax - rightOperand)
           || (rightOperand < 0 && leftOperand < IntMin - rightOperand);
}

/// True when `leftOperand - rightOperand` is not representable.
[[nodiscard]] constexpr bool sub_overflows(Int leftOperand, Int rightOperand) noexcept
{
    return (rightOperand < 0 && leftOperand > IntMax + rightOperand)
           || (rightOperand > 0 && leftOperand < IntMin + rightOperand);
}

/// True when `leftOperand * rightOperand` is not representable.
[[nodiscard]] constexpr bool mul_overflows(Int leftOperand, Int rightOperand) noexcept
{
    if (leftOperand == 0 || rightOperand == 0)
        return false;
    if (leftOperand > 0)
        return rightOperand > 0 ? leftOperand > IntMax / rightOperand : rightOperand < IntMin / leftOperand;
    return rightOperand > 0 ? leftOperand < IntMin / rightOperand : leftOperand < IntMax / rightOperand;
}

// Named *_or_none, not checked_*: `checked_` is reserved elsewhere in this
// library for a function returning `std::expected<T, ArithmeticError>`. These
// return `std::optional<Int>` -- a different contract that must not share the
// prefix meant to promise the first one.

[[nodiscard]] constexpr std::optional<Int> add_checked_or_none(Int leftOperand, Int rightOperand) noexcept
{
    if (add_overflows(leftOperand, rightOperand))
        return std::nullopt;
    FORMULA_CENSUS_NOTE(Intermediate, magnitude(leftOperand + rightOperand));
    return leftOperand + rightOperand;
}

[[nodiscard]] constexpr std::optional<Int> sub_checked_or_none(Int leftOperand, Int rightOperand) noexcept
{
    if (sub_overflows(leftOperand, rightOperand))
        return std::nullopt;
    FORMULA_CENSUS_NOTE(Intermediate, magnitude(leftOperand - rightOperand));
    return leftOperand - rightOperand;
}

[[nodiscard]] constexpr std::optional<Int> mul_checked_or_none(Int leftOperand, Int rightOperand) noexcept
{
    if (mul_overflows(leftOperand, rightOperand))
        return std::nullopt;
    FORMULA_CENSUS_NOTE(Intermediate, magnitude(leftOperand * rightOperand));
    return leftOperand * rightOperand;
}

/// Greatest common divisor. `gcd(0, n) == n` and `gcd(0, 0) == 0`.
/// Unsigned so that `magnitude(IntMin)` is a legal argument.
[[nodiscard]] constexpr std::uint64_t gcd(std::uint64_t leftOperand, std::uint64_t rightOperand) noexcept
{
    while (rightOperand != 0)
    {
        std::uint64_t const remainder = leftOperand % rightOperand;
        leftOperand = rightOperand;
        rightOperand = remainder;
    }
    return leftOperand;
}

struct DivMod
{
    Int quotient {};
    Int remainder {};
};

/// Floored division: the remainder is always in `[0, denominator)`, unlike the
/// language's truncating `/` and `%`. Every rounding decision in this library is
/// expressed as "which side of `remainder / denominator` the value sits on",
/// which only reads cleanly with a non-negative remainder.
///
/// @pre `divisor > 0`.
[[nodiscard]] constexpr DivMod floor_divmod(Int dividend, Int divisor) noexcept
{
    Int truncated = dividend / divisor;
    Int remainder = dividend % divisor;
    if (remainder < 0)
    {
        // Safe: a non-zero remainder means |quotient| is strictly below
        // |numerator| / denominator, so the decrement cannot reach IntMin, and
        // remainder is greater than -denominator.
        --truncated;
        remainder += divisor;
    }
    return { truncated, remainder };
}

/// `10^exponent` for `0 <= exponent <= 18`; `nullopt` otherwise. 10^18 is the
/// largest power of ten representable in Int.
[[nodiscard]] constexpr std::optional<Int> pow10(int exponent) noexcept
{
    if (exponent < 0 || exponent > 18)
        return std::nullopt;
    Int power = 1;
    for (int multiplied = 0; multiplied < exponent; ++multiplied)
        power *= 10;
    return power;
}

/// Number of decimal digits in `|value|`. Zero has one digit.
[[nodiscard]] constexpr int decimal_digits(Int operandValue) noexcept
{
    std::uint64_t remaining = magnitude(operandValue);
    int digitCount = 1;
    while (remaining >= 10U)
    {
        remaining /= 10U;
        ++digitCount;
    }
    return digitCount;
}

/// `operandValue * 10^exponent`, or `nullopt` on overflow or an out-of-range exponent.
[[nodiscard]] constexpr std::optional<Int> mul_pow10(Int operandValue, int exponent) noexcept
{
    std::optional<Int> const powerOfTen = pow10(exponent);
    if (!powerOfTen)
        return std::nullopt;
    return mul_checked_or_none(operandValue, *powerOfTen);
}

// ---- 128 bits: `Int128`'s checked operations, and helpers that read
// `Rational::Int` whatever its width -------------------------------------------

/// @p operandValue's magnitude as 128 bits.
[[nodiscard]] constexpr UInt128 wide_magnitude(Int operandValue) noexcept
{
    return UInt128::from_u64(magnitude(operandValue));
}

/// @p operandValue's magnitude: 2^127 for the minimum.
[[nodiscard]] constexpr UInt128 wide_magnitude(Int128 operandValue) noexcept
{
    return magnitude(operandValue);
}

/// @p operandValue as a 64-bit integer, which it always is.
[[nodiscard]] constexpr std::optional<Int> narrow_to_int64(Int operandValue) noexcept
{
    return operandValue;
}

/// @p operandValue as a 64-bit integer, or nothing when it does not fit.
[[nodiscard]] constexpr std::optional<Int> narrow_to_int64(Int128 operandValue) noexcept
{
    return operandValue.to_int64();
}

/// The 64-bit integer whose two's complement bits are @p wordPattern's low
/// word. @pre the value fits: the high word is the low word's sign.
[[nodiscard]] constexpr Int int_from_pattern(UInt128 wordPattern, std::type_identity<Int>) noexcept
{
    // Well defined since C++20: conversion to a signed type is modular.
    return static_cast<Int>(wordPattern.lowWord);
}

/// The `Int128` whose two's complement bits are @p wordPattern.
[[nodiscard]] constexpr Int128 int_from_pattern(UInt128 wordPattern, std::type_identity<Int128>) noexcept
{
    return Int128::from_words(wordPattern.highWord, wordPattern.lowWord);
}

/// @p operandValue's two's complement bits.
[[nodiscard]] constexpr UInt128 pattern_of(Int128 operandValue) noexcept
{
    return UInt128 { operandValue.high_word(), operandValue.low_word() };
}

// The sum and the difference are formed on the bit patterns, which wrap, so
// that an overflow is detected without `Int128`'s own operators ever being
// handed a result that does not fit.

[[nodiscard]] constexpr std::optional<Int128> add_checked_or_none(Int128 leftOperand, Int128 rightOperand) noexcept
{
    Int128 const added = int_from_pattern(u128_add(pattern_of(leftOperand), pattern_of(rightOperand)),
                                          std::type_identity<Int128> {});
    if (leftOperand.is_negative() == rightOperand.is_negative() && added.is_negative() != leftOperand.is_negative())
        return std::nullopt;
    FORMULA_CENSUS_NOTE(Intermediate, magnitude(added));
    return added;
}

[[nodiscard]] constexpr std::optional<Int128> sub_checked_or_none(Int128 leftOperand, Int128 rightOperand) noexcept
{
    Int128 const subtracted = int_from_pattern(u128_sub(pattern_of(leftOperand), pattern_of(rightOperand)),
                                               std::type_identity<Int128> {});
    if (leftOperand.is_negative() != rightOperand.is_negative() && subtracted.is_negative() != leftOperand.is_negative())
        return std::nullopt;
    FORMULA_CENSUS_NOTE(Intermediate, magnitude(subtracted));
    return subtracted;
}

[[nodiscard]] constexpr std::optional<Int128> mul_checked_or_none(Int128 leftOperand, Int128 rightOperand) noexcept
{
    std::optional<UInt128> const productMagnitude = u128_mul_checked(magnitude(leftOperand), magnitude(rightOperand));
    if (!productMagnitude)
        return std::nullopt;
    bool const negative = !productMagnitude->is_zero() && leftOperand.is_negative() != rightOperand.is_negative();
    UInt128 const largestMagnitude = negative ? UInt128 { std::uint64_t { 1 } << 63, 0 }
                                              : UInt128 { ~(std::uint64_t { 1 } << 63), ~std::uint64_t { 0 } };
    if (largestMagnitude < *productMagnitude)
        return std::nullopt;
    FORMULA_CENSUS_NOTE(Intermediate, *productMagnitude);
    return signed_from_magnitude(*productMagnitude, negative);
}

/// The greatest common divisor of two 128-bit magnitudes (`u128_gcd`).
[[nodiscard]] constexpr UInt128 gcd(UInt128 leftOperand, UInt128 rightOperand) noexcept
{
    return u128_gcd(leftOperand, rightOperand);
}

/// `floor_divmod`'s answer on 128 bits.
struct WideDivMod
{
    /// The floored quotient.
    Int128 quotient {};
    /// The remainder, in `[0, divisor)`.
    Int128 remainder {};
};

/// Floored division on 128 bits, as `floor_divmod` on 64. @pre `divisor > 0`.
[[nodiscard]] constexpr WideDivMod floor_divmod(Int128 dividend, Int128 divisor) noexcept
{
    Int128 truncated = dividend / divisor;
    Int128 remainderLeft = dividend % divisor;
    if (remainderLeft < 0)
    {
        --truncated;
        remainderLeft += divisor;
    }
    return { truncated, remainderLeft };
}

/// Number of decimal digits in `|operandValue|`: at most 39. Zero has one.
[[nodiscard]] constexpr int decimal_digits(Int128 operandValue) noexcept
{
    return u128_decimal(magnitude(operandValue)).length;
}

/// `operandValue * 10^exponent`, for the exponents 0 to 18 `mul_pow10` takes
/// on 64 bits, or nothing on overflow or an exponent out of that range.
[[nodiscard]] constexpr std::optional<Int128> mul_pow10(Int128 operandValue, int exponent) noexcept
{
    std::optional<Int> const powerOfTen = pow10(exponent);
    if (!powerOfTen)
        return std::nullopt;
    return mul_checked_or_none(operandValue, Int128 { *powerOfTen });
}

} // namespace formula::detail
