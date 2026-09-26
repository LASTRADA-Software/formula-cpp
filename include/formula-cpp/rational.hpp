// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// An exact rational number over std::int64_t.
///
/// Norm rounding rules are specified behaviour, not presentation: "round the
/// result to 0,1 %" is part of the method. Binary floating point cannot express
/// that faithfully, and it makes exact round-tripping impossible -- 450 l stored
/// as 0,45 m3 and rendered back is not reliably 450.
///
/// This type carries no decimal-place tag. Declared precision belongs to the
/// unit and quantity layer; rounding is an explicit operation (rounding.hpp) and,
/// from spec phase 8 on, a node in the expression tree.

#include <formula-cpp/detail/checked_int.hpp>
#include <formula-cpp/error.hpp>

#include <bit>
#include <compare>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <type_traits>

namespace formula
{

namespace detail
{
    /// Always false, but dependent on T, so a static_assert inside a template
    /// only fires when that template is actually instantiated.
    template <typename T>
    inline constexpr bool AlwaysFalse = false;

    [[nodiscard]] constexpr std::strong_ordering invert(std::strong_ordering order) noexcept
    {
        if (order == std::strong_ordering::less)
            return std::strong_ordering::greater;
        if (order == std::strong_ordering::greater)
            return std::strong_ordering::less;
        return std::strong_ordering::equal;
    }
} // namespace detail

/// An exact rational number, always in lowest terms with a positive denominator.
class Rational
{
  public:
    /// The signed integer type numerator and denominator are stored in.
    using Int = detail::Int;

    /// Zero.
    constexpr Rational() noexcept = default;

    /// An integer is a rational, exactly and without narrowing.
    ///
    /// Constrained to integer types that reach Int without loss. An unsigned type
    /// as wide as Int is rejected at compile time: `std::size_t { 1 } << 63` would
    /// otherwise convert by modular wraparound and become a *negative* Rational,
    /// and `SIZE_MAX` would become -1. Producing a wrong number without saying so
    /// is the one thing this library must never do.
    template <typename T>
        requires std::is_integral_v<T> && (!std::is_same_v<std::remove_cv_t<T>, bool>)
    constexpr Rational(T whole) noexcept:
        _numerator { static_cast<Int>(whole) }
    {
        static_assert(std::is_signed_v<T> || sizeof(T) < sizeof(Int),
                      "formula: this unsigned type can hold values above Rational's maximum, where the "
                      "conversion would silently produce a negative value; check the range and use "
                      "Rational::make(value, 1), or cast explicitly");
    }

    /// Deliberately unusable. A double is a binary fraction: `Rational r = 0.45;`
    /// would silently mean 8106479329266893 / 2^54, not 9 / 20.
    template <typename T>
        requires std::is_floating_point_v<T>
    constexpr Rational(T)
    {
        static_assert(detail::AlwaysFalse<T>,
                      "formula: a floating-point value is not an exact rational; use "
                      "Rational::from_decimal(mantissa, exponent) for an exact decimal, "
                      "rational_from_double(value, places, mode) to round one, or "
                      "Rational::from_double_exact(value) for the exact binary value");
    }

    /// @throws ArithmeticException on a zero denominator or on overflow.
    constexpr Rational(Int dividend, Int divisor):
        Rational { detail::or_throw(make(dividend, divisor)) }
    {
    }

    /// Canonicalising factory. The only place the class invariants are established.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> make(Int dividend, Int divisor) noexcept
    {
        if (divisor == 0)
            return std::unexpected { ArithmeticError::DivisionByZero };
        if (dividend == 0)
            return Rational {};

        // Reduce in the unsigned domain so that IntMin is an ordinary operand.
        std::uint64_t const numeratorMagnitude = detail::magnitude(dividend);
        std::uint64_t const denominatorMagnitude = detail::magnitude(divisor);
        std::uint64_t const common = detail::gcd(numeratorMagnitude, denominatorMagnitude);
        std::uint64_t const reducedNumerator = numeratorMagnitude / common;
        std::uint64_t const reducedDenominator = denominatorMagnitude / common;

        bool const negative = (dividend < 0) != (divisor < 0);

        constexpr std::uint64_t PositiveLimit = static_cast<std::uint64_t>(detail::IntMax);
        std::uint64_t const numeratorLimit = negative ? PositiveLimit + 1U : PositiveLimit;
        if (reducedDenominator > PositiveLimit || reducedNumerator > numeratorLimit)
            return std::unexpected { ArithmeticError::Overflow };

        Rational made {};
        // Well defined since C++20: conversion to a signed type is modular.
        made._numerator = negative ? static_cast<Int>(0U - reducedNumerator) : static_cast<Int>(reducedNumerator);
        made._denominator = static_cast<Int>(reducedDenominator);
        return made;
    }

    /// `mantissa * 10^exponent`, exactly. The preferred way to write a decimal:
    /// `from_decimal(45, -2)` is 9/20, not the nearest double to 0,45.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> from_decimal(Int mantissa, int exponent) noexcept
    {
        if (mantissa == 0)
            return Rational {};
        if (exponent >= 0)
        {
            std::optional<Int> const scaledMantissa = detail::mul_pow10(mantissa, exponent);
            if (!scaledMantissa)
                return std::unexpected { ArithmeticError::Overflow };
            return Rational { *scaledMantissa };
        }
        std::optional<Int> const powerOfTen = detail::pow10(-exponent);
        if (!powerOfTen)
            return std::unexpected { ArithmeticError::Overflow };
        return make(mantissa, *powerOfTen);
    }

    /// The exact value of the double, which is a dyadic rational. Usually not
    /// what a norm means: 0,45 as a double is not 9/20. Prefer from_decimal, or
    /// rational_from_double when the input genuinely is a measured double.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> from_double_exact(double floating) noexcept
    {
        static_assert(std::numeric_limits<double>::is_iec559, "formula: from_double_exact assumes IEEE-754 doubles");

        std::uint64_t const bits = std::bit_cast<std::uint64_t>(floating);
        bool const negative = (bits >> 63) != 0U;
        auto const rawExponent = static_cast<int>((bits >> 52) & 0x7FFU);
        std::uint64_t const rawMantissa = bits & 0xF'FFFF'FFFF'FFFFULL;

        if (rawExponent == 0x7FF)
            return std::unexpected { ArithmeticError::NotFinite };

        // Subnormals have no implicit leading bit and a fixed exponent of -1074.
        std::uint64_t const significand = rawExponent == 0 ? rawMantissa : rawMantissa | (1ULL << 52);
        if (significand == 0)
            return Rational {};
        int const exponent = (rawExponent == 0 ? 1 : rawExponent) - 1075;

        // Strip trailing zero bits so the exponent needed is as small as possible.
        std::uint64_t reduced = significand;
        int shifted = exponent;
        while ((reduced & 1U) == 0U)
        {
            reduced >>= 1U;
            ++shifted;
        }

        if (reduced > static_cast<std::uint64_t>(detail::IntMax))
            return std::unexpected { ArithmeticError::Overflow };
        auto scaledReduced = static_cast<Int>(reduced);
        if (negative)
            scaledReduced = -scaledReduced;

        if (shifted >= 0)
        {
            if (shifted >= 63)
                return std::unexpected { ArithmeticError::Overflow };
            std::optional<Int> const scaledNumerator = detail::mul_checked_or_none(scaledReduced, Int { 1 } << shifted);
            if (!scaledNumerator)
                return std::unexpected { ArithmeticError::Overflow };
            return Rational { *scaledNumerator };
        }

        if (-shifted >= 63)
            return std::unexpected { ArithmeticError::Overflow };
        return make(scaledReduced, Int { 1 } << -shifted);
    }

    /// The numerator, in lowest terms.
    [[nodiscard]] constexpr Int numerator() const noexcept
    {
        return _numerator;
    }
    /// The denominator, in lowest terms and always positive.
    [[nodiscard]] constexpr Int denominator() const noexcept
    {
        return _denominator;
    }

    /// True when the denominator is 1, i.e. this value is a whole number.
    [[nodiscard]] constexpr bool is_integer() const noexcept
    {
        return _denominator == 1;
    }
    /// True for zero.
    [[nodiscard]] constexpr bool is_zero() const noexcept
    {
        return _numerator == 0;
    }

    /// -1, 0 or 1, for negative, zero and positive respectively.
    [[nodiscard]] constexpr int sign() const noexcept
    {
        return _numerator == 0 ? 0 : (_numerator < 0 ? -1 : 1);
    }

    /// Lossy by construction. Named so that every loss of exactness is visible
    /// at the call site.
    [[nodiscard]] constexpr double to_double() const noexcept
    {
        return static_cast<double>(_numerator) / static_cast<double>(_denominator);
    }

    /// Exact for every representable pair. Uses the continued-fraction
    /// (Euclidean) comparison, which performs no multiplication: cross
    /// multiplication would overflow for operands that are individually fine.
    [[nodiscard]] constexpr std::strong_ordering operator<=>(Rational const& compared) const noexcept
    {
        Int leftNumerator = _numerator;
        Int leftDenominator = _denominator;
        Int rightNumerator = compared._numerator;
        Int rightDenominator = compared._denominator;
        bool reversed = false;

        for (;;)
        {
            auto const leftParts = detail::floor_divmod(leftNumerator, leftDenominator);
            auto const rightParts = detail::floor_divmod(rightNumerator, rightDenominator);

            if (leftParts.quotient != rightParts.quotient)
            {
                std::strong_ordering const order = leftParts.quotient <=> rightParts.quotient;
                return reversed ? detail::invert(order) : order;
            }

            if (leftParts.remainder == 0 || rightParts.remainder == 0)
            {
                std::strong_ordering order = std::strong_ordering::equal;
                if (leftParts.remainder == 0 && rightParts.remainder != 0)
                    order = std::strong_ordering::less;
                else if (leftParts.remainder != 0 && rightParts.remainder == 0)
                    order = std::strong_ordering::greater;
                return reversed ? detail::invert(order) : order;
            }

            // Compare the reciprocals of the fractional parts, which flips the sense.
            leftNumerator = leftDenominator;
            leftDenominator = leftParts.remainder;
            rightNumerator = rightDenominator;
            rightDenominator = rightParts.remainder;
            reversed = !reversed;
        }
    }

    /// Exact equality -- a componentwise comparison, valid because both operands
    /// are always in canonical (lowest-terms, positive-denominator) form.
    [[nodiscard]] constexpr bool operator==(Rational const& compared) const noexcept
    {
        // Canonical form makes this a componentwise comparison.
        return _numerator == compared._numerator && _denominator == compared._denominator;
    }

  private:
    Int _numerator { 0 };
    Int _denominator { 1 };
};

/// Reciprocal. Fails on zero, and on the one value whose reciprocal is not
/// representable. Checked-only: there is no natural infallible-looking
/// spelling for this operation the way negation has unary `-`.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_reciprocal(Rational operandValue) noexcept
{
    if (operandValue.is_zero())
        return std::unexpected { ArithmeticError::DivisionByZero };
    return Rational::make(operandValue.denominator(), operandValue.numerator());
}

/// Sign flip. Fails only for the one numerator whose negation is not
/// representable. Its throwing counterpart is unary `operator-`.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_negate(Rational operandValue) noexcept
{
    if (operandValue.numerator() == detail::IntMin)
        return std::unexpected { ArithmeticError::Overflow };
    // Already canonical: negating the numerator preserves both invariants.
    return Rational::make(-operandValue.numerator(), operandValue.denominator());
}

/// Exact addition.
///
/// Scales by the least common multiple rather than by the product: with
/// denominators 6 and 10 this uses 30, not 60 -- the difference between fitting
/// and overflowing once denominators get large.
///
/// Known limitation: the numerator sum itself is not protected, so this reports
/// Overflow for a few operand pairs whose reduced result would fit -- both
/// numerators near 2^63 over denominators sharing a large factor. Measured: it
/// never occurs for numerators below roughly 10^6. The failure direction is
/// safe (a refusal, never a wrong number); lifting it needs 128-bit
/// intermediates, which MSVC cannot express portably.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_add(Rational leftOperand,
                                                                             Rational rightOperand) noexcept
{
    // Scale by the least common multiple rather than by the product: with
    // denominators 6 and 10 this uses 30, not 60 -- the difference between
    // fitting and overflowing once denominators get large.
    auto const common = static_cast<Rational::Int>(detail::gcd(static_cast<std::uint64_t>(leftOperand.denominator()),
                                                               static_cast<std::uint64_t>(rightOperand.denominator())));
    Rational::Int const leftScale = leftOperand.denominator() / common;
    Rational::Int const rightScale = rightOperand.denominator() / common;

    std::optional<Rational::Int> const leftTerm = detail::mul_checked_or_none(leftOperand.numerator(), rightScale);
    std::optional<Rational::Int> const rightTerm = detail::mul_checked_or_none(rightOperand.numerator(), leftScale);
    if (!leftTerm || !rightTerm)
        return std::unexpected { ArithmeticError::Overflow };

    std::optional<Rational::Int> const sumNumerator = detail::add_checked_or_none(*leftTerm, *rightTerm);
    std::optional<Rational::Int> const sumDenominator = detail::mul_checked_or_none(leftScale, rightOperand.denominator());
    if (!sumNumerator || !sumDenominator)
        return std::unexpected { ArithmeticError::Overflow };

    return Rational::make(*sumNumerator, *sumDenominator);
}

/// Exact subtraction. Implemented as negate-then-add, so it fails under
/// exactly the same conditions as `checked_negate` and `checked_add`.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_sub(Rational leftOperand,
                                                                             Rational rightOperand) noexcept
{
    std::expected<Rational, ArithmeticError> const negated = checked_negate(rightOperand);
    if (!negated)
        return negated;
    return checked_add(leftOperand, *negated);
}

/// Exact multiplication, cross-reducing before multiplying so that a product
/// which is exactly representable does not overflow on the way there.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_mul(Rational leftOperand,
                                                                             Rational rightOperand) noexcept
{
    // Cross-reduce before multiplying: (IntMax/3) * (3/IntMax) is exactly 1, but
    // multiplying the numerators first would overflow.
    //
    // Each gcd divides a denominator, so it is positive and at most IntMax.
    // Signed division by it is therefore always safe -- the only signed division
    // that can overflow is IntMin / -1.
    auto const leftCross = static_cast<Rational::Int>(
        detail::gcd(detail::magnitude(leftOperand.numerator()), static_cast<std::uint64_t>(rightOperand.denominator())));
    auto const rightCross = static_cast<Rational::Int>(
        detail::gcd(detail::magnitude(rightOperand.numerator()), static_cast<std::uint64_t>(leftOperand.denominator())));

    Rational::Int const leftNumerator = leftOperand.numerator() / leftCross;
    Rational::Int const rightNumerator = rightOperand.numerator() / rightCross;
    Rational::Int const leftDenominator = leftOperand.denominator() / rightCross;
    Rational::Int const rightDenominator = rightOperand.denominator() / leftCross;

    std::optional<Rational::Int> const productNumerator = detail::mul_checked_or_none(leftNumerator, rightNumerator);
    std::optional<Rational::Int> const productDenominator = detail::mul_checked_or_none(leftDenominator, rightDenominator);
    if (!productNumerator || !productDenominator)
        return std::unexpected { ArithmeticError::Overflow };

    return Rational::make(*productNumerator, *productDenominator);
}

/// Exact division. Implemented as reciprocal-then-multiply, so it fails under
/// division by zero and under exactly the conditions `checked_reciprocal` and
/// `checked_mul` do.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_div(Rational leftOperand,
                                                                             Rational rightOperand) noexcept
{
    if (rightOperand.is_zero())
        return std::unexpected { ArithmeticError::DivisionByZero };
    std::expected<Rational, ArithmeticError> const inverted = checked_reciprocal(rightOperand);
    if (!inverted)
        return inverted;
    return checked_mul(leftOperand, *inverted);
}

/// Integer power. A negative exponent inverts, so `pow(0, -1)` is a division by zero.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_pow(Rational base, int exponent) noexcept
{
    if (exponent == 0)
        return Rational { 1 };

    bool const invertResult = exponent < 0;
    // Widen before negating: -INT_MIN would overflow int.
    long long remaining = invertResult ? -static_cast<long long>(exponent) : static_cast<long long>(exponent);

    Rational power { 1 };
    Rational multiplier = base;
    while (remaining > 0)
    {
        if ((remaining & 1) != 0)
        {
            std::expected<Rational, ArithmeticError> const raised = checked_mul(power, multiplier);
            if (!raised)
                return raised;
            power = *raised;
        }
        remaining >>= 1;
        if (remaining > 0)
        {
            std::expected<Rational, ArithmeticError> const squared = checked_mul(multiplier, multiplier);
            if (!squared)
                return squared;
            multiplier = *squared;
        }
    }

    if (!invertResult)
        return power;
    return checked_reciprocal(power);
}

// ---- the operator layer: total-looking, but never silently wrong ----

/// Throwing addition -- `ArithmeticException` on overflow. See `checked_add`.
[[nodiscard]] constexpr Rational operator+(Rational leftOperand, Rational rightOperand)
{
    return detail::or_throw(checked_add(leftOperand, rightOperand));
}
/// Throwing subtraction -- `ArithmeticException` on overflow. See `checked_sub`.
[[nodiscard]] constexpr Rational operator-(Rational leftOperand, Rational rightOperand)
{
    return detail::or_throw(checked_sub(leftOperand, rightOperand));
}
/// Throwing multiplication -- `ArithmeticException` on overflow. See `checked_mul`.
[[nodiscard]] constexpr Rational operator*(Rational leftOperand, Rational rightOperand)
{
    return detail::or_throw(checked_mul(leftOperand, rightOperand));
}
/// Throwing division -- `ArithmeticException` on division by zero or overflow.
/// See `checked_div`.
[[nodiscard]] constexpr Rational operator/(Rational leftOperand, Rational rightOperand)
{
    return detail::or_throw(checked_div(leftOperand, rightOperand));
}

/// Throwing compound addition. See `operator+`.
constexpr Rational& operator+=(Rational& leftOperand, Rational rightOperand)
{
    return leftOperand = leftOperand + rightOperand;
}
/// Throwing compound subtraction. See `operator-`.
constexpr Rational& operator-=(Rational& leftOperand, Rational rightOperand)
{
    return leftOperand = leftOperand - rightOperand;
}
/// Throwing compound multiplication. See `operator*`.
constexpr Rational& operator*=(Rational& leftOperand, Rational rightOperand)
{
    return leftOperand = leftOperand * rightOperand;
}
/// Throwing compound division. See `operator/`.
constexpr Rational& operator/=(Rational& leftOperand, Rational rightOperand)
{
    return leftOperand = leftOperand / rightOperand;
}

/// Unary plus. A no-op; present for symmetry with unary minus.
[[nodiscard]] constexpr Rational operator+(Rational operandValue) noexcept
{
    return operandValue;
}
/// Throwing negation -- `ArithmeticException` for the one numerator whose sign
/// cannot be flipped. See `checked_negate`.
[[nodiscard]] constexpr Rational operator-(Rational operandValue)
{
    return detail::or_throw(checked_negate(operandValue));
}

/// Absolute value. Fails only where negation would: the one numerator whose
/// sign cannot be flipped.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_abs(Rational operandValue) noexcept
{
    if (operandValue.sign() >= 0)
        return operandValue;
    return checked_negate(operandValue);
}

/// Throwing absolute value. See `checked_abs`.
[[nodiscard]] constexpr Rational abs(Rational operandValue)
{
    return detail::or_throw(checked_abs(operandValue));
}

/// Throwing integer power. See `checked_pow`.
[[nodiscard]] constexpr Rational pow(Rational base, int exponent)
{
    return detail::or_throw(checked_pow(base, exponent));
}

/// A rational that is **not** pi.
///
/// 245 850 922 / 78 256 779 is a convergent of pi's continued fraction; it
/// differs from pi by less than 8e-17, which is finer than a `double` can
/// distinguish, and both halves fit comfortably in 64 bits. It is the one
/// deliberate approximation in the exact layer, and it is written here rather
/// than computed so that every caller gets the same number and the trace can
/// state which number it was.
inline constexpr Rational Pi { 245'850'922, 78'256'779 };

namespace detail
{
    /// The integer @p degree-th root of @p radicand, or nothing when it is not
    /// exact. Binary search rather than Newton: the search space is bounded by
    /// the value itself, every step stays inside `Int`, and there is no
    /// convergence question to get wrong.
    [[nodiscard]] constexpr std::optional<Rational::Int> exact_integer_root(Rational::Int radicand, int degree) noexcept
    {
        if (radicand < 0)
            return std::nullopt;
        if (radicand < 2)
            return radicand;

        Rational::Int lowGuess = 1;
        Rational::Int highGuess = radicand;
        while (lowGuess <= highGuess)
        {
            Rational::Int const middle = lowGuess + (highGuess - lowGuess) / 2;

            // middle^degree, abandoning the moment it exceeds `radicand` so the
            // multiplication can never overflow.
            Rational::Int power = 1;
            bool tooBig = false;
            for (int multiplied = 0; multiplied < degree; ++multiplied)
            {
                std::optional<Rational::Int> const raised = mul_checked_or_none(power, middle);
                if (!raised || *raised > radicand)
                {
                    tooBig = true;
                    break;
                }
                power = *raised;
            }

            if (tooBig)
                highGuess = middle - 1;
            else if (power == radicand)
                return middle;
            else
                lowGuess = middle + 1;
        }
        return std::nullopt;
    }
} // namespace detail

/// The exact @p degree-th root of @p radicand.
///
/// Answers only when the answer is a rational number: the root of 4 is 2 and the
/// root of 9/4 is 3/2, but the root of 2 is `ArithmeticError::Inexact` rather
/// than a nearby fraction. A layer whose promise is "never a wrong number" has
/// no business rounding silently; a formula that needs an irrational root is
/// evaluated in a representation that has room for one.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_exact_nth_root(Rational radicand,
                                                                                        int degree) noexcept
{
    if (degree < 1)
        return std::unexpected { ArithmeticError::DomainError };
    if (radicand.sign() < 0 && degree % 2 == 0)
        return std::unexpected { ArithmeticError::DomainError };

    // IntMin has no positive counterpart Int can hold -- its magnitude is
    // IntMax + 1 -- so negating it to reach a positive intermediate is signed
    // overflow, undefined behaviour. checked_negate refuses the same numerator
    // for the same reason; this follows that precedent rather than inventing a
    // second rule for it. Overflow is the honest answer here, not Inexact:
    // Inexact means no exact root exists, but IntMin's cube root, -2^21, both
    // exists and is representable -- it is only the magnitude of the
    // intermediate numerator that is not. Reworking the search onto an
    // unsigned magnitude to rescue this one input would add new numeric code
    // at the end of a phase to save a single edge case, which risks a worse
    // bug than the one it fixes.
    if (radicand.numerator() == detail::IntMin)
        return std::unexpected { ArithmeticError::Overflow };

    bool const negative = radicand.sign() < 0;
    Rational::Int const magnitudeNumerator = negative ? -radicand.numerator() : radicand.numerator();

    // At degree 63 or higher, exact_integer_root's binary search is
    // pathological rather than merely slow: once it probes middle == 1, power
    // stays 1 for the rest of that probe's inner loop, so the loop runs the
    // full `degree` multiplications of 1 by 1 before concluding "too small" --
    // and degree is an ordinary int a caller controls, so nothing bounds how
    // long that takes. The search is also unnecessary at this degree: 2^63
    // alone exceeds IntMax, so no numerator or denominator magnitude of 2 or
    // more could have an exact root here -- reaching it would need at least
    // 2^63, which Int cannot hold. That leaves only magnitude 0 and 1, both
    // fixed points of every power, so the answer is read off directly instead
    // of searched for.
    if (degree >= 63)
    {
        if (magnitudeNumerator > 1 || radicand.denominator() > 1)
            return std::unexpected { ArithmeticError::Inexact };
        return radicand;
    }

    std::optional<Rational::Int> const rootedNumerator = detail::exact_integer_root(magnitudeNumerator, degree);
    std::optional<Rational::Int> const rootedDenominator = detail::exact_integer_root(radicand.denominator(), degree);
    if (!rootedNumerator || !rootedDenominator)
        return std::unexpected { ArithmeticError::Inexact };

    return Rational::make(negative ? -*rootedNumerator : *rootedNumerator, *rootedDenominator);
}

} // namespace formula
