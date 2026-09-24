// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Dimensional analysis: a structural exponent vector over the seven SI base
/// dimensions, usable as a non-type template parameter so that a dimension is
/// part of a type rather than a runtime tag.

#include <cstdint>
#include <cstdlib>
#include <limits>

namespace formula
{

/// A rational exponent on one base dimension.
///
/// Deliberately a separate type from `Rational`, and deliberately all-public. A
/// class used as a non-type template parameter must be *structural*: every
/// non-static data member public, recursively. `Rational` keeps its members
/// private -- which is exactly what lets its `operator==` compare componentwise,
/// since no code path can build a non-canonical value -- so it cannot be used
/// here. This type pays for its public members by canonicalising in its factory.
///
/// Always in lowest terms with a positive denominator. Build one with
/// `exponent(...)`. Aggregate initialisation bypasses that and is a mistake:
/// `Exponent { 2, 4 }` and `Exponent { 1, 2 }` are the same number but
/// different objects, and as template arguments they name different types.
struct Exponent
{
    /// The numerator, in lowest terms.
    std::int32_t numerator = 0;
    /// The denominator, in lowest terms and always positive.
    std::int32_t denominator = 1;

    /// Memberwise equality -- meaningful because both fields are always canonical.
    [[nodiscard]] constexpr bool operator==(Exponent const&) const noexcept = default;
};

namespace detail
{
    /// Deliberately NOT `constexpr`. Calling it makes the enclosing expression a
    /// non-constant one, so an invalid exponent is a compile error at the point of
    /// use rather than a silently wrong value -- and the diagnostic names this
    /// function, which is why the name is a sentence. Dimensions are built in
    /// constant expressions, so this is where the error belongs. It is defined,
    /// not merely declared, because a runtime call must still link; reaching it at
    /// runtime is a programming error with no recovery.
    [[noreturn]] inline void formula_exponent_denominator_must_not_be_zero()
    {
        std::abort();
    }

    /// Same mechanism, for an exponent whose reduced form does not fit 32 bits.
    [[noreturn]] inline void formula_exponent_out_of_range()
    {
        std::abort();
    }

    [[nodiscard]] constexpr std::int64_t exponent_gcd(std::int64_t lhs, std::int64_t rhs) noexcept
    {
        if (lhs < 0)
            lhs = -lhs;
        if (rhs < 0)
            rhs = -rhs;
        while (rhs != 0)
        {
            std::int64_t const remainder = lhs % rhs;
            lhs = rhs;
            rhs = remainder;
        }
        return lhs == 0 ? 1 : lhs;
    }

    /// Reduces to lowest terms with a positive denominator, then narrows back to
    /// the stored width. Intermediates are 64-bit, which is wide enough that no
    /// combination of two stored exponents can overflow on the way. The widest
    /// product is 2^31 * (2^31 - 1), not (2^31 - 1) squared: a stored numerator
    /// can be `INT32_MIN`, whose magnitude is one MORE than `INT32_MAX`. The
    /// widest expression, the numerator of `operator+`, is twice that product:
    ///
    ///     9223372032559808512  worst case
    ///     9223372036854775807  INT64_MAX
    ///     ---------------------------------
    ///            4294967295    headroom, exactly 2^32 - 1
    ///
    /// and that worst case is reachable rather than hypothetical, since
    /// `exponent(INT32_MIN, INT32_MAX)` is already canonical -- 2^31 and 2^31 - 1
    /// are coprime, the latter being prime. Changing the stored width or these
    /// formulas means redoing this arithmetic.
    ///
    /// The two outcomes that would break the type are rejected here rather than
    /// stored. A zero denominator names no number, and would give `exponent(0, 0)`
    /// a different representation from `exponent(0, 1)`: two distinct types for
    /// one physical dimension, the exact failure this type exists to prevent. A
    /// reduced result outside `std::int32_t` is the other. Neither is caught by
    /// the narrowing itself -- a `static_cast` from a wider integer type is
    /// well-defined modular arithmetic and therefore perfectly legal in a constant
    /// expression, which is precisely why it would wrap in silence. Rejecting
    /// means calling a non-`constexpr` sentinel, making each one a compile error
    /// wherever a dimension is built, that being a constant expression by
    /// construction. Every `Exponent` in the library is made here, so this one
    /// place covers `exponent()` and every operator.
    [[nodiscard]] constexpr Exponent reduced(std::int64_t numerator, std::int64_t denominator) noexcept
    {
        if (denominator == 0)
            formula_exponent_denominator_must_not_be_zero();
        if (denominator < 0)
        {
            numerator = -numerator;
            denominator = -denominator;
        }
        std::int64_t const common = exponent_gcd(numerator, denominator);
        numerator /= common;
        denominator /= common;
        // On the 64-bit values, before the narrowing that would hide it. The
        // denominator needs no lower bound: it is positive by this point.
        if (numerator < std::numeric_limits<std::int32_t>::min() || numerator > std::numeric_limits<std::int32_t>::max()
            || denominator > std::numeric_limits<std::int32_t>::max())
            formula_exponent_out_of_range();
        return { static_cast<std::int32_t>(numerator), static_cast<std::int32_t>(denominator) };
    }
} // namespace detail

/// The only sanctioned way to make an `Exponent`: canonicalises.
[[nodiscard]] constexpr Exponent exponent(std::int32_t numerator, std::int32_t denominator = 1) noexcept
{
    return detail::reduced(numerator, denominator);
}

/// Adding exponents is what multiplying the dimensions they belong to does.
[[nodiscard]] constexpr Exponent operator+(Exponent lhs, Exponent rhs) noexcept
{
    return detail::reduced(static_cast<std::int64_t>(lhs.numerator) * rhs.denominator
                               + static_cast<std::int64_t>(rhs.numerator) * lhs.denominator,
                           static_cast<std::int64_t>(lhs.denominator) * rhs.denominator);
}

/// Subtracting exponents is what dividing the dimensions they belong to does.
[[nodiscard]] constexpr Exponent operator-(Exponent lhs, Exponent rhs) noexcept
{
    return detail::reduced(static_cast<std::int64_t>(lhs.numerator) * rhs.denominator
                               - static_cast<std::int64_t>(rhs.numerator) * lhs.denominator,
                           static_cast<std::int64_t>(lhs.denominator) * rhs.denominator);
}

/// Negation.
[[nodiscard]] constexpr Exponent operator-(Exponent value) noexcept
{
    return detail::reduced(-static_cast<std::int64_t>(value.numerator), value.denominator);
}

/// Raising a dimension to an integer power scales its exponents.
[[nodiscard]] constexpr Exponent operator*(Exponent value, std::int32_t factor) noexcept
{
    return detail::reduced(static_cast<std::int64_t>(value.numerator) * factor, value.denominator);
}

/// Taking an nth root divides them -- the operation integer exponents cannot express.
[[nodiscard]] constexpr Exponent operator/(Exponent value, std::int32_t divisor) noexcept
{
    return detail::reduced(value.numerator, static_cast<std::int64_t>(value.denominator) * divisor);
}

/// True for the exponent of a dimension a quantity does not depend on at all.
[[nodiscard]] constexpr bool is_zero(Exponent value) noexcept
{
    return value.numerator == 0;
}

/// True when the exponent is a whole number, not a genuine fraction such as one half.
[[nodiscard]] constexpr bool is_integer(Exponent value) noexcept
{
    return value.denominator == 1;
}

/// An exponent vector over the seven SI base dimensions.
///
/// Structural, so it can be a non-type template parameter -- which is the point:
/// a dimension belongs to a *type*, checked when the program is compiled, not to
/// a value checked when it runs. Verified on MSVC, clang-cl and clang++,
/// including that two translation units agree on the mangling.
struct Dimension
{
    /// Exponent on length (SI base unit: metre).
    Exponent length {};
    /// Exponent on mass (SI base unit: kilogram).
    Exponent mass {};
    /// Exponent on time (SI base unit: second).
    Exponent time {};
    /// Exponent on electric current (SI base unit: ampere).
    Exponent current {};
    /// Exponent on thermodynamic temperature (SI base unit: kelvin).
    Exponent temperature {};
    /// Exponent on amount of substance (SI base unit: mole).
    Exponent amount {};
    /// Exponent on luminous intensity (SI base unit: candela).
    Exponent luminosity {};

    /// Memberwise equality across all seven exponents.
    [[nodiscard]] constexpr bool operator==(Dimension const&) const noexcept = default;
};

/// Multiplying quantities adds their dimensions' exponents.
[[nodiscard]] constexpr Dimension operator*(Dimension lhs, Dimension rhs) noexcept
{
    return { lhs.length + rhs.length,
             lhs.mass + rhs.mass,
             lhs.time + rhs.time,
             lhs.current + rhs.current,
             lhs.temperature + rhs.temperature,
             lhs.amount + rhs.amount,
             lhs.luminosity + rhs.luminosity };
}

/// Dividing subtracts them.
[[nodiscard]] constexpr Dimension operator/(Dimension lhs, Dimension rhs) noexcept
{
    return { lhs.length - rhs.length,
             lhs.mass - rhs.mass,
             lhs.time - rhs.time,
             lhs.current - rhs.current,
             lhs.temperature - rhs.temperature,
             lhs.amount - rhs.amount,
             lhs.luminosity - rhs.luminosity };
}

/// Raising a quantity to an integer power scales every exponent of its dimension.
[[nodiscard]] constexpr Dimension power(Dimension value, std::int32_t exponentOfPower) noexcept
{
    return { value.length * exponentOfPower,      value.mass * exponentOfPower,
             value.time * exponentOfPower,        value.current * exponentOfPower,
             value.temperature * exponentOfPower, value.amount * exponentOfPower,
             value.luminosity * exponentOfPower };
}

/// The nth root. Integer exponents cannot express the result at all -- the
/// square root of an area is a length, but the square root of a length is
/// length to the one half, and norm formulas do take such roots.
[[nodiscard]] constexpr Dimension nth_root(Dimension value, std::int32_t degree) noexcept
{
    return { value.length / degree,      value.mass / degree,   value.time / degree, value.current / degree,
             value.temperature / degree, value.amount / degree, value.luminosity / degree };
}

/// True for a quantity with no dependence on any base dimension -- a pure ratio.
[[nodiscard]] constexpr bool is_dimensionless(Dimension value) noexcept
{
    return value == Dimension {};
}

/// Named dimensions. Users compose these rather than spelling exponents, which
/// keeps the representation swappable.
namespace dim
{
    /// Dimensionless -- every exponent zero.
    inline constexpr Dimension Scalar {};
    /// The base dimension of length.
    inline constexpr Dimension Length { .length = exponent(1) };
    /// The base dimension of mass.
    inline constexpr Dimension Mass { .mass = exponent(1) };
    /// The base dimension of time.
    inline constexpr Dimension Time { .time = exponent(1) };
    /// The base dimension of electric current.
    inline constexpr Dimension Current { .current = exponent(1) };
    /// The base dimension of thermodynamic temperature.
    inline constexpr Dimension Temperature { .temperature = exponent(1) };
    /// The base dimension of amount of substance.
    inline constexpr Dimension Amount { .amount = exponent(1) };
    /// The base dimension of luminous intensity.
    inline constexpr Dimension Luminosity { .luminosity = exponent(1) };

    /// Length squared.
    inline constexpr Dimension Area = Length * Length;
    /// Length cubed.
    inline constexpr Dimension Volume = Area * Length;
    /// Mass per volume.
    inline constexpr Dimension Density = Mass / Volume;
    /// Length per time.
    inline constexpr Dimension Velocity = Length / Time;
    /// Velocity per time.
    inline constexpr Dimension Acceleration = Velocity / Time;
    /// Mass times acceleration.
    inline constexpr Dimension Force = Mass * Acceleration;
    /// Force per area.
    inline constexpr Dimension Pressure = Force / Area;
    /// Force times length.
    inline constexpr Dimension Energy = Force * Length;
    /// The reciprocal of time.
    inline constexpr Dimension Frequency = Scalar / Time;
} // namespace dim

/// True when two dimensions are identical.
template <Dimension Left, Dimension Right>
inline constexpr bool SameDimension = (Left == Right);

/// Fails to compile, loudly and legibly, when two dimensions differ.
///
/// The indirection through a named template is deliberate and was measured: an
/// inline `static_assert(Left == Right, ...)` at the point of use prints only
/// the operand type names, while instantiating a template *on the values* makes
/// every supported compiler print the exponent vectors themselves --
///
///     RequireSameDimension<Dimension{...length 3...}, Dimension{...mass 1...}>
///
/// -- so the reader sees volume against mass rather than two opaque template
/// ids. The wording below is ours, which is what allows the negative-compile
/// test harness to assert why a compile failed rather than only that it did.
///
/// **It fires only when the type is completed.** The assertion lives in the class
/// body, so it runs when the class template is instantiated -- and naming the
/// specialisation is not instantiating it. Measured, one form per translation
/// unit, on cl and clang-cl:
///
///     using Checked = RequireSameDimension<dim::Volume, dim::Mass>;  // SILENT
///     void f(RequireSameDimension<dim::Volume, dim::Mass>);          // SILENT
///     RequireSameDimension<dim::Volume, dim::Mass>::value            // fires
///     sizeof(RequireSameDimension<dim::Volume, dim::Mass>)           // fires
///     RequireSameDimension<dim::Volume, dim::Mass> checked {};       // fires
///
/// The first two compile clean with mismatched dimensions. Write `::value`:
///
///     static_assert(RequireSameDimension<Left, Right>::value);
///
/// An alias that is never touched is a guard that never guards, and it looks
/// exactly like one that does. Where a plain bool is wanted, use
/// `SameDimension<Left, Right>` -- a variable template, so always evaluated; it
/// simply cannot print the vectors, which is what this one is for.
template <Dimension Left, Dimension Right>
struct RequireSameDimension
{
    // "in this diagnostic", not "above": clang puts the vectors inside this very
    // error line, in its `due to requirement` clause, and again in a note below;
    // cl puts them only in a note below. Measured on all three. Nothing prints
    // them above the message, so do not send the reader to look there.
    static_assert(Left == Right,
                  "formula: these two dimensions are not the same; the offending exponent vectors "
                  "appear in this diagnostic as the template arguments of RequireSameDimension, in "
                  "the order length, mass, time, current, temperature, amount, luminosity");

    /// Always `true` once reached -- the `static_assert` above already failed
    /// compilation otherwise. Present so `::value` is the spelling that instantiates
    /// the class template; see the class comment for why that spelling matters.
    static constexpr bool value = true;
};

} // namespace formula
