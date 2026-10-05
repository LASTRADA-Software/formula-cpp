// SPDX-License-Identifier: Apache-2.0
//
// The overflow census: how many of the 127 bits of `Rational`'s 128-bit
// numerator and denominator real formulas use. Built as its own program, with
// FORMULA_OVERFLOW_CENSUS defined, so that every integer the library's
// arithmetic forms at run time is told to the tally (`census_tally.hpp`).
//
// docs/numeric-headroom.md's tables are what this program prints on its
// `@census:<table>:` lines, never typed: cmake/CheckCensusPage.cmake
// rebuilds them, with the census twins' lines, and `docs.numeric-headroom`
// fails when the page differs.
//
// Every evaluation here runs at run time, on purpose: a constant evaluation
// tells the census nothing.
#include "census_tally.hpp"

#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
namespace unit = formula::unit;
using formula::Rational;
using formula::detail::CensusRole;

[[nodiscard]] Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return Rational { numerator, denominator };
}

/// What one evaluation used: each role's largest magnitude, in bits, and the
/// headroom left of 127 by the largest signed one.
struct Used
{
    int numeratorBits;
    int denominatorBits;
    int intermediateBits;
    int unsignedBits;
    /// The most bits a wide intermediate used (`CensusRole::Wide`), 0 when none was formed.
    int wideBits;

    [[nodiscard]] int headroom() const noexcept
    {
        return 127 - std::min(127, std::max({ numeratorBits, denominatorBits, intermediateBits }));
    }
};

/// Runs @p evaluation with the tally reset, and reads what it used.
template <typename Evaluation>
[[nodiscard]] Used census_of(Evaluation&& evaluation)
{
    formula_census::reset();
    std::forward<Evaluation>(evaluation)();
    return Used { formula_census::bits_used(CensusRole::Numerator),
                  formula_census::bits_used(CensusRole::Denominator),
                  formula_census::bits_used(CensusRole::Intermediate),
                  formula_census::bits_used(CensusRole::Unsigned),
                  formula_census::bits_used(CensusRole::Wide) };
}

/// Prints one line of the page's table @p table, for
/// cmake/CheckCensusPage.cmake to collect.
void emit(char const* table, std::string const& line)
{
    std::println("@census:{}:{}", table, line);
}

/// Prints one row of the statistics table, in the page's shape.
void print_row(char const* label, Used const& used)
{
    emit("statistics",
         "| " + std::string { label } + " | " + std::to_string(used.numeratorBits) + " | "
             + std::to_string(used.denominatorBits) + " | " + std::to_string(used.intermediateBits) + " | "
             + std::to_string(used.unsignedBits) + " | " + std::to_string(used.headroom()) + " |");
}

/// A deterministic source of invented determinations: splitmix64, seeded.
class Draws
{
  public:
    explicit Draws(std::uint64_t seed):
        _state { seed }
    {
    }

    /// A value in [low, high], as an integer count of 10^-places.
    [[nodiscard]] Rational between(std::int64_t low, std::int64_t high, int places)
    {
        std::int64_t scale = 1;
        for (int step = 0; step < places; ++step)
            scale *= 10;
        auto const span = static_cast<std::uint64_t>((high - low) * scale) + 1U;
        auto const drawn = static_cast<std::int64_t>(next() % span);
        return rat(low * scale + drawn, scale);
    }

  private:
    [[nodiscard]] std::uint64_t next() noexcept
    {
        _state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t mixed = _state;
        mixed = (mixed ^ (mixed >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        mixed = (mixed ^ (mixed >> 27U)) * 0x94D049BB133111EBULL;
        return mixed ^ (mixed >> 31U);
    }

    std::uint64_t _state;
};

// ---- Quantities, invented ------------------------------------------------------

struct Mass: formula::Quantity<Mass, "m", "mass of a determination", unit::Gram>
{
};
struct Spread: formula::Quantity<Spread, "s", "spread of the determinations", unit::Gram>
{
};
struct Determinations: formula::Quantity<Determinations, "n", "number of determinations", unit::One>
{
};
inline constexpr formula::Unit GramSquared { .dimension = formula::dim::Mass * formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1'000'000,
                                             .symbolText = formula::symbol("g2"),
                                             .decimals = 4 };
struct MassVariance: formula::Quantity<MassVariance, "s2", "variance of the determinations", GramSquared>
{
};
struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", unit::Gram>
{
};
struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", unit::Percent>
{
};
struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", unit::Millimetre>
{
};
struct Area: formula::Quantity<Area, "A", "cross-sectional area", unit::SquareMetre>
{
};

struct FailureLoad: formula::Quantity<FailureLoad, "F", "maximum load at failure", unit::Newton>
{
};
struct Strength: formula::Quantity<Strength, "f", "compressive strength", unit::Megapascal>
{
};

/// A cylinder's compressive strength, as the methods example's cylinder
/// variant states it.
inline constexpr auto cylinderStrength = formula::constant<unit::One>(Rational { 4 }) * formula::var<FailureLoad>
                                         / (formula::pi * formula::pow<2>(formula::var<Diameter>));

/// examples/expressions.cpp's circular area, which that example writes as
/// `yields<Area>(formula::pi * formula::pow<2>(var<Diameter>) / 4)`: the same
/// tree, since its bare `4` is the dimensionless coefficient `Rational { 4 }`
/// spelled here.
inline constexpr auto circularArea = formula::pi * formula::pow<2>(formula::var<Diameter>) / formula::Rational { 4 };

template <typename Q, std::size_t N, std::size_t... At>
[[nodiscard]] auto measured_of(std::array<Rational, N> const& values, std::index_sequence<At...>)
{
    return formula::measured_series<Q>(formula::Measured<Q> { values[At] }...);
}

/// @p values as a series environment of @p Q.
template <typename Q, std::size_t N>
[[nodiscard]] auto series_environment(std::array<Rational, N> const& values)
{
    return formula::environment(measured_of<Q>(values, std::make_index_sequence<N> {}));
}

inline constexpr formula::Verdict repeatTest { "discard the determinations and repeat the test" };

template <std::size_t N, typename Criterion>
[[nodiscard]] constexpr auto rejection_of(Criterion criterion)
{
    return formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<2>, formula::KeepAtLeast<3>>(
            formula::series<Mass, N>, criterion, repeatTest);
}

inline constexpr auto sixPercent = formula::deviation_from_mean(Rational { 6, 100 } * formula::pass_mean<Mass>);
inline constexpr auto sevenQuarters = formula::deviation_in_stddevs(formula::number(Rational { 7, 4 }));

// The statistics fixtures (rejection_tests.cpp's shared fixtures), in grams.
std::array<Rational, 6> const fixtureA { rat(402, 10), rat(398, 10), rat(405, 10), rat(44), rat(40), rat(433, 10) };
std::array<Rational, 6> const fixtureB { rat(402, 10), rat(398, 10), rat(405, 10), rat(452, 10), rat(40), rat(372, 10) };
std::array<Rational, 5> const fixtureC { rat(40), rat(40), rat(44), rat(40), rat(36) };
std::array<Rational, 5> const fixtureD { rat(40), rat(40), rat(40), rat(40), rat(425, 10) };
std::array<Rational, 5> const fixtureE { rat(40), rat(40), rat(40), rat(40), rat(40) };
std::array<Rational, 3> const fixtureF { rat(1), rat(25, 10), rat(4) };

/// The mean, variance and range of @p values, and whether all three are
/// values.
template <std::size_t N>
[[nodiscard]] bool dispersion_of(std::array<Rational, N> const& values)
{
    auto const inputs = series_environment<Mass>(values);
    auto const mean = formula::checked_evaluate<Mass>(formula::sample_mean(formula::series<Mass, N>), inputs);
    auto const variance =
        formula::checked_evaluate<MassVariance>(formula::sample_variance(formula::series<Mass, N>), inputs);
    auto const range = formula::checked_evaluate<Spread>(formula::sample_range(formula::series<Mass, N>), inputs);
    return mean.has_value() && variance.has_value() && range.has_value();
}

/// @p values' spread reported exactly at @p Places decimal places of g.
template <int Places, std::size_t N>
[[nodiscard]] bool spread_of(std::array<Rational, N> const& values)
{
    auto const spread =
        formula::rounded_sqrt<unit::Gram, formula::DecimalPlaces { Places }, formula::RoundingMode::HalfAwayFromZero>(
            formula::sample_variance(formula::series<Mass, N>));
    return formula::checked_evaluate<Spread>(spread, series_environment<Mass>(values)).has_value();
}

// ---- The norm-shaped cases, numbers invented ------------------------------------

// Twenty masses at 3 decimal places of g, near 40 g.
std::array<Rational, 20> const twentyMasses {
    rat(40217, 1000), rat(39883, 1000), rat(40061, 1000), rat(40349, 1000), rat(39707, 1000),
    rat(40113, 1000), rat(39951, 1000), rat(40287, 1000), rat(39829, 1000), rat(40193, 1000),
    rat(40031, 1000), rat(39769, 1000), rat(40401, 1000), rat(39917, 1000), rat(40157, 1000),
    rat(39853, 1000), rat(40239, 1000), rat(39991, 1000), rat(40073, 1000), rat(39811, 1000),
};

// Six masses at 6 decimal places of g -- microgram
// resolution. Their variance overflowed 64 bits.
std::array<Rational, 6> const sixAtMicrograms { rat(40053270, 1000000), rat(39475922, 1000000), rat(39025798, 1000000),
                                                rat(40615904, 1000000), rat(39418416, 1000000), rat(40131659, 1000000) };

// Screen openings for the 64-point grading curve, in millimetres: invented,
// strictly increasing, unevenly spaced, three significant digits each, and
// none a Renard R40 value or a sieve size (the three-digit primes from 101).
inline constexpr std::array<std::int64_t, 64> openingPrimes {
    101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211,
    223, 227, 229, 233, 239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317, 331, 337, 347,
    349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419, 421, 431, 433, 439, 443, 449, 457, 461,
};

template <std::size_t... At>
[[nodiscard]] constexpr formula::BreakpointTable<64> openings_of(std::index_sequence<At...>)
{
    return formula::BreakpointTable<64> { formula::breakpoint(openingPrimes[At])... };
}

inline constexpr formula::BreakpointTable<64> openings = openings_of(std::make_index_sequence<64> {});

/// The mass retained on each of the 64 screens, at 3 decimal places of g:
/// drawn, from a fixed seed, between 1 and 99 g.
[[nodiscard]] std::array<Rational, 64> retained_on_each()
{
    Draws draws { 64 };
    std::array<Rational, 64> retained {};
    for (Rational& onScreen: retained)
        onScreen = draws.between(1, 99, 3);
    return retained;
}

/// The percentage passing each screen, from the cumulative retained, and one
/// reading between two screens.
[[nodiscard]] bool grading_curve_read()
{
    auto const inputs = series_environment<Retained>(retained_on_each());
    constexpr auto retained = formula::series<Retained, 64>;
    constexpr auto passing =
        formula::constant<unit::Percent>(Rational { 100 })
        - formula::cumulative<formula::CumulativeDirection::FromLast>(retained) / formula::sum(retained);
    constexpr auto grading = formula::curve(formula::domain<unit::Millimetre, openings>, passing);
    // 177 mm, between the 173 and 179 mm screens: three significant digits,
    // and no R40 value.
    auto const read = formula::checked_evaluate<Passing>(
        formula::interpolate_at(grading, formula::constant<unit::Millimetre>(Rational { 177 })), inputs);
    return read.has_value() && !read->measurement().is_absent();
}

/// Across @p samples six-element samples near 40 g at @p Places decimal
/// places: how many @p evaluate refused with Overflow, and the least headroom
/// any other left.
struct Survey
{
    int overflowed;
    int leastHeadroom;
};

template <int Places, typename Evaluate>
[[nodiscard]] Survey survey(int samples, Evaluate&& evaluate)
{
    Draws draws { 20260926 };
    Survey found { 0, 127 };
    for (int drawn = 0; drawn < samples; ++drawn)
    {
        std::array<Rational, 6> sample {};
        for (Rational& determination: sample)
            determination = draws.between(39, 41, Places);
        bool overflowed = false;
        Used const used = census_of([&] { overflowed = !evaluate(series_environment<Mass>(sample)); });
        if (overflowed)
            ++found.overflowed;
        else
            found.leastHeadroom = std::min(found.leastHeadroom, used.headroom());
    }
    return found;
}

// ---- Least squares: three data shapes ---------------------------------------------

// Point k of each shape, in coherent SI -- seconds and newtons -- so the fit
// sees exactly these numbers. Invented.
struct FitPoint
{
    Rational x;
    Rational y;
};

/// One decimal place: x = (13k + 7)/10 s, y = (29k + 3(k mod 5) + 101)/10 N.
[[nodiscard]] FitPoint one_decimal_point(std::int64_t k)
{
    return { rat(13 * k + 7, 10), rat(29 * k + 3 * (k % 5) + 101, 10) };
}

/// Three decimal places at a magnitude of thousands, a load cell's reading:
/// x = k + 0.241 s + (37k mod 1000) ms, y = 2410 N + (3217k + (7919k mod 997))
/// mN.
[[nodiscard]] FitPoint three_decimals_point(std::int64_t k)
{
    return { rat(1000 * k + 241 + (k * 37) % 1000, 1000), rat(2'410'000 + 3217 * k + (k * 7919) % 997, 1000) };
}

/// A different denominator on every point, the shape that overflows:
/// ((k + 1)/(k + 2) s, (2k + 3)/(k + 3) N).
[[nodiscard]] FitPoint distinct_denominators_point(std::int64_t k)
{
    return { rat(k + 1, k + 2), rat(2 * k + 3, k + 3) };
}

/// The fit's two coefficients over the first @p count points of @p shape,
/// through `LinearLeastSquares::compute`, and whether it overflowed.
template <typename Shape>
[[nodiscard]] bool fit_overflows(Shape shape, std::size_t count)
{
    std::vector<Rational> times;
    std::vector<Rational> forces;
    for (std::size_t at = 0; at < count; ++at)
    {
        FitPoint const point = shape(static_cast<std::int64_t>(at));
        times.push_back(point.x);
        forces.push_back(point.y);
    }
    auto const fitted = formula::LinearLeastSquares::compute<Rational>(std::span<Rational const> { times },
                                                                       std::span<Rational const> { forces });
    return !fitted.has_value() && fitted.error() == formula::ArithmeticError::Overflow;
}

/// Over every size from 2 to 128 points: which overflowed, and the least
/// headroom any other left.
struct FitScan
{
    std::vector<std::size_t> overflowing;
    int leastHeadroom = 127;
    /// The most bits a wide intermediate of the fit used, over the sizes that answered; empty for a route
    /// that computes in `Rational` and forms none.
    std::optional<int> widestWide;

    [[nodiscard]] std::string row(char const* label) const
    {
        return "| " + std::string { label } + " | " + std::to_string(overflowing.size()) + " of 127 | "
               + (overflowing.empty() ? std::string { "none" } : std::to_string(overflowing.front()) + " points") + " | "
               + std::to_string(leastHeadroom) + " | "
               + (widestWide.has_value() ? std::to_string(*widestWide) : std::string { "--" }) + " |";
    }
};

template <typename Shape>
[[nodiscard]] FitScan scan_fit(Shape shape)
{
    FitScan found;
    for (std::size_t count = 2; count <= 128; ++count)
    {
        bool overflowed = false;
        Used const used = census_of([&] { overflowed = fit_overflows(shape, count); });
        if (overflowed)
            found.overflowing.push_back(count);
        else
            found.leastHeadroom = std::min(found.leastHeadroom, used.headroom());
    }
    return found;
}

struct FitTime: formula::Quantity<FitTime, "t", "time of a reading", unit::Second>
{
};
struct FitForce: formula::Quantity<FitForce, "F_r", "force read", unit::Newton>
{
};
struct FitLength: formula::Quantity<FitLength, "L", "length read", unit::Millimetre>
{
};

/// The slope of the first @p N points of @p shape through the node --
/// `linear_least_squares` over a curve, as a formula states it -- and whether
/// it overflowed.
template <std::size_t N, typename Shape>
[[nodiscard]] bool fit_node_overflows(Shape shape)
{
    std::array<formula::Measured<FitTime>, N> times;
    std::array<formula::Measured<FitForce>, N> forces;
    for (std::size_t at = 0; at < N; ++at)
    {
        FitPoint const point = shape(static_cast<std::int64_t>(at));
        times[at] = formula::Measured<FitTime> { point.x };
        forces[at] = formula::Measured<FitForce> { point.y };
    }
    auto const inputs =
        formula::environment(formula::MeasuredSeries<FitTime, N> { times }, formula::MeasuredSeries<FitForce, N> { forces });
    constexpr auto fit = formula::linear_least_squares(
        formula::curve(formula::series<FitTime, N>, formula::series<FitForce, N>), { .reference = "Example Standard 12" });
    auto const slope = formula::checked_evaluate_si<Rational>(formula::opaque_output<"slope">(fit), inputs);
    return !slope.has_value() && slope.error() == formula::ArithmeticError::Overflow;
}

/// The coherent unit of a force over a time, N/s: the rounded fit's unit here.
inline constexpr formula::Unit newtonPerSecond { .dimension = formula::dim::Force / formula::dim::Time,
                                                 .symbolText = formula::symbol("N/s"),
                                                 .decimals = 4 };

/// The slope over the first @p count points of @p shape, rounded to 4 dp of
/// N/s the way `rounded_output` rounds it -- `LinearLeastSquares::compute_exact`,
/// then `detail::rounded_in_unit` -- and whether it overflowed.
template <typename Shape>
[[nodiscard]] bool rounded_fit_overflows(Shape shape, std::size_t count)
{
    std::vector<Rational> times;
    std::vector<Rational> forces;
    for (std::size_t at = 0; at < count; ++at)
    {
        FitPoint const point = shape(static_cast<std::int64_t>(at));
        times.push_back(point.x);
        forces.push_back(point.y);
    }
    auto const fitted = formula::LinearLeastSquares::compute_exact(std::span<Rational const> { times },
                                                                   std::span<Rational const> { forces });
    if (!fitted.has_value())
        return fitted.error() == formula::ArithmeticError::Overflow;
    auto const slope = formula::detail::rounded_in_unit(
        (*fitted)[1], newtonPerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven);
    return !slope.has_value() && slope.error() == formula::ArithmeticError::Overflow;
}

/// `scan_fit` for the rounded route.
template <typename Shape>
[[nodiscard]] FitScan scan_rounded_fit(Shape shape)
{
    FitScan found;
    for (std::size_t count = 2; count <= 128; ++count)
    {
        bool overflowed = false;
        Used const used = census_of([&] { overflowed = rounded_fit_overflows(shape, count); });
        if (overflowed)
            found.overflowing.push_back(count);
        else
        {
            found.leastHeadroom = std::min(found.leastHeadroom, used.headroom());
            found.widestWide = std::max(found.widestWide.value_or(0), used.wideBits);
        }
    }
    return found;
}

/// The first @p N points of @p shape through the node --
/// `rounded_output<"slope", N/s, 4 dp>` of `linear_least_squares` over a curve
/// -- and whether it overflowed.
template <std::size_t N, typename Shape>
[[nodiscard]] bool rounded_fit_node_overflows(Shape shape)
{
    std::array<formula::Measured<FitTime>, N> times;
    std::array<formula::Measured<FitForce>, N> forces;
    for (std::size_t at = 0; at < N; ++at)
    {
        FitPoint const point = shape(static_cast<std::int64_t>(at));
        times[at] = formula::Measured<FitTime> { point.x };
        forces[at] = formula::Measured<FitForce> { point.y };
    }
    auto const inputs =
        formula::environment(formula::MeasuredSeries<FitTime, N> { times }, formula::MeasuredSeries<FitForce, N> { forces });
    constexpr auto fit = formula::linear_least_squares(
        formula::curve(formula::series<FitTime, N>, formula::series<FitForce, N>), { .reference = "Example Standard 12" });
    constexpr auto roundedSlope =
        formula::rounded_output<"slope", newtonPerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
            fit);
    auto const slope = formula::checked_evaluate_si<Rational>(roundedSlope, inputs);
    return !slope.has_value() && slope.error() == formula::ArithmeticError::Overflow;
}

/// The variance of a six-element sample, and whether it is a value.
[[nodiscard]] bool variance_is_value(auto const& inputs)
{
    return formula::checked_evaluate<MassVariance>(formula::sample_variance(formula::series<Mass, 6>), inputs).has_value();
}

/// A rejection of a six-element sample by @p criterion, and whether it did
/// not overflow.
template <typename Criterion>
[[nodiscard]] bool rejection_is_not_overflow(Criterion criterion, auto const& inputs)
{
    auto const outcome = formula::checked_evaluate_rejection<Mass>(rejection_of<6>(criterion), inputs);
    return outcome.has_value() || outcome.error().error != formula::ArithmeticError::Overflow;
}

// ---- Regression over observations: where each route stops -------------------------

// Declared here: the library states no unit of millimetres per second.
inline constexpr formula::Unit millimetrePerSecond { .dimension = formula::dim::Velocity,
                                                     .magnitudeNumerator = 1,
                                                     .magnitudeDenominator = 1000,
                                                     .symbolText = formula::symbol("mm/s"),
                                                     .decimals = 4 };

/// Four decimal places, the guide's fifty readings extended: x = k + 1 + (7919k
/// mod 997) / 10^4 s, y = 2410 + 3.17 k + ((3217 k mod 1009) - 504) / 10^4 mm.
[[nodiscard]] FitPoint four_decimals_point(std::int64_t k)
{
    return { rat(10'000 * (k + 1) + (7919 * k) % 997, 10'000),
             rat(24'100'000 + 31'700 * k + (3217 * k) % 1009 - 504, 10'000) };
}

/// Whether the line through the first @p count points of @p shape overflows
/// through `opaque_output` (the slope, exactly) and through `rounded_output`
/// (the slope at 4 dp of @p SlopeUnit, R^2 floored at 6 dp), with the values
/// read as observations of @p Y.
template <typename Y, formula::Unit SlopeUnit, typename Shape>
[[nodiscard]] std::pair<bool, bool> observed_line_overflows(Shape shape, std::size_t count)
{
    std::vector<Rational> times;
    std::vector<Rational> readings;
    for (std::size_t at = 0; at < count; ++at)
    {
        FitPoint const point = shape(static_cast<std::int64_t>(at));
        times.push_back(point.x);
        readings.push_back(point.y);
    }
    auto const inputs = formula::environment(*formula::MeasuredObservations<FitTime, 128>::from(times),
                                             *formula::MeasuredObservations<Y, 128>::from(readings));
    constexpr auto fit = formula::linear_least_squares(
        formula::observations<FitTime, 128>, formula::observations<Y, 128>, { .reference = "Example Standard 12" });
    // A size answers or is Overflow: any other error would read as a size that
    // did not overflow.
    auto const overflowed = [count](auto const& evaluated) {
        INFO("points " << count);
        CHECK((evaluated.has_value() || evaluated.error() == formula::ArithmeticError::Overflow));
        return !evaluated.has_value() && evaluated.error() == formula::ArithmeticError::Overflow;
    };
    bool const exact = overflowed(formula::checked_evaluate_si<Rational>(formula::opaque_output<"slope">(fit), inputs));
    bool const rounded =
        overflowed(formula::checked_evaluate_si<Rational>(
            formula::rounded_output<"slope", SlopeUnit, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(fit),
            inputs))
        || overflowed(formula::checked_evaluate_si<Rational>(
            formula::
                rounded_output<"r squared", formula::unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::Floor>(
                    fit),
            inputs));
    return { exact, rounded };
}

/// Over every size from @p from to 128: which overflowed on each route.
struct RouteScan
{
    std::size_t sizes = 0;
    std::vector<std::size_t> exactOverflowing;
    std::vector<std::size_t> roundedOverflowing;

    [[nodiscard]] std::string row(char const* label) const
    {
        auto const first = [](std::vector<std::size_t> const& overflowing) {
            return overflowing.empty() ? std::string { "none" } : std::to_string(overflowing.front()) + " points";
        };
        return "| " + std::string { label } + " | " + std::to_string(exactOverflowing.size()) + " of "
               + std::to_string(sizes) + " | " + first(exactOverflowing) + " | " + std::to_string(roundedOverflowing.size())
               + " of " + std::to_string(sizes) + " | " + first(roundedOverflowing) + " |";
    }
};

template <typename Overflows>
[[nodiscard]] RouteScan scan_routes(std::size_t from, Overflows overflows)
{
    RouteScan found;
    for (std::size_t count = from; count <= 128; ++count)
    {
        ++found.sizes;
        auto const [exact, rounded] = overflows(count);
        if (exact)
            found.exactOverflowing.push_back(count);
        if (rounded)
            found.roundedOverflowing.push_back(count);
    }
    return found;
}

struct FitWarmth: formula::Quantity<FitWarmth, "T_r", "temperature of a reading", unit::Celsius>
{
};

/// A temperature at 1 dp in degrees Celsius: 20.0 + (7k mod 13) / 10.
[[nodiscard]] Rational warmth_at(std::int64_t k)
{
    return rat(200 + (7 * k) % 13, 10);
}

/// How the regression of the first @p count three-decimal loads on their times
/// and a temperature came out through `opaque_output` and through
/// `rounded_output` (coefficient 1 at 4 dp of N/s, R^2 floored at 6 dp).
struct TwoRegressorRoutes
{
    /// The exact route's answer: coefficient 1's, or the error.
    std::optional<formula::ArithmeticError> exactError;
    /// The rounded route's: coefficient 1's, then R^2's.
    std::optional<formula::ArithmeticError> roundedError;
};

[[nodiscard]] TwoRegressorRoutes two_regressors_routes(std::size_t count)
{
    std::vector<Rational> times;
    std::vector<Rational> warmths;
    std::vector<Rational> forces;
    for (std::size_t at = 0; at < count; ++at)
    {
        FitPoint const point = three_decimals_point(static_cast<std::int64_t>(at));
        times.push_back(point.x);
        warmths.push_back(warmth_at(static_cast<std::int64_t>(at)));
        forces.push_back(point.y);
    }
    auto const inputs = formula::environment(*formula::MeasuredObservations<FitTime, 128>::from(times),
                                             *formula::MeasuredObservations<FitWarmth, 128>::from(warmths),
                                             *formula::MeasuredObservations<FitForce, 128>::from(forces));
    constexpr auto fit = formula::multiple_least_squares(
        formula::regressors(formula::observations<FitTime, 128>, formula::observations<FitWarmth, 128>),
        formula::observations<FitForce, 128>,
        { .reference = "Example Standard 12" });
    auto const failure = [](auto const& evaluated) -> std::optional<formula::ArithmeticError> {
        if (evaluated.has_value())
            return std::nullopt;
        return evaluated.error();
    };
    TwoRegressorRoutes routes;
    routes.exactError =
        failure(formula::checked_evaluate_si<Rational>(formula::opaque_output<"coefficient 1">(fit), inputs));
    routes.roundedError = failure(formula::checked_evaluate_si<Rational>(
        formula::
            rounded_output<"coefficient 1", newtonPerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
                fit),
        inputs));
    if (!routes.roundedError.has_value())
        routes.roundedError = failure(formula::checked_evaluate_si<Rational>(
            formula::
                rounded_output<"r squared", formula::unit::One, formula::DecimalPlaces { 6 }, formula::RoundingMode::Floor>(
                    fit),
            inputs));
    return routes;
}
} // namespace

// ---- The instrument's own control -------------------------------------------------

TEST_CASE("the census reports 0 bits of headroom for the largest Int128, and Overflow one step further", "[census]")
{
    constexpr formula::Int128 largest = std::numeric_limits<formula::Int128>::max();
    // (2^126 - 1) + 2^126 = 2^127 - 1 from two 126- and 127-bit operands,
    // built outside the count: the sum's 127 bits are the addition's own
    // intermediate, which only add_checked_or_none's hook reports.
    Rational const lowHalf { (formula::Int128 { 1 } << 126) - 1 };
    Rational const highHalf { formula::Int128 { 1 } << 126 };
    Used const atTheLimit =
        census_of([&] { REQUIRE(formula::checked_add(lowHalf, highHalf).value() == Rational { largest }); });
    CHECK(atTheLimit.headroom() == 0);
    CHECK(atTheLimit.intermediateBits == 127);
    // 2^63 * 2^62 = 2^125, one bit short of using all 127: the product is
    // mul_checked_or_none's intermediate, from operands of 64 and 63 bits.
    Rational const factorA { formula::Int128 { 1 } << 63 };
    Rational const factorB { formula::Int128 { 1 } << 62 };
    Used const oneShort = census_of(
        [&] { REQUIRE(formula::checked_mul(factorA, factorB).value() == Rational { formula::Int128 { 1 } << 125 }); });
    CHECK(oneShort.headroom() == 1);
    CHECK(oneShort.intermediateBits == 126);
    // One step further is the library's Overflow, never a figure: a product
    // that overflows leaves the count with its operands' 65 bits at most.
    CHECK(formula::checked_add(Rational { largest }, rat(1)).error() == formula::ArithmeticError::Overflow);
    Rational const tooWideA { formula::Int128 { 1 } << 64 };
    Rational const tooWideB { formula::Int128 { 1 } << 63 };
    Used const overflowed = census_of(
        [&] { REQUIRE(formula::checked_mul(tooWideA, tooWideB).error() == formula::ArithmeticError::Overflow); });
    CHECK(overflowed.intermediateBits <= 65);
    CHECK(overflowed.numeratorBits <= 65);
    // A constant evaluation tells the census nothing.
    Used const constant = census_of([] {
        constexpr auto added = formula::checked_add(Rational { 1 << 20 }, Rational { 1 << 20 });
        static_assert(added.has_value());
    });
    CHECK(constant.headroom() == 127);
}

// ---- The census set -----------------------------------------------------------------

TEST_CASE("census: statistics, outlier rejections and spreads over the six fixtures", "[census]")
{
    emit("statistics", "| formula | numerator bits | denominator bits | intermediate bits | unsigned bits | headroom |");
    emit("statistics", "|---|---|---|---|---|---|");
    print_row("fixture A: mean, variance, range", census_of([] { REQUIRE(dispersion_of(fixtureA)); }));
    print_row("fixture B: mean, variance, range", census_of([] { REQUIRE(dispersion_of(fixtureB)); }));
    print_row("fixture C: mean, variance, range", census_of([] { REQUIRE(dispersion_of(fixtureC)); }));
    print_row("fixture D: mean, variance, range", census_of([] { REQUIRE(dispersion_of(fixtureD)); }));
    print_row("fixture E: mean, variance, range", census_of([] { REQUIRE(dispersion_of(fixtureE)); }));
    print_row("fixture F: mean, variance, range", census_of([] { REQUIRE(dispersion_of(fixtureF)); }));
    print_row("fixture A: rejection, 6 % of the mean", census_of([] {
                  REQUIRE(formula::checked_evaluate_rejection<Mass>(rejection_of<6>(sixPercent),
                                                                    series_environment<Mass>(fixtureA))
                              .has_value());
              }));
    print_row("fixture B: rejection, 7/4 standard deviations", census_of([] {
                  REQUIRE(formula::checked_evaluate_rejection<Mass>(rejection_of<6>(sevenQuarters),
                                                                    series_environment<Mass>(fixtureB))
                              .has_value());
              }));
    print_row("fixture B: rejection, gap to range 9/20", census_of([] {
                  REQUIRE(formula::checked_evaluate_rejection<Mass>(
                              rejection_of<6>(formula::gap_to_range(formula::number(Rational { 9, 20 }))),
                              series_environment<Mass>(fixtureB))
                              .has_value());
              }));
    print_row("fixture A: spread at 2 dp", census_of([] { REQUIRE(spread_of<2>(fixtureA)); }));
    print_row("fixture A: spread at 3 dp", census_of([] { REQUIRE(spread_of<3>(fixtureA)); }));
    print_row("fixture A: spread at 4 dp", census_of([] { REQUIRE(spread_of<4>(fixtureA)); }));
    print_row("fixture A: spread at 6 dp", census_of([] { REQUIRE(spread_of<6>(fixtureA)); }));
    print_row("fixture F: exact root at 0 dp", census_of([] { REQUIRE(spread_of<0>(fixtureF)); }));
}

TEST_CASE("census: cumulative sums and interpolation", "[census]")
{
    std::array<Rational, 5> const screens { rat(130), rat(210), rat(95), rat(340), rat(28) };
    print_row("passing from the cumulative retained, 5 screens", census_of([&] {
                  constexpr auto passing =
                      formula::constant<unit::Percent>(Rational { 100 })
                      - formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<Retained, 5>)
                            / formula::sum(formula::series<Retained, 5>);
                  REQUIRE(
                      formula::checked_evaluate_series<Passing>(passing, series_environment<Retained>(screens)).has_value());
              }));
    std::array<Rational, 5> const passingAt { rat(894, 25), rat(1154, 25), rat(1574, 25), rat(1764, 25), rat(2444, 25) };
    print_row("interpolation along a 5-point grading curve", census_of([&] {
                  constexpr formula::BreakpointTable<5> fivePrimes { formula::breakpoint(11),
                                                                     formula::breakpoint(29),
                                                                     formula::breakpoint(41),
                                                                     formula::breakpoint(59),
                                                                     formula::breakpoint(83) };
                  constexpr auto grading =
                      formula::curve(formula::domain<unit::Metre, fivePrimes>, formula::series<Passing, 5>);
                  REQUIRE(formula::checked_evaluate<Passing>(
                              formula::interpolate_at(grading, formula::constant<unit::Metre>(Rational { 47 })),
                              series_environment<Passing>(passingAt))
                              .has_value());
              }));
}

TEST_CASE("census: the norm-shaped cases", "[census]")
{
    Used const twenty = census_of([] { REQUIRE(dispersion_of(twentyMasses)); });
    print_row("20 masses at 3 dp: mean, variance, range", twenty);
    Used const curve = census_of([] { REQUIRE(grading_curve_read()); });
    print_row("64-point grading curve: cumulative percentages, one reading", curve);
    Used const spreadAtThree = census_of([] { REQUIRE(spread_of<3>(twentyMasses)); });
    print_row("20 masses at 3 dp: spread at 3 dp", spreadAtThree);

    // The named realistic case: six masses at micrograms, which overflowed 64
    // bits in kg^2, now answer exactly.
    auto const named = formula::checked_evaluate<MassVariance>(formula::sample_variance(formula::series<Mass, 6>),
                                                               series_environment<Mass>(sixAtMicrograms));
    REQUIRE(named.has_value());
    REQUIRE(named->is_value());
    CHECK(named->measurement().value() == Rational { 2026588050217, 6000000000000 });
    auto const namedRejection =
        formula::checked_evaluate_rejection<Mass>(rejection_of<6>(sevenQuarters), series_environment<Mass>(sixAtMicrograms));
    CHECK(namedRejection.has_value());

    emit("resolution", "| formula | resolution | overflowed | least headroom |");
    emit("resolution", "|---|---|---|---|");
    for (auto const& [label, places, found]: {
             std::tuple { "variance", 4, survey<4>(1000, [](auto const& inputs) { return variance_is_value(inputs); }) },
             std::tuple { "variance", 5, survey<5>(1000, [](auto const& inputs) { return variance_is_value(inputs); }) },
             std::tuple { "variance", 6, survey<6>(1000, [](auto const& inputs) { return variance_is_value(inputs); }) },
             std::tuple {
                 "rejection by 7/4 standard deviations",
                 4,
                 survey<4>(1000, [](auto const& inputs) { return rejection_is_not_overflow(sevenQuarters, inputs); }) },
             std::tuple {
                 "rejection by 7/4 standard deviations",
                 5,
                 survey<5>(1000, [](auto const& inputs) { return rejection_is_not_overflow(sevenQuarters, inputs); }) },
             std::tuple {
                 "rejection by 7/4 standard deviations",
                 6,
                 survey<6>(1000, [](auto const& inputs) { return rejection_is_not_overflow(sevenQuarters, inputs); }) },
             std::tuple {
                 "rejection by 6 % of the mean",
                 4,
                 survey<4>(1000, [](auto const& inputs) { return rejection_is_not_overflow(sixPercent, inputs); }) },
             std::tuple {
                 "rejection by 6 % of the mean",
                 5,
                 survey<5>(1000, [](auto const& inputs) { return rejection_is_not_overflow(sixPercent, inputs); }) },
             std::tuple {
                 "rejection by 6 % of the mean",
                 6,
                 survey<6>(1000, [](auto const& inputs) { return rejection_is_not_overflow(sixPercent, inputs); }) },
         })
        emit("resolution",
             "| " + std::string { label } + " | " + std::to_string(places) + " dp | " + std::to_string(found.overflowed)
                 + " of 1000 | " + std::to_string(found.leastHeadroom) + " |");
}

// ---- Regression pins ------------------------------------------------------------------

TEST_CASE("the norm-shaped cases keep the headroom they were measured with, less 4 bits", "[census]")
{
    // Measured on cl 19.51 at the commit that stores `Rational` in 128 bits:
    // 82, 101 and 100 bits of headroom, and fixture A's 3 dp spread forming
    // 20-bit numerators and 26 of rounded_sqrt's 128 unsigned bits. A change
    // that quietly spends more fails here, not in a user's formula. Measured
    // when these pins were added: without checked_mul's cross-reduction the
    // 64-point curve uses 40 bits rather than 26, the 3 dp spread's
    // numerators grow to 29 bits, and the control below reads 42; with
    // checked_add scaling a sum by the product of the denominators rather
    // than their least common multiple, the twenty masses' variance failed
    // to evaluate at 64 bits; at 128 bits it evaluates, and the headroom
    // check below is what fails.
    Used const twenty = census_of([] { REQUIRE(dispersion_of(twentyMasses)); });
    CHECK(twenty.headroom() >= 82 - 4);
    Used const curve = census_of([] { REQUIRE(grading_curve_read()); });
    CHECK(curve.headroom() >= 101 - 4);
    Used const spreadAtThree = census_of([] { REQUIRE(spread_of<3>(fixtureA)); });
    CHECK(spreadAtThree.headroom() >= 100 - 4);
    CHECK(spreadAtThree.numeratorBits <= 20 + 4);
    CHECK(spreadAtThree.unsignedBits <= 26 + 4);
}

TEST_CASE("checked_mul's cross-reduction keeps a product's intermediates small (a stress control)", "[census]")
{
    // (2^40 / 3) * (3 / 2^20) is 2^20. Cross-reduced first, no intermediate
    // passes 21 bits; multiplied first, the numerator is 3 * 2^40, 42 bits.
    Used const product = census_of([] {
        REQUIRE(formula::checked_mul(rat(std::int64_t { 1 } << 40, 3), rat(3, std::int64_t { 1 } << 20)).value()
                == rat(std::int64_t { 1 } << 20));
    });
    CHECK(product.intermediateBits <= 21);
}

TEST_CASE("census: a cylinder's cross-section and its strength, for d from 101 to 163 mm", "[census]")
{
    // The library's pi is 245850922/78256779: 28 and 27 bits. The area,
    // pi * d^2 / 4, is examples/expressions.cpp's; the strength,
    // 4 * F / (pi * d^2) at F = 89.3 kN, is a cylinder variant's, as the
    // methods example states it. Each diameter is evaluated at run time; a
    // failure is re-done by hand in the evaluator's order to name the step
    // the arithmetic refused.
    struct Found
    {
        std::vector<std::int64_t> overflowing;
        std::vector<std::string> refusedAt;
        int leastHeadroom = 127;

        void add(std::int64_t millimetres, std::string const& step)
        {
            overflowing.push_back(millimetres);
            if (std::find(refusedAt.begin(), refusedAt.end(), step) == refusedAt.end())
                refusedAt.push_back(step);
        }

        [[nodiscard]] std::string row(char const* label) const
        {
            std::string diameters;
            for (std::int64_t const millimetres: overflowing)
                diameters += (diameters.empty() ? "" : ", ") + std::to_string(millimetres);
            std::string steps;
            for (std::string const& step: refusedAt)
                steps += (steps.empty() ? "" : ", ") + step;
            return "| " + std::string { label } + " | " + (diameters.empty() ? "none" : diameters + " mm") + " | "
                   + (steps.empty() ? "--" : steps) + " | " + std::to_string(leastHeadroom) + " |";
        }
    };
    Found area;
    Found strength;
    Rational const fourTimesLoad = rat(4 * 89'300);
    for (std::int64_t millimetres = 101; millimetres <= 163; ++millimetres)
    {
        auto const inputs = formula::environment(formula::Measured<Diameter> { rat(millimetres) },
                                                 formula::Measured<FailureLoad> { rat(89'300) });
        Rational const inMetres = rat(millimetres, 1000);
        auto const squared = formula::checked_mul(inMetres, inMetres);
        auto const timesPi = squared.has_value() ? formula::checked_mul(formula::Pi, *squared) : squared;
        std::string const productRefused = !squared.has_value() ? "d^2" : !timesPi.has_value() ? "pi * d^2" : "";

        bool areaFailed = false;
        Used const areaUsed =
            census_of([&] { areaFailed = !formula::checked_evaluate<Area>(circularArea, inputs).has_value(); });
        if (areaFailed)
            area.add(millimetres, productRefused.empty() ? std::string { "/ 4" } : productRefused);
        else
            area.leastHeadroom = std::min(area.leastHeadroom, areaUsed.headroom());

        bool strengthFailed = false;
        Used const strengthUsed =
            census_of([&] { strengthFailed = !formula::checked_evaluate<Strength>(cylinderStrength, inputs).has_value(); });
        if (strengthFailed)
        {
            auto const divided = timesPi.has_value() ? formula::checked_div(fourTimesLoad, *timesPi) : timesPi;
            strength.add(millimetres,
                         !productRefused.empty() ? productRefused
                         : !divided.has_value()  ? std::string { "4F / (pi * d^2)" }
                                                 : std::string { "the conversion to MPa" });
        }
        else
            strength.leastHeadroom = std::min(strength.leastHeadroom, strengthUsed.headroom());
    }
    emit("cylinder", "| formula | overflows at d = | refused at | least headroom otherwise |");
    emit("cylinder", "|---|---|---|---|");
    emit("cylinder", area.row("area, pi * d^2 / 4 (the expressions example)"));
    emit("cylinder", strength.row("strength, 4F / (pi * d^2), F = 89.3 kN, in MPa (the methods example's cylinder)"));
    // What the page says, pinned: neither overflows at any diameter, and each
    // keeps its measured headroom, 79 and 63 bits, less the census's usual 4.
    // At 139 mm, which overflowed 64 bits at the division, the strength is
    // exact.
    CHECK(area.overflowing.empty());
    CHECK(area.leastHeadroom >= 79 - 4);
    CHECK(strength.overflowing.empty());
    CHECK(strength.refusedAt.empty());
    CHECK(strength.leastHeadroom >= 63 - 4);
    auto const at139 = formula::checked_evaluate<Strength>(
        cylinderStrength,
        formula::environment(formula::Measured<Diameter> { rat(139) }, formula::Measured<FailureLoad> { rat(89'300) }));
    REQUIRE(at139.has_value());
    REQUIRE(at139->is_value());
    CHECK(at139->measurement().value() == Rational { 13976660729400, 2375042831981 });
}

TEST_CASE("census: least squares over 2 to 128 points", "[census]")
{
    // The three shapes, through the library's own fit: which sizes
    // overflow, and what the others leave. An overflowing fit is the library's
    // Overflow, never a line.
    // The least-squares tests' fixtures, through the node as a method states the fit: t = 1,
    // 2, 4, 7 s against L = 10.2, 10.9, 12.1, 14.3 mm, whose lengths are
    // converted to metres first; and five distinct denominators, well below
    // the twenty-eight that overflow.
    print_row(
        "least squares, the 4-point fixture: slope and intercept", census_of([] {
            auto const inputs =
                formula::environment(formula::measured_series<FitTime>(formula::Measured<FitTime> { rat(1) },
                                                                       formula::Measured<FitTime> { rat(2) },
                                                                       formula::Measured<FitTime> { rat(4) },
                                                                       formula::Measured<FitTime> { rat(7) }),
                                     formula::measured_series<FitLength>(formula::Measured<FitLength> { rat(102, 10) },
                                                                         formula::Measured<FitLength> { rat(109, 10) },
                                                                         formula::Measured<FitLength> { rat(121, 10) },
                                                                         formula::Measured<FitLength> { rat(143, 10) }));
            constexpr auto fit =
                formula::linear_least_squares(formula::curve(formula::series<FitTime, 4>, formula::series<FitLength, 4>),
                                              { .reference = "Example Standard 12" });
            auto const slope = formula::checked_evaluate_si<Rational>(formula::opaque_output<"slope">(fit), inputs);
            auto const intercept = formula::checked_evaluate_si<Rational>(formula::opaque_output<"intercept">(fit), inputs);
            REQUIRE(slope.has_value());
            REQUIRE(intercept.has_value());
            CHECK(**slope == rat(19, 28'000)); // 19/28 mm/s
        }));
    print_row("least squares, 5 points on distinct denominators (stress control)",
              census_of([] { REQUIRE(!fit_overflows(distinct_denominators_point, 5)); }));

    FitScan const oneDecimal = scan_fit(one_decimal_point);
    FitScan const threeDecimals = scan_fit(three_decimals_point);
    FitScan const distinct = scan_fit(distinct_denominators_point);
    FitScan const roundedThree = scan_rounded_fit(three_decimals_point);
    FitScan const roundedDistinct = scan_rounded_fit(distinct_denominators_point);
    std::string const wideHeader =
        "widest fit intermediate (of " + std::to_string(formula::LinearLeastSquares::exact_limbs * 32) + " bits)";
    emit("least-squares",
         "| data (invented) | sizes that overflow | first to overflow | least headroom otherwise | " + wideHeader + " |");
    emit("least-squares", "|---|---|---|---|---|");
    emit("least-squares", oneDecimal.row("readings at 1 dp (realistic)"));
    emit("least-squares", threeDecimals.row("readings at 3 dp near 2410 N, a load cell's (realistic)"));
    emit("least-squares", distinct.row("a different denominator on every point (stress control)"));
    emit("least-squares",
         roundedThree.row("the slope rounded to 4 dp by rounded_output: readings at 3 dp near 2410 N (realistic)"));
    emit("least-squares",
         roundedDistinct.row("the slope rounded to 4 dp by rounded_output: "
                             "a different denominator on every point (stress control)"));

    // What the page says, pinned: readings at one and three decimal places
    // never overflow, up to 128 points; a different denominator on every
    // point does from 28 points, at every size after. The node agrees with
    // the fit it calls on both sides of 28.
    CHECK(oneDecimal.overflowing.empty());
    CHECK(threeDecimals.overflowing.empty());
    REQUIRE(!distinct.overflowing.empty());
    CHECK(distinct.overflowing.front() == 28);
    CHECK(distinct.overflowing.size() == 101);
    CHECK(!fit_node_overflows<128>(three_decimals_point));
    CHECK(!fit_node_overflows<27>(distinct_denominators_point));
    CHECK(fit_node_overflows<28>(distinct_denominators_point));

    // The rounded route, measured with this algorithm: the realistic readings
    // never outgrow 256 bits; a different denominator on every point does,
    // in compute_exact, from 58 points. The node agrees on both sides of 58.
    CHECK(roundedThree.overflowing.empty());
    REQUIRE(!roundedDistinct.overflowing.empty());
    CHECK(roundedDistinct.overflowing.front() == 58);
    CHECK(roundedDistinct.overflowing.size() == 71);
    CHECK(!rounded_fit_node_overflows<128>(three_decimals_point));
    CHECK(!rounded_fit_node_overflows<57>(distinct_denominators_point));
    CHECK(rounded_fit_node_overflows<58>(distinct_denominators_point));
    // How close the exact fit came to its width, at the sizes that answered: readings at 3 dp use 68 of its
    // bits, a different denominator on every point 249. The Rational routes form no wide integer.
    CHECK(roundedThree.widestWide == 68);
    CHECK(roundedDistinct.widestWide == 249);
    CHECK_FALSE(oneDecimal.widestWide.has_value());
    CHECK(formula::LinearLeastSquares::exact_limbs * 32 == 256);
}

TEST_CASE("census: a line through observations, exact and rounded, over 2 to 128 points", "[census]")
{
    RouteScan const threeDecimals = scan_routes(2, [](std::size_t count) {
        return observed_line_overflows<FitForce, newtonPerSecond>(three_decimals_point, count);
    });
    RouteScan const fourDecimals = scan_routes(2, [](std::size_t count) {
        return observed_line_overflows<FitLength, millimetrePerSecond>(four_decimals_point, count);
    });
    RouteScan const distinct = scan_routes(2, [](std::size_t count) {
        return observed_line_overflows<FitForce, newtonPerSecond>(distinct_denominators_point, count);
    });
    emit("regression",
         "| data (invented) | exact route: sizes that overflow | first | rounded route: sizes that overflow | first |");
    emit("regression", "|---|---|---|---|---|");
    emit("regression", threeDecimals.row("a line through readings at 3 dp near 2410 N (realistic)"));
    emit("regression", fourDecimals.row("a line through readings at 4 dp near 2410 mm (realistic)"));
    emit("regression", distinct.row("a line through a different denominator on every point (stress control)"));

    // Two regressors, 3 to 128 rows. Every size must answer or be Overflow:
    // a design that were singular would be a DomainError, not an overflow, and
    // would read as a size that did not overflow.
    RouteScan twoRegressors;
    for (std::size_t count = 3; count <= 128; ++count)
    {
        TwoRegressorRoutes const routes = two_regressors_routes(count);
        INFO("rows " << count);
        CHECK((!routes.exactError.has_value() || *routes.exactError == formula::ArithmeticError::Overflow));
        CHECK((!routes.roundedError.has_value() || *routes.roundedError == formula::ArithmeticError::Overflow));
        ++twoRegressors.sizes;
        if (routes.exactError.has_value())
            twoRegressors.exactOverflowing.push_back(count);
        if (routes.roundedError.has_value())
            twoRegressors.roundedOverflowing.push_back(count);
    }
    emit("regression",
         twoRegressors.row("two regressors: readings at 3 dp and a temperature at 1 dp in degrees Celsius (realistic)"));

    // What the page says, pinned: the realistic rows stop on neither route;
    // a different denominator on every point stops the exact route at 22
    // points and the rounded one at 62.
    CHECK(threeDecimals.roundedOverflowing.empty());
    CHECK(fourDecimals.roundedOverflowing.empty());
    CHECK(threeDecimals.exactOverflowing.empty());
    CHECK(fourDecimals.exactOverflowing.empty());
    REQUIRE(!distinct.exactOverflowing.empty());
    CHECK(distinct.exactOverflowing.front() == 22);
    REQUIRE(!distinct.roundedOverflowing.empty());
    CHECK(distinct.roundedOverflowing.front() == 62);
    CHECK(twoRegressors.roundedOverflowing.empty());
    CHECK(twoRegressors.exactOverflowing.empty());
}

TEST_CASE("the census draws the samples tools/census/exact_sizes.py draws", "[census]")
{
    // The exact companion mirrors this generator and fixture to size the
    // exact results of the same samples. `exact_sizes.py --self-check`
    // prints the literals below, and CTest's census.exact-sizes-self-check
    // pins its output to them: an edit to either side's generator, or to the
    // variance's definition, fails one or the other.
    Draws sixPlaces { 20260926 };
    std::array<Rational, 6> const atSix { rat(4035209, 100000),  rat(20006919, 500000), rat(40234577, 1000000),
                                          rat(20376907, 500000), rat(3931329, 100000),  rat(39674913, 1000000) };
    for (Rational const& each: atSix)
        CHECK(sixPlaces.between(39, 41, 6) == each);

    Draws fourPlaces { 20260926 };
    std::array<Rational, 6> const atFour { rat(48943, 1250),  rat(195969, 5000), rat(393317, 10000),
                                           rat(102131, 2500), rat(198753, 5000), rat(97743, 2500) };
    std::array<Rational, 6> sample {};
    for (Rational& determination: sample)
        determination = fourPlaces.between(39, 41, 4);
    CHECK(sample == atFour);
    auto const variance =
        formula::checked_evaluate<MassVariance>(formula::sample_variance(formula::series<Mass, 6>), series_environment<Mass>(sample));
    REQUIRE(variance.has_value());
    CHECK(variance->measurement().value() == rat(454295463, 1000000000));
}
