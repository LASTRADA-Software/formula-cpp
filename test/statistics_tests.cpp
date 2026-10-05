// SPDX-License-Identifier: Apache-2.0
#include "statistics_cross_tu.hpp"

#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <tuple>
#include <vector>

namespace
{
namespace unit = formula::unit;
using formula::Rational;
using formula::var;

[[nodiscard]] constexpr Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return Rational { numerator, denominator };
}

/// A determination, in grams.
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", unit::Gram>
{
};
/// How many determinations were made: a bare number.
struct Determinations: formula::Quantity<Determinations, "n", "number of determinations", unit::One>
{
};
/// Two series whose per-element ratio is averaged.
struct Wet: formula::Quantity<Wet, "m_w", "wet mass", unit::Gram>
{
};
struct Dry: formula::Quantity<Dry, "m_d", "dry mass", unit::Gram>
{
};
/// A bare number: a mean of ratios.
struct Ratio: formula::Quantity<Ratio, "q", "ratio", unit::One>
{
};
/// Stated in kilograms, the coherent unit, so that one element can be the
/// largest `Rational` there is without its conversion failing first.
struct Heavy: formula::Quantity<Heavy, "m_h", "heavy mass", unit::Kilogram>
{
};

/// A gram squared, the unit a variance of masses in grams is stated in.
/// Invented here rather than shipped: the library has no squared mass unit.
inline constexpr formula::Unit GramSquared { .dimension = formula::dim::Mass * formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1'000'000,
                                             .symbolText = formula::symbol("g2"),
                                             .decimals = 4 };
/// A variance of masses, held by the evaluator in kg^2.
struct MassVariance: formula::Quantity<MassVariance, "s2", "variance of the determinations", GramSquared>
{
};
/// A spread or a range of masses, in grams.
struct Spread: formula::Quantity<Spread, "s", "spread of the determinations", unit::Gram>
{
};

[[nodiscard]] constexpr formula::Measured<Mass> grams(Rational value)
{
    return formula::Measured<Mass> { value };
}

// Fixture A, invented: 40.2, 39.8, 40.5, 44.0, 40.0 and 43.3 g. Every element
// differs from every other; the mean is 41.3 g.
inline constexpr auto fixtureA = formula::environment(formula::measured_series<Mass>(
    grams(rat(402, 10)), grams(rat(398, 10)), grams(rat(405, 10)), grams(rat(44)), grams(rat(40)), grams(rat(433, 10))));

// Fixture A with its third determination not made.
inline constexpr auto fixtureAMissingThird =
    formula::environment(formula::measured_series<Mass>(grams(rat(402, 10)),
                                                        grams(rat(398, 10)),
                                                        formula::Measured<Mass>::absent(),
                                                        grams(rat(44)),
                                                        grams(rat(40)),
                                                        grams(rat(433, 10))));

inline constexpr auto determinations = formula::series<Mass, 6>;

/// The one variant of a method that reports the mean of six determinations.
struct MeanOfSix
{
};

inline constexpr auto meanMethod = formula::method(
    formula::variants(formula::variant<MeanOfSix>(formula::sample_mean(determinations))),
    formula::rounding_rule<unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());

// Fixture B, invented: 40.2, 39.8, 40.5, 45.2, 40.0 and 37.2 g, a mean of
// 2429/60 g (40.48333... g), which the method rounds to 40.5 g.
inline constexpr auto fixtureB = formula::environment(formula::measured_series<Mass>(grams(rat(402, 10)),
                                                                                     grams(rat(398, 10)),
                                                                                     grams(rat(405, 10)),
                                                                                     grams(rat(452, 10)),
                                                                                     grams(rat(40)),
                                                                                     grams(rat(372, 10))));
} // namespace

TEST_CASE("sample_mean averages every element, in coherent SI, and returns to the declared unit", "[statistics]")
{
    // Fixture A: 41.3 g. A mean that dropped the last element gives 40.9 g;
    // one that divided by N - 1 gives 49.56 g; one left in SI, 0.0413.
    constexpr auto mean = formula::checked_evaluate<Mass>(formula::sample_mean(determinations), fixtureA);
    STATIC_REQUIRE(mean->measurement().value() == rat(413, 10));
    STATIC_REQUIRE(
        formula::checked_evaluate<Determinations>(formula::sample_count(determinations), fixtureA)->measurement().value()
        == rat(6));
    STATIC_REQUIRE(decltype(formula::sample_mean(determinations))::dimension == formula::dim::Mass);
    STATIC_REQUIRE(decltype(formula::sample_count(determinations))::dimension == formula::dim::Scalar);

    // Fixture B: 2429/60 g, which no decimal rounding reaches.
    STATIC_REQUIRE(formula::checked_evaluate<Mass>(formula::sample_mean(determinations), fixtureB)->measurement().value()
                   == rat(2429, 60));
}

TEST_CASE("one absent determination makes the mean and the count absent", "[statistics]")
{
    // A skip-absent mean gives 2073/50 g (41.46 g) and a count of 5; both are
    // refused here.
    STATIC_REQUIRE(formula::checked_evaluate<Mass>(formula::sample_mean(determinations), fixtureAMissingThird)->is_empty());
    STATIC_REQUIRE(
        formula::checked_evaluate<Determinations>(formula::sample_count(determinations), fixtureAMissingThird)->is_empty());
}

TEST_CASE("a mean of one element is that element; a count of one is one", "[statistics]")
{
    constexpr auto single = formula::environment(formula::measured_series<Mass>(grams(rat(433, 10))));
    STATIC_REQUIRE(
        formula::checked_evaluate<Mass>(formula::sample_mean(formula::series<Mass, 1>), single)->measurement().value()
        == rat(433, 10));
    STATIC_REQUIRE(formula::checked_evaluate<Determinations>(formula::sample_count(formula::series<Mass, 1>), single)
                       ->measurement()
                       .value()
                   == rat(1));
}

TEST_CASE("a statistic over elementwise arithmetic is the statistic of the per-element values", "[statistics]")
{
    // Ratios 3/2, 5/4 and 7/5: their mean is 83/60. The ratio of the totals,
    // 270 g / 190 g = 27/19, would be another number.
    constexpr auto pairs = formula::environment(
        formula::measured_series<Wet>(
            formula::Measured<Wet> { rat(150) }, formula::Measured<Wet> { rat(50) }, formula::Measured<Wet> { rat(70) }),
        formula::measured_series<Dry>(
            formula::Measured<Dry> { rat(100) }, formula::Measured<Dry> { rat(40) }, formula::Measured<Dry> { rat(50) }));
    constexpr auto meanRatio = formula::sample_mean(formula::series<Wet, 3> / formula::series<Dry, 3>);
    STATIC_REQUIRE(decltype(meanRatio)::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(formula::checked_evaluate<Ratio>(meanRatio, pairs)->measurement().value() == rat(83, 60));
}

TEST_CASE("a sample that fails fails its statistics, with the sample's own error", "[statistics]")
{
    // The second ratio divides by zero: the mean and the count relay it,
    // neither absent nor a mean of the others.
    constexpr auto pairs = formula::environment(
        formula::measured_series<Wet>(
            formula::Measured<Wet> { rat(150) }, formula::Measured<Wet> { rat(50) }, formula::Measured<Wet> { rat(70) }),
        formula::measured_series<Dry>(
            formula::Measured<Dry> { rat(100) }, formula::Measured<Dry> { rat(0) }, formula::Measured<Dry> { rat(50) }));
    constexpr auto ratios = formula::series<Wet, 3> / formula::series<Dry, 3>;
    STATIC_REQUIRE(formula::checked_evaluate<Ratio>(formula::sample_mean(ratios), pairs).error()
                   == formula::ArithmeticError::DivisionByZero);
    STATIC_REQUIRE(formula::checked_evaluate<Determinations>(formula::sample_count(ratios), pairs).error()
                   == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("a total that overflows fails the mean, and the trace names the determination", "[statistics]")
{
    // The largest Rational, then 1 kg: the total overflows at the second
    // determination. Never a wrapped, negative mean.
    constexpr auto heavy = formula::environment(
        formula::measured_series<Heavy>(formula::Measured<Heavy> { Rational { std::numeric_limits<Rational::Int>::max() } },
                                        formula::Measured<Heavy> { rat(1) },
                                        formula::Measured<Heavy> { rat(2) }));
    constexpr auto mean = formula::sample_mean(formula::series<Heavy, 3>);
    STATIC_REQUIRE(formula::checked_evaluate<Heavy>(mean, heavy).error() == formula::ArithmeticError::Overflow);

    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Heavy>(mean, heavy, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
          == "1. m_h = 170141183460469231731687303715884105727 kg; 1 kg; 2 kg\n"
             "2. sample_mean(#1) = overflow in exact arithmetic at element 2\n");

    // The largest 64-bit integer, which overflowed 64 bits, then 1 kg and
    // 2 kg: a mean of (2^63 + 2)/3 kg.
    constexpr auto heavy64 = formula::environment(
        formula::measured_series<Heavy>(formula::Measured<Heavy> { Rational { std::numeric_limits<std::int64_t>::max() } },
                                        formula::Measured<Heavy> { rat(1) },
                                        formula::Measured<Heavy> { rat(2) }));
    STATIC_REQUIRE(formula::checked_evaluate<Heavy>(mean, heavy64)->measurement().value()
                   == Rational { (Rational::Int { 1 } << 63) + 2, 3 });
}

TEST_CASE("sample statistics evaluate at runtime, and in double", "[statistics]")
{
    auto const mean = formula::checked_evaluate<Mass>(formula::sample_mean(determinations), fixtureA);
    REQUIRE(mean.has_value());
    CHECK(mean->measurement().value() == rat(413, 10));
    auto const inDouble =
        formula::checked_evaluate_si<double>(formula::sample_mean(determinations), fixtureA, formula::NullSink {});
    REQUIRE(inDouble.has_value());
    REQUIRE(inDouble->has_value());
    CHECK(**inDouble > 0.04129);
    CHECK(**inDouble < 0.04131);
}

TEST_CASE("a sample statistic is one step over the sample's own step", "[statistics][trace-render]")
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Mass>(
        formula::sample_mean(determinations), fixtureA, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
             "2. sample_mean(#1) = 413/10 g\n");

    formula::Trace<> counted {};
    (void) formula::checked_evaluate<Determinations>(
        formula::sample_count(determinations), fixtureA, formula::RecordingSink<> { counted });
    CHECK(formula::render_trace(counted, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
             "2. sample_count(#1) = 6\n");

    // An absent determination: the statistic is not measured, and nowhere 0.
    formula::Trace<> absent {};
    (void) formula::checked_evaluate<Mass>(
        formula::sample_mean(determinations), fixtureAMissingThird, formula::RecordingSink<> { absent });
    CHECK(formula::render_trace(absent, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; (not measured); 44 g; 40 g; 433/10 g\n"
             "2. sample_mean(#1) = (not measured)\n");
}

TEST_CASE("a sample statistic renders as a call on its sample, and the result carries no marker", "[statistics][render]")
{
    CHECK(formula::render(formula::sample_mean(determinations)) == "sample_mean(m(i))");
    CHECK(formula::render(formula::sample_count(determinations)) == "sample_count(m(i))");
    CHECK(formula::render<formula::Dialect::Markdown>(formula::sample_mean(determinations)) == "sample_mean(`m(i)`)");
    CHECK(formula::render<formula::Dialect::Markdown>(formula::sample_count(determinations)) == "sample_count(`m(i)`)");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::sample_mean(determinations)) == "\\overline{{m}_{i}}");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::sample_count(determinations)) == "n({m}_{i})");
}

TEST_CASE("a page lists the sample's quantity once, as a series, however many statistics read it", "[statistics][document]")
{
    formula::Documentation const page =
        formula::document(formula::sample_mean(determinations) * formula::sample_count(determinations));
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "m");
    CHECK(page.symbols[0].shape == formula::ValueShape::Series);
    CHECK(page.symbols[0].length == 6);
}

TEST_CASE("each statistic documents its sample on its own", "[statistics][document]")
{
    // Each alone, so that a statistic whose own walk named nothing leaves
    // the page without its row -- in a product, another statistic's walk
    // would supply it.
    for (formula::Documentation const& page: { formula::document(formula::sample_count(determinations)),
                                               formula::document(formula::sample_mean(determinations)),
                                               formula::document(formula::sample_variance(determinations)),
                                               formula::document(formula::sample_range(determinations)) })
    {
        REQUIRE(page.symbols.size() == 1);
        CHECK(page.symbols[0].symbol == "m");
        CHECK(page.symbols[0].shape == formula::ValueShape::Series);
        CHECK(page.symbols[0].length == 6);
    }
}

TEST_CASE("an empty sample cannot be written, and the statistics refuse one all the same", "[statistics]")
{
    // series<Q, 0> is refused where it is written (`series.hpp`), and a
    // rejection keeps at least one determination (KeepAtLeast<m>, m >= 1), so
    // no sample a formula can name is empty. The guards stay, for the next
    // sample source: a mean over none is a division by zero and a range a
    // domain error -- never a read of the first element of an empty array,
    // which these constant evaluations would refuse to compile. (The
    // variance's own guard, fewer than two, is the n = 1 case above.)
    constexpr formula::detail::SampleValue<Rational, 1> none { .values = { rat(40) }, .positions = { 0 }, .count = 0 };
    constexpr auto meanOfNone = [](formula::detail::SampleValue<Rational, 1> const& sampled) {
        std::optional<std::size_t> failedAt;
        return formula::detail::mean_of(sampled, failedAt);
    }(none);
    STATIC_REQUIRE(meanOfNone.error() == formula::ArithmeticError::DivisionByZero);
    STATIC_REQUIRE(formula::detail::range_of(none).error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a failure position is amended only onto a failed statistic, and only printed within its sample",
          "[statistics][trace-render]")
{
    constexpr auto heavy = formula::environment(
        formula::measured_series<Heavy>(formula::Measured<Heavy> { Rational { std::numeric_limits<Rational::Int>::max() } },
                                        formula::Measured<Heavy> { rat(1) },
                                        formula::Measured<Heavy> { rat(2) }));
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate<Heavy>(formula::sample_mean(formula::series<Heavy, 3>), heavy, sink);
    // A position past the sample's three elements, told by hand: the step
    // takes it, being a failed mean, and the renderer declines to print it.
    sink.sample_failed_at(4);
    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
          == "1. m_h = 170141183460469231731687303715884105727 kg; 1 kg; 2 kg\n"
             "2. sample_mean(#1) = overflow in exact arithmetic at (no such element)\n");

    // A present mean is never given a failure position.
    formula::Trace<> present {};
    formula::RecordingSink<> presentSink { present };
    (void) formula::checked_evaluate<Mass>(formula::sample_mean(determinations), fixtureA, presentSink);
    presentSink.sample_failed_at(0);
    CHECK(!present.steps.back().failedElement.has_value());
    CHECK(formula::render_trace(present, { .maxSteps = 10 }).ends_with("2. sample_mean(#1) = 413/10 g\n"));
}

TEST_CASE("a mean joins a method: evaluated, rounded, rendered in a vocabulary, documented and traced",
          "[statistics][method]")
{
    // Fixture A's mean is 41.3 g, which 1 dp leaves alone; fixture B's,
    // 2429/60 g, the method rounds to 40.5 g -- in SI, 81/2000 kg. A method
    // that ignored its rule would give 2429/60000 kg.
    STATIC_REQUIRE(**formula::evaluate_method<MeanOfSix>(meanMethod, fixtureA) == rat(413, 10000));
    STATIC_REQUIRE(**formula::evaluate_method<MeanOfSix>(meanMethod, fixtureB) == rat(81, 2000));

    // The jurisdiction's symbol, marked as a series inside the statistic,
    // and the statistic itself unmarked.
    constexpr auto variantExpression = std::get<0>(meanMethod.variantSet.cases).expression;
    constexpr auto jurisdiction = formula::vocabulary(formula::renames<Mass>("x_m"));
    CHECK(formula::render(variantExpression, jurisdiction) == "sample_mean(x_m(i))");
    CHECK(formula::render<formula::Dialect::Markdown>(variantExpression, jurisdiction) == "sample_mean(`x_m(i)`)");
    CHECK(formula::render<formula::Dialect::LaTeX>(variantExpression, jurisdiction) == "\\overline{{x_m}_{i}}");

    formula::Documentation const page = formula::document(variantExpression, jurisdiction);
    CHECK(page.formula == "sample_mean(x_m(i))");
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "x_m");
    CHECK(page.symbols[0].shape == formula::ValueShape::Series);
    CHECK(page.symbols[0].length == 6);

    formula::Trace<> trace {};
    (void) formula::evaluate_method<MeanOfSix>(meanMethod, fixtureB, formula::RecordingSink { trace, jurisdiction });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
          == "1. x_m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
             "2. sample_mean(#1) = 2429/60 g\n"
             "3. round(#2, in g) = 81/2 g [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "4. #3 = 81/2 g [variant MeanOfSix (1st of 1), selected by tag]\n");
}

// ------------------------------------------------------------ dispersion

namespace
{
// Fixture E, invented: five equal determinations of 40.0 g.
inline constexpr auto fixtureE = formula::environment(
    formula::measured_series<Mass>(grams(rat(40)), grams(rat(40)), grams(rat(40)), grams(rat(40)), grams(rat(40))));

/// The variance of the six determinations.
inline constexpr auto variance = formula::sample_variance(determinations);
/// Their range.
inline constexpr auto range = formula::sample_range(determinations);

/// The spread reported exactly: the variance's root, rounded to 2 dp of g.
inline constexpr auto spread =
    formula::rounded_sqrt<unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(variance);
} // namespace

TEST_CASE("sample_variance divides by n - 1, and sample_range is the largest less the smallest", "[statistics]")
{
    // Fixture A: squared deviations 1.21, 2.25, 0.64, 7.29, 1.69 and 4.00 g^2
    // total 17.08 g^2, so 427/125 g^2 over n - 1 = 5, held as kg^2. Over
    // n = 6 (the population variance) it would be 427/150 g^2.
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(variance, fixtureA)->measurement().value() == rat(427, 125));
    STATIC_REQUIRE(formula::checked_evaluate_si(variance, fixtureA)->value() == rat(427, 125'000'000));
    STATIC_REQUIRE(decltype(variance)::dimension == formula::dim::Mass * formula::dim::Mass);
    // 44.0 - 39.8 = 4.2 g. The extremes are the fourth and the second
    // determinations, not the ends: last - first would give 3.1 g.
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(range, fixtureA)->measurement().value() == rat(42, 10));
    STATIC_REQUIRE(decltype(range)::dimension == formula::dim::Mass);

    // Fixture B: 4057/600 g^2 (the population variance, 4057/720), and
    // 45.2 - 37.2 = 8.0 g (last - first would give -3.0 g).
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(variance, fixtureB)->measurement().value() == rat(4057, 600));
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(range, fixtureB)->measurement().value() == rat(8));
}

TEST_CASE("equal determinations have no dispersion, exactly", "[statistics]")
{
    // Fixture E: variance 0 and range 0, never a division by zero.
    constexpr auto equal = formula::series<Mass, 5>;
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(formula::sample_variance(equal), fixtureE)->measurement().value()
                   == rat(0));
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(formula::sample_range(equal), fixtureE)->measurement().value()
                   == rat(0));
}

TEST_CASE("the variance of six masses read to the microgram answers exactly", "[statistics]")
{
    // The sample that overflowed 64 bits in kg^2: in g^2 its variance is
    // 2026588050217/6000000000000, exactly.
    auto const sixAtMicrograms = formula::environment(formula::measured_series<Mass>(
        grams(rat(40053270, 1000000)), grams(rat(39475922, 1000000)), grams(rat(39025798, 1000000)),
        grams(rat(40615904, 1000000)), grams(rat(39418416, 1000000)), grams(rat(40131659, 1000000))));
    auto const microgramVariance = formula::checked_evaluate<MassVariance>(
        formula::sample_variance(formula::series<Mass, 6>), sixAtMicrograms);
    REQUIRE(microgramVariance.has_value());
    REQUIRE(microgramVariance->is_value());
    CHECK(microgramVariance->measurement().value() == formula::Rational { 2026588050217, 6000000000000 });
}

TEST_CASE("one determination has a range of 0 and no variance", "[statistics]")
{
    // A variance needs two determinations: n - 1 = 0 is refused as a domain
    // error, never read as 0 or as a division by zero.
    constexpr auto single = formula::environment(formula::measured_series<Mass>(grams(rat(433, 10))));
    STATIC_REQUIRE(
        formula::checked_evaluate<MassVariance>(formula::sample_variance(formula::series<Mass, 1>), single).error()
        == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(
        formula::checked_evaluate<Spread>(formula::sample_range(formula::series<Mass, 1>), single)->measurement().value()
        == rat(0));
}

TEST_CASE("one absent determination makes the variance and the range absent", "[statistics]")
{
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(variance, fixtureAMissingThird)->is_empty());
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(range, fixtureAMissingThird)->is_empty());
}

namespace
{
/// The textbook one-pass sample variance, (sum x^2 - (sum x)^2 / n) / (n - 1),
/// of @p kilograms, in exact arithmetic: the form `sample_variance` does not
/// use, kept here to show where it runs out.
[[nodiscard]] constexpr std::expected<Rational, formula::ArithmeticError> one_pass_variance(
    std::span<Rational const> kilograms) noexcept
{
    Rational massTotal { 0 };
    Rational squaresTotal { 0 };
    for (Rational const& mass: kilograms)
    {
        std::expected<Rational, formula::ArithmeticError> const squared = formula::checked_mul(mass, mass);
        if (!squared)
            return std::unexpected { squared.error() };
        std::expected<Rational, formula::ArithmeticError> const squaresSoFar = formula::checked_add(squaresTotal, *squared);
        if (!squaresSoFar)
            return std::unexpected { squaresSoFar.error() };
        std::expected<Rational, formula::ArithmeticError> const massSoFar = formula::checked_add(massTotal, mass);
        if (!massSoFar)
            return std::unexpected { massSoFar.error() };
        squaresTotal = *squaresSoFar;
        massTotal = *massSoFar;
    }
    auto const sizeOf = static_cast<std::int64_t>(kilograms.size());
    std::expected<Rational, formula::ArithmeticError> const totalSquared = formula::checked_mul(massTotal, massTotal);
    if (!totalSquared)
        return std::unexpected { totalSquared.error() };
    std::expected<Rational, formula::ArithmeticError> const overSize = formula::checked_div(*totalSquared, rat(sizeOf));
    if (!overSize)
        return std::unexpected { overSize.error() };
    std::expected<Rational, formula::ArithmeticError> const deviations = formula::checked_sub(squaresTotal, *overSize);
    if (!deviations)
        return std::unexpected { deviations.error() };
    return formula::checked_div(*deviations, rat(sizeOf - 1));
}

/// Fixture A scaled by @p scaleBy, in kilograms.
[[nodiscard]] constexpr std::array<Rational, 6> fixture_a_kilograms(Rational::Int scaleBy) noexcept
{
    return { Rational { 402 * scaleBy, 10'000 }, Rational { 398 * scaleBy, 10'000 }, Rational { 405 * scaleBy, 10'000 },
             Rational { 440 * scaleBy, 10'000 }, Rational { 400 * scaleBy, 10'000 }, Rational { 433 * scaleBy, 10'000 } };
}

/// Fixture A scaled by @p scaleBy, as the evaluator reads it: in grams.
[[nodiscard]] constexpr auto fixture_a_scaled(Rational::Int scaleBy) noexcept
{
    return formula::environment(formula::measured_series<Mass>(grams(Rational { 402 * scaleBy, 10 }),
                                                               grams(Rational { 398 * scaleBy, 10 }),
                                                               grams(Rational { 405 * scaleBy, 10 }),
                                                               grams(Rational { 440 * scaleBy, 10 }),
                                                               grams(Rational { 400 * scaleBy, 10 }),
                                                               grams(Rational { 433 * scaleBy, 10 })));
}
} // namespace

TEST_CASE("at large magnitudes the two-pass variance holds past where the one-pass formula overflows", "[statistics]")
{
    // Fixture A scaled by 2^57: 40.2 g becomes about 5.8e18 g. The textbook
    // one-pass form, (sum x^2 - (sum x)^2 / n) / (n - 1), computed exactly in
    // kg, first overflows at a scale of 2^57, where the square of the
    // masses' sum, 247.8 g times 2^57, outgrows 127 bits. At 2^56 it still
    // answers, and agrees with the two-pass form. The two-pass form (the mean, then the
    // squared deviations from it) squares only the deviations, and holds up
    // to 2^62 (see the overflow test below) -- at 2^57 it gives exactly
    // 427/125 g^2 times 2^114, in kg^2. At fine resolution it is the other
    // way round -- see the header.
    constexpr Rational::Int below = Rational::Int { 1 } << 56;
    constexpr Rational::Int scale = Rational::Int { 1 } << 57;
    STATIC_REQUIRE(one_pass_variance(fixture_a_kilograms(below)).value()
                   == Rational { Rational::Int { 427 } << 106, 1'953'125 });
    STATIC_REQUIRE(formula::checked_evaluate_si(variance, fixture_a_scaled(below))->value()
                   == Rational { Rational::Int { 427 } << 106, 1'953'125 });
    STATIC_REQUIRE(one_pass_variance(fixture_a_kilograms(scale)).error() == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(formula::checked_mul(Rational { 2478 * scale, 10'000 }, Rational { 2478 * scale, 10'000 }).error()
                   == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(formula::checked_evaluate_si(variance, fixture_a_scaled(scale))->value()
                   == Rational { Rational::Int { 427 } << 108, 1'953'125 });
}

TEST_CASE("a variance that overflows fails with Overflow, naming the determination, in either pass",
          "[statistics][trace-render]")
{
    // The squares pass: 9e27 g and -9e27 g have a mean of 0, and the first
    // squared deviation, (9e24 kg)^2, leaves 128 bits.
    constexpr Rational::Int ninePer = Rational::Int { 9'000'000'000'000'000'000 } * 1'000'000'000;
    constexpr auto opposite =
        formula::environment(formula::measured_series<Mass>(grams(Rational { ninePer }), grams(Rational { -ninePer })));
    constexpr auto pair = formula::sample_variance(formula::series<Mass, 2>);
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(pair, opposite).error() == formula::ArithmeticError::Overflow);
    formula::Trace<> squares {};
    (void) formula::checked_evaluate<MassVariance>(pair, opposite, formula::RecordingSink<> { squares });
    CHECK(formula::render_trace(squares, { .maxSteps = 10 })
          == "1. m = 9000000000000000000000000000 g; -9000000000000000000000000000 g\n"
             "2. sample_variance(#1) = overflow in exact arithmetic at element 1\n");
    // 9e18 g and -9e18 g, which overflowed 64 bits: 2 * (9e18)^2 g^2.
    constexpr auto opposite64 = formula::environment(
        formula::measured_series<Mass>(grams(rat(9'000'000'000'000'000'000)), grams(rat(-9'000'000'000'000'000'000))));
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(pair, opposite64)->measurement().value()
                   == Rational { Rational::Int { 9'000'000'000'000'000'000 } * 9'000'000'000'000'000'000 * 2 });

    // Fixture A scaled by 2^63, where the two-pass form first fails: its mean
    // still fits, and a squared deviation does not, at the fourth
    // determination.
    constexpr auto scaledA = fixture_a_scaled(Rational::Int { 1 } << 63);
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(variance, scaledA).error() == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(formula::checked_evaluate_si(formula::sample_mean(determinations), scaledA).has_value());
    formula::Trace<> squaresLate {};
    (void) formula::checked_evaluate<MassVariance>(variance, scaledA, formula::RecordingSink<> { squaresLate });
    CHECK(formula::render_trace(squaresLate, { .maxSteps = 10 })
          == "1. m = 1853897779407809937408/5 g; 1835451035334100385792/5 g; 373546567492618420224 g; "
             "405828369621610135552 g; 368934881474191032320 g; 1996860045979058962432/5 g\n"
             "2. sample_variance(#1) = overflow in exact arithmetic at element 4\n");
    // Scaled by 2^62, the last power of two the two-pass form holds, the
    // variance in kg^2 is 427/125 g^2 times 2^124: 427 * 2^118 / 1953125,
    // a 127-bit numerator. Converted to the declared g^2 it gains 10^6 and
    // overflows from 2^60; 2^59 is the last scale it answers at.
    STATIC_REQUIRE(formula::checked_evaluate_si(variance, fixture_a_scaled(Rational::Int { 1 } << 62))->value()
                   == Rational { Rational::Int { 427 } << 118, 1'953'125 });
    STATIC_REQUIRE(formula::checked_evaluate_si(variance, fixture_a_scaled(Rational::Int { 1 } << 60)).has_value());
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(variance, fixture_a_scaled(Rational::Int { 1 } << 60)).error()
                   == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(
        formula::checked_evaluate<MassVariance>(variance, fixture_a_scaled(Rational::Int { 1 } << 59))->measurement().value()
        == Rational { Rational::Int { 427 } << 118, 125 });
    // Scaled by 2^31, which overflowed 64 bits, the variance is fixture A's
    // 427/125 g^2 times 2^62.
    constexpr std::int64_t scale64 = std::int64_t { 1 } << 31;
    constexpr auto scaledA64 = formula::environment(formula::measured_series<Mass>(grams(rat(402 * scale64, 10)),
                                                                                   grams(rat(398 * scale64, 10)),
                                                                                   grams(rat(405 * scale64, 10)),
                                                                                   grams(rat(440 * scale64, 10)),
                                                                                   grams(rat(400 * scale64, 10)),
                                                                                   grams(rat(433 * scale64, 10))));
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(variance, scaledA64)->measurement().value()
                   == Rational { Rational::Int { 427 } << 62, 125 });

    // The mean pass: the largest Rational and 1 kg, whose total overflows at
    // the second determination before any deviation is taken -- the mean
    // alone fails there too.
    constexpr auto heavy = formula::environment(
        formula::measured_series<Heavy>(formula::Measured<Heavy> { Rational { std::numeric_limits<Rational::Int>::max() } },
                                        formula::Measured<Heavy> { rat(1) }));
    constexpr auto heavyVariance = formula::sample_variance(formula::series<Heavy, 2>);
    STATIC_REQUIRE(formula::checked_evaluate_si(heavyVariance, heavy).error() == formula::ArithmeticError::Overflow);
    STATIC_REQUIRE(formula::checked_evaluate_si(formula::sample_mean(formula::series<Heavy, 2>), heavy).error()
                   == formula::ArithmeticError::Overflow);
    formula::Trace<> meanPass {};
    (void) formula::checked_evaluate_si(heavyVariance, heavy, formula::RecordingSink<> { meanPass });
    CHECK(formula::render_trace(meanPass, { .maxSteps = 10 })
          == "1. m_h = 170141183460469231731687303715884105727 kg; 1 kg\n"
             "2. sample_variance(#1) = overflow in exact arithmetic at element 2\n");
    // The largest 64-bit integer and 1 kg, which overflowed 64 bits: a
    // variance of (2^63 - 2)^2 / 2 kg^2.
    constexpr auto heavy64 = formula::environment(
        formula::measured_series<Heavy>(formula::Measured<Heavy> { Rational { std::numeric_limits<std::int64_t>::max() } },
                                        formula::Measured<Heavy> { rat(1) }));
    constexpr Rational::Int lessTwo = (Rational::Int { 1 } << 63) - 2;
    STATIC_REQUIRE(formula::checked_evaluate_si(heavyVariance, heavy64)->value() == Rational { lessTwo * lessTwo, 2 });
}

TEST_CASE("dispersion of negative determinations", "[statistics]")
{
    // -3, -8 and -5 g: range 5 g, variance 19/3 g^2. A running maximum
    // started at zero would give 8 g.
    constexpr auto below =
        formula::environment(formula::measured_series<Mass>(grams(rat(-3)), grams(rat(-8)), grams(rat(-5))));
    constexpr auto three = formula::series<Mass, 3>;
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(formula::sample_range(three), below)->measurement().value() == rat(5));
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(formula::sample_variance(three), below)->measurement().value()
                   == rat(19, 3));
    // -3, 6 and -1 g, mixed signs: range 9 g, variance 67/3 g^2. Extremes
    // compared on magnitudes would give 6 - 1 = 5 g.
    constexpr auto mixed =
        formula::environment(formula::measured_series<Mass>(grams(rat(-3)), grams(rat(6)), grams(rat(-1))));
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(formula::sample_range(three), mixed)->measurement().value() == rat(9));
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(formula::sample_variance(three), mixed)->measurement().value()
                   == rat(67, 3));
}

TEST_CASE("under double, a NaN determination is the range, wherever it stands", "[statistics]")
{
    // Comparisons with a NaN are false: a running minimum and maximum would
    // skip one unless it came first, and the range would depend on the order
    // of entry. It is the range, first, middle or last.
    constexpr double nan = std::numeric_limits<double>::quiet_NaN();
    for (std::size_t at = 0; at < 3; ++at)
    {
        formula::detail::SampleValue<double, 3> sampled;
        sampled.values = { 1.0, 4.0, 2.0 };
        sampled.positions = { 0, 1, 2 };
        sampled.count = 3;
        sampled.values[at] = nan;
        std::expected<double, formula::ArithmeticError> const ranged = formula::detail::range_of(sampled);
        REQUIRE(ranged.has_value());
        CHECK(*ranged != *ranged);
    }
}

TEST_CASE("the spread is reported exactly: the rounded root of the variance", "[statistics][rounded_root]")
{
    // sqrt(427/125) = 1.8482... g -> 1.85 g; sqrt(4057/600) = 2.6003... g ->
    // 2.60 g. A population variance would give 1.69 g and 2.37 g.
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(spread, fixtureA)->measurement().value() == rat(185, 100));
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(spread, fixtureB)->measurement().value() == rat(260, 100));

    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Spread>(spread, fixtureA, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
             "2. sample_variance(#1) = 427/125000000 kg^2\n"
             "3. round(sqrt(#2), to 2 dp of g) = 37/20 g [nearest, ties away from zero]\n");
    CHECK(formula::render(spread) == "round(sqrt(sample_variance(m(i))), to 2 dp of g)");
}

TEST_CASE("equal determinations: the spread is 0 exactly, and a precision check over it is satisfied",
          "[statistics][precision]")
{
    constexpr auto equal = formula::series<Mass, 5>;
    constexpr auto equalSpread =
        formula::rounded_sqrt<unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
            formula::sample_variance(equal));
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(equalSpread, fixtureE)->measurement().value() == rat(0));
    constexpr auto agree =
        formula::constraint(equalSpread <= formula::precision_limit<formula::PrecisionKind::Repeatability>(
                                formula::sample_mean(equal),
                                formula::constant<unit::Gram>(rat(1, 10)) + rat(1, 50) * formula::precision_level<Mass>),
                            formula::Verdict { "repeat the determinations" });
    STATIC_REQUIRE(formula::check(agree, fixtureE).is_satisfied());
}

namespace
{
// Four invented determinations, 40.35, 40.45, 40.50 and 40.55 g: range
// 0.2 g, mean 3237/80 = 40.4625 g. The critical-value table below is
// invented and plainly so -- non-monotone, with a factor of 11/50 at n = 4
// that no published table holds.
inline constexpr formula::SampleSizeTable<5> JoinSizes { 3, 4, 5, 6, 8 };

inline constexpr auto fourDeterminations = formula::environment(
    formula::measured_series<Mass>(grams(rat(807, 20)), grams(rat(809, 20)), grams(rat(81, 2)), grams(rat(811, 20))));

// The range against the table's factor times the repeatability limit at
// the level of the mean:
//  - at the mean, 11/50 * (1/10 + 3237/80 / 50) = 40007/200000 g =
//    0.200035 g, so 0.2 g is satisfied;
//  - at the first determination (the level-is-the-first-result mutation),
//    11/50 * (1/10 + 40.35 / 50) = 9977/50000 g = 0.19954 g: violated;
//  - at level 0 (an unbound placeholder), 0.022 g: violated;
//  - with the count ignored and n = 3 read instead, 10 * 0.90925 g: still
//    satisfied -- the count has tests of its own.
inline constexpr auto rangeCheck = formula::constraint(
    formula::sample_range(formula::series<Mass, 4>)
        <= formula::critical_value<JoinSizes, unit::One>(formula::sample_count(formula::series<Mass, 4>),
                                                         { rat(10), rat(11, 50), rat(20), rat(50), rat(40) })
               * formula::precision_limit<formula::PrecisionKind::Repeatability>(
                   formula::sample_mean(formula::series<Mass, 4>),
                   formula::constant<unit::Gram>(rat(1, 10)) + rat(1, 50) * formula::precision_level<Mass>),
    formula::Verdict { "repeat the determinations" });
} // namespace

TEST_CASE("a range checked against a critical value times a precision limit at the mean", "[statistics][precision]")
{
    STATIC_REQUIRE(formula::check(rangeCheck, fourDeterminations).is_satisfied());
    // The same check with the level taken as the first determination fails:
    // that is the mutation the numbers above were chosen to kill.
    constexpr auto atFirst = formula::constraint(
        formula::sample_range(formula::series<Mass, 4>)
            <= formula::critical_value<JoinSizes, unit::One>(formula::sample_count(formula::series<Mass, 4>),
                                                             { rat(10), rat(11, 50), rat(20), rat(50), rat(40) })
                   * formula::precision_limit<formula::PrecisionKind::Repeatability>(
                       formula::constant<unit::Gram>(rat(807, 20)),
                       formula::constant<unit::Gram>(rat(1, 10)) + rat(1, 50) * formula::precision_level<Mass>),
        formula::Verdict { "repeat the determinations" });
    STATIC_REQUIRE(formula::check(atFirst, fourDeterminations).is_violated());

    formula::Trace<> trace {};
    (void) formula::check(rangeCheck, fourDeterminations, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 40 })
          == "1. m = 807/20 g; 809/20 g; 81/2 g; 811/20 g\n"
             "2. sample_range(#1) = 1/5 g\n"
             "3. m = 807/20 g; 809/20 g; 81/2 g; 811/20 g\n"
             "4. sample_count(#3) = 4\n"
             "5. critical(#4) = 11/50 [critical value at n = 4]\n"
             "6. m = 807/20 g; 809/20 g; 81/2 g; 811/20 g\n"
             "7. sample_mean(#6) = 3237/80 g\n"
             "8. level (pass 1 of 2) = #7 = 3237/80 g\n"
             "9. 1/10 g\n"
             "10. 1/50\n"
             "11. level = 3237/80 g [bound by #14]\n"
             "12. #10 * #11 = 3237/4000 g\n"
             "13. #9 + #12 = 3637/4000 g\n"
             "14. r at level #8 (pass 2 of 2) = #13 = 3637/4000 g\n"
             "15. #5 * #14 = 40007/200000 g\n"
             "16. require #2 <= #15 [satisfied]\n");
}

TEST_CASE("the variance and the range trace, render and document as the mean does",
          "[statistics][trace-render][render][document]")
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate<MassVariance>(variance, fixtureA, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
             "2. sample_variance(#1) = 427/125000000 kg^2\n");
    formula::Trace<> ranged {};
    (void) formula::checked_evaluate<Spread>(range, fixtureA, formula::RecordingSink<> { ranged });
    CHECK(formula::render_trace(ranged, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
             "2. sample_range(#1) = 21/5 g\n");
    formula::Trace<> absent {};
    (void) formula::checked_evaluate<MassVariance>(variance, fixtureAMissingThird, formula::RecordingSink<> { absent });
    CHECK(formula::render_trace(absent, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; (not measured); 44 g; 40 g; 433/10 g\n"
             "2. sample_variance(#1) = (not measured)\n");

    constexpr auto jurisdiction = formula::vocabulary(formula::renames<Mass>("x_m"));
    CHECK(formula::render(variance, jurisdiction) == "sample_variance(x_m(i))");
    CHECK(formula::render(range, jurisdiction) == "sample_range(x_m(i))");
    CHECK(formula::render<formula::Dialect::Markdown>(variance, jurisdiction) == "sample_variance(`x_m(i)`)");
    CHECK(formula::render<formula::Dialect::Markdown>(range, jurisdiction) == "sample_range(`x_m(i)`)");
    CHECK(formula::render<formula::Dialect::LaTeX>(variance, jurisdiction) == "s^{2}({x_m}_{i})");
    CHECK(formula::render<formula::Dialect::LaTeX>(range, jurisdiction) == "\\operatorname{range}({x_m}_{i})");

    formula::Documentation const page = formula::document(spread + range, jurisdiction);
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "x_m");
    CHECK(page.symbols[0].shape == formula::ValueShape::Series);
    CHECK(page.symbols[0].length == 6);
}

TEST_CASE("a statistic used from two translation units is one formula", "[statistics]")
{
    // 13, 21 and 35 g: 23 g, whichever unit evaluates it.
    statistics_cross_tu::Read const here = formula::checked_evaluate<statistics_cross_tu::Specimen>(
        statistics_cross_tu::meanOfSpecimens, statistics_cross_tu::inputs);
    statistics_cross_tu::Read const there = statistics_mean_in_other_tu();
    REQUIRE(here.has_value());
    REQUIRE(there.has_value());
    CHECK(here->measurement().value() == rat(23));
    CHECK(there->measurement().value() == rat(23));
}

namespace
{
/// Room for eight determinations, however many were made: a bound, not a
/// count.
inline constexpr auto observedMasses = formula::observations<Mass, 8>;

// Fixture A again, as six observations in a set that holds eight.
inline constexpr auto fixtureAObserved = formula::environment(
    formula::MeasuredObservations<Mass, 8>(rat(402, 10), rat(398, 10), rat(405, 10), rat(44), rat(40), rat(433, 10)));
} // namespace

TEST_CASE("observations are a sample: the same determinations give the same statistics as a series", "[statistics]")
{
    // Fixture A, six of eight places filled: 41.3 g from 6. Divided by the
    // capacity, the mean would be 247.8 / 8 = 30.975 g; counted as eight, the
    // two unfilled places would join as zeros -- the range 44 g, the count 8.
    constexpr auto mean = formula::checked_evaluate<Mass>(formula::sample_mean(observedMasses), fixtureAObserved);
    STATIC_REQUIRE(mean->measurement().value() == rat(413, 10));
    STATIC_REQUIRE(
        mean->measurement().value()
        == formula::checked_evaluate<Mass>(formula::sample_mean(determinations), fixtureA)->measurement().value());
    STATIC_REQUIRE(formula::checked_evaluate<Determinations>(formula::sample_count(observedMasses), fixtureAObserved)
                       ->measurement()
                       .value()
                   == rat(6));
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(formula::sample_variance(observedMasses), fixtureAObserved)
                       ->measurement()
                       .value()
                   == rat(427, 125));
    STATIC_REQUIRE(
        formula::checked_evaluate<Spread>(formula::sample_range(observedMasses), fixtureAObserved)->measurement().value()
        == rat(42, 10));
}

TEST_CASE("observations filled to their capacity are a sample; one more is refused before any statistic", "[statistics]")
{
    // Eight: fixture A and 41 and 39 g, 327.8 g over 8.
    std::array<Rational, 9> const read { rat(402, 10), rat(398, 10), rat(405, 10), rat(44), rat(40),
                                         rat(433, 10), rat(41),      rat(39),      rat(50) };
    auto const filled = formula::MeasuredObservations<Mass, 8>::from(std::span<Rational const> { read }.first(8));
    REQUIRE(filled.has_value());
    auto const eight = formula::environment(*filled);
    auto const mean = formula::checked_evaluate<Mass>(formula::sample_mean(observedMasses), eight);
    REQUIRE(mean.has_value());
    CHECK(mean->measurement().value() == rat(1639, 40));
    auto const counted = formula::checked_evaluate<Determinations>(formula::sample_count(observedMasses), eight);
    REQUIRE(counted.has_value());
    CHECK(counted->measurement().value() == rat(8));
    // Nine: nothing is dropped to fit, so no mean is taken of eight of them.
    auto const overfilled =
        formula::MeasuredObservations<Mass, 8>::from(read).transform([](formula::MeasuredObservations<Mass, 8> const& made) {
            return formula::checked_evaluate<Mass>(formula::sample_mean(observedMasses), formula::environment(made));
        });
    CHECK(overfilled.error() == formula::ObservationsOverCapacity { .given = 9, .capacity = 8 });
}

TEST_CASE("observations of none reach the empty-sample guards: a count of 0 and no mean", "[statistics]")
{
    // No sample a formula names at compile time is empty; observations, whose
    // count is data, can be.
    constexpr auto none = formula::environment(formula::MeasuredObservations<Mass, 8>());
    STATIC_REQUIRE(
        formula::checked_evaluate<Determinations>(formula::sample_count(observedMasses), none)->measurement().value()
        == rat(0));
    STATIC_REQUIRE(formula::checked_evaluate<Mass>(formula::sample_mean(observedMasses), none).error()
                   == formula::ArithmeticError::DivisionByZero);
    STATIC_REQUIRE(formula::checked_evaluate<Spread>(formula::sample_range(observedMasses), none).error()
                   == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(formula::checked_evaluate<MassVariance>(formula::sample_variance(observedMasses), none).error()
                   == formula::ArithmeticError::DomainError);
}

TEST_CASE("a statistic of observations is one step over the observations' own step", "[statistics][trace-render]")
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Mass>(
        formula::sample_mean(observedMasses), fixtureAObserved, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
             "2. sample_mean(#1) = 413/10 g\n");
    formula::Trace<> counted {};
    (void) formula::checked_evaluate<Determinations>(
        formula::sample_count(observedMasses), fixtureAObserved, formula::RecordingSink<> { counted });
    CHECK(formula::render_trace(counted, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
             "2. sample_count(#1) = 6\n");
}

TEST_CASE("a statistic of observations that fails names the observation, as a series' failure would",
          "[statistics][trace-render]")
{
    // 1 kg and 2^126 - 1 kg total 2^126 kg; the third, 2^126 + 9 kg, takes
    // the total past 2^127 - 1. The position counts observations, as
    // FailureSite::InputObservation does -- not elements.
    constexpr Rational::Int half = Rational::Int { 1 } << 126;
    constexpr auto heavyObserved = formula::environment(
        formula::MeasuredObservations<Heavy, 3>(rat(1), Rational { half - 1 }, Rational { half + 9 }));
    constexpr auto mean = formula::sample_mean(formula::observations<Heavy, 3>);
    STATIC_REQUIRE(formula::checked_evaluate<Heavy>(mean, heavyObserved).error() == formula::ArithmeticError::Overflow);
    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Heavy>(mean, heavyObserved, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
          == "1. m_h = 1 kg; 85070591730234615865843651857942052863 kg; 85070591730234615865843651857942052873 kg\n"
             "2. sample_mean(#1) = overflow in exact arithmetic at observation 3\n");
    // With 2^62 - 1 kg and 2^62 + 9 kg, which overflowed 64 bits, the mean
    // is (2^63 + 9)/3 kg.
    constexpr auto heavyObserved64 = formula::environment(formula::MeasuredObservations<Heavy, 3>(
        rat(1), Rational { 4611686018427387903 }, Rational { 4611686018427387913 }));
    STATIC_REQUIRE(formula::checked_evaluate<Heavy>(mean, heavyObserved64)->measurement().value()
                   == Rational { (Rational::Int { 1 } << 63) + 9, 3 });
}

TEST_CASE("a statistic of observations renders on them, and the page gives their capacity as a bound",
          "[statistics][render][document]")
{
    CHECK(formula::render(formula::sample_mean(observedMasses)) == "sample_mean(m(i))");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::sample_count(observedMasses)) == "n({m}_{i})");
    for (formula::Documentation const& page: { formula::document(formula::sample_count(observedMasses)),
                                               formula::document(formula::sample_mean(observedMasses)),
                                               formula::document(formula::sample_variance(observedMasses)),
                                               formula::document(formula::sample_range(observedMasses)) })
    {
        REQUIRE(page.symbols.size() == 1);
        CHECK(page.symbols[0].symbol == "m");
        CHECK(page.symbols[0].shape == formula::ValueShape::Observations);
        CHECK(page.symbols[0].length == 8);
    }
}

TEST_CASE("a statistic of fewer observations than their capacity, in double, is a constant expression", "[statistics]")
{
    // Six made in room for eight: the two unfilled places are written, so
    // nothing indeterminate is copied -- which clang refuses in a constant
    // expression, and which at run time would read an indeterminate double.
    constexpr auto inDouble =
        formula::checked_evaluate_si<double>(formula::sample_mean(observedMasses), fixtureAObserved, formula::NullSink {});
    STATIC_REQUIRE(**inDouble > 0.04129);
    STATIC_REQUIRE(**inDouble < 0.04131);
    constexpr auto rangeInDouble =
        formula::checked_evaluate_si<double>(formula::sample_range(observedMasses), fixtureAObserved, formula::NullSink {});
    STATIC_REQUIRE(**rangeInDouble > 0.00419);
    STATIC_REQUIRE(**rangeInDouble < 0.00421);
}

TEST_CASE("a statistic of observations read at run time", "[statistics]")
{
    std::vector<Rational> const read { rat(402, 10), rat(398, 10), rat(405, 10), rat(44), rat(40), rat(433, 10) };
    auto const observed = formula::MeasuredObservations<Mass, 8>::from(read);
    REQUIRE(observed.has_value());
    auto const mean = formula::checked_evaluate<Mass>(formula::sample_mean(observedMasses), formula::environment(*observed));
    REQUIRE(mean.has_value());
    CHECK(mean->measurement().value() == rat(413, 10));
    auto const inDouble = formula::checked_evaluate_si<double>(
        formula::sample_mean(observedMasses), formula::environment(*observed), formula::NullSink {});
    REQUIRE(inDouble.has_value());
    REQUIRE(inDouble->has_value());
    CHECK(**inDouble > 0.04129);
    CHECK(**inDouble < 0.04131);
}
