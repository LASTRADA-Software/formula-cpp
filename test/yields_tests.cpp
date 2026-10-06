// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <type_traits>
#include <utility>

namespace
{
namespace unit = formula::unit;
using formula::var;
using Rise = formula::Quantity<struct YieldsRiseTag, "h", "height gained", unit::Millimetre>;
using Run = formula::Quantity<struct YieldsRunTag, "L", "horizontal distance covered", unit::Millimetre>;
using Gradient = formula::Quantity<struct YieldsGradientTag, "s", "road gradient", unit::One>;

constexpr auto ratio = formula::yields<Gradient>(var<Rise> / var<Run>);
constexpr auto batch = formula::environment(formula::Measured<Rise> { 163 }, formula::Measured<Run> { 307 });

[[nodiscard]] constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// The series fixture of `series_tests.cpp`: retained masses of 130, 210, 95,
// 340 and 28 g, with the 95 g screen left unmeasured, so that every element
// differs from every other. The result is asked for in kilograms, so that a
// result left in the expression's grams shows in the numbers.
struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", unit::Gram>
{
};
struct RetainedKilograms:
    formula::Quantity<RetainedKilograms, "m_k", "mass retained on a screen, in kilograms", unit::Kilogram>
{
};
constexpr auto inputs = formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { rat(130) },
                                                                                formula::Measured<Retained> { rat(210) },
                                                                                formula::Measured<Retained>::absent(),
                                                                                formula::Measured<Retained> { rat(340) },
                                                                                formula::Measured<Retained> { rat(28) }));

// The rejection fixture of `rejection_tests.cpp`: sample A, 40.2, 39.8, 40.5,
// 44.0, 40.0 and 43.3 g, rejected by a 6 % deviation from each pass's mean,
// which settles at 321/8 g after rejecting elements 3 and 5.
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", unit::Gram>
{
};

template <typename... Values>
[[nodiscard]] constexpr auto sampleOf(Values... values)
{
    return formula::environment(formula::measured_series<Mass>(formula::Measured<Mass> { values }...));
}

inline constexpr auto fixtureA = sampleOf(rat(402, 10), rat(398, 10), rat(405, 10), rat(44), rat(40), rat(433, 10));
inline constexpr formula::Verdict repeatTest { "discard the determinations and repeat the test" };
inline constexpr formula::Citation exampleCited { .title = "Example Standard", .section = "7.4" };
inline constexpr auto sixPercent = formula::deviation_from_mean(rat(6, 100) * formula::pass_mean<Mass>);
constexpr auto MostExtreme = formula::PerPass::MostExtreme;
constexpr auto Keep = formula::OnLimit::Keep;
inline constexpr auto rejectionA = formula::without_outliers<MostExtreme, Keep, formula::AtMost<2>, formula::KeepAtLeast<4>>(
    formula::series<Mass, 6>, sixPercent, repeatTest, exampleCited);

// The retry fixture of `retry_tests.cpp`, w_k = 6.08 g + w_{k-1} / 2 from
// 0 g, accepted once it rises by at most 0.76 g: a value that publishes no
// dimension.
struct Estimate: formula::Quantity<Estimate, "w", "an invented iterated estimate", unit::Gram>
{
};
inline constexpr auto fourAttempts = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(
    formula::starting_from(formula::constant<unit::Gram>(rat(0))),
    formula::constant<unit::Gram>(rat(152, 25)) + formula::previous_attempt<Estimate> / rat(2),
    formula::previous_attempt<Estimate> - formula::this_attempt<Estimate> >= formula::constant<unit::Gram>(rat(-19, 25)),
    formula::Verdict { "repeat the determination" },
    formula::Citation { .title = "Settled estimate", .reference = "Example Standard 12", .section = "6" });
} // namespace

TEST_CASE("yields: the result quantity is named once, where the formula is written", "[yields]")
{
    STATIC_REQUIRE(std::is_same_v<decltype(formula::checked_evaluate(ratio, batch)),
                                  std::expected<formula::Outcome<Gradient>, formula::ArithmeticError>>);
    STATIC_REQUIRE(formula::checked_evaluate(ratio, batch) == formula::checked_evaluate<Gradient>(ratio.expression, batch));
    STATIC_REQUIRE(formula::checked_evaluate<Gradient>(ratio, batch) == formula::checked_evaluate(ratio, batch));
    STATIC_REQUIRE(formula::number_of(formula::evaluate(ratio, batch)) == formula::Rational { 163, 307 });
    STATIC_REQUIRE(formula::evaluate<Gradient>(ratio, batch) == formula::evaluate(ratio, batch));
    STATIC_REQUIRE(std::is_same_v<typename decltype(ratio)::quantity, Gradient>);
}

TEST_CASE("yields: explain, render and document see the formula itself", "[yields]")
{
    auto const explained = formula::explain(ratio, batch);
    CHECK(explained.outcome == formula::explain<Gradient>(ratio.expression, batch).outcome);
    CHECK(formula::render(ratio) == formula::render(ratio.expression));
    CHECK(formula::document(ratio).formula == formula::document(ratio.expression).formula);

    // The trace is the formula's own, and so is every other spelling: a
    // result named again, a dialect, a vocabulary and number options.
    CHECK(explained.trace.steps.size() == formula::explain<Gradient>(ratio.expression, batch).trace.steps.size());
    CHECK(formula::explain<Gradient>(ratio, batch).outcome == explained.outcome);
    auto const checkedExplained = formula::checked_explain(ratio, batch);
    REQUIRE(checkedExplained.has_value());
    CHECK(checkedExplained->outcome == explained.outcome);
    CHECK(formula::checked_explain<Gradient>(ratio, batch)->outcome == explained.outcome);
    CHECK(formula::render(ratio) == "h / L");
    CHECK(formula::render<formula::Dialect::LaTeX>(ratio) == formula::render<formula::Dialect::LaTeX>(ratio.expression));
    constexpr auto renamedRise = formula::vocabulary(formula::renames<Rise>("H"));
    CHECK(formula::render(ratio, renamedRise) == "H / L");
    CHECK(formula::render(ratio, formula::RenderOptions {}) == "h / L");
    CHECK(formula::document(ratio, renamedRise).formula == "H / L");
    CHECK(formula::document<formula::Dialect::Markdown>(ratio).formula
          == formula::document<formula::Dialect::Markdown>(ratio.expression).formula);

    // A value that publishes no dimension is not asked about one where it is
    // bound: the verb it is handed to judges it.
    constexpr auto boundRetry = formula::yields<Estimate>(fourAttempts);
    STATIC_REQUIRE(decltype(boundRetry)::valid);
    CHECK(formula::render(boundRetry) == formula::render(fourAttempts));
}

TEST_CASE("yields: around documented(), and as a calculation's definition", "[yields]")
{
    constexpr auto cited = formula::yields<Gradient>(
        formula::documented(var<Rise> / var<Run>, { .title = "Road gradient", .reference = "Example Standard 1:2020" }));
    STATIC_REQUIRE(formula::number_of(formula::checked_evaluate(cited, batch)) == formula::Rational { 163, 307 });
    constexpr auto definition = formula::define(ratio);
    STATIC_REQUIRE(std::is_same_v<typename decltype(definition)::quantity, Gradient>);

    // The citation inside is the formula's; the definition holds the formula
    // itself, as `define<Q>` of it does.
    CHECK(formula::document(cited).citations.size() == 1);
    STATIC_REQUIRE(
        std::is_same_v<std::remove_const_t<decltype(definition)>, decltype(formula::define<Gradient>(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::define<Gradient>(ratio)), std::remove_const_t<decltype(definition)>>);
}

TEST_CASE("yields: a series is evaluated for the quantity it is bound to", "[yields][series]")
{
    constexpr auto retainedInKilograms = formula::yields<RetainedKilograms>(formula::series<Retained, 5>);
    constexpr auto evaluated = formula::checked_evaluate_series(retainedInKilograms, inputs);
    STATIC_REQUIRE(std::is_same_v<std::remove_const_t<decltype(evaluated)>,
                                  std::expected<formula::SeriesOutcome<RetainedKilograms, 5>, formula::SeriesFailure>>);
    // 130 g is 13/100 kg, and the unmeasured element stays unmeasured.
    STATIC_REQUIRE(evaluated->element(0).value() == rat(13, 100));
    STATIC_REQUIRE(evaluated->element(2).is_absent());
    STATIC_REQUIRE(evaluated->element(4).value() == rat(7, 250));
    STATIC_REQUIRE(evaluated == formula::checked_evaluate_series<RetainedKilograms>(formula::series<Retained, 5>, inputs));
    STATIC_REQUIRE(formula::checked_evaluate_series<RetainedKilograms>(retainedInKilograms, inputs) == evaluated);

    auto const explained = formula::explain_series(retainedInKilograms, inputs);
    CHECK(explained.outcome == evaluated);
    CHECK(explained.trace.steps.size()
          == formula::explain_series<RetainedKilograms>(formula::series<Retained, 5>, inputs).trace.steps.size());
    CHECK(formula::explain_series<RetainedKilograms>(retainedInKilograms, inputs).outcome == evaluated);
}

TEST_CASE("yields: a rejection of outliers is evaluated for the quantity it is bound to", "[yields][rejection]")
{
    constexpr auto settledMass = formula::yields<Mass>(rejectionA);
    constexpr auto rejected = formula::checked_evaluate_rejection(settledMass, fixtureA);
    constexpr auto unbound = formula::checked_evaluate_rejection<Mass>(rejectionA, fixtureA);
    STATIC_REQUIRE(rejected->outcome().measurement().value() == rat(321, 8));
    STATIC_REQUIRE(rejected->outcome() == unbound->outcome());
    STATIC_REQUIRE(rejected->passes() == 3);
    STATIC_REQUIRE(rejected->rejected().size() == 2);
    STATIC_REQUIRE(rejected->rejected()[1] == formula::RejectedElement { 5, 2 });
    STATIC_REQUIRE(formula::checked_evaluate_rejection<Mass>(settledMass, fixtureA)->outcome() == unbound->outcome());

    auto const explained = formula::explain_rejection(settledMass, fixtureA);
    REQUIRE(explained.outcome.has_value());
    CHECK(explained.outcome->outcome() == unbound->outcome());
    CHECK(explained.trace.steps.size() == formula::explain_rejection<Mass>(rejectionA, fixtureA).trace.steps.size());
    CHECK(formula::explain_rejection<Mass>(settledMass, fixtureA).outcome->outcome() == unbound->outcome());
}

TEST_CASE("yields: every verb hands on the sink and the vocabulary it is given", "[yields][trace]")
{
    auto const written = [](formula::Trace<> const& recorded) {
        return formula::render_trace(recorded, { .maxSteps = 100 });
    };

    // A sink handed to a bound formula's verb hears what the unbound verb
    // tells it: the same steps, written the same way.
    auto const unbound = formula::traced(
        [&](auto recordingSink) { return formula::checked_evaluate<Gradient>(ratio.expression, batch, recordingSink); });
    REQUIRE(!unbound.trace.steps.empty());
    auto const checked =
        formula::traced([&](auto recordingSink) { return formula::checked_evaluate(ratio, batch, recordingSink); });
    CHECK(checked.outcome == unbound.outcome);
    CHECK(written(checked.trace) == written(unbound.trace));
    auto const thrown = formula::traced([&](auto recordingSink) { return formula::evaluate(ratio, batch, recordingSink); });
    CHECK(thrown.outcome == *unbound.outcome);
    CHECK(written(thrown.trace) == written(unbound.trace));

    constexpr auto retainedInKilograms = formula::yields<RetainedKilograms>(formula::series<Retained, 5>);
    auto const seriesUnbound = formula::traced([&](auto recordingSink) {
        return formula::checked_evaluate_series<RetainedKilograms>(formula::series<Retained, 5>, inputs, recordingSink);
    });
    REQUIRE(!seriesUnbound.trace.steps.empty());
    auto const seriesBound = formula::traced(
        [&](auto recordingSink) { return formula::checked_evaluate_series(retainedInKilograms, inputs, recordingSink); });
    CHECK(seriesBound.outcome == seriesUnbound.outcome);
    CHECK(written(seriesBound.trace) == written(seriesUnbound.trace));

    constexpr auto settledMass = formula::yields<Mass>(rejectionA);
    auto const rejectionUnbound = formula::traced(
        [&](auto recordingSink) { return formula::checked_evaluate_rejection<Mass>(rejectionA, fixtureA, recordingSink); });
    REQUIRE(!rejectionUnbound.trace.steps.empty());
    auto const rejectionBound = formula::traced(
        [&](auto recordingSink) { return formula::checked_evaluate_rejection(settledMass, fixtureA, recordingSink); });
    CHECK(written(rejectionBound.trace) == written(rejectionUnbound.trace));

    // A vocabulary handed to an explain twin writes the trace as it does for
    // the unbound formula -- and each renaming shows in the trace, so a
    // vocabulary left behind would too.
    constexpr auto renamedRise = formula::vocabulary(formula::renames<Rise>("H"));
    auto const renamed = written(formula::explain<Gradient>(ratio.expression, batch, renamedRise).trace);
    REQUIRE(renamed != written(formula::explain<Gradient>(ratio.expression, batch).trace));
    CHECK(written(formula::explain(ratio, batch, renamedRise).trace) == renamed);
    CHECK(written(formula::checked_explain(ratio, batch, renamedRise)->trace) == renamed);

    constexpr auto renamedRetained = formula::vocabulary(formula::renames<Retained>("R"));
    auto const renamedSeries =
        written(formula::explain_series<RetainedKilograms>(formula::series<Retained, 5>, inputs, renamedRetained).trace);
    REQUIRE(renamedSeries
            != written(formula::explain_series<RetainedKilograms>(formula::series<Retained, 5>, inputs).trace));
    CHECK(written(formula::explain_series(retainedInKilograms, inputs, renamedRetained).trace) == renamedSeries);

    constexpr auto renamedMass = formula::vocabulary(formula::renames<Mass>("m_s"));
    auto const renamedRejection = written(formula::explain_rejection<Mass>(rejectionA, fixtureA, renamedMass).trace);
    REQUIRE(renamedRejection != written(formula::explain_rejection<Mass>(rejectionA, fixtureA).trace));
    CHECK(written(formula::explain_rejection(settledMass, fixtureA, renamedMass).trace) == renamedRejection);
}

TEST_CASE("yields: a bound formula is an operand, standing for the formula it holds", "[yields]")
{
    // On either side of each arithmetic operator, beside a formula, a bare
    // `Rational` or another bound formula, and negated: the type the
    // `.expression` spelling gives.
    constexpr auto one = formula::number(rat(1));
    STATIC_REQUIRE(std::is_same_v<decltype(ratio + one), decltype(ratio.expression + one)>);
    STATIC_REQUIRE(std::is_same_v<decltype(one + ratio), decltype(one + ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio - one), decltype(ratio.expression - one)>);
    STATIC_REQUIRE(std::is_same_v<decltype(one - ratio), decltype(one - ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(var<Rise> * ratio), decltype(var<Rise> * ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio * var<Rise>), decltype(ratio.expression * var<Rise>)>);
    STATIC_REQUIRE(std::is_same_v<decltype(var<Rise> / ratio), decltype(var<Rise> / ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio / var<Rise>), decltype(ratio.expression / var<Rise>)>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio + rat(1)), decltype(ratio.expression + rat(1))>);
    STATIC_REQUIRE(std::is_same_v<decltype(rat(1) - ratio), decltype(rat(1) - ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio * rat(2)), decltype(ratio.expression * rat(2))>);
    STATIC_REQUIRE(std::is_same_v<decltype(rat(2) / ratio), decltype(rat(2) / ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio * ratio), decltype(ratio.expression * ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(-ratio), decltype(-ratio.expression)>);

    // Beside a series, a bound formula is broadcast as the formula it holds
    // is, and a bound series combines element by element as its series does.
    constexpr auto retainedInKilograms = formula::yields<RetainedKilograms>(formula::series<Retained, 5>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::series<Retained, 5> * ratio),
                                  decltype(formula::series<Retained, 5> * ratio.expression)>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(retainedInKilograms / rat(2)), decltype(retainedInKilograms.expression / rat(2))>);
    STATIC_REQUIRE(std::is_same_v<decltype(-retainedInKilograms), decltype(-retainedInKilograms.expression)>);

    // The value is the formula's: 307 mm times 163/307 is 163 mm.
    STATIC_REQUIRE(formula::checked_evaluate<Rise>(var<Run> * ratio, batch)
                   == formula::checked_evaluate<Rise>(var<Run> * ratio.expression, batch));
    STATIC_REQUIRE(formula::number_of(formula::checked_evaluate<Rise>(var<Run> * ratio, batch)) == rat(163));

    // Written as the formula it holds: the bound quantity is not named.
    CHECK(formula::render(var<Run> * ratio) == formula::render(var<Run> * ratio.expression));
}

namespace
{
using Load = formula::Quantity<struct YieldsLoadTag, "F", "maximum load", unit::Kilonewton>;
using Area = formula::Quantity<struct YieldsAreaTag, "A_c", "loaded area", unit::SquareMillimetre>;
using Strength = formula::Quantity<struct YieldsStrengthTag, "f_c", "compressive strength", unit::Megapascal>;
using SideA = formula::Quantity<struct YieldsSideATag, "a", "first side of the loaded face", unit::Millimetre>;
using SideB = formula::Quantity<struct YieldsSideBTag, "b", "second side of the loaded face", unit::Millimetre>;

constexpr auto loadedArea = formula::yields<Area>(var<SideA> * var<SideB>);
constexpr auto strength = formula::yields<Strength>(var<Load> / loadedArea);
} // namespace

TEST_CASE("yields: a bound formula inside another bound formula", "[yields]")
{
    STATIC_REQUIRE(std::is_same_v<decltype(strength.expression), decltype(var<Load> / loadedArea.expression)>);

    // 675 kN over a 150 mm by 150 mm face is 30 MPa.
    constexpr auto specimen = formula::environment(
        formula::Measured<SideA> { 150 }, formula::Measured<SideB> { 150 }, formula::Measured<Load> { 675 });
    constexpr auto result = formula::checked_evaluate(strength, specimen);
    STATIC_REQUIRE(std::is_same_v<std::remove_const_t<decltype(result)>,
                                  std::expected<formula::Outcome<Strength>, formula::ArithmeticError>>);
    REQUIRE(result.has_value());
    CHECK(result->measurement().value() == rat(30));

    // The same formula written in one piece.
    constexpr auto nested = formula::yields<Strength>(var<Load> / formula::yields<Area>(var<SideA> * var<SideB>));
    STATIC_REQUIRE(std::is_same_v<decltype(nested), decltype(strength)>);
    CHECK(formula::render(nested) == formula::render(var<Load> / (var<SideA> * var<SideB>) ));
}

TEST_CASE("yields: a bound formula is a comparand, standing for the formula it holds", "[yields]")
{
    constexpr auto limit = formula::constant<unit::One>(rat(9, 20));
    STATIC_REQUIRE(std::is_same_v<decltype(ratio < limit), decltype(ratio.expression < limit)>);
    STATIC_REQUIRE(std::is_same_v<decltype(limit < ratio), decltype(limit < ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio <= limit), decltype(ratio.expression <= limit)>);
    STATIC_REQUIRE(std::is_same_v<decltype(limit <= ratio), decltype(limit <= ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio > limit), decltype(ratio.expression > limit)>);
    STATIC_REQUIRE(std::is_same_v<decltype(limit > ratio), decltype(limit > ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio >= limit), decltype(ratio.expression >= limit)>);
    STATIC_REQUIRE(std::is_same_v<decltype(limit >= ratio), decltype(limit >= ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio == limit), decltype(ratio.expression == limit)>);
    STATIC_REQUIRE(std::is_same_v<decltype(limit == ratio), decltype(limit == ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio != limit), decltype(ratio.expression != limit)>);
    STATIC_REQUIRE(std::is_same_v<decltype(limit != ratio), decltype(limit != ratio.expression)>);
    STATIC_REQUIRE(std::is_same_v<decltype(ratio == ratio), decltype(ratio.expression == ratio.expression)>);

    // A comparison of formulas is a predicate, not a truth value, so neither
    // a formula nor a bound one is equality-comparable in a concept's sense.
    using Bound = std::remove_const_t<decltype(ratio)>;
    using Held = std::remove_const_t<decltype(ratio.expression)>;
    using Limit = std::remove_const_t<decltype(limit)>;
    STATIC_REQUIRE(!std::equality_comparable<Held>);
    STATIC_REQUIRE(!std::equality_comparable<Bound>);
    STATIC_REQUIRE(!std::equality_comparable_with<Held, Limit>);
    STATIC_REQUIRE(!std::equality_comparable_with<Bound, Limit>);

    // A constraint over a bound formula checks what one over the formula
    // checks: 163/307 is above 9/20.
    constexpr auto tooShallow = formula::constraint(ratio >= limit, formula::Verdict { "too shallow" });
    constexpr auto heldTooShallow = formula::constraint(ratio.expression >= limit, formula::Verdict { "too shallow" });
    STATIC_REQUIRE(std::is_same_v<decltype(tooShallow), decltype(heldTooShallow)>);
    STATIC_REQUIRE(formula::check(tooShallow, batch).is_satisfied());
    STATIC_REQUIRE(formula::check(tooShallow, batch) == formula::check(heldTooShallow, batch));
}

namespace
{
inline constexpr auto threePlaces = formula::DecimalPlaces { 3 };
inline constexpr auto threeDigits = formula::SignificantDigits { 3 };
inline constexpr auto halfEven = formula::RoundingMode::HalfEven;
inline constexpr formula::DecimalRounding thousandth { unit::One, threePlaces, halfEven };
inline constexpr formula::SignificantRounding threeFigures { unit::One, threeDigits, halfEven };
inline constexpr formula::Citation sourceCited { .title = "Example Standard", .section = "4.1" };

// A length bound to a quantity of its own, for the lookups and the snap:
// 163 + 307 = 470 mm.
using Span = formula::Quantity<struct YieldsSpanTag, "l", "span", unit::Millimetre>;
constexpr auto span = formula::yields<Span>(var<Rise> + var<Run>);
inline constexpr formula::BandTable<2> spanBands { formula::band(0, 1, 300, 1), formula::band(300, 1, 900, 1) };
inline constexpr formula::BreakpointTable<2> spanPoints { formula::breakpoint(0), formula::breakpoint(900) };
inline constexpr formula::SampleSizeTable<3> countSizes { 3, 4, 5 };
} // namespace

TEST_CASE("yields: a bound formula as the operand of a function, a rounding or an escape", "[yields]")
{
    // function.hpp
    STATIC_REQUIRE(std::is_same_v<decltype(formula::pow<2>(ratio)), decltype(formula::pow<2>(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::sqrt(ratio)), decltype(formula::sqrt(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::cbrt(ratio)), decltype(formula::cbrt(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::root<5>(ratio)), decltype(formula::root<5>(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::ln(ratio)), decltype(formula::ln(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::log10(ratio)), decltype(formula::log10(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::exp(ratio)), decltype(formula::exp(ratio.expression))>);

    // precision.hpp
    constexpr auto Repeatability = formula::PrecisionKind::Repeatability;
    STATIC_REQUIRE(std::is_same_v<decltype(formula::abs(ratio)), decltype(formula::abs(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::precision_limit<Repeatability>(span, var<Rise>)),
                                  decltype(formula::precision_limit<Repeatability>(span.expression, var<Rise>))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::precision_limit<Repeatability>(var<Rise>, span)),
                                  decltype(formula::precision_limit<Repeatability>(var<Rise>, span.expression))>);

    // rounding_node.hpp, rounded_root.hpp and rounded_transcendental.hpp
    STATIC_REQUIRE(std::is_same_v<decltype(formula::rounded<unit::One, threePlaces, halfEven>(ratio)),
                                  decltype(formula::rounded<unit::One, threePlaces, halfEven>(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::rounded<thousandth>(ratio)),
                                  decltype(formula::rounded<thousandth>(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::rounded_to_digits<unit::One, threeDigits, halfEven>(ratio)),
                                  decltype(formula::rounded_to_digits<unit::One, threeDigits, halfEven>(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::rounded_to_digits<threeFigures>(ratio)),
                                  decltype(formula::rounded_to_digits<threeFigures>(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::rounded_sqrt<unit::One, threePlaces, halfEven>(ratio)),
                                  decltype(formula::rounded_sqrt<unit::One, threePlaces, halfEven>(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::rounded_sqrt<thousandth>(ratio)),
                                  decltype(formula::rounded_sqrt<thousandth>(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::rounded_ln<threePlaces, halfEven>(ratio)),
                                  decltype(formula::rounded_ln<threePlaces, halfEven>(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::rounded_log10<threePlaces, halfEven>(ratio)),
                                  decltype(formula::rounded_log10<threePlaces, halfEven>(ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::rounded_exp<threePlaces, halfEven>(ratio)),
                                  decltype(formula::rounded_exp<threePlaces, halfEven>(ratio.expression))>);

    // escape.hpp
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::numeric_value_of<unit::One, "a gradient is a bare number">(ratio)),
                       decltype(formula::numeric_value_of<unit::One, "a gradient is a bare number">(ratio.expression))>);

    // The value through a bound operand is the formula's: the square of
    // 163/307 times (307/163) squared is 1.
    STATIC_REQUIRE(formula::number_of(formula::checked_evaluate<Gradient>(
                       formula::pow<2>(ratio) * formula::pow<2>(var<Run> / var<Rise>), batch))
                   == rat(1));
}

TEST_CASE("yields: a bound formula documented, and as a branch of when", "[yields]")
{
    // citation.hpp: the citation wraps the formula the bound one holds; the
    // binding does not carry through.
    STATIC_REQUIRE(std::is_same_v<decltype(formula::documented(ratio, sourceCited)),
                                  decltype(formula::documented(ratio.expression, sourceCited))>);
    constexpr auto cited = formula::documented(ratio, { .title = "Road gradient", .reference = "Example Standard 1:2020" });
    CHECK(formula::document(cited).citations.size() == 1);

    // conditional.hpp
    constexpr auto half = formula::number(rat(1, 2));
    constexpr auto steep = ratio > half;
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::when(steep, ratio, half)), decltype(formula::when(steep, ratio.expression, half))>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::when(steep, half, ratio)), decltype(formula::when(steep, half, ratio.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::when(steep, ratio, ratio)),
                                  decltype(formula::when(steep, ratio.expression, ratio.expression))>);
}

TEST_CASE("yields: a bound formula as a lookup's key, a count or a snapped value", "[yields]")
{
    // lookup.hpp
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::banded_lookup<unit::Millimetre, spanBands, unit::One>(span, { 1, 2 })),
                       decltype(formula::banded_lookup<unit::Millimetre, spanBands, unit::One>(span.expression, { 1, 2 }))>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::interpolating_lookup<unit::Millimetre, spanPoints, unit::One>(span, { 1, 2 })),
                       decltype(formula::interpolating_lookup<unit::Millimetre, spanPoints, unit::One>(span.expression,
                                                                                                       { 1, 2 }))>);

    // critical_value.hpp
    STATIC_REQUIRE(std::is_same_v<decltype(formula::critical_value<countSizes, unit::One>(ratio, { 1, 2, 3 })),
                                  decltype(formula::critical_value<countSizes, unit::One>(ratio.expression, { 1, 2, 3 }))>);

    // snap.hpp
    constexpr auto TowardLower = formula::SnapTie::TowardLower;
    STATIC_REQUIRE(std::is_same_v<decltype(formula::snapped<unit::Millimetre, spanPoints, TowardLower>(span)),
                                  decltype(formula::snapped<unit::Millimetre, spanPoints, TowardLower>(span.expression))>);

    // The key read is the formula's: 470 mm is in the second band.
    STATIC_REQUIRE(formula::number_of(formula::checked_evaluate<Gradient>(
                       formula::banded_lookup<unit::Millimetre, spanBands, unit::One>(span, { 1, 2 }), batch))
                   == rat(2));
}

TEST_CASE("yields: a bound series in the series, statistics and conformity builders", "[yields][series]")
{
    constexpr auto retainedInKilograms = formula::yields<RetainedKilograms>(formula::series<Retained, 5>);
    constexpr auto held = retainedInKilograms.expression;
    using Held = std::remove_const_t<decltype(held)>;
    STATIC_REQUIRE(std::is_same_v<Held, std::remove_const_t<decltype(formula::series<Retained, 5>)>>);

    // series.hpp
    constexpr auto FromLast = formula::CumulativeDirection::FromLast;
    STATIC_REQUIRE(std::is_same_v<decltype(formula::cumulative<FromLast>(retainedInKilograms)),
                                  decltype(formula::cumulative<FromLast>(held))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::sum(retainedInKilograms)), decltype(formula::sum(held))>);
    constexpr formula::PlacesTable<5> places { threePlaces, threePlaces, threePlaces, threePlaces, threePlaces };
    STATIC_REQUIRE(std::is_same_v<decltype(formula::rounded_elementwise<unit::Gram, places, halfEven>(retainedInKilograms)),
                                  decltype(formula::rounded_elementwise<unit::Gram, places, halfEven>(held))>);
    constexpr formula::DecimalRounding thousandthGram { unit::Gram, threePlaces, halfEven };
    STATIC_REQUIRE(std::is_same_v<decltype(formula::rounded_elementwise<thousandthGram>(retainedInKilograms)),
                                  decltype(formula::rounded_elementwise<thousandthGram>(held))>);

    // statistics.hpp
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::sample_count(retainedInKilograms)), decltype(formula::sample_count(held))>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::sample_mean(retainedInKilograms)), decltype(formula::sample_mean(held))>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::sample_variance(retainedInKilograms)), decltype(formula::sample_variance(held))>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::sample_range(retainedInKilograms)), decltype(formula::sample_range(held))>);

    // conformity.hpp
    constexpr formula::LimitRow nonNegative { formula::limit(rat(0)), formula::unbounded };
    constexpr formula::Envelope<5> envelope { nonNegative, nonNegative, nonNegative, nonNegative, nonNegative };
    constexpr formula::Verdict negative { "a negative mass" };
    STATIC_REQUIRE(std::is_same_v<decltype(formula::conformity<unit::Gram>(retainedInKilograms, envelope, negative)),
                                  decltype(formula::conformity<unit::Gram>(held, envelope, negative))>);

    // The total is the series' own: absent, as one screen was not measured.
    STATIC_REQUIRE(formula::checked_evaluate<Retained>(formula::sum(retainedInKilograms), inputs)
                   == formula::checked_evaluate<Retained>(formula::sum(held), inputs));
}

TEST_CASE("yields: a bound formula in a rejection of outliers and a retry", "[yields][rejection][retry]")
{
    // rejection.hpp
    constexpr auto sixPercentOfMean = formula::yields<Mass>(rat(6, 100) * formula::pass_mean<Mass>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::deviation_from_mean(sixPercentOfMean)),
                                  decltype(formula::deviation_from_mean(sixPercentOfMean.expression))>);
    constexpr auto factor = formula::yields<Gradient>(formula::number(rat(7, 4)));
    STATIC_REQUIRE(std::is_same_v<decltype(formula::deviation_in_stddevs(factor)),
                                  decltype(formula::deviation_in_stddevs(factor.expression))>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::gap_to_range(factor)), decltype(formula::gap_to_range(factor.expression))>);
    constexpr auto sample = formula::yields<Mass>(formula::series<Mass, 6>);
    constexpr auto overBound = formula::without_outliers<MostExtreme, Keep, formula::AtMost<2>, formula::KeepAtLeast<4>>(
        sample, sixPercent, repeatTest, exampleCited);
    STATIC_REQUIRE(std::is_same_v<decltype(overBound), decltype(rejectionA)>);

    // The rejection over a bound sample settles where rejectionA does.
    constexpr auto settled = formula::checked_evaluate_rejection<Mass>(overBound, fixtureA);
    REQUIRE(settled.has_value());
    CHECK(settled->outcome().measurement().value() == rat(321, 8));

    // retry.hpp
    constexpr auto fromZero = formula::yields<Estimate>(formula::constant<unit::Gram>(rat(0)));
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::starting_from(fromZero)), decltype(formula::starting_from(fromZero.expression))>);
    constexpr auto attempt = formula::yields<Estimate>(formula::constant<unit::Gram>(rat(152, 25))
                                                       + formula::previous_attempt<Estimate> / rat(2));
    constexpr auto risesLittle =
        formula::previous_attempt<Estimate> - formula::this_attempt<Estimate> >= formula::constant<unit::Gram>(rat(-19, 25));
    constexpr formula::Verdict repeat { "repeat the determination" };
    constexpr auto AtFirstAttempt = formula::FirstJudged::AtFirstAttempt;
    constexpr auto bound = formula::retry<Estimate, 4, AtFirstAttempt>(
        formula::starting_from(fromZero), attempt, risesLittle, repeat, exampleCited);
    STATIC_REQUIRE(std::is_same_v<decltype(bound), decltype(fourAttempts)>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::retry<Estimate, 4, AtFirstAttempt>(attempt, risesLittle, repeat, exampleCited)),
                       decltype(formula::retry<Estimate, 4, AtFirstAttempt>(
                           attempt.expression, risesLittle, repeat, exampleCited))>);
}

namespace
{
inline constexpr formula::Citation fitCited { .reference = "Example Standard 12" };
inline constexpr auto riseRun = formula::curve(formula::series<Rise, 4>, formula::series<Run, 4>);
inline constexpr formula::BandTable<2> runClasses { formula::band(0, 1, 300, 1), formula::band(300, 1, 900, 1) };
} // namespace

TEST_CASE("yields: a bound formula in a curve, an opaque call and a fit", "[yields][opaque]")
{
    // curve.hpp
    constexpr auto runs = formula::yields<Run>(formula::series<Run, 4>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::curve(formula::series<Rise, 4>, runs)),
                                  decltype(formula::curve(formula::series<Rise, 4>, runs.expression))>);
    constexpr auto boundCurve = formula::yields<Run>(riseRun);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::interpolate_at(boundCurve, span)),
                                  decltype(formula::interpolate_at(riseRun, span.expression))>);
    constexpr auto NonDecreasing = formula::Monotone::NonDecreasing;
    STATIC_REQUIRE(std::is_same_v<decltype(formula::splice<NonDecreasing>(boundCurve, riseRun)),
                                  decltype(formula::splice<NonDecreasing>(riseRun, riseRun))>);

    // opaque.hpp
    STATIC_REQUIRE(std::is_same_v<decltype(formula::opaque<formula::LinearLeastSquares>(fitCited, boundCurve)),
                                  decltype(formula::opaque<formula::LinearLeastSquares>(fitCited, riseRun))>);
    constexpr auto boundFit = formula::yields<Run>(formula::linear_least_squares(riseRun, fitCited));
    STATIC_REQUIRE(std::is_same_v<decltype(formula::opaque_output<"intercept">(boundFit)),
                                  decltype(formula::opaque_output<"intercept">(boundFit.expression))>);
    constexpr auto twoPlaces = formula::DecimalPlaces { 2 };
    constexpr formula::DecimalRounding hundredthMillimetre { unit::Millimetre, twoPlaces, halfEven };
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::rounded_output<"intercept", unit::Millimetre, twoPlaces, halfEven>(boundFit)),
                       decltype(formula::rounded_output<"intercept", unit::Millimetre, twoPlaces, halfEven>(
                           boundFit.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::rounded_output<"intercept", hundredthMillimetre>(boundFit)),
                                  decltype(formula::rounded_output<"intercept", hundredthMillimetre>(boundFit.expression))>);

    // least_squares.hpp
    STATIC_REQUIRE(std::is_same_v<decltype(formula::linear_least_squares(boundCurve, fitCited)),
                                  decltype(formula::linear_least_squares(riseRun, fitCited))>);
    constexpr auto runObservations = formula::yields<Run>(formula::observations<Run, 8>);
    constexpr auto riseObservations = formula::observations<Rise, 8>;
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::linear_least_squares(riseObservations, runObservations, fitCited)),
                       decltype(formula::linear_least_squares(riseObservations, runObservations.expression, fitCited))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::regressors(riseObservations, runObservations)),
                                  decltype(formula::regressors(riseObservations, runObservations.expression))>);
    STATIC_REQUIRE(std::is_same_v<decltype(formula::multiple_least_squares(
                                      formula::regressors(riseObservations), runObservations, fitCited)),
                                  decltype(formula::multiple_least_squares(
                                      formula::regressors(riseObservations), runObservations.expression, fitCited))>);

    // binning.hpp
    STATIC_REQUIRE(std::is_same_v<decltype(formula::binned<unit::Millimetre, runClasses>(runObservations)),
                                  decltype(formula::binned<unit::Millimetre, runClasses>(runObservations.expression))>);
}

namespace
{
struct SteepVariant
{
};
struct YieldsReference
{
};
struct YieldsBatch
{
};
} // namespace

TEST_CASE("yields: a bound formula as a variant, an overlay's formula and a read from a record", "[yields][method]")
{
    // method.hpp
    STATIC_REQUIRE(std::is_same_v<decltype(formula::variant<SteepVariant>(ratio)),
                                  decltype(formula::variant<SteepVariant>(ratio.expression))>);

    // overlay.hpp
    STATIC_REQUIRE(std::is_same_v<decltype(formula::replace_variant<SteepVariant>(ratio, sourceCited)),
                                  decltype(formula::replace_variant<SteepVariant>(ratio.expression, sourceCited))>);
    constexpr auto halfRun = formula::yields<Run>(var<Run> / rat(2));
    STATIC_REQUIRE(std::is_same_v<decltype(formula::add_derived<Rise>(halfRun, sourceCited)),
                                  decltype(formula::add_derived<Rise>(halfRun.expression, sourceCited))>);

    // record.hpp
    STATIC_REQUIRE(std::is_same_v<decltype(formula::from_record<YieldsReference>(ratio)),
                                  decltype(formula::from_record<YieldsReference>(ratio.expression))>);
    constexpr auto sameBatch = formula::same_lineage<YieldsBatch>();
    STATIC_REQUIRE(std::is_same_v<decltype(formula::from_record<YieldsReference>(ratio, sameBatch)),
                                  decltype(formula::from_record<YieldsReference>(ratio.expression, sameBatch))>);
}
