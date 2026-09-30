// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::var;
using WaterVolume = formula::Quantity<struct YieldsWaterTag, "V_w", "effective water content", unit::Litre>;
using CementVolume = formula::Quantity<struct YieldsCementTag, "V_c", "cement content", unit::Litre>;
using WaterCementRatio = formula::Quantity<struct YieldsRatioTag, "w/c", "ratio of water to cement", unit::One>;

constexpr auto ratio = formula::yields<WaterCementRatio>(var<WaterVolume> / var<CementVolume>);
constexpr auto batch = formula::environment(formula::Measured<WaterVolume> { 163 }, formula::Measured<CementVolume> { 307 });

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
                                  std::expected<formula::Outcome<WaterCementRatio>, formula::ArithmeticError>>);
    STATIC_REQUIRE(formula::checked_evaluate(ratio, batch)
                   == formula::checked_evaluate<WaterCementRatio>(ratio.expression, batch));
    STATIC_REQUIRE(formula::checked_evaluate<WaterCementRatio>(ratio, batch) == formula::checked_evaluate(ratio, batch));
    STATIC_REQUIRE(formula::number_of(formula::evaluate(ratio, batch)) == formula::Rational { 163, 307 });
    STATIC_REQUIRE(formula::evaluate<WaterCementRatio>(ratio, batch) == formula::evaluate(ratio, batch));
    STATIC_REQUIRE(std::is_same_v<typename decltype(ratio)::quantity, WaterCementRatio>);
}

TEST_CASE("yields: explain, render and document see the formula itself", "[yields]")
{
    auto const explained = formula::explain(ratio, batch);
    CHECK(explained.outcome == formula::explain<WaterCementRatio>(ratio.expression, batch).outcome);
    CHECK(formula::render(ratio) == formula::render(ratio.expression));
    CHECK(formula::document(ratio).formula == formula::document(ratio.expression).formula);

    // The trace is the formula's own, and so is every other spelling: a
    // result named again, a dialect, a vocabulary and number options.
    CHECK(explained.trace.steps.size() == formula::explain<WaterCementRatio>(ratio.expression, batch).trace.steps.size());
    CHECK(formula::explain<WaterCementRatio>(ratio, batch).outcome == explained.outcome);
    auto const checkedExplained = formula::checked_explain(ratio, batch);
    REQUIRE(checkedExplained.has_value());
    CHECK(checkedExplained->outcome == explained.outcome);
    CHECK(formula::checked_explain<WaterCementRatio>(ratio, batch)->outcome == explained.outcome);
    CHECK(formula::render(ratio) == "V_w / V_c");
    CHECK(formula::render<formula::Dialect::LaTeX>(ratio) == formula::render<formula::Dialect::LaTeX>(ratio.expression));
    constexpr auto renamedWater = formula::vocabulary(formula::renames<WaterVolume>("W"));
    CHECK(formula::render(ratio, renamedWater) == "W / V_c");
    CHECK(formula::render(ratio, formula::RenderOptions {}) == "V_w / V_c");
    CHECK(formula::document(ratio, renamedWater).formula == "W / V_c");
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
    constexpr auto cited = formula::yields<WaterCementRatio>(formula::documented(
        var<WaterVolume> / var<CementVolume>, { .title = "Water/cement ratio", .reference = "Example Standard 1:2020" }));
    STATIC_REQUIRE(formula::number_of(formula::checked_evaluate(cited, batch)) == formula::Rational { 163, 307 });
    constexpr auto definition = formula::define(ratio);
    STATIC_REQUIRE(std::is_same_v<typename decltype(definition)::quantity, WaterCementRatio>);

    // The citation inside is the formula's; the definition holds the formula
    // itself, as `define<Q>` of it does.
    CHECK(formula::document(cited).citations.size() == 1);
    STATIC_REQUIRE(std::is_same_v<std::remove_const_t<decltype(definition)>,
                                  decltype(formula::define<WaterCementRatio>(ratio.expression))>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(formula::define<WaterCementRatio>(ratio)), std::remove_const_t<decltype(definition)>>);
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
