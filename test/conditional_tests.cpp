// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Strength: formula::Quantity<Strength, "f", "measured strength", unit::Megapascal>
{
};

[[nodiscard]] constexpr auto strengthOf(long long value)
{
    return formula::environment(formula::Measured<Strength> { formula::Rational { value } });
}
} // namespace

TEST_CASE("a threshold selects between two formulas", "[conditional]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto chosen = formula::when(overFifty,
                                          var<Strength> * formula::number(formula::Rational { 2 }),
                                          var<Strength> * formula::number(formula::Rational { 4 }));

    constexpr auto high = formula::checked_evaluate<Strength>(chosen, strengthOf(60));
    REQUIRE(high.has_value());
    CHECK(high->measurement().value() == formula::Rational { 120 });

    constexpr auto low = formula::checked_evaluate<Strength>(chosen, strengthOf(40));
    REQUIRE(low.has_value());
    CHECK(low->measurement().value() == formula::Rational { 160 });
}

TEST_CASE("the branch not taken is never evaluated", "[conditional]")
{
    // The else branch divides by zero. If both branches were evaluated, this
    // would fail; because only the selected one runs, it does not. This is
    // not a micro-optimisation -- a guarded formula exists precisely because
    // the other branch is invalid for these inputs.
    constexpr auto nonZero = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 0 });
    constexpr auto guarded = formula::when(nonZero,
                                           var<Strength>,
                                           var<Strength> / formula::number(formula::Rational { 0 }));

    constexpr auto result = formula::checked_evaluate<Strength>(guarded, strengthOf(60));
    REQUIRE(result.has_value());
    CHECK(result->measurement().value() == formula::Rational { 60 });

    // The value alone does not test this test's own title. An evaluator that
    // ran both branches and selected afterwards -- which is precisely the
    // implementation conditional.hpp's file comment says this is not -- would
    // still answer 60: the else branch's error would be produced and thrown
    // away. So count the steps, which is the one place the difference shows.
    //
    // Four, and no more: the predicate's two sides, the one branch that ran,
    // and the conditional itself. Evaluating the else branch as well would
    // add three (its variable, its zero, and the division) for seven.
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const traced = formula::checked_evaluate_si<formula::Rational>(guarded, strengthOf(60), sink);

    REQUIRE(traced.has_value());
    REQUIRE(trace.steps.size() == 4);
    CHECK(trace.steps[trace.root()].branch == formula::Branch::Then);
    // Nothing in the derivation failed, which is a second way of saying the
    // divide-by-zero branch was never dispatched rather than dispatched and
    // discarded.
    for (auto const& step: trace.steps)
        CHECK_FALSE(step.error.has_value());
}

TEST_CASE("an absent predicate makes the result absent, not the else branch", "[conditional]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto chosen =
        formula::when(overFifty, var<Strength>, formula::constant<unit::Megapascal>(formula::Rational { 0 }));
    constexpr auto environment = formula::environment(formula::Measured<Strength>::absent());

    constexpr auto result = formula::checked_evaluate<Strength>(chosen, environment);
    REQUIRE(result.has_value());
    CHECK(result->is_empty());
}
