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

#include <formula-cpp/formula.hpp>

#include <cstdio>
#include <expected>
#include <string_view>

namespace
{
using formula::Dimension;
using formula::Exponent;

/// Prints one base dimension's exponent as "^p" when integral, "^(p/q)" when
/// not, and nothing at all when the exponent is zero.
void print_exponent(std::string_view baseName, Exponent value)
{
    if (formula::is_zero(value))
        return;
    int const nameLength = static_cast<int>(baseName.size());
    if (formula::is_integer(value))
        std::printf(" %.*s^%d", nameLength, baseName.data(), value.numerator);
    else
        std::printf(" %.*s^(%d/%d)", nameLength, baseName.data(), value.numerator, value.denominator);
}

/// Prints a Dimension as its seven-exponent vector and then its named base
/// dimensions, each omitted when its exponent is zero. There is no
/// formula::operator<<: the library keeps <ostream>/<format> out of its public
/// headers, so a consumer that wants to print a Dimension writes this itself,
/// as this example does.
void print_dimension(char const* label, Dimension value)
{
    std::printf("%s =", label);
    print_exponent("L", value.length);
    print_exponent("M", value.mass);
    print_exponent("T", value.time);
    print_exponent("I", value.current);
    print_exponent("Theta", value.temperature);
    print_exponent("N", value.amount);
    print_exponent("J", value.luminosity);
    // A slot not in use holds a zero exponent, which prints nothing.
    for (formula::NamedBase const& base: value.namedBases)
        print_exponent(formula::view(base.name), base.exponent);
    if (formula::is_dimensionless(value))
        std::printf(" (dimensionless)");
    std::printf("\n");
}
} // namespace

int main()
{
    namespace dim = formula::dim;
    namespace unit = formula::unit;
    using formula::BoundsCheck;
    using formula::Rational;
    using formula::RoundingMode;
    using formula::Unit;

    // ---- 1. Composing dimensions from named constants ----
    //
    // Nobody writes Dimension{.length = exponent(2)} by hand; the algebra
    // composes named constants instead, so the representation stays swappable.
    Dimension const area = dim::Length * dim::Length;
    Dimension const volume = area * dim::Length;
    Dimension const density = dim::Mass / volume;

    print_dimension("area (length * length)", area);
    print_dimension("volume (area * length)", volume);
    print_dimension("density (mass / volume)", density);

    bool const compositionMatches = area == dim::Area && volume == dim::Volume && density == dim::Density;
    std::printf("composed dimensions match the named constants: %s\n", compositionMatches ? "yes" : "no");

    // ---- 2. A dimension only a rational exponent can express ----
    //
    // The square root of an area is a length -- an everyday integer power.
    // The square root of a LENGTH is length to the one half, which no integer
    // exponent can name at all. Norm-style size formulas do take such roots.
    Dimension const rootOfLength = formula::nth_root(dim::Length, 2);
    print_dimension("sqrt(length)", rootOfLength);
    bool const rootIsHalfPower = rootOfLength.length == formula::exponent(1, 2);
    std::printf("sqrt(length) has exponent one half: %s\n", rootIsHalfPower ? "yes" : "no");

    // ---- 3. Exact conversion that round-trips: 450 l to m3 and back ----
    Rational const volumeInLitres = *Rational::from_decimal(450, 0);
    Rational const volumeInCubicMetres = formula::convert(volumeInLitres, unit::Litre, unit::CubicMetre);
    Rational const volumeBackInLitres = formula::convert(volumeInCubicMetres, unit::CubicMetre, unit::Litre);

    std::printf("450 l = %lld/%lld m3\n",
                static_cast<long long>(volumeInCubicMetres.numerator()),
                static_cast<long long>(volumeInCubicMetres.denominator()));
    std::printf("... converted back = %lld l\n", static_cast<long long>(volumeBackInLitres.numerator()));
    bool const volumeRoundTrips = volumeBackInLitres == volumeInLitres;
    std::printf("volume round trip exact: %s\n", volumeRoundTrips ? "yes" : "no");

    // ---- 4. The affine case: 100 degC to K and back, then degF to degC ----
    //
    // Conversion moves a POINT on a scale, not a difference: 100 degC is not
    // 100 K, it is 100 K above the offset between the two scales.
    Rational const tempInCelsius = *Rational::make(100, 1);
    Rational const tempInKelvin = formula::convert(tempInCelsius, unit::Celsius, unit::Kelvin);
    Rational const tempBackInCelsius = formula::convert(tempInKelvin, unit::Kelvin, unit::Celsius);

    std::printf("100 degC = %lld/%lld K\n",
                static_cast<long long>(tempInKelvin.numerator()),
                static_cast<long long>(tempInKelvin.denominator()));
    std::printf("... converted back = %lld degC\n", static_cast<long long>(tempBackInCelsius.numerator()));
    bool const temperatureRoundTrips = tempBackInCelsius == tempInCelsius;
    std::printf("temperature round trip exact: %s\n", temperatureRoundTrips ? "yes" : "no");

    // The second affine scale. -40 is where degrees Fahrenheit and degrees
    // Celsius meet, so it converts to itself. 100 degF is a number of degrees
    // Celsius that is a fraction, 340/9, not a terminating decimal, and it is
    // kept as that fraction: converting divides by 9 and rounds nothing.
    Rational const minusFortyInFahrenheit = *Rational::make(-40, 1);
    Rational const minusFortyInCelsius = formula::convert(minusFortyInFahrenheit, unit::Fahrenheit, unit::Celsius);
    Rational const hundredInFahrenheit = *Rational::make(100, 1);
    Rational const hundredFahrenheitInCelsius = formula::convert(hundredInFahrenheit, unit::Fahrenheit, unit::Celsius);

    std::printf("-40 degF = %lld degC\n", static_cast<long long>(minusFortyInCelsius.numerator()));
    std::printf("100 degF = %lld/%lld degC\n",
                static_cast<long long>(hundredFahrenheitInCelsius.numerator()),
                static_cast<long long>(hundredFahrenheitInCelsius.denominator()));
    bool const fahrenheitConvertsExactly = minusFortyInCelsius == *Rational::make(-40, 1)
                                           && hundredFahrenheitInCelsius == *Rational::make(340, 9);

    // ---- 5. Power and energy: a kilowatt-hour is exactly 3600000 joules ----
    //
    // A watt-hour is the energy of one watt sustained for an hour, 3600
    // joules, and a kilowatt-hour is a thousand of them: the factor is a whole
    // number, so the conversion needs no rounded constant.
    Rational const oneKilowattHour = *Rational::make(1, 1);
    Rational const kilowattHourInJoules = formula::convert(oneKilowattHour, unit::KilowattHour, unit::Joule);

    std::printf("1 kWh = %lld J\n", static_cast<long long>(kilowattHourInJoules.numerator()));
    bool const kilowattHourIsExact = kilowattHourInJoules == *Rational::make(3600000, 1);

    // ---- 6. A unit's declared precision applied to a computed value ----
    //
    // A generic density and a generic volume, multiplied to a mass -- the
    // point is that the RESULT of a calculation, not a literal, gets rounded
    // to the unit it will be reported in. unit::Kilogram declares 3 decimals.
    Rational const genericDensity = *Rational::make(1000, 7); // an arbitrary density, in kg/m3
    Rational const computedMass = genericDensity * volumeInCubicMetres;
    Rational const roundedMass =
        *formula::checked_round_to_declared(computedMass, unit::Kilogram, RoundingMode::HalfAwayFromZero);

    std::printf("computed mass = %lld/%lld kg\n",
                static_cast<long long>(computedMass.numerator()),
                static_cast<long long>(computedMass.denominator()));
    std::printf("rounded to kg's declared precision (%d places) = %.*f kg\n",
                formula::declared_decimals(unit::Kilogram).value,
                formula::declared_decimals(unit::Kilogram).value,
                roundedMass.to_double());

    // 450/7 kg is 64.2857..., which at kilogram's three declared places is
    // 64.286. Asserted, not merely printed: the documentation quotes this
    // number, and without a check here changing the rounding mode silently
    // changes it while the example still reports success.
    bool const massRoundsAsDocumented = roundedMass == *Rational::from_decimal(64286, -3);

    // ---- 7. Bounds: NotChecked is not a verdict, WithinBounds is ----
    constexpr Unit BoundedGauge { .dimension = dim::Scalar,
                                  .magnitudeNumerator = 1,
                                  .magnitudeDenominator = 100,
                                  .symbolText = formula::symbol("%"),
                                  .decimals = 1,
                                  .bounds = formula::bounds(0, 1, 100, 1) };

    BoundsCheck const unboundedVerdict = *formula::checked_within_bounds(*Rational::make(1000000, 1), unit::Litre);
    BoundsCheck const boundedVerdict = *formula::checked_within_bounds(*Rational::make(42, 1), BoundedGauge);

    std::printf("unbounded unit (litre) reports: %s\n", formula::describe(unboundedVerdict).data());
    std::printf("bounded gauge at 42%%: %s\n", formula::describe(boundedVerdict).data());
    bool const boundsBehaveAsDocumented =
        unboundedVerdict == BoundsCheck::NotChecked && boundedVerdict == BoundsCheck::WithinBounds;

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
    print_dimension("tariff (EUR / energy)", tariff);
    print_dimension("tariff * energy", tariffTimesEnergy);

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

    Rational const priceInEuros = *Rational::make(250, 1);
    Rational const priceInCents = formula::convert(priceInEuros, Euro, EuroCent);
    Rational const priceBackInEuros = formula::convert(priceInCents, EuroCent, Euro);
    std::expected<Rational, formula::ArithmeticError> const priceInYen =
        formula::checked_convert(priceInEuros, Euro, Yen);

    std::printf("250 EUR = %lld ct\n", static_cast<long long>(priceInCents.numerator()));
    std::printf("... converted back = %lld EUR\n", static_cast<long long>(priceBackInEuros.numerator()));
    std::printf("250 EUR to JPY: %s\n",
                priceInYen.has_value() ? "converted" : formula::describe(priceInYen.error()).data());
    bool const moneyBehavesAsDocumented = tariffTimesEnergy == euros && !(tariff == euros)
                                          && priceInCents == *Rational::make(25000, 1)
                                          && priceBackInEuros == priceInEuros && !priceInYen.has_value()
                                          && priceInYen.error() == formula::ArithmeticError::DomainError;

    // ---- summary ----
    bool const allChecksPassed = compositionMatches && rootIsHalfPower && volumeRoundTrips && temperatureRoundTrips
                                  && fahrenheitConvertsExactly && kilowattHourIsExact && massRoundsAsDocumented
                                  && boundsBehaveAsDocumented && moneyBehavesAsDocumented;
    std::printf("all checks passed: %s\n", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
