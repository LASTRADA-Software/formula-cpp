// SPDX-License-Identifier: Apache-2.0
//
// How fast a cyclist rides on a given power: the steady-state speed of a rider
// and bike on a road, with no wind, after the model Martin et al. validated.
//
// The power a rider holds balances what slows them down -- rolling resistance
// and gravity, both in proportion to the weight, and air drag, in proportion
// to the square of the speed:
//
//     P = v * (m * g * (C_rr + s) + 1/2 * rho * C_dA * v^2)
//
// Solved for the speed, that is a cubic in v. Wherever it has a single real
// root, that root is Cardano's: with k = 1/2 * rho * C_dA,
// F = m * g * (C_rr + s), a = P / (2k) and b = F / (3k),
//
//     v = cbrt(a + sqrt(a^2 + b^3)) + cbrt(a - sqrt(a^2 + b^3))
//
// It shows:
//
//   - the rolling coefficient taken from the road surface by an exact lookup;
//   - the gradient from the height a road rises over the distance it runs;
//   - each step a quantity of its own, the speed's formula cited, and the
//     whole calculation rendered as plain text and as LaTeX, with its symbol
//     table;
//   - every step calculated exactly but the speed, whose roots have no exact
//     value, so the speed is evaluated in double from the exact steps;
//   - the speed on a flat road and on a 3 % climb at 250 W, and a steep
//     descent at no power at all, where the formula has no real answer and
//     says so.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <print>

namespace
{
namespace unit = formula::unit;
using formula::var;
using namespace formula::literals;

// ---- Units the library does not ship ----
//
// Each is declared over its dimension: four coherent combinations of SI units,
// and the kilometre an hour, which is not coherent.
inline constexpr formula::Unit MetrePerSecondSquared { .dimension = formula::dim::Acceleration,
                                                       .symbolText = formula::symbol("m/s2"),
                                                       .decimals = 5 };
inline constexpr formula::Unit KilogramPerMetre { .dimension = formula::dim::Mass / formula::dim::Length,
                                                  .symbolText = formula::symbol("kg/m"),
                                                  .decimals = 3 };
inline constexpr formula::Unit SquareMetrePerSecondSquared { .dimension = formula::dim::Velocity * formula::dim::Velocity,
                                                             .symbolText = formula::symbol("m2/s2"),
                                                             .decimals = 3 };
inline constexpr formula::Unit CubicMetrePerSecondCubed { .dimension = formula::dim::Velocity * formula::dim::Velocity
                                                                       * formula::dim::Velocity,
                                                          .symbolText = formula::symbol("m3/s3"),
                                                          .decimals = 3 };
// One kilometre an hour is 1000 m in 3600 s: 5/18 of a metre a second.
inline constexpr formula::Unit KilometrePerHour { .dimension = formula::dim::Velocity,
                                                  .magnitudeNumerator = 5,
                                                  .magnitudeDenominator = 18,
                                                  .symbolText = formula::symbol("km/h"),
                                                  .decimals = 1 };

// ---- The inputs ----
using RiderMass = formula::Quantity<struct RiderMassTag, "m_r", "rider's mass", unit::Kilogram>;
using BikeMass = formula::Quantity<struct BikeMassTag, "m_b", "bike's mass", unit::Kilogram>;
using DragArea = formula::Quantity<struct DragAreaTag, "C_dA", "drag area", unit::SquareMetre>;
using AirDensity = formula::Quantity<struct AirDensityTag, "rho", "density of the air", unit::KilogramPerCubicMetre>;
using Power = formula::Quantity<struct PowerTag, "P", "power the rider holds", unit::Watt>;
using Rise = formula::Quantity<struct RiseTag, "h", "height gained", unit::Metre>;
using Run = formula::Quantity<struct RunTag, "L", "horizontal distance covered", unit::Metre>;

// ---- The calculated values ----
using RollingCoefficient =
    formula::Quantity<struct RollingCoefficientTag, "C_rr", "rolling resistance coefficient", unit::One>;
using Gradient = formula::Quantity<struct GradientTag, "s", "road gradient", unit::One>;
using TotalMass = formula::Quantity<struct TotalMassTag, "m", "mass of rider and bike", unit::Kilogram>;
using ResistingForce =
    formula::Quantity<struct ResistingForceTag, "F", "rolling resistance and gravity together", unit::Newton>;
using DragFactor = formula::Quantity<struct DragFactorTag, "k", "air drag per square of speed", KilogramPerMetre>;
using PowerTerm = formula::Quantity<struct PowerTermTag, "a", "power over twice the drag factor", CubicMetrePerSecondCubed>;
using ForceTerm =
    formula::Quantity<struct ForceTermTag, "b", "force over three times the drag factor", SquareMetrePerSecondSquared>;
using Speed = formula::Quantity<struct SpeedTag, "v", "steady-state speed", unit::MetrePerSecond>;
using SpeedInKmh = formula::Quantity<struct SpeedInKmhTag, "v_kmh", "steady-state speed", KilometrePerHour>;

// Standard gravity, as defined: exactly 9.80665 m/s2.
inline constexpr auto gravity = formula::constant<MetrePerSecondSquared>(9.80665_r);

// ---- The road surface ----
//
// The surface names its row of the table; it is a category, not a quantity.
// No row for a surface the table does not list is ever made up: an exact
// lookup of a key in no row is a DomainError.
enum class RoadSurface : std::uint8_t
{
    Asphalt = 1,
    Cobbles = 2,
    Gravel = 3,
};

inline constexpr formula::KeyTable<RoadSurface, 3> SurfaceKeys {
    RoadSurface::Asphalt,
    RoadSurface::Cobbles,
    RoadSurface::Gravel,
};

// Typical rolling resistance coefficients of a road tyre on each surface --
// values commonly quoted for illustration, not taken from a standard's table.
[[nodiscard]] constexpr auto rollingCoefficient(RoadSurface surface)
{
    return formula::yields<RollingCoefficient>(
        formula::exact_lookup<SurfaceKeys, unit::One>(surface, { 0.004_r, 0.02_r, 0.01_r }));
}

// ---- The steps ----
inline constexpr auto gradient = formula::yields<Gradient>(var<Rise> / var<Run>);
inline constexpr auto totalMass = formula::yields<TotalMass>(var<RiderMass> + var<BikeMass>);
inline constexpr auto resistingForce =
    formula::yields<ResistingForce>(var<TotalMass> * gravity * (var<RollingCoefficient> + var<Gradient>) );
inline constexpr auto dragFactor = formula::yields<DragFactor>(formula::Rational { 1, 2 } * var<AirDensity> * var<DragArea>);
inline constexpr auto powerTerm = formula::yields<PowerTerm>(var<Power> / (2 * var<DragFactor>) );
inline constexpr auto forceTerm = formula::yields<ForceTerm>(var<ResistingForce> / (3 * var<DragFactor>) );

// The speed, read from a and b by name, so its rendering stays one line.
inline constexpr auto discriminant = formula::pow<2>(var<PowerTerm>) + formula::pow<3>(var<ForceTerm>);
inline constexpr auto speed = formula::yields<Speed>(formula::documented(
    formula::cbrt(var<PowerTerm> + formula::sqrt(discriminant))
        + formula::cbrt(var<PowerTerm> - formula::sqrt(discriminant)),
    { .title = "Validation of a mathematical model for road cycling power",
      .reference = "J. C. Martin, D. L. Milliken, J. E. Cobb, K. L. McFadden and A. R. Coggan, "
                   "Journal of Applied Biomechanics, 1998",
      .text = "A simplified form of the model, without drivetrain or bearing losses: the power a rider holds "
              "balances rolling resistance, gravity and air drag; in steady state, with no wind and a small "
              "gradient, P = v * (m * g * (C_rr + s) + 1/2 * rho * C_dA * v^2), solved here for v." }));

// How LaTeX writes the two symbols plain text cannot: a subscript of two
// letters, and the Greek letter.
inline constexpr auto latexSymbols =
    formula::vocabulary(formula::renames<RollingCoefficient>("C_{rr}"), formula::renames<AirDensity>("\\rho"));

// ---- The calculation ----
//
// A function of the surface: the surface is a category, not a measurement,
// so it chooses the lookup's row rather than being read from the inputs.
[[nodiscard]] constexpr auto ride(RoadSurface surface)
{
    return formula::calculation(formula::define(rollingCoefficient(surface)),
                                formula::define(gradient),
                                formula::define(totalMass),
                                formula::define(resistingForce),
                                formula::define(dragFactor),
                                formula::define(powerTerm),
                                formula::define(forceTerm),
                                formula::define(speed));
}

/// A rider of 75 kg on a bike of 8.5 kg, with a drag area of 0.32 m2, in air
/// of 1.225 kg/m3, holding @p watts on a road rising @p rise over 1000 m.
[[nodiscard]] constexpr auto riding(formula::Rational watts, formula::Rational rise)
{
    return formula::environment(formula::Measured<RiderMass> { 75 },
                                formula::Measured<BikeMass> { 8.5_r },
                                formula::Measured<DragArea> { 0.32_r },
                                formula::Measured<AirDensity> { 1.225_r },
                                formula::Measured<Power> { watts },
                                formula::Measured<Rise> { rise },
                                formula::Measured<Run> { 1000 });
}

/// The speed of one ride, in m/s and in km/h; both absent when an input is.
struct RideSpeed
{
    formula::Measured<Speed> inMetresPerSecond;
    formula::Measured<SpeedInKmh> inKilometresPerHour;
};

/// Calculates @p surface's ride on @p inputs: every step exactly on a
/// worksheet, then the speed in double from the exact a and b, rounded to the
/// millimetre a second.
[[nodiscard]] std::expected<RideSpeed, formula::ArithmeticError> ride_on(RoadSurface surface,
                                                                         decltype(riding(0, 0)) const& inputs)
{
    auto sheet = formula::worksheet(ride(surface), inputs);
    auto const [a, b] = sheet.checked_calculate<PowerTerm, ForceTerm>();
    if (!a)
        return std::unexpected { a.error() };
    if (!b)
        return std::unexpected { b.error() };

    auto const inSi =
        formula::checked_evaluate_si<double>(speed.expression, formula::environment(a->measurement(), b->measurement()));
    if (!inSi)
        return std::unexpected { inSi.error() };
    // An input nobody measured leaves the speed absent: not zero, not a failure.
    if (!inSi->has_value())
        return RideSpeed { .inMetresPerSecond = {}, .inKilometresPerHour = {} };

    // In the coherent unit, which m/s is: the double is a speed in m/s.
    auto const exact = formula::rational_from_double(**inSi, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfEven);
    if (!exact)
        return std::unexpected { exact.error() };
    formula::Measured<Speed> const inMetresPerSecond { *exact };
    auto const inKilometresPerHour = formula::checked_convert_to<SpeedInKmh>(inMetresPerSecond);
    if (!inKilometresPerHour)
        return std::unexpected { inKilometresPerHour.error() };
    return RideSpeed { .inMetresPerSecond = inMetresPerSecond, .inKilometresPerHour = *inKilometresPerHour };
}
} // namespace

int main()
{
    bool ok = true;
    auto check = [&ok](char const* what, bool condition) {
        std::println("{:<58} {}", what, condition ? "yes" : "NO");
        ok = ok && condition;
    };

    // Every number shown as a decimal where that is its exact value: 9.80665,
    // not 196133/20000.
    auto const decimals = formula::NumberStyle::exact_decimal();
    auto const onAsphalt = ride(RoadSurface::Asphalt);

    // ---- 1. The calculation, one step a line ----
    std::println("the calculation, in the order it calculates:\n{}\n", formula::render(onAsphalt, { .numbers = decimals }));
    std::println("in LaTeX:\n{}\n",
                 formula::render<formula::Dialect::LaTeX>(onAsphalt, latexSymbols, { .numbers = decimals }));

    // ---- 2. Its symbol table and its citation ----
    formula::Documentation const page = formula::document(onAsphalt, { .numbers = decimals });
    std::println("its symbol table:");
    for (formula::SymbolEntry const& entry: page.symbols)
        std::println("  {:<5} {:<6} {}", entry.symbol, entry.unit, entry.description);
    std::println("");
    auto const calculatedRows = std::ranges::count_if(
        page.symbols, [](formula::SymbolEntry const& entry) { return entry.calculatedAs.has_value(); });
    check("15 rows: 8 calculated, 7 inputs", page.symbols.size() == 15 && calculatedRows == 8);

    if (page.citations.empty())
    {
        std::println("the speed's formula cites nothing");
        return 1;
    }
    formula::Citation const& citation = page.citations.front();
    std::println("\nthe speed, after:\n  {}\n  {}", citation.title, citation.reference);

    // ---- 3. The speed, in double, from the exact steps ----
    //
    // Every step up to a and b is a sum, product or quotient of exact numbers,
    // and is calculated exactly. The speed's roots have no exact value, so it
    // is evaluated in double instead (ride_on above).
    auto const flat = ride_on(RoadSurface::Asphalt, riding(250, 0));
    if (!flat)
    {
        std::println("flat road at 250 W: {}", formula::describe(flat.error()));
        return 1;
    }
    std::println("\nflat road, asphalt, 250 W:     {} ({:.1HalfEven})", flat->inMetresPerSecond, flat->inKilometresPerHour);

    auto const climb = ride_on(RoadSurface::Asphalt, riding(250, 30));
    if (!climb)
    {
        std::println("3 % climb at 250 W: {}", formula::describe(climb.error()));
        return 1;
    }
    std::println("3 % climb, asphalt, 250 W:     {} ({:.1HalfEven})", climb->inMetresPerSecond, climb->inKilometresPerHour);

    auto const cobbled = ride_on(RoadSurface::Cobbles, riding(250, 0));
    if (!cobbled)
    {
        std::println("flat cobbles at 250 W: {}", formula::describe(cobbled.error()));
        return 1;
    }
    std::println(
        "flat road, cobbles, 250 W:     {} ({:.1HalfEven})", cobbled->inMetresPerSecond, cobbled->inKilometresPerHour);

    // An absent speed compares below every number, so it passes none of these.
    auto const flatKmh = formula::number_of(flat->inKilometresPerHour);
    auto const climbKmh = formula::number_of(climb->inKilometresPerHour);
    auto const cobbledKmh = formula::number_of(cobbled->inKilometresPerHour);
    check("about 37 km/h on the flat", flatKmh > 36.5_r && flatKmh < 38_r);
    check("about 24 km/h up a 3 % climb", climbKmh > 23.5_r && climbKmh < 25_r);
    check("slower on cobbles than on asphalt", cobbledKmh > 0_r && cobbledKmh < flatKmh);

    // ---- 4. A steep descent, freewheeling ----
    //
    // At no power on an 8 % descent, gravity outweighs rolling resistance and
    // F is negative, so a^2 + b^3 is too. The cubic then has three real roots,
    // and Cardano's formula reaches them only through complex numbers: in real
    // arithmetic, its square root has no answer. The formula says so, rather
    // than answering a wrong speed. (The speed freewheeling settles at is one
    // of those roots, sqrt(-F / k); this formula is not the one for it.)
    auto const descent = ride_on(RoadSurface::Asphalt, riding(0, -80));
    std::println("8 % descent, asphalt, 0 W:     {}",
                 descent.has_value() ? "a speed, where the square root has no real answer"
                                     : formula::describe(descent.error()));
    check("Cardano's square root has no real answer: a DomainError",
          !descent.has_value() && descent.error() == formula::ArithmeticError::DomainError);

    std::println("\nall checks passed: {}", ok ? "yes" : "no");
    return ok ? 0 : 1;
}
