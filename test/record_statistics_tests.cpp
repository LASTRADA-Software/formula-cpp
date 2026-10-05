// SPDX-License-Identifier: Apache-2.0
//
// Statistics, outlier rejections and precision limits, read from
// another record. Each records steps of its own, on paths of its own -- a
// rejection's passes and verdict through `push_rejection_step`, a precision
// limit's level through `precision_level_produced` -- and every step read
// inside a `from_record` scope must say which record it was read from, on
// every path (`RecordingSink::stamp_origin`).
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Reference
{
};

struct Mass: formula::Quantity<Mass, "m", "mass of a determination", unit::Gram>
{
};
struct ResultA: formula::Quantity<ResultA, "x_A", "first determination", unit::Gram>
{
};
struct ResultB: formula::Quantity<ResultB, "x_B", "second determination", unit::Gram>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

constexpr formula::Measured<Mass> grams(formula::Rational value)
{
    return formula::Measured<Mass> { value };
}

// The reference holds the statistics fixture A, 40.2, 39.8, 40.5, 44.0, 40.0 and
// 43.3 g, and fixture P's pair, 40 g and 40.905 g. This record holds other
// values of each, so a step read from the wrong record gives another number.
inline constexpr auto here = formula::environment(
    formula::measured_series<Mass>(grams(rat(41)), grams(rat(41)), grams(rat(41)), grams(rat(41)), grams(rat(41)),
                                   grams(rat(41))),
    formula::Measured<ResultA> { rat(30) }, formula::Measured<ResultB> { rat(30) });
inline constexpr auto there = formula::environment(
    formula::measured_series<Mass>(grams(rat(402, 10)), grams(rat(398, 10)), grams(rat(405, 10)), grams(rat(44)),
                                   grams(rat(40)), grams(rat(433, 10))),
    formula::Measured<ResultA> { rat(40) }, formula::Measured<ResultB> { rat(40905, 1000) });

inline constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));

/// Every step of @p recorded says it was read from the reference.
bool every_step_from_reference(formula::Trace<> const& recorded)
{
    for (formula::Step<> const& step: recorded.steps)
    {
        std::optional<formula::RecordOrigin> const readFrom = formula::origin_of(recorded, step);
        if (!readFrom.has_value() || readFrom->role() != "Reference")
            return false;
    }
    return !recorded.steps.empty();
}

/// How many steps of @p recorded are of @p kind.
std::size_t steps_of(formula::Trace<> const& recorded, formula::StepKind kind)
{
    std::size_t counted = 0;
    for (formula::Step<> const& step: recorded.steps)
        counted += step.kind == kind ? 1 : 0;
    return counted;
}
} // namespace

TEST_CASE("a statistic read from another record is stamped with that record", "[record-statistics]")
{
    // Fixture A's mean, 41.3 g; this record's would be 41 g.
    constexpr auto meanThere = formula::from_record<Reference>(formula::sample_mean(formula::series<Mass, 6>));
    formula::Trace<> recorded {};
    auto const evaluated =
        formula::checked_evaluate_si<formula::Rational>(meanThere, records, formula::RecordingSink { recorded });
    REQUIRE(evaluated.has_value());
    CHECK(**evaluated == rat(413, 10'000)); // 41.3 g, in kilograms
    CHECK(steps_of(recorded, formula::StepKind::SampleMean) == 1);
    CHECK(every_step_from_reference(recorded));
}

TEST_CASE("an outlier rejection read from another record is stamped with that record, pass by pass",
          "[record-statistics]")
{
    // The statistics fixture A under a 6 % deviation from each pass's mean: pass
    // 1 rejects 44.0 g, pass 2 rejects 43.3 g, pass 3 settles at 321/8 g.
    // Every pass, rejection and verdict is a step `push_rejection_step`
    // records, and each is the reference's.
    constexpr auto survivorsThere = formula::from_record<Reference>(formula::sample_mean(
        formula::without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<2>,
                                  formula::KeepAtLeast<4>>(
            formula::series<Mass, 6>, formula::deviation_from_mean(rat(6, 100) * formula::pass_mean<Mass>),
            formula::Verdict { "discard the determinations and repeat the test" })));
    formula::Trace<> recorded {};
    auto const evaluated =
        formula::checked_evaluate_si<formula::Rational>(survivorsThere, records, formula::RecordingSink { recorded });
    REQUIRE(evaluated.has_value());
    CHECK(**evaluated == rat(321, 8'000)); // 321/8 g, in kilograms
    CHECK(steps_of(recorded, formula::StepKind::RejectionPass) >= 1);
    CHECK(steps_of(recorded, formula::StepKind::OutlierRejected) == 2);
    CHECK(steps_of(recorded, formula::StepKind::RejectionSettled) == 1);
    INFO(formula::render_trace(recorded, { .maxSteps = 60 }));
    CHECK(every_step_from_reference(recorded));
}

TEST_CASE("a precision limit read from another record is stamped with that record, its level included",
          "[record-statistics]")
{
    // The statistics fixture P: the level is the pair's mean, 40.4525 g, and r =
    // 0.1 g + level / 50 = 0.90905 g. The level's first pass is a step
    // `precision_level_produced` records, and it is the reference's.
    constexpr auto limitThere = formula::from_record<Reference>(formula::precision_limit<formula::PrecisionKind::Repeatability>(
        (var<ResultA> + var<ResultB>) / rat(2),
        formula::constant<unit::Gram>(rat(1, 10)) + rat(1, 50) * formula::precision_level<ResultA>));
    formula::Trace<> recorded {};
    auto const evaluated =
        formula::checked_evaluate_si<formula::Rational>(limitThere, records, formula::RecordingSink { recorded });
    REQUIRE(evaluated.has_value());
    CHECK(**evaluated == rat(90905, 100'000'000)); // 0.90905 g, in kilograms
    CHECK(steps_of(recorded, formula::StepKind::PrecisionLevel) >= 1);
    CHECK(steps_of(recorded, formula::StepKind::PrecisionLimit) == 1);
    INFO(formula::render_trace(recorded, { .maxSteps = 60 }));
    CHECK(every_step_from_reference(recorded));
}
