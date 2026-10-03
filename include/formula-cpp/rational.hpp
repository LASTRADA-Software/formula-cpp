// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// An exact rational number over `formula::Int128`.
///
/// Norm rounding rules are specified behaviour, not presentation: "round the
/// result to 0.1 %" is part of the method. Binary floating point cannot express
/// that faithfully, and it makes exact round-tripping impossible -- 450 l stored
/// as 0.45 m3 and rendered back is not reliably 450.
///
/// This type carries no decimal-place tag. Declared precision belongs to the
/// unit and quantity layer; rounding is an explicit operation (rounding.hpp) and,
/// from spec phase 8 on, a node in the expression tree.

#include <formula-cpp/detail/checked_int.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/int128.hpp>

#include <bit>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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
    /// The signed integer numerator and denominator are stored in: 128 bits
    /// (`int128.hpp`).
    using Int = Int128;

    /// Zero.
    constexpr Rational() noexcept = default;

    /// An integer is a rational, exactly and without narrowing.
    ///
    /// Every built-in integer type of up to 64 bits fits `Int` exactly.
    template <typename T>
        requires std::is_integral_v<T> && (!std::is_same_v<std::remove_cv_t<T>, bool>) && (sizeof(T) <= 8)
    constexpr Rational(T whole) noexcept:
        _numerator { whole }
    {
    }

    /// An `Int`, exactly.
    constexpr Rational(Int whole) noexcept:
        _numerator { whole }
    {
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
        FORMULA_CENSUS_NOTE(Numerator, detail::magnitude(dividend));
        FORMULA_CENSUS_NOTE(Denominator, detail::magnitude(divisor));
        if (dividend == 0)
            return Rational {};

        // Reduce in the unsigned domain so that the minimum is an ordinary operand.
        detail::UInt128 const numeratorMagnitude = detail::magnitude(dividend);
        detail::UInt128 const denominatorMagnitude = detail::magnitude(divisor);
        detail::UInt128 const common = detail::gcd(numeratorMagnitude, denominatorMagnitude);
        detail::UInt128 const reducedNumerator = detail::u128_divmod(numeratorMagnitude, common).quotient;
        detail::UInt128 const reducedDenominator = detail::u128_divmod(denominatorMagnitude, common).quotient;

        bool const negative = (dividend < 0) != (divisor < 0);

        constexpr detail::UInt128 PositiveLimit = detail::magnitude(std::numeric_limits<Int>::max());
        detail::UInt128 const numeratorLimit =
            negative ? detail::u128_add(PositiveLimit, detail::UInt128::from_u64(1)) : PositiveLimit;
        if (PositiveLimit < reducedDenominator || numeratorLimit < reducedNumerator)
            return std::unexpected { ArithmeticError::Overflow };

        Rational made {};
        made._numerator = detail::signed_from_magnitude(reducedNumerator, negative);
        made._denominator = detail::signed_from_magnitude(reducedDenominator, false);
        return made;
    }

    /// `mantissa * 10^exponent`, exactly. The preferred way to write a decimal:
    /// `from_decimal(45, -2)` is 9/20, not the nearest double to 0.45.
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
    /// what a norm means: 0.45 as a double is not 9/20. Prefer from_decimal, or
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

        Int scaledReduced { reduced };
        if (negative)
            scaledReduced = -scaledReduced;

        if (shifted >= 0)
        {
            if (shifted >= 127)
                return std::unexpected { ArithmeticError::Overflow };
            std::optional<Int> const scaledNumerator = detail::mul_checked_or_none(scaledReduced, Int { 1 } << shifted);
            if (!scaledNumerator)
                return std::unexpected { ArithmeticError::Overflow };
            return Rational { *scaledNumerator };
        }

        if (-shifted >= 127)
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
        return _numerator.to_double() / _denominator.to_double();
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

namespace detail
{
    /// The `Rational::Int` of magnitude @p magnitudeOf, negative when
    /// @p negative, or nothing when `Rational::Int` cannot hold it: one
    /// spelling for code that builds a numerator from a wide magnitude,
    /// whatever `Rational::Int`'s width.
    [[nodiscard]] constexpr std::optional<Rational::Int> rational_int_from_magnitude(UInt128 magnitudeOf,
                                                                                     bool negative) noexcept
    {
        UInt128 const largestPositive = wide_magnitude(std::numeric_limits<Rational::Int>::max());
        UInt128 const largestAllowed = negative ? u128_add(largestPositive, UInt128::from_u64(1)) : largestPositive;
        if (largestAllowed < magnitudeOf)
            return std::nullopt;
        UInt128 const wordPattern = negative ? u128_sub(UInt128 {}, magnitudeOf) : magnitudeOf;
        return int_from_pattern(wordPattern, std::type_identity<Rational::Int> {});
    }
} // namespace detail

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
    if (operandValue.numerator() == std::numeric_limits<Rational::Int>::min())
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
/// numerators near 2^127 over denominators sharing a large factor. The failure
/// direction is safe: a refusal, never a wrong number.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_add(Rational leftOperand,
                                                                             Rational rightOperand) noexcept
{
    // Scale by the least common multiple rather than by the product: with
    // denominators 6 and 10 this uses 30, not 60 -- the difference between
    // fitting and overflowing once denominators get large.
    Rational::Int const common = detail::signed_from_magnitude(
        detail::gcd(detail::magnitude(leftOperand.denominator()), detail::magnitude(rightOperand.denominator())), false);
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
    // Cross-reduce before multiplying: (max/3) * (3/max) is exactly 1, but
    // multiplying the numerators first would overflow.
    //
    // Each gcd divides a denominator, so it is positive and at most `Int`'s
    // maximum. Signed division by it is therefore always safe -- the only
    // signed division that can overflow is the minimum / -1.
    Rational::Int const leftCross = detail::signed_from_magnitude(
        detail::gcd(detail::magnitude(leftOperand.numerator()), detail::magnitude(rightOperand.denominator())), false);
    Rational::Int const rightCross = detail::signed_from_magnitude(
        detail::gcd(detail::magnitude(rightOperand.numerator()), detail::magnitude(leftOperand.denominator())), false);

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
    /// A `_r` literal whose exact value `Rational` cannot hold: too many
    /// significant digits, or a denominator above `Int`'s range. Deliberately
    /// not `constexpr`, like `formula_exponent_out_of_range`
    /// (`dimension.hpp`): the literal operator is `consteval`, so reaching
    /// this fails to compile and the diagnostic names it.
    [[noreturn]] inline void formula_rational_literal_out_of_range()
    {
        std::abort();
    }

    /// A `_r` literal spelled as something other than a decimal: a
    /// hexadecimal or binary integer, or an integer with a leading zero,
    /// which C++ reads as octal. Same mechanism as above.
    [[noreturn]] inline void formula_rational_literal_not_a_decimal()
    {
        std::abort();
    }

    /// The exact value of a decimal literal's spelling: digits, an optional
    /// fraction, an optional exponent, and digit separators.
    consteval Rational rational_from_spelling(char const* spelling)
    {
        std::size_t at = 0;
        // Also true for an exponent: that is how `0x1E` gets past the leading guard, to be refused at its `x`.
        bool const hasPoint = [&] {
            for (std::size_t probe = 0; spelling[probe] != '\0'; ++probe)
                if (spelling[probe] == '.' || spelling[probe] == 'e' || spelling[probe] == 'E')
                    return true;
            return false;
        }();
        if (spelling[0] == '0' && spelling[1] != '\0' && !hasPoint)
            formula_rational_literal_not_a_decimal(); // 017, 0x1F, 0b101
        Int mantissa = 0;
        int decimalScale = 0;  // decimal exponent the digits carry
        int pendingZeros = 0;  // fractional zeros not yet multiplied in
        bool inFraction = false;
        for (; spelling[at] != '\0' && spelling[at] != 'e' && spelling[at] != 'E'; ++at)
        {
            char const symbolAt = spelling[at];
            if (symbolAt == '\'')
                continue;
            if (symbolAt == '.')
            {
                inFraction = true;
                continue;
            }
            if (symbolAt < '0' || symbolAt > '9') // the only refusal of a hexadecimal spelling holding e or E, such as 0x1E
                formula_rational_literal_not_a_decimal();
            int const digitValue = symbolAt - '0';
            if (inFraction && digitValue == 0)
            {
                ++pendingZeros;
                continue;
            }
            for (int zero = 0; zero <= pendingZeros; ++zero) // the zeros, then this digit's place
            {
                int const placed = zero == pendingZeros ? digitValue : 0;
                if (mantissa > (IntMax - placed) / 10)
                    formula_rational_literal_out_of_range();
                mantissa = mantissa * 10 + placed;
            }
            if (inFraction)
                decimalScale -= pendingZeros + 1;
            pendingZeros = 0;
        }
        if (spelling[at] == 'e' || spelling[at] == 'E')
        {
            ++at;
            bool const negative = spelling[at] == '-';
            if (spelling[at] == '-' || spelling[at] == '+')
                ++at;
            int written = 0;
            for (; spelling[at] != '\0'; ++at)
            {
                if (spelling[at] == '\'')
                    continue;
                written = written * 10 + (spelling[at] - '0');
                if (written > 1'000)
                    formula_rational_literal_out_of_range();
            }
            decimalScale += negative ? -written : written;
        }
        std::expected<Rational, ArithmeticError> const made = Rational::from_decimal(mantissa, decimalScale);
        if (!made)
            formula_rational_literal_out_of_range();
        return *made;
    }
} // namespace detail

inline namespace literals
{
    /// An exact decimal: `27.3_r` is 273/10, never the `double` nearest it.
    /// An exponent scales exactly (`1.5e-3_r` is 3/2000); `-27.3_r` is
    /// `Rational`'s own negation. A spelling `Rational` cannot hold, or one
    /// that is not a decimal (`0x1F_r`, and `017_r`, which C++ reads as
    /// octal), fails to compile, naming `formula_rational_literal_out_of_range`
    /// or `formula_rational_literal_not_a_decimal`.
    consteval Rational operator""_r(char const* spelling)
    {
        return detail::rational_from_spelling(spelling);
    }
} // namespace literals

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

    // `Int`'s minimum, -2^127, has no positive counterpart `Int` can hold --
    // its magnitude is the maximum + 1 -- so negating it to reach a positive
    // intermediate breaks `Int`'s contract. checked_negate refuses the same
    // numerator for the same reason; this follows that precedent rather than
    // inventing a second rule for it. Overflow is the honest answer here, not
    // Inexact: Inexact means no exact root exists, but the minimum's 127th
    // root, -2, both exists and is representable -- it is only the magnitude
    // of the intermediate numerator that is not. Reworking the search onto an
    // unsigned magnitude to rescue this one input would add new numeric code
    // at the end of a phase to save a single edge case, which risks a worse
    // bug than the one it fixes.
    if (radicand.numerator() == std::numeric_limits<Rational::Int>::min())
        return std::unexpected { ArithmeticError::Overflow };

    bool const negative = radicand.sign() < 0;
    Rational::Int const magnitudeNumerator = negative ? -radicand.numerator() : radicand.numerator();

    // At degree 127 or higher, exact_integer_root's binary search is
    // pathological rather than merely slow: once it probes middle == 1, power
    // stays 1 for the rest of that probe's inner loop, so the loop runs the
    // full `degree` multiplications of 1 by 1 before concluding "too small" --
    // and degree is an ordinary int a caller controls, so nothing bounds how
    // long that takes. The search is also unnecessary at this degree: 2^127
    // alone exceeds the largest `Int`, so no numerator or denominator
    // magnitude of 2 or more could have an exact root here -- reaching it
    // would need at least 2^127, which `Int` cannot hold. That leaves only
    // magnitude 0 and 1, both fixed points of every power, so the answer is
    // read off directly instead of searched for.
    if (degree >= 127)
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
