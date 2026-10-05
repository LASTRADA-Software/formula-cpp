// SPDX-License-Identifier: Apache-2.0
//
// Dimensions and units.
//
// Generic physics only, no standard cited: composing dimensions from named
// constants rather than spelling exponents, a dimension only a rational
// exponent can express, an exact round-tripping conversion, the affine case a
// temperature scale needs on both affine scales, an energy unit whose factor to
// the joule is a whole number, a unit's declared display precision applied to a
// computed value, a bounds check that tells "never checked" apart from
// "checked and passed", and a base dimension the SI does not have: money.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>

#include <expected>
#include <optional>
#include <print>
#include <string_view>

int main()
{
    namespace dim = formula::dim;
    namespace unit = formula::unit;
    using formula::BoundsCheck;
    using formula::Dimension;
    using formula::Rational;
    using formula::RoundingMode;
    using formula::Unit;
    using namespace formula::literals;

    // ---- 1. Composing dimensions from named constants ----
    //
    // Nobody writes Dimension{.length = exponent(2)} by hand; the algebra
    // composes named constants instead, so the representation stays swappable.
    Dimension const area = dim::Length * dim::Length;
    Dimension const volume = area * dim::Length;
    Dimension const density = dim::Mass / volume;

    // `{}` of a Dimension (format.hpp) writes each base whose exponent is not
    // zero, in the order L, M, T, I, Theta, N, J, then each named base by name.
    std::println("area (length * length) = {}", area);
    std::println("volume (area * length) = {}", volume);
    std::println("density (mass / volume) = {}", density);

    bool const compositionMatches = area == dim::Area && volume == dim::Volume && density == dim::Density;
    std::println("composed dimensions match the named constants: {}", compositionMatches ? "yes" : "no");

    // ---- 2. A dimension only a rational exponent can express ----
    //
    // The square root of an area is a length -- an everyday integer power.
    // The square root of a LENGTH is length to the one half, which no integer
    // exponent can name at all. Norm-style size formulas do take such roots.
    Dimension const rootOfLength = formula::nth_root(dim::Length, 2);
    std::println("sqrt(length) = {}", rootOfLength);
    bool const rootIsHalfPower = rootOfLength.length == formula::exponent(1, 2);
    std::println("sqrt(length) has exponent one half: {}", rootIsHalfPower ? "yes" : "no");

    // ---- 3. Exact conversion that round-trips: 450 l to m3 and back ----
    //
    // `{:/}` writes a Rational as its fraction; `{}` writes the exact decimal
    // where there is one, and the fraction otherwise.
    Rational const volumeInLitres = 450;
    Rational const volumeInCubicMetres = formula::convert(volumeInLitres, unit::Litre, unit::CubicMetre);
    Rational const volumeBackInLitres = formula::convert(volumeInCubicMetres, unit::CubicMetre, unit::Litre);

    std::println("{} l = {:/} m3", volumeInLitres, volumeInCubicMetres);
    std::println("... converted back = {} l", volumeBackInLitres);
    bool const volumeRoundTrips = volumeBackInLitres == volumeInLitres;
    std::println("volume round trip exact: {}", volumeRoundTrips ? "yes" : "no");

    // ---- 4. The affine case: 100 degC to K and back, then degF to degC ----
    //
    // Conversion moves a POINT on a scale, not a difference: 100 degC is not
    // 100 K, it is 100 K above the offset between the two scales.
    Rational const tempInCelsius = 100;
    Rational const tempInKelvin = formula::convert(tempInCelsius, unit::Celsius, unit::Kelvin);
    Rational const tempBackInCelsius = formula::convert(tempInKelvin, unit::Kelvin, unit::Celsius);

    std::println("{} degC = {:/} K", tempInCelsius, tempInKelvin);
    std::println("... converted back = {} degC", tempBackInCelsius);
    bool const temperatureRoundTrips = tempBackInCelsius == tempInCelsius;
    std::println("temperature round trip exact: {}", temperatureRoundTrips ? "yes" : "no");

    // The second affine scale. -40 is where degrees Fahrenheit and degrees
    // Celsius meet, so it converts to itself. 100 degF is a number of degrees
    // Celsius that is a fraction, 340/9, not a terminating decimal, and it is
    // kept as that fraction: converting divides by 9 and rounds nothing.
    Rational const minusFortyInFahrenheit = -40;
    Rational const minusFortyInCelsius = formula::convert(minusFortyInFahrenheit, unit::Fahrenheit, unit::Celsius);
    Rational const hundredInFahrenheit = 100;
    Rational const hundredFahrenheitInCelsius = formula::convert(hundredInFahrenheit, unit::Fahrenheit, unit::Celsius);

    std::println("{} degF = {} degC", minusFortyInFahrenheit, minusFortyInCelsius);
    std::println("{} degF = {:/} degC", hundredInFahrenheit, hundredFahrenheitInCelsius);
    bool const fahrenheitConvertsExactly =
        minusFortyInCelsius == -40_r && hundredFahrenheitInCelsius == Rational { 340, 9 };

    // ---- 5. Power and energy: a kilowatt-hour is exactly 3600000 joules ----
    //
    // A watt-hour is the energy of one watt sustained for an hour, 3600
    // joules, and a kilowatt-hour is a thousand of them: the factor is a whole
    // number, so the conversion needs no rounded constant.
    Rational const oneKilowattHour = 1;
    Rational const kilowattHourInJoules = formula::convert(oneKilowattHour, unit::KilowattHour, unit::Joule);

    std::println("{} kWh = {} J", oneKilowattHour, kilowattHourInJoules);
    bool const kilowattHourIsExact = kilowattHourInJoules == 3600000_r;

    // ---- 6. A unit's declared precision applied to a computed value ----
    //
    // A generic density and a generic volume, multiplied to a mass -- the
    // point is that the RESULT of a calculation, not a literal, gets rounded
    // to the unit it will be reported in. unit::Kilogram declares 3 decimals.
    // The rounding returns a std::expected -- the rounded value, or the
    // arithmetic error that stopped it -- and it is checked before it is read.
    Rational const genericDensity { 1000, 7 }; // an arbitrary density, in kg/m3
    Rational const computedMass = genericDensity * volumeInCubicMetres;
    auto const roundedMass =
        formula::checked_round_to_declared(computedMass, unit::Kilogram, RoundingMode::HalfAwayFromZero);
    if (!roundedMass)
    {
        std::println("rounding the computed mass: {}", roundedMass.error());
        return 1;
    }

    std::println("computed mass = {:/} kg", computedMass);
    std::println("rounded to kg's declared precision ({} places) = {} kg",
                 formula::declared_decimals(unit::Kilogram).value,
                 *roundedMass);

    // 450/7 kg is 64.2857..., which at kilogram's three declared places is
    // 64.286. Asserted, not merely printed: the documentation quotes this
    // number, and without a check here changing the rounding mode silently
    // changes it while the example still reports success.
    bool const massRoundsAsDocumented = *roundedMass == 64.286_r;

    // ---- 7. Bounds: NotChecked is not a verdict, WithinBounds is ----
    constexpr Unit BoundedGauge { .dimension = dim::Scalar,
                                  .magnitudeNumerator = 1,
                                  .magnitudeDenominator = 100,
                                  .symbolText = formula::symbol("%"),
                                  .decimals = 1,
                                  .bounds = formula::bounds(0, 1, 100, 1) };

    constexpr auto unboundedVerdict = formula::checked_within_bounds(1'000'000, unit::Litre);
    constexpr auto boundedVerdict = formula::checked_within_bounds(42, BoundedGauge);
    static_assert(unboundedVerdict.has_value() && boundedVerdict.has_value());

    // `{}` of a BoundsCheck writes its describe() words.
    std::println("unbounded unit (litre) reports: {}", *unboundedVerdict);
    std::println("bounded gauge at 42%: {}", *boundedVerdict);

    // Limits held at run time -- here a catalogue row with a minimum and no
    // maximum -- need no unit to carry them, and either end may be absent.
    std::optional<Rational> const catalogueMinimum = Rational { 25 };
    Rational const gaugeReading = 42;
    auto const catalogueVerdict = formula::checked_within(gaugeReading, catalogueMinimum, std::nullopt);
    if (!catalogueVerdict)
    {
        std::println("checking the gauge against the catalogue: {}", catalogueVerdict.error());
        return 1;
    }
    std::println("gauge at 42% against a catalogue minimum of 25%: {}", *catalogueVerdict);

    bool const boundsBehaveAsDocumented = *unboundedVerdict == BoundsCheck::NotChecked
                                          && *boundedVerdict == BoundsCheck::WithinBounds
                                          && *catalogueVerdict == BoundsCheck::WithinBounds;

    // ---- 8. A base dimension the SI does not have: money ----
    //
    // A currency is not a bare number, so it gets a base dimension of its
    // own, named by the application: base_dimension("EUR"). The unit named
    // after the base has magnitude one, and a cent is a hundredth of it. A tariff
    // in euros per energy times an energy is euros; and euros never convert
    // into yen, because an exchange rate is data -- a quantity in yen per euro
    // -- not a conversion factor.
    Dimension const euros = formula::base_dimension("EUR");
    Dimension const tariff = euros / dim::Energy;
    Dimension const tariffTimesEnergy = tariff * dim::Energy;
    std::println("tariff (EUR / energy) = {}", tariff);
    std::println("tariff * energy = {}", tariffTimesEnergy);

    constexpr Unit Euro { .dimension = formula::base_dimension("EUR"),
                          .symbolText = formula::symbol("EUR"),
                          .decimals = 2 };
    constexpr Unit EuroCent { .dimension = formula::base_dimension("EUR"),
                              .magnitudeNumerator = 1,
                              .magnitudeDenominator = 100,
                              .symbolText = formula::symbol("ct"),
                              .decimals = 0 };
    constexpr Unit Yen { .dimension = formula::base_dimension("JPY"),
                         .symbolText = formula::symbol("JPY"),
                         .decimals = 0 };

    Rational const priceInEuros = 250;
    Rational const priceInCents = formula::convert(priceInEuros, Euro, EuroCent);
    Rational const priceBackInEuros = formula::convert(priceInCents, EuroCent, Euro);
    std::expected<Rational, formula::ArithmeticError> const priceInYen =
        formula::checked_convert(priceInEuros, Euro, Yen);

    std::println("{} EUR = {} ct", priceInEuros, priceInCents);
    std::println("... converted back = {} EUR", priceBackInEuros);
    std::println("{} EUR to JPY: {}",
                 priceInEuros,
                 priceInYen.has_value() ? std::string_view { "converted" } : formula::describe(priceInYen.error()));
    bool const moneyBehavesAsDocumented = tariffTimesEnergy == euros && tariff != euros && priceInCents == 25000_r
                                          && priceBackInEuros == priceInEuros && !priceInYen.has_value()
                                          && priceInYen.error() == formula::ArithmeticError::DomainError;

    // ---- summary ----
    bool const allChecksPassed = compositionMatches && rootIsHalfPower && volumeRoundTrips && temperatureRoundTrips
                                  && fahrenheitConvertsExactly && kilowattHourIsExact && massRoundsAsDocumented
                                  && boundsBehaveAsDocumented && moneyBehavesAsDocumented;
    std::println("all checks passed: {}", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
