// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Units: a dimension, an exact conversion to the coherent SI unit, a display
/// symbol, a declared decimal precision and optional validity bounds.
///
/// Every type here is *structural*, so a unit can be a non-type template
/// parameter -- a quantity's declaration names its unit as a template argument.
/// That rules out `std::string_view` for the symbol and `Rational` for the
/// magnitude, both of which keep private members. Storage is therefore plain
/// public fields; the convenient types appear at the point of use.

#include <formula-cpp/dimension.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <string_view>

namespace formula
{

/// Bytes available for a unit symbol, including the terminator. Enough for the
/// UTF-8 spellings that occur in practice: `m3`, `°C` (3 bytes), `µm` (3). A
/// symbol that does not fit is a compile error (see `symbol()`), never a
/// silent truncation; bump this deliberately if a real symbol ever needs more.
inline constexpr std::size_t SymbolCapacity = 16;

/// A fixed-capacity symbol. An array of a structural type is structural, which a
/// `std::string_view` is not -- and unlike a `FixedString<N>` template this keeps
/// `Unit` a single non-template type, so every unit has the same type.
struct Symbol
{
    /// The symbol's UTF-8 bytes, zero-terminated as produced by `symbol()`;
    /// read with `view()`, which does not assume that and scans instead of
    /// trusting a terminator -- `Symbol` is a public aggregate, so a caller
    /// can fill `characters` directly and leave no room for one.
    char characters[SymbolCapacity] {};

    /// Memberwise equality -- the full `SymbolCapacity` bytes, terminator
    /// included when the value is one `symbol()` produced.
    [[nodiscard]] constexpr bool operator==(Symbol const&) const noexcept = default;
};

namespace detail
{
    /// Deliberately NOT `constexpr`, for the same reason as
    /// `formula_exponent_out_of_range` in dimension.hpp: `symbol()` runs in
    /// exactly the same context -- a constant expression building a constant
    /// that determines a `Unit`'s type -- and has exactly the same consequence
    /// when it goes wrong. Truncating instead of refusing would let two
    /// distinct symbols collapse into the same `Symbol` object and therefore
    /// the same NTTP type, and could split a multi-byte UTF-8 character in
    /// half. Calling this makes the enclosing expression a non-constant one,
    /// so the mistake is a compile error at the point of use. Defined, not
    /// merely declared, because a runtime call must still link; reaching it at
    /// runtime is a programming error with no recovery.
    [[noreturn]] inline void formula_unit_symbol_too_long()
    {
        std::abort();
    }
} // namespace detail

/// Builds a Symbol from a byte string. Refuses -- see
/// `formula_unit_symbol_too_long` -- rather than truncating when the text does
/// not fit in `SymbolCapacity` bytes including the terminator; every symbol
/// shipped by this library is well within the limit.
[[nodiscard]] constexpr Symbol symbol(char const* text) noexcept
{
    Symbol result {};
    std::size_t index = 0;
    while (text[index] != '\0')
    {
        if (index + 1 >= SymbolCapacity)
            detail::formula_unit_symbol_too_long();
        result.characters[index] = text[index];
        ++index;
    }
    return result;
}

/// Reads a Symbol back as a view. The storage has to be structural; this does not.
///
/// The scan is bounded by `SymbolCapacity` rather than left to the terminator,
/// and that is not belt-and-braces. `Symbol` is a public aggregate -- it has to
/// be, or `Unit` is not structural and cannot be a template argument -- so a
/// caller can fill `characters` directly, and exactly `SymbolCapacity` bytes of
/// text is a legal initialiser that leaves no room for a terminator. Handing
/// that to `std::string_view { value.characters }` reads until it happens to
/// find a zero somewhere after the array. Measured on a `Symbol` followed by
/// seven bytes of padding: 23 characters returned from a 16-byte array, the
/// neighbours included. A symbol built by `symbol()` is always terminated, but
/// this function cannot assume its argument came from there.
[[nodiscard]] constexpr std::string_view view(Symbol const& value) noexcept
{
    std::size_t length = 0;
    while (length < SymbolCapacity && value.characters[length] != '\0')
        ++length;
    return std::string_view { value.characters, length };
}

/// Deleted: binding a temporary here would return a view into a `Symbol` that
/// is already destroyed by the time the caller reads through it -- e.g.
/// `view(symbol("mm"))`. Measured silent on cl /W4, clang-cl /W4 and
/// `clang++ -Wall -Wextra -Wdangling`. Bind the `Symbol` to a named local
/// first, then call `view()` on that.
std::string_view view(Symbol&&) = delete;

/// Optional validity range, in the unit's own scale, as exact rationals.
struct Bounds
{
    /// Whether a range was declared at all -- `false` for a unit with no bounds.
    bool present = false;
    /// Numerator of the declared minimum.
    std::int64_t lowNumerator = 0;
    /// Denominator of the declared minimum.
    std::int64_t lowDenominator = 1;
    /// Numerator of the declared maximum.
    std::int64_t highNumerator = 0;
    /// Denominator of the declared maximum.
    std::int64_t highDenominator = 1;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(Bounds const&) const noexcept = default;
};

/// Builds a declared `Bounds` from the low and high range, each as a numerator/denominator pair.
[[nodiscard]] constexpr Bounds bounds(std::int64_t lowNumerator,
                                      std::int64_t lowDenominator,
                                      std::int64_t highNumerator,
                                      std::int64_t highDenominator) noexcept
{
    return { true, lowNumerator, lowDenominator, highNumerator, highDenominator };
}

/// A unit of measurement.
///
/// The conversion to the coherent SI unit is affine and exact:
///
///     value_in_SI = value * (magnitudeNumerator / magnitudeDenominator)
///                         + (offsetNumerator / offsetDenominator)
///
/// stated as integer pairs so the whole descriptor stays structural, and applied
/// by multiply-then-divide so that 30 MPa is exactly 30000000 Pa and converts
/// back to exactly 30.
struct Unit
{
    /// What this unit measures.
    Dimension dimension {};
    /// Numerator of the multiplicative factor to the coherent SI unit.
    std::int64_t magnitudeNumerator = 1;
    /// Denominator of the multiplicative factor to the coherent SI unit.
    std::int64_t magnitudeDenominator = 1;
    /// Numerator of the additive offset to the coherent SI unit.
    std::int64_t offsetNumerator = 0;
    /// Denominator of the additive offset to the coherent SI unit.
    std::int64_t offsetDenominator = 1;
    /// How the unit is written: `mm`, `°C`, and so on.
    Symbol symbolText {};
    /// The declared display precision -- see `declared_decimals`.
    std::int32_t decimals = 3;
    /// The declared validity range, if any -- see `checked_within_bounds`.
    Bounds bounds {};

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(Unit const&) const noexcept = default;
};

/// Named units. The `decimals` values are ordinary engineering defaults, not
/// requirements from any standard; a caller that needs a different precision
/// states it at the point of use.
namespace unit
{
    /// The coherent, dimensionless unit -- a bare number.
    inline constexpr Unit One { .dimension = dim::Scalar, .symbolText = symbol(""), .decimals = 3 };
    /// One part in a hundred.
    inline constexpr Unit Percent { .dimension = dim::Scalar,
                                    .magnitudeNumerator = 1,
                                    .magnitudeDenominator = 100,
                                    .symbolText = symbol("%"),
                                    .decimals = 1 };

    /// The coherent SI unit of length.
    inline constexpr Unit Metre { .dimension = dim::Length, .symbolText = symbol("m"), .decimals = 3 };
    /// One hundredth of a metre.
    inline constexpr Unit Centimetre { .dimension = dim::Length,
                                       .magnitudeNumerator = 1,
                                       .magnitudeDenominator = 100,
                                       .symbolText = symbol("cm"),
                                       .decimals = 1 };
    /// One thousandth of a metre.
    inline constexpr Unit Millimetre { .dimension = dim::Length,
                                       .magnitudeNumerator = 1,
                                       .magnitudeDenominator = 1000,
                                       .symbolText = symbol("mm"),
                                       .decimals = 1 };
    /// One thousand metres.
    inline constexpr Unit Kilometre { .dimension = dim::Length,
                                      .magnitudeNumerator = 1000,
                                      .symbolText = symbol("km"),
                                      .decimals = 3 };

    /// The coherent SI unit of area.
    inline constexpr Unit SquareMetre { .dimension = dim::Area, .symbolText = symbol("m2"), .decimals = 4 };
    /// The coherent SI unit of volume.
    inline constexpr Unit CubicMetre { .dimension = dim::Volume, .symbolText = symbol("m3"), .decimals = 4 };
    /// One thousandth of a cubic metre.
    inline constexpr Unit Litre { .dimension = dim::Volume,
                                  .magnitudeNumerator = 1,
                                  .magnitudeDenominator = 1000,
                                  .symbolText = symbol("l"),
                                  .decimals = 1 };
    /// One thousandth of a litre.
    inline constexpr Unit Millilitre { .dimension = dim::Volume,
                                       .magnitudeNumerator = 1,
                                       .magnitudeDenominator = 1000000,
                                       .symbolText = symbol("ml"),
                                       .decimals = 1 };

    /// The coherent SI unit of mass.
    inline constexpr Unit Kilogram { .dimension = dim::Mass, .symbolText = symbol("kg"), .decimals = 3 };
    /// One thousandth of a kilogram.
    inline constexpr Unit Gram { .dimension = dim::Mass,
                                 .magnitudeNumerator = 1,
                                 .magnitudeDenominator = 1000,
                                 .symbolText = symbol("g"),
                                 .decimals = 1 };
    /// One thousand kilograms.
    inline constexpr Unit Tonne { .dimension = dim::Mass,
                                  .magnitudeNumerator = 1000,
                                  .symbolText = symbol("t"),
                                  .decimals = 3 };

    /// The coherent SI unit of time.
    inline constexpr Unit Second { .dimension = dim::Time, .symbolText = symbol("s"), .decimals = 2 };
    /// Sixty seconds.
    inline constexpr Unit Minute { .dimension = dim::Time,
                                   .magnitudeNumerator = 60,
                                   .symbolText = symbol("min"),
                                   .decimals = 2 };
    /// Sixty minutes.
    inline constexpr Unit Hour { .dimension = dim::Time,
                                 .magnitudeNumerator = 3600,
                                 .symbolText = symbol("h"),
                                 .decimals = 2 };

    /// The coherent SI unit of thermodynamic temperature.
    inline constexpr Unit Kelvin { .dimension = dim::Temperature, .symbolText = symbol("K"), .decimals = 2 };
    /// The affine unit, and the reason `Unit` carries an offset at all.
    inline constexpr Unit Celsius { .dimension = dim::Temperature,
                                    .offsetNumerator = 27315,
                                    .offsetDenominator = 100,
                                    .symbolText = symbol("\xc2\xb0" "C"),
                                    .decimals = 1 };

    /// The coherent SI unit of pressure.
    inline constexpr Unit Pascal { .dimension = dim::Pressure, .symbolText = symbol("Pa"), .decimals = 0 };
    /// One million pascals.
    inline constexpr Unit Megapascal { .dimension = dim::Pressure,
                                       .magnitudeNumerator = 1000000,
                                       .symbolText = symbol("MPa"),
                                       .decimals = 1 };
} // namespace unit

/// Fails to compile when two units measure different dimensions.
///
/// Same shape and same reason as `RequireSameDimension`: instantiating a named
/// template on the values makes the compiler print the offending dimensions,
/// and the wording is ours so the negative-compile harness can assert the
/// reason rather than merely the failure.
///
/// **It fires only when the type is completed** -- the identical hazard
/// `RequireSameDimension` documents at length in dimension.hpp, with the
/// measured five-form table: a bare alias or a function parameter of this
/// type compiles silently even when `From` and `To` differ, because naming
/// the specialisation is not instantiating it. Only `::value`, `sizeof(...)`,
/// or a variable of this type forces completion and runs the `static_assert`.
/// See that note rather than this one repeating it.
template <Unit From, Unit To>
struct RequireSameUnitDimension
{
    // "in this diagnostic", not "above": clang puts the units inside this very
    // error line, in its `due to requirement` clause, and again in a note
    // below; cl puts them only in a note below. Same measurement as
    // RequireSameDimension in dimension.hpp. Nothing prints them above the
    // message, so do not send the reader to look there.
    static_assert(From.dimension == To.dimension,
                  "formula: these two units measure different dimensions, so no conversion between "
                  "them exists; the offending units appear in this diagnostic as the template "
                  "arguments of RequireSameUnitDimension");

    /// Always `true` once reached -- the `static_assert` above already failed
    /// compilation otherwise. Present so `::value` is the spelling that instantiates
    /// the class template; see the class comment for why that spelling matters.
    static constexpr bool value = true;
};

/// Converts @p value from @p from into @p to, exactly.
///
/// Applies integer factors by multiply-then-divide rather than a precomputed
/// floating-point factor, so 30 MPa is exactly 30000000 Pa and converts back to
/// exactly 30. The offset makes the conversion affine, which is what degrees
/// Celsius need; for units without one it is zero and drops out.
///
/// Converts a POINT on the scale, not a difference: 1 degC becomes 274,15 K, not
/// 1 K. A difference-preserving conversion is a different operation and is not
/// this one.
///
/// @return the converted value, or an error if the dimensions differ, either
///         unit's magnitude is zero, or an intermediate is not representable.
///         Never a wrong number.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_convert(Rational value,
                                                                                 Unit from,
                                                                                 Unit to) noexcept
{
    if (!(from.dimension == to.dimension))
        return std::unexpected { ArithmeticError::DomainError };

    // A magnitude of zero describes no scale: it is a malformed unit, not a
    // value to convert, and `checked_within_bounds` refuses the analogous
    // malformed-descriptor case (an inverted range) for the same reason. Left
    // unguarded, a zero numerator turns any value into 0/1 and reports
    // success -- the exact wrong-number-dressed-as-a-right-one this function's
    // own doc comment promises never to produce. `Unit` is a public aggregate,
    // so this cannot rely on every instance having come through a factory.
    if (from.magnitudeNumerator == 0 || to.magnitudeNumerator == 0)
        return std::unexpected { ArithmeticError::DomainError };

    std::expected<Rational, ArithmeticError> const fromMagnitude =
        Rational::make(from.magnitudeNumerator, from.magnitudeDenominator);
    std::expected<Rational, ArithmeticError> const fromOffset =
        Rational::make(from.offsetNumerator, from.offsetDenominator);
    std::expected<Rational, ArithmeticError> const toMagnitude =
        Rational::make(to.magnitudeNumerator, to.magnitudeDenominator);
    std::expected<Rational, ArithmeticError> const toOffset =
        Rational::make(to.offsetNumerator, to.offsetDenominator);
    if (!fromMagnitude)
        return fromMagnitude;
    if (!fromOffset)
        return fromOffset;
    if (!toMagnitude)
        return toMagnitude;
    if (!toOffset)
        return toOffset;

    std::expected<Rational, ArithmeticError> const scaled = checked_mul(value, *fromMagnitude);
    if (!scaled)
        return scaled;
    std::expected<Rational, ArithmeticError> const inSi = checked_add(*scaled, *fromOffset);
    if (!inSi)
        return inSi;
    std::expected<Rational, ArithmeticError> const shifted = checked_sub(*inSi, *toOffset);
    if (!shifted)
        return shifted;
    return checked_div(*shifted, *toMagnitude);
}

/// @throws ArithmeticException when the conversion cannot be represented.
[[nodiscard]] constexpr Rational convert(Rational value, Unit from, Unit to)
{
    return detail::or_throw(checked_convert(value, from, to));
}

/// The outcome of checking a value against its unit's declared bounds.
enum class BoundsCheck : std::uint8_t
{
    /// Bounds were declared and the value lies within them, inclusive.
    WithinBounds,
    /// Below the declared minimum.
    BelowMinimum,
    /// Above the declared maximum.
    AboveMaximum,
    /// The unit declares no bounds, so nothing was checked. Deliberately NOT
    /// the same as WithinBounds: a value that was never checked must not be
    /// reported as one that was checked and passed.
    NotChecked,
    /// There was no value to check. Distinct from NotChecked, which says the
    /// unit declares no range: a measurement nobody took and a range nobody
    /// declared are different facts, and a report that shows them as one is the
    /// collapse NotChecked exists to prevent.
    NotMeasured,
};

/// `outcome` in prose, for a trace or an error message.
[[nodiscard]] constexpr std::string_view describe(BoundsCheck outcome) noexcept
{
    switch (outcome)
    {
        case BoundsCheck::WithinBounds: return "within the declared bounds";
        case BoundsCheck::BelowMinimum: return "below the declared minimum";
        case BoundsCheck::AboveMaximum: return "above the declared maximum";
        case BoundsCheck::NotChecked: return "no bounds declared for this unit";
        case BoundsCheck::NotMeasured: return "no value was measured";
    }
    return "unknown bounds outcome";
}

/// Checks @p value, expressed in @p unitOfValue, against that unit's bounds.
[[nodiscard]] constexpr std::expected<BoundsCheck, ArithmeticError> checked_within_bounds(
    Rational value, Unit unitOfValue) noexcept
{
    if (!unitOfValue.bounds.present)
        return BoundsCheck::NotChecked;

    std::expected<Rational, ArithmeticError> const low =
        Rational::make(unitOfValue.bounds.lowNumerator, unitOfValue.bounds.lowDenominator);
    std::expected<Rational, ArithmeticError> const high =
        Rational::make(unitOfValue.bounds.highNumerator, unitOfValue.bounds.highDenominator);
    if (!low)
        return std::unexpected { low.error() };
    if (!high)
        return std::unexpected { high.error() };

    // A unit whose declared minimum exceeds its maximum is a malformed unit,
    // not a value to be judged. Reporting BelowMinimum or AboveMaximum here
    // would be a wrong answer dressed up as a real one; refuse instead.
    if (*low > *high)
        return std::unexpected { ArithmeticError::DomainError };

    if (value < *low)
        return BoundsCheck::BelowMinimum;
    if (value > *high)
        return BoundsCheck::AboveMaximum;
    return BoundsCheck::WithinBounds;
}

/// The unit's declared display precision, as the rounding layer's own type.
[[nodiscard]] constexpr DecimalPlaces declared_decimals(Unit unitOfValue) noexcept
{
    return DecimalPlaces { unitOfValue.decimals };
}

/// Rounds @p value to the precision its unit declares.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round_to_declared(
    Rational value, Unit unitOfValue, RoundingMode mode) noexcept
{
    return checked_round(value, declared_decimals(unitOfValue), mode);
}

/// @throws ArithmeticException when the checked form would report an error.
[[nodiscard]] constexpr Rational round_to_declared(Rational value, Unit unitOfValue, RoundingMode mode)
{
    return detail::or_throw(checked_round_to_declared(value, unitOfValue, mode));
}

} // namespace formula
