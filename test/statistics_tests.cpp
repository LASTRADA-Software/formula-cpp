// SPDX-License-Identifier: Apache-2.0
#include "statistics_cross_tu.hpp"

#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <tuple>

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

TEST_CASE("one absent determination makes the mean and the count absent (T2)", "[statistics]")
{
    // A skip-absent mean gives 41.5 g and a count of 5; both are refused here.
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
        formula::measured_series<Heavy>(formula::Measured<Heavy> { Rational { std::numeric_limits<std::int64_t>::max() } },
                                        formula::Measured<Heavy> { rat(1) },
                                        formula::Measured<Heavy> { rat(2) }));
    constexpr auto mean = formula::sample_mean(formula::series<Heavy, 3>);
    STATIC_REQUIRE(formula::checked_evaluate<Heavy>(mean, heavy).error() == formula::ArithmeticError::Overflow);

    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Heavy>(mean, heavy, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
          == "1. m_h = 9223372036854775807 kg; 1 kg; 2 kg\n"
             "2. sample_mean(#1) = overflow in exact arithmetic at element 2\n");
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
