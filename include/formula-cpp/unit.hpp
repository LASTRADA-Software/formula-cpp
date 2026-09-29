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
[[nodiscard]] constexpr Symbol symbol(char const* spelling) noexcept
{
    Symbol built {};
    std::size_t characterIndex = 0;
    while (spelling[characterIndex] != '\0')
    {
        if (characterIndex + 1 >= SymbolCapacity)
            detail::formula_unit_symbol_too_long();
        built.characters[characterIndex] = spelling[characterIndex];
        ++characterIndex;
    }
    return built;
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
[[nodiscard]] constexpr std::string_view view(Symbol const& unitSymbol) noexcept
{
    std::size_t symbolLength = 0;
    while (symbolLength < SymbolCapacity && unitSymbol.characters[symbolLength] != '\0')
        ++symbolLength;
    return std::string_view { unitSymbol.characters, symbolLength };
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
///
/// **Symbols are emitted verbatim in plain text and in Markdown.**
/// `render.hpp` appends `view(unit.symbolText)` to the number without escaping
/// it there; in LaTeX it escapes it and sets it inside `\mathrm{...}`. So the
/// symbol text has to be safe in Markdown by construction, and that rules out
/// two characters a unit might otherwise want:
///
///  - `*`, which is Markdown emphasis. Two symbols carrying one on a single
///    rendered line italicise everything between them, so a pascal second is
///    spelled `Pa.s` here rather than `Pa*s`.
///  - `[`, `]` and `(`, which `render_tests.cpp` already guards against for
///    the same reason -- they read as a Markdown link.
///
/// `/` is safe in all three and is used freely (`kg/m3`, `m/s`). Multi-byte
/// UTF-8 passes through byte-for-byte, as `Celsius` has always relied on, and
/// `Micrometre` and `PerMille` rely on it too. `Percent`'s `%` is a LaTeX
/// comment character, and safe all the same: `render.hpp` escapes every
/// unit symbol it writes into LaTeX (`detail::latex_math_words`), so a symbol
/// holding one of TeX's specials is shown rather than obeyed.
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
    /// One part in a thousand. One decimal, as `Percent` has: a per-mille
    /// figure is a content or a deviation, and is read at the same kind of
    /// resolution as the percentage it sits beside.
    inline constexpr Unit PerMille { .dimension = dim::Scalar,
                                     .magnitudeNumerator = 1,
                                     .magnitudeDenominator = 1000,
                                     .symbolText = symbol("\xe2\x80\xb0"),
                                     .decimals = 1 };
    /// One part in a million. No decimals: a figure in parts per million is
    /// already at the resolution the number carries, and a fraction of one part
    /// is not something a content determination distinguishes.
    inline constexpr Unit PartsPerMillion { .dimension = dim::Scalar,
                                            .magnitudeNumerator = 1,
                                            .magnitudeDenominator = 1000000,
                                            .symbolText = symbol("ppm"),
                                            .decimals = 0 };
    /// A mass fraction written as a mass per mass. Exactly `PartsPerMillion`'s
    /// magnitude -- a milligram in a kilogram *is* one part in a million -- and
    /// deliberately a second name for it rather than a second scale, because
    /// both spellings occur and a result must convert between them exactly.
    /// `unit_tests.cpp` pins that equality so the two cannot drift apart.
    inline constexpr Unit MilligramPerKilogram { .dimension = dim::Scalar,
                                                 .magnitudeNumerator = 1,
                                                 .magnitudeDenominator = 1000000,
                                                 .symbolText = symbol("mg/kg"),
                                                 .decimals = 0 };

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
    /// One tenth of a millimetre -- a unit in its own right, not a millimetre
    /// displayed to one decimal. A penetration is measured and reported as a
    /// whole number of these, which is why it is spelled as its own unit and
    /// why it declares no decimals at all.
    inline constexpr Unit Decimillimetre { .dimension = dim::Length,
                                           .magnitudeNumerator = 1,
                                           .magnitudeDenominator = 10000,
                                           .symbolText = symbol("dmm"),
                                           .decimals = 0 };
    /// One millionth of a metre. No decimals: a sieve aperture or a film
    /// thickness in micrometres is read as a whole number, and a tenth of a
    /// micrometre is below what such a measurement resolves.
    inline constexpr Unit Micrometre { .dimension = dim::Length,
                                       .magnitudeNumerator = 1,
                                       .magnitudeDenominator = 1000000,
                                       .symbolText = symbol("\xc2\xb5" "m"),
                                       .decimals = 0 };
    /// One thousand metres.
    inline constexpr Unit Kilometre { .dimension = dim::Length,
                                      .magnitudeNumerator = 1000,
                                      .symbolText = symbol("km"),
                                      .decimals = 3 };

    /// The coherent SI unit of area.
    inline constexpr Unit SquareMetre { .dimension = dim::Area, .symbolText = symbol("m2"), .decimals = 4 };
    /// One ten-thousandth of a square metre. Two decimals, because a hundredth
    /// of a square centimetre is exactly one square millimetre: a specimen
    /// cross-section then reads at the same resolution whichever of the two
    /// units it is stated in.
    inline constexpr Unit SquareCentimetre { .dimension = dim::Area,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 10000,
                                             .symbolText = symbol("cm2"),
                                             .decimals = 2 };
    /// One millionth of a square metre. No decimals: a cross-section in square
    /// millimetres runs to thousands, and a whole square millimetre is the step
    /// such an area is worked out and reported in.
    inline constexpr Unit SquareMillimetre { .dimension = dim::Area,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1000000,
                                             .symbolText = symbol("mm2"),
                                             .decimals = 0 };
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
    /// One millionth of a cubic metre -- exactly `Millilitre`'s magnitude, and
    /// the same one decimal, because a cubic centimetre and a millilitre are
    /// the same volume under two names. Both spellings occur, so both exist
    /// here; `unit_tests.cpp` pins that they convert to each other identically.
    inline constexpr Unit CubicCentimetre { .dimension = dim::Volume,
                                            .magnitudeNumerator = 1,
                                            .magnitudeDenominator = 1000000,
                                            .symbolText = symbol("cm3"),
                                            .decimals = 1 };
    /// One thousand-millionth of a cubic metre. No decimals: a volume stated in
    /// cubic millimetres -- a loss from a specimen, say -- runs to hundreds or
    /// thousands, and a whole cubic millimetre is the step it is reported in.
    inline constexpr Unit CubicMillimetre { .dimension = dim::Volume,
                                            .magnitudeNumerator = 1,
                                            .magnitudeDenominator = 1000000000,
                                            .symbolText = symbol("mm3"),
                                            .decimals = 0 };

    /// The coherent SI unit of mass.
    inline constexpr Unit Kilogram { .dimension = dim::Mass, .symbolText = symbol("kg"), .decimals = 3 };
    /// One thousandth of a kilogram.
    inline constexpr Unit Gram { .dimension = dim::Mass,
                                 .magnitudeNumerator = 1,
                                 .magnitudeDenominator = 1000,
                                 .symbolText = symbol("g"),
                                 .decimals = 1 };
    /// One millionth of a kilogram. One decimal, as `Gram` has: an analytical
    /// balance reads to a tenth of a milligram, which is the resolution a
    /// content determination by mass is actually weighed at.
    inline constexpr Unit Milligram { .dimension = dim::Mass,
                                      .magnitudeNumerator = 1,
                                      .magnitudeDenominator = 1000000,
                                      .symbolText = symbol("mg"),
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
    /// Twenty-four hours. No decimals, unlike the three units above it: an age
    /// in days is counted rather than measured, so a fraction of one is not a
    /// finer reading of the same thing.
    inline constexpr Unit Day { .dimension = dim::Time,
                                .magnitudeNumerator = 86400,
                                .symbolText = symbol("d"),
                                .decimals = 0 };

    /// The coherent SI unit of thermodynamic temperature.
    inline constexpr Unit Kelvin { .dimension = dim::Temperature, .symbolText = symbol("K"), .decimals = 2 };
    /// One of the two affine units, and the reason `Unit` carries an offset at
    /// all; `Fahrenheit` is the other. Zero degrees Celsius is 273.15 kelvin.
    /// Conversion moves a point on the scale, not a difference, so one degree
    /// Celsius converts to 274.15 kelvin, not to one kelvin; `checked_convert`
    /// does not convert differences.
    inline constexpr Unit Celsius { .dimension = dim::Temperature,
                                    .offsetNumerator = 27315,
                                    .offsetDenominator = 100,
                                    .symbolText = symbol("\xc2\xb0" "C"),
                                    .decimals = 1 };
    /// The other affine unit. A degree is exactly 5/9 of a kelvin, and zero
    /// degrees Fahrenheit is exactly 459.67 * 5/9 = 45967/180 kelvin, so no
    /// conversion rounds: 32 degrees is 273.15 kelvin, and -40 degrees is -40
    /// degrees Celsius. Converting to Celsius divides by 9, so a reading is
    /// often a fraction that is not a terminating decimal, and is kept as that
    /// fraction: 100 degrees is 340/9 degrees Celsius, not a rounded 37.78. Like
    /// `Celsius`, it converts a point, not a difference. One decimal, `Celsius`'s.
    inline constexpr Unit Fahrenheit { .dimension = dim::Temperature,
                                       .magnitudeNumerator = 5,
                                       .magnitudeDenominator = 9,
                                       .offsetNumerator = 45967,
                                       .offsetDenominator = 180,
                                       .symbolText = symbol("\xc2\xb0" "F"),
                                       .decimals = 1 };

    /// The coherent SI unit of force. One decimal rather than `Pascal`'s
    /// none: a newton is a coarse enough unit that reporting a tenth of one is
    /// ordinary, where a tenth of a pascal is not. An engineering default like
    /// every other in this namespace, not a requirement from any standard.
    inline constexpr Unit Newton { .dimension = dim::Force, .symbolText = symbol("N"), .decimals = 1 };
    /// One thousand newtons. Two decimals rather than `Newton`'s one: a
    /// hundredth of a kilonewton is ten newtons, which is about what a load
    /// frame's reading carries, where a third decimal would claim a newton of
    /// resolution the machine does not have. An engineering default, as every
    /// other in this namespace is.
    inline constexpr Unit Kilonewton { .dimension = dim::Force,
                                       .magnitudeNumerator = 1000,
                                       .symbolText = symbol("kN"),
                                       .decimals = 2 };

    /// The coherent SI unit of pressure.
    inline constexpr Unit Pascal { .dimension = dim::Pressure, .symbolText = symbol("Pa"), .decimals = 0 };
    /// One thousand pascals. One decimal, as `Megapascal` has: a stress or a
    /// pressure is quoted to a tenth of whichever of these units carries it.
    inline constexpr Unit Kilopascal { .dimension = dim::Pressure,
                                       .magnitudeNumerator = 1000,
                                       .symbolText = symbol("kPa"),
                                       .decimals = 1 };
    /// One million pascals.
    inline constexpr Unit Megapascal { .dimension = dim::Pressure,
                                       .magnitudeNumerator = 1000000,
                                       .symbolText = symbol("MPa"),
                                       .decimals = 1 };
    /// A newton on a square millimetre: exactly one megapascal, since a square
    /// millimetre is exactly a millionth of a square metre. The second spelling
    /// exists because both are written in practice, and it is the same
    /// magnitude on purpose -- `unit_tests.cpp` pins the two together so a
    /// later edit to one cannot quietly move it away from the other. One
    /// decimal, `Megapascal`'s, because it *is* `Megapascal`.
    inline constexpr Unit NewtonPerSquareMillimetre { .dimension = dim::Pressure,
                                                      .magnitudeNumerator = 1000000,
                                                      .symbolText = symbol("N/mm2"),
                                                      .decimals = 1 };
    /// One thousand million pascals. One decimal, as the two above: an elastic
    /// modulus is quoted to a tenth of a gigapascal.
    inline constexpr Unit Gigapascal { .dimension = dim::Pressure,
                                       .magnitudeNumerator = 1000000000,
                                       .symbolText = symbol("GPa"),
                                       .decimals = 1 };

    /// The coherent SI unit of energy. One decimal, for the same reason
    /// `Newton` has one: a joule is a coarse enough unit that a tenth of one is
    /// an ordinary thing to report.
    inline constexpr Unit Joule { .dimension = dim::Energy, .symbolText = symbol("J"), .decimals = 1 };
    /// One thousand joules, at `Joule`'s precision: a fracture or impact work
    /// in kilojoules is quoted to a tenth.
    inline constexpr Unit Kilojoule { .dimension = dim::Energy,
                                      .magnitudeNumerator = 1000,
                                      .symbolText = symbol("kJ"),
                                      .decimals = 1 };
    /// The coherent SI unit of power: a joule per second. One decimal, as
    /// `Joule` has.
    inline constexpr Unit Watt { .dimension = dim::Power, .symbolText = symbol("W"), .decimals = 1 };
    /// One thousand watts. Three decimals, so that the last digit is one watt.
    inline constexpr Unit Kilowatt { .dimension = dim::Power,
                                     .magnitudeNumerator = 1000,
                                     .symbolText = symbol("kW"),
                                     .decimals = 3 };
    /// The energy of one watt sustained for an hour: exactly 3600 joules. An
    /// energy, not a power -- the two are different dimensions and the type
    /// system keeps them apart. One decimal, `Joule`'s.
    inline constexpr Unit WattHour { .dimension = dim::Energy,
                                     .magnitudeNumerator = 3600,
                                     .symbolText = symbol("Wh"),
                                     .decimals = 1 };
    /// One thousand watt-hours: exactly 3600000 joules, the unit an electricity
    /// bill is usually written in. Three decimals, so that the last digit is one
    /// watt-hour.
    inline constexpr Unit KilowattHour { .dimension = dim::Energy,
                                         .magnitudeNumerator = 3600000,
                                         .symbolText = symbol("kWh"),
                                         .decimals = 3 };

    /// The coherent SI unit of frequency. One decimal: a loading frequency is
    /// set and reported to a tenth of a hertz.
    inline constexpr Unit Hertz { .dimension = dim::Frequency, .symbolText = symbol("Hz"), .decimals = 1 };

    /// The coherent SI unit of velocity. Three decimals, the default the other
    /// coherent units here carry. A rate such as a permeability runs many
    /// orders of magnitude smaller than that and would round to nothing: such a
    /// caller states its own precision at the point of use, which is what the
    /// note at the top of this namespace means.
    inline constexpr Unit MetrePerSecond { .dimension = dim::Velocity,
                                           .symbolText = symbol("m/s"),
                                           .decimals = 3 };
    /// One millimetre in a minute, which is exactly 1/60000 of a metre per
    /// second. A rate a method fixes and a result depends on, so it is a unit
    /// rather than a display of one; two decimals, the resolution such a rate
    /// is set to.
    inline constexpr Unit MillimetrePerMinute { .dimension = dim::Velocity,
                                                .magnitudeNumerator = 1,
                                                .magnitudeDenominator = 60000,
                                                .symbolText = symbol("mm/min"),
                                                .decimals = 2 };

    /// The coherent SI unit of density. No decimals: a density in kilograms per
    /// cubic metre runs to thousands and is reported as a whole number.
    inline constexpr Unit KilogramPerCubicMetre { .dimension = dim::Density,
                                                  .symbolText = symbol("kg/m3"),
                                                  .decimals = 0 };
    /// A gram in a cubic centimetre: exactly one thousand kilograms per cubic
    /// metre. Three decimals, so that the last digit is still a whole kilogram
    /// per cubic metre -- the same density reads at the same resolution in
    /// either unit.
    inline constexpr Unit GramPerCubicCentimetre { .dimension = dim::Density,
                                                   .magnitudeNumerator = 1000,
                                                   .symbolText = symbol("g/cm3"),
                                                   .decimals = 3 };
    /// A megagram in a cubic metre: the same magnitude as
    /// `GramPerCubicCentimetre`, one thousand kilograms per cubic metre, under
    /// the other spelling that occurs. Same three decimals, and
    /// `unit_tests.cpp` pins the pair together.
    inline constexpr Unit MegagramPerCubicMetre { .dimension = dim::Density,
                                                  .magnitudeNumerator = 1000,
                                                  .symbolText = symbol("Mg/m3"),
                                                  .decimals = 3 };

    /// The coherent SI unit of mass per area. Three decimals, so the last digit
    /// is a whole gram per square metre -- the resolution the quantity is
    /// actually specified at.
    inline constexpr Unit KilogramPerSquareMetre { .dimension = dim::MassPerArea,
                                                   .symbolText = symbol("kg/m2"),
                                                   .decimals = 3 };
    /// One thousandth of a kilogram per square metre. No decimals: a sheet or a
    /// membrane is specified and reported as a whole number of grams per square
    /// metre.
    inline constexpr Unit GramPerSquareMetre { .dimension = dim::MassPerArea,
                                               .magnitudeNumerator = 1,
                                               .magnitudeDenominator = 1000,
                                               .symbolText = symbol("g/m2"),
                                               .decimals = 0 };

    /// One thousand newtons on a metre of width. One decimal, the resolution a
    /// force per unit width is reported at.
    inline constexpr Unit KilonewtonPerMetre { .dimension = dim::ForcePerLength,
                                               .magnitudeNumerator = 1000,
                                               .symbolText = symbol("kN/m"),
                                               .decimals = 1 };
    /// A newton on a millimetre of width: exactly one kilonewton per metre, the
    /// same quantity under the other spelling. Same one decimal, and the pair
    /// is pinned in `unit_tests.cpp` the way `Megapascal` and
    /// `NewtonPerSquareMillimetre` are.
    inline constexpr Unit NewtonPerMillimetre { .dimension = dim::ForcePerLength,
                                                .magnitudeNumerator = 1000,
                                                .symbolText = symbol("N/mm"),
                                                .decimals = 1 };

    /// The coherent SI unit of dynamic viscosity. Three decimals, the default
    /// the other coherent units here carry, and it is the small end that needs
    /// them: a dynamic viscosity in pascal seconds runs from thousandths for a
    /// thin liquid to hundreds for a stiff binder.
    ///
    /// The symbol is written `Pa.s`, not `Pa*s`. An asterisk is Markdown
    /// emphasis, and a unit symbol is emitted into a Markdown document
    /// verbatim -- see the note on symbols in this file's namespace comment.
    inline constexpr Unit PascalSecond { .dimension = dim::DynamicViscosity,
                                         .symbolText = symbol("Pa.s"),
                                         .decimals = 3 };
    /// One thousandth of a pascal second. One decimal: a viscosity stated in
    /// these runs to hundreds or thousands, so a tenth is already finer than
    /// the measurement.
    inline constexpr Unit MillipascalSecond { .dimension = dim::DynamicViscosity,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 1000,
                                              .symbolText = symbol("mPa.s"),
                                              .decimals = 1 };

    /// One millionth of a square metre per second, the coherent SI unit of
    /// kinematic viscosity -- the *other* viscosity, and not interchangeable
    /// with `PascalSecond`: the two differ by a density and the dimensions say
    /// so. One decimal, the resolution such a value is reported at.
    inline constexpr Unit SquareMillimetrePerSecond { .dimension = dim::KinematicViscosity,
                                                      .magnitudeNumerator = 1,
                                                      .magnitudeDenominator = 1000000,
                                                      .symbolText = symbol("mm2/s"),
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

/// Converts @p magnitude from @p from into @p to, exactly.
///
/// Applies integer factors by multiply-then-divide rather than a precomputed
/// floating-point factor, so 30 MPa is exactly 30000000 Pa and converts back to
/// exactly 30. The offset makes the conversion affine, which is what degrees
/// Celsius and degrees Fahrenheit need; for units without one it is zero and
/// drops out.
///
/// Converts a POINT on the scale, not a difference: 1 degC becomes 274.15 K, not
/// 1 K. A difference-preserving conversion is a different operation and is not
/// this one.
///
/// @return the converted value, or an error if the dimensions differ, either
///         unit's magnitude is zero, or an intermediate is not representable.
///         Never a wrong number.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_convert(Rational magnitude,
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

    std::expected<Rational, ArithmeticError> const scaledValue = checked_mul(magnitude, *fromMagnitude);
    if (!scaledValue)
        return scaledValue;
    std::expected<Rational, ArithmeticError> const inSi = checked_add(*scaledValue, *fromOffset);
    if (!inSi)
        return inSi;
    std::expected<Rational, ArithmeticError> const shifted = checked_sub(*inSi, *toOffset);
    if (!shifted)
        return shifted;
    return checked_div(*shifted, *toMagnitude);
}

/// @throws ArithmeticException when the conversion cannot be represented.
[[nodiscard]] constexpr Rational convert(Rational magnitude, Unit from, Unit to)
{
    return detail::or_throw(checked_convert(magnitude, from, to));
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

/// `boundsCheck` in prose, for a trace or an error message.
[[nodiscard]] constexpr std::string_view describe(BoundsCheck boundsCheck) noexcept
{
    switch (boundsCheck)
    {
        case BoundsCheck::WithinBounds: return "within the declared bounds";
        case BoundsCheck::BelowMinimum: return "below the declared minimum";
        case BoundsCheck::AboveMaximum: return "above the declared maximum";
        case BoundsCheck::NotChecked: return "no bounds declared for this unit";
        case BoundsCheck::NotMeasured: return "no value was measured";
    }
    return "unknown bounds outcome";
}

/// Checks @p magnitude, expressed in @p unitOfValue, against that unit's bounds.
[[nodiscard]] constexpr std::expected<BoundsCheck, ArithmeticError> checked_within_bounds(Rational magnitude,
                                                                                          Unit unitOfValue) noexcept
{
    if (!unitOfValue.bounds.present)
        return BoundsCheck::NotChecked;

    std::expected<Rational, ArithmeticError> const lowBound =
        Rational::make(unitOfValue.bounds.lowNumerator, unitOfValue.bounds.lowDenominator);
    std::expected<Rational, ArithmeticError> const highBound =
        Rational::make(unitOfValue.bounds.highNumerator, unitOfValue.bounds.highDenominator);
    if (!lowBound)
        return std::unexpected { lowBound.error() };
    if (!highBound)
        return std::unexpected { highBound.error() };

    // A unit whose declared minimum exceeds its maximum is a malformed unit,
    // not a value to be judged. Reporting BelowMinimum or AboveMaximum here
    // would be a wrong answer dressed up as a real one; refuse instead.
    if (*lowBound > *highBound)
        return std::unexpected { ArithmeticError::DomainError };

    if (magnitude < *lowBound)
        return BoundsCheck::BelowMinimum;
    if (magnitude > *highBound)
        return BoundsCheck::AboveMaximum;
    return BoundsCheck::WithinBounds;
}

/// The unit's declared display precision, as the rounding layer's own type.
[[nodiscard]] constexpr DecimalPlaces declared_decimals(Unit unitOfValue) noexcept
{
    return DecimalPlaces { unitOfValue.decimals };
}

/// Rounds @p magnitude to the precision its unit declares.
[[nodiscard]] constexpr std::expected<Rational, ArithmeticError> checked_round_to_declared(
    Rational magnitude, Unit unitOfValue, RoundingMode roundingMode) noexcept
{
    return checked_round(magnitude, declared_decimals(unitOfValue), roundingMode);
}

/// @throws ArithmeticException when the checked form would report an error.
[[nodiscard]] constexpr Rational round_to_declared(Rational magnitude, Unit unitOfValue, RoundingMode roundingMode)
{
    return detail::or_throw(checked_round_to_declared(magnitude, unitOfValue, roundingMode));
}

} // namespace formula
