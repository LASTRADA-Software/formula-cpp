// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/trace.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <expected>
#include <type_traits>
#include <vector>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Mass: formula::Quantity<Mass, "m", "specimen mass", unit::Kilogram>
{
};
struct Volume: formula::Quantity<Volume, "V", "specimen volume", unit::CubicMetre>
{
};
struct Density: formula::Quantity<Density, "rho", "bulk density", formula::coherent(formula::dim::Density)>
{
};
struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "water volume", unit::Litre>
{
};
struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", unit::Millimetre>
{
};
struct Strength: formula::Quantity<Strength, "f", "measured strength", unit::Megapascal>
{
};

[[nodiscard]] auto environmentOf(long long mass, long long volume)
{
    return formula::environment(formula::Measured<Mass> { formula::Rational { mass } },
                                formula::Measured<Volume> { formula::Rational { volume } });
}

// The predicate every phase-8 conditional test below shares: strength over 50
// MPa. Kept at namespace scope so the mutation test (further down) can name
// its exact type.
constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
constexpr auto chosen = formula::when(overFifty, var<Strength>, var<Strength> * formula::Rational { 2 });

[[nodiscard]] auto strengthOf(formula::Rational value)
{
    return formula::environment(formula::Measured<Strength> { value });
}

// The constraint every Constraint-step test below shares: strength at least
// 30 MPa, phrased the way a standard's rejection rule reads. Kept at
// namespace scope for the same reason `overFifty`/`chosen` above are.
constexpr auto atLeastThirty =
    formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 30 }),
                        formula::Verdict { "reject the specimen" });

// The one-operand arity: the predicate's left side divides by a measured
// zero, so its right side is never dispatched -- the same shape
// trace_render_tests.cpp's own one-operand Conditional case uses.
constexpr auto leftSideErrors =
    formula::constraint((var<Strength> / formula::number(formula::Rational { 0 }))
                             > formula::constant<unit::Megapascal>(formula::Rational { 0 }),
                        formula::Verdict { "result is unusable" });

// The two-operand arity that is still Invalid: the left side resolves, so
// the right side is dispatched, and the right side is the one that divides
// by a measured zero.
constexpr auto rightSideErrors =
    formula::constraint(var<Strength> > (var<Strength> / formula::number(formula::Rational { 0 })),
                        formula::Verdict { "result is unusable" });

// A second constraint over the independent Diameter quantity, so a set of
// two checked together can fail differently on each -- the only way to tell
// which Constraint step swallowed which operands.
constexpr auto diameterAtMost100 =
    formula::constraint(var<Diameter> <= formula::constant<unit::Millimetre>(formula::Rational { 100 }),
                        formula::Verdict { "specimen exceeds diameter tolerance" });
} // namespace

TEST_CASE("a trace records one step per node, children before parents", "[trace]")
{
    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(density, environmentOf(6, 3), sink);

    REQUIRE(result.has_value());
    REQUIRE(trace.steps.size() == 4);

    // Post-order: m, m^2, V, then the division.
    CHECK(trace.steps[0].kind == formula::StepKind::Variable);
    CHECK(trace.steps[0].symbol == "m");
    CHECK(trace.steps[0].value == formula::Rational { 6 });

    CHECK(trace.steps[1].kind == formula::StepKind::Power);
    CHECK(trace.steps[1].exponent == 2);
    CHECK(trace.steps[1].value == formula::Rational { 36 });
    REQUIRE(trace.steps[1].operands.size() == 1);
    CHECK(trace.steps[1].operands[0] == 0);

    CHECK(trace.steps[2].kind == formula::StepKind::Variable);
    CHECK(trace.steps[2].symbol == "V");

    CHECK(trace.steps[3].kind == formula::StepKind::Divide);
    CHECK(trace.steps[3].value == formula::Rational { 12 });
    REQUIRE(trace.steps[3].operands.size() == 2);
    CHECK(trace.steps[3].operands[0] == 1);
    CHECK(trace.steps[3].operands[1] == 2);

    // The root is the last step: nothing claimed it.
    CHECK(trace.root() == 3);
}

TEST_CASE("a step records the unit its value was declared in", "[trace]")
{
    // 180 l and 500 ml are both volumes, so both are stored as cubic metres --
    // the coherent SI unit of their dimension, and the only scale on which the
    // addition means anything. `unit` is what remembers that nobody typed
    // cubic metres.
    constexpr auto mix = var<WaterVolume> + formula::constant<unit::Millilitre>(formula::Rational { 500 });
    auto const environment = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(mix, environment, sink);

    REQUIRE(result.has_value());
    REQUIRE(trace.steps.size() == 3);

    // A variable: the unit its quantity is declared in. The value beside it is
    // the same volume in the coherent SI unit -- 180 l *is* 9/50 m3 -- which is
    // exactly why the unit has to be recorded separately.
    CHECK(trace.steps[0].kind == formula::StepKind::Variable);
    CHECK(trace.steps[0].unit == unit::Litre);
    CHECK(trace.steps[0].value == formula::Rational { 9, 50 });

    // A constant: its own unit, which need not be the variable's.
    CHECK(trace.steps[1].kind == formula::StepKind::Constant);
    CHECK(trace.steps[1].unit == unit::Millilitre);
    CHECK(trace.steps[1].value == formula::Rational { 1, 2000 });

    // Anything computed has no declared unit of its own, so the coherent SI
    // unit of its dimension is the truthful answer -- not the unit of either
    // operand, which a sum of litres and millilitres shows there is no
    // defensible way to pick.
    CHECK(trace.steps[2].kind == formula::StepKind::Add);
    CHECK(trace.steps[2].unit == formula::coherent(formula::dim::Volume));
    CHECK(trace.steps[2].value == formula::Rational { 361, 2000 });
}

TEST_CASE("a parent records the error that reached it", "[trace]")
{
    // Dividing by zero fails in the right operand of the outer division. That
    // operand does not short-circuit anything below itself -- its own two
    // children both run -- so the outer division still gets both of its
    // operands; what it must also do is carry the error the right side
    // reported, rather than dropping it once the arithmetic itself never ran.
    constexpr auto bad = var<Mass> / (var<Volume> / formula::number(formula::Rational { 0 }));

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(bad, environmentOf(6, 3), sink);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == formula::ArithmeticError::DivisionByZero);

    // The outer division recorded the operands it actually got: its left side
    // and the failing right side. Nothing was invented for what never ran.
    auto const& outer = trace.steps[trace.root()];
    CHECK(outer.kind == formula::StepKind::Divide);
    CHECK(outer.error == formula::ArithmeticError::DivisionByZero);
    CHECK(outer.operands.size() == 2);
}

TEST_CASE("an absent variable is recorded as absent, not as an error", "[trace]")
{
    constexpr auto density = var<Mass> / var<Volume>;
    auto const environment = formula::environment(formula::Measured<Mass>::absent(),
                                                  formula::Measured<Volume> { formula::Rational { 3 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(density, environment, sink);

    REQUIRE(result.has_value());
    CHECK_FALSE(result->has_value());

    CHECK(trace.steps[0].kind == formula::StepKind::Variable);
    CHECK_FALSE(trace.steps[0].value.has_value());
    CHECK_FALSE(trace.steps[0].error.has_value());
}

TEST_CASE("a citation contributes its own step", "[trace]")
{
    constexpr auto cited = formula::documented(var<Mass> / var<Volume>,
                                               { .title = "Bulk density",
                                                 .reference = "Example Standard 7:2020",
                                                 .section = "4.1" });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(cited, environmentOf(6, 3), sink);

    REQUIRE(result.has_value());
    auto const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::Documented);
    CHECK(root.citation.title == "Bulk density");
    CHECK(root.citation.reference == "Example Standard 7:2020");
    // It forwards its inner value unchanged -- invisible to arithmetic, but
    // not invisible to the trace: "why this formula" is what a trace is for.
    CHECK(root.value == formula::Rational { 2 });
}

TEST_CASE("a short-circuit two levels down does not let its ancestor steal a cousin's step", "[trace]")
{
    // Multiply(var<Mass>, Divide(Divide(var<Volume>, 0), var<Mass>)).
    // (Multiply rather than Subtract so the two sides need not share a
    // dimension.) The inner Divide's own left operand (var<Volume>/0) errors,
    // so its right var<Mass> is never dispatched -- a genuine one-operand
    // short circuit, unlike the committed short-circuit test above, whose
    // failing side is the outer node's *right* operand and therefore never
    // skips a dispatch at all. Here the outer Multiply itself never
    // short-circuits (its own left, var<Mass>, succeeds), so it always has
    // both of its own two children. A fixed-arity scheme has no way to know
    // the middle Divide got only one operand: it claims two regardless,
    // reaching past the middle node and stealing the outer Multiply's own
    // left child -- verified by the same mutation as Task 4's step 6.
    constexpr auto bad3 =
        var<Mass> * ((var<Volume> / formula::number(formula::Rational { 0 })) / var<Mass>);

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(bad3, environmentOf(6, 3), sink);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == formula::ArithmeticError::DivisionByZero);

    auto const& outer = trace.steps[trace.root()];
    CHECK(outer.kind == formula::StepKind::Multiply);
    // Outer never short-circuits: it always dispatches both of its own two
    // children (var<Mass>, then the middle Divide), so it must have two.
    CHECK(outer.operands.size() == 2);
}

TEST_CASE("a second walk into the same Trace does not leave the first walk's root unclaimed forever",
          "[trace]")
{
    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;

    formula::Trace<> trace {};

    {
        formula::RecordingSink<> sink { trace };
        auto const result = formula::checked_evaluate_si<formula::Rational>(density, environmentOf(6, 3), sink);
        REQUIRE(result.has_value());
    }
    REQUIRE(trace.steps.size() == 4);
    REQUIRE(trace.unclaimed == std::vector<std::size_t> { trace.root() });

    // Constructing a second RecordingSink over the same Trace begins a new
    // walk. Without clearing `unclaimed`, the first walk's root (index 3)
    // would still be sitting there with nothing left to claim it, so it
    // would accumulate forever across repeated reuse of one Trace.
    {
        formula::RecordingSink<> sink { trace };
        auto const result = formula::checked_evaluate_si<formula::Rational>(density, environmentOf(10, 5), sink);
        REQUIRE(result.has_value());
    }

    CHECK(trace.steps.size() == 8);
    // Only the second walk's root remains unclaimed -- not both roots.
    CHECK(trace.unclaimed == std::vector<std::size_t> { trace.root() });
    CHECK(trace.root() == 7);
}

// `Step::operands` holds indices into the owning `Trace`, never a child
// `Step` by value or by pointer. That is the structural property that makes
// destruction iterative rather than recursive: `std::vector<Step>`'s own
// destructor just walks its elements in a loop, and destroying a `Step` never
// destroys another `Step` -- there is nothing here for a recursive teardown
// to even begin. It holds regardless of how many steps a `Trace` has or how
// they were put there, so no runtime test of any size can exercise it any
// more than this assertion already does.
static_assert(std::is_same_v<decltype(formula::Step<> {}.operands), std::vector<std::size_t>>,
              "formula: a Step's operands must be indices, not owned child steps, or teardown would "
              "recurse");

TEST_CASE("the arena holds a large number of steps without incident", "[trace]")
{
    // This is a size/throughput smoke test, not a proof about recursion --
    // that guarantee is established above, once, by construction. Pushing
    // 200,000 steps directly (bypassing RecordingSink, whose own behaviour is
    // covered elsewhere) merely shows that `Trace` has no hidden limit or
    // quadratic behaviour at a size no real formula will ever reach.
    formula::Trace<> trace {};
    for (std::size_t i = 0; i < 200'000; ++i)
    {
        formula::Step<> step {};
        step.kind = formula::StepKind::Add;
        if (i > 0)
            step.operands.push_back(i - 1);
        trace.steps.push_back(std::move(step));
    }
    CHECK(trace.steps.size() == 200'000);
}

TEST_CASE("explain returns the same outcome evaluate would, plus the derivation", "[trace]")
{
    // Density is Mass / Volume (dim::Density, per dimension.hpp), so the
    // expression has to be the plain ratio -- not the `pow<2>(var<Mass>) /
    // var<Volume>` used elsewhere in this file for Task 4's arena tests,
    // whose dimension is Mass^2 / Volume and does not match Density. Using
    // that expression here fails RequireResultDimension's static_assert.
    constexpr auto density = var<Mass> / var<Volume>;
    auto const environment = environmentOf(6, 3);

    auto const plain = formula::evaluate<Density>(density, environment);
    auto const explained = formula::explain<Density>(density, environment);

    // Memberwise equality across every Outcome alternative (kind, value,
    // source, verdict and invalid-reason labels) -- not merely that both
    // happen to hold a value. Tracing observes; it must not participate.
    CHECK(explained.outcome == plain);
    CHECK(explained.trace.steps.size() == 3);
    REQUIRE_FALSE(explained.trace.empty());
    CHECK(explained.trace.steps[explained.trace.root()].value == formula::Rational { 2 });
}

TEST_CASE("explain returns an empty trace when the result is a manual override", "[trace]")
{
    // An override answers the question outright: checked_evaluate returns it
    // without ever dispatching the expression, so nothing runs and nothing
    // is recorded. That is correct, not a gap -- an overridden number was
    // not derived, so there is nothing to trace -- but it means root() is out
    // of bounds on the trace this produces, which is exactly why callers must
    // check empty() before reading it (see the test above, and trace.hpp).
    constexpr auto density = var<Mass> / var<Volume>;
    auto const overridden =
        formula::environment(formula::Measured<Mass> { formula::Rational { 6 } },
                             formula::Measured<Volume> { formula::Rational { 3 } },
                             formula::entered(formula::Measured<Density> { formula::Rational { 999 } }));

    auto const explained = formula::explain<Density>(density, overridden);

    REQUIRE(explained.outcome.is_value());
    CHECK(explained.outcome.is_overridden());
    CHECK(explained.outcome.measurement().value() == formula::Rational { 999 });
    CHECK(explained.trace.empty());
    CHECK(explained.trace.steps.size() == 0);
}

// --------------------------------------------------------------- phase 8

TEST_CASE("a Round step records its own declared unit and granularity, and the pre-rounding value stays "
          "visible on its operand's own step",
          "[trace]")
{
    // 12.34 mm to one decimal place, half away from zero, is 12.3 mm -- the
    // same example rounding_node_tests.cpp verifies directly against
    // checked_evaluate. The Round step does not duplicate the pre-rounding
    // value onto itself: it is already visible on operand #1, the same way a
    // Negate or Power step never restates its own operand's value either --
    // that is Task 6's answer to "how does a Round step make the change of
    // value visible" (see the report for the reasoning).
    constexpr auto node =
        formula::rounded<unit::Millimetre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Diameter>);
    auto const environment = formula::environment(formula::Measured<Diameter> { formula::Rational { 1234, 100 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(node, environment, sink);

    REQUIRE(result.has_value());
    REQUIRE(trace.steps.size() == 2);

    CHECK(trace.steps[0].kind == formula::StepKind::Variable);
    CHECK(trace.steps[0].value == formula::Rational { 617, 50000 });   // 12.34 mm, in coherent SI (m)

    CHECK(trace.steps[1].kind == formula::StepKind::Round);
    // The node's own declared unit, not the coherent SI one: "rounded to 1
    // dp" means nothing without saying 1 dp of what.
    CHECK(trace.steps[1].unit == unit::Millimetre);
    CHECK(trace.steps[1].granularity == 1);
    // The tie rule too: it can be the entire reason a rounded value is one
    // number rather than the next one along, and it reaches no other output.
    CHECK(trace.steps[1].mode == formula::RoundingMode::HalfAwayFromZero);
    CHECK(trace.steps[1].value == formula::Rational { 123, 10000 });   // 12.3 mm, in coherent SI (m)
    REQUIRE(trace.steps[1].operands.size() == 1);
    CHECK(trace.steps[1].operands[0] == 0);
}

TEST_CASE("a RoundSignificant step records its own declared unit and granularity", "[trace]")
{
    // 12.34 mm to two significant digits is 12 mm -- the same example
    // rounding_node_tests.cpp verifies directly.
    constexpr auto node = formula::rounded_to_digits<unit::Millimetre, formula::SignificantDigits { 2 },
                                                     formula::RoundingMode::HalfAwayFromZero>(var<Diameter>);
    auto const environment = formula::environment(formula::Measured<Diameter> { formula::Rational { 1234, 100 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(node, environment, sink);

    REQUIRE(result.has_value());
    REQUIRE(trace.steps.size() == 2);

    CHECK(trace.steps[1].kind == formula::StepKind::RoundSignificant);
    CHECK(trace.steps[1].unit == unit::Millimetre);
    CHECK(trace.steps[1].granularity == 2);
    CHECK(trace.steps[1].mode == formula::RoundingMode::HalfAwayFromZero);
    CHECK(trace.steps[1].value == formula::Rational { 3, 250 });   // 12 mm, in coherent SI (m)
    REQUIRE(trace.steps[1].operands.size() == 1);
    CHECK(trace.steps[1].operands[0] == 0);
}

TEST_CASE("a Conditional step records the then branch it took, and every operand along the way", "[trace]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result =
        formula::checked_evaluate_si<formula::Rational>(chosen, strengthOf(formula::Rational { 60 }), sink);

    REQUIRE(result.has_value());
    REQUIRE(trace.steps.size() == 4);

    // Post-order: the predicate's two sides, then the branch it selected.
    CHECK(trace.steps[0].kind == formula::StepKind::Variable);   // predicate lhs: f
    CHECK(trace.steps[1].kind == formula::StepKind::Constant);   // predicate rhs: 50 MPa
    CHECK(trace.steps[2].kind == formula::StepKind::Variable);   // thenBranch: f

    auto const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::Conditional);
    CHECK(root.branch == formula::Branch::Then);
    // Which way the two sides were compared, not merely that they were: the
    // trace is the artefact that survives away from the formula text, and a
    // step naming two operands with no operator between them cannot be
    // checked against the method it came from. `WhenNode` re-exports it from
    // the predicate, which is not a Node and so gets no step of its own.
    CHECK(root.comparison == formula::Comparison::Greater);
    REQUIRE(root.operands.size() == 3);
    CHECK(root.operands[0] == 0);
    CHECK(root.operands[1] == 1);
    CHECK(root.operands[2] == 2);
    CHECK(root.value == formula::Rational { 60'000'000 });   // 60 MPa, in coherent SI (Pa)
}

TEST_CASE("a Conditional step records the else branch it took", "[trace]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result =
        formula::checked_evaluate_si<formula::Rational>(chosen, strengthOf(formula::Rational { 40 }), sink);

    REQUIRE(result.has_value());
    // Predicate lhs, predicate rhs, the elseBranch's own var, its constant 2,
    // and the Multiply that combines them, then the Conditional itself.
    REQUIRE(trace.steps.size() == 6);

    auto const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::Conditional);
    CHECK(root.branch == formula::Branch::Else);
    CHECK(root.comparison == formula::Comparison::Greater);
    // The elseBranch's own root (the Multiply) already claimed its own two
    // children, so the Conditional's operands are the predicate's two sides
    // plus that one Multiply step -- not five.
    REQUIRE(root.operands.size() == 3);
    CHECK(root.operands[0] == 0);
    CHECK(root.operands[1] == 1);
    CHECK(root.operands[2] == 4);
    CHECK(root.value == formula::Rational { 80'000'000 });   // 80 MPa, in coherent SI (Pa)
}

TEST_CASE("a Conditional step records no branch when the predicate is absent -- not the else branch",
          "[trace]")
{
    // A bool cannot distinguish this state from "the predicate held false" --
    // exactly why Branch has three states rather than two.
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(
        chosen, formula::environment(formula::Measured<Strength>::absent()), sink);

    REQUIRE(result.has_value());
    CHECK_FALSE(result->has_value());

    auto const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::Conditional);
    CHECK(root.branch == formula::Branch::Neither);
    // Recorded even though no branch ran: what the step could not decide is
    // still a comparison, and a reader needs to know which one.
    CHECK(root.comparison == formula::Comparison::Greater);
    // Neither branch ran, so only the predicate's own two operands were
    // claimed -- not three.
    REQUIRE(root.operands.size() == 2);
    CHECK_FALSE(root.value.has_value());
}

TEST_CASE("a NumericValue step records its justification and the unit it read from", "[trace]")
{
    constexpr auto node = formula::numeric_value_of<unit::Megapascal, "empirical fit only valid in MPa">(var<Strength>);
    auto const environment = strengthOf(formula::Rational { 70 });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(node, environment, sink);

    REQUIRE(result.has_value());
    REQUIRE(trace.steps.size() == 2);

    auto const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::NumericValue);
    CHECK(root.justification == "empirical fit only valid in MPa");
    // The unit it read from, kept separate from `unit` -- see trace.hpp's
    // comment on `Step::sourceUnit` for why folding the two together would
    // break rendering.
    CHECK(root.sourceUnit == unit::Megapascal);
    // Its OWN `unit` stays the coherent SI of its (scalar) dimension: a
    // NumericValueNode's dimension is always Scalar, and the unit it names
    // measures its operand's dimension instead, which is not the same thing.
    CHECK(root.unit == formula::coherent(formula::dim::Scalar));
    CHECK(root.dimension == formula::dim::Scalar);
    // The bare number itself: 70 MPa read in MPa is just 70, no conversion.
    CHECK(root.value == formula::Rational { 70 });
    REQUIRE(root.operands.size() == 1);
    CHECK(root.operands[0] == 0);
}

// ------------------------------------------------------- Constraint steps

TEST_CASE("a Constraint step records a satisfied verdict and both predicate operands", "[trace]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const outcome = formula::check(atLeastThirty, strengthOf(formula::Rational { 45 }), sink);

    CHECK(outcome.is_satisfied());
    REQUIRE(trace.steps.size() == 3);

    auto const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::Constraint);
    CHECK(root.comparison == formula::Comparison::GreaterOrEqual);
    CHECK(root.outcome.is_satisfied());
    CHECK_FALSE(root.outcome.verdict().has_value());
    // The predicate's own two sides -- f, then 30 MPa -- exactly as a
    // Conditional step claims its predicate's two sides.
    REQUIRE(root.operands.size() == 2);
    CHECK(root.operands[0] == 0);
    CHECK(root.operands[1] == 1);
}

TEST_CASE("a Constraint step records a violated verdict, carrying it", "[trace]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const outcome = formula::check(atLeastThirty, strengthOf(formula::Rational { 20 }), sink);

    CHECK(outcome.is_violated());

    auto const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::Constraint);
    CHECK(root.comparison == formula::Comparison::GreaterOrEqual);
    CHECK(root.outcome.is_violated());
    REQUIRE(root.outcome.verdict().has_value());
    CHECK(root.outcome.verdict()->label == std::string_view { "reject the specimen" });
    REQUIRE(root.operands.size() == 2);
}

TEST_CASE("a Constraint step records not-checked when the predicate is absent -- not satisfied", "[trace]")
{
    // A bool cannot distinguish this state from "the predicate held true" --
    // exactly why check() reports four states rather than two, and why the
    // step must record which of the four it was, not merely a value.
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const outcome =
        formula::check(atLeastThirty, formula::environment(formula::Measured<Strength>::absent()), sink);

    CHECK(outcome.is_not_checked());

    auto const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::Constraint);
    // Recorded even though it never resolved -- both sides were still
    // dispatched, so the comparison is still named, exactly as a Conditional
    // step's `comparison` is recorded for a predicate that never resolved.
    CHECK(root.comparison == formula::Comparison::GreaterOrEqual);
    CHECK(root.outcome.is_not_checked());
    CHECK_FALSE(root.outcome.is_satisfied());
    CHECK_FALSE(root.outcome.verdict().has_value());
    // Both sides were dispatched even though the left one came back absent --
    // not one operand, which is reserved for a left side that raised an
    // arithmetic error and so was never followed by a dispatch of the right.
    REQUIRE(root.operands.size() == 2);
}

TEST_CASE("a Constraint step records Invalid with one operand when the predicate's left side errors", "[trace]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const outcome = formula::check(leftSideErrors, strengthOf(formula::Rational { 60 }), sink);

    CHECK(outcome.is_invalid());
    // Strength's Variable, the Constant 0, the Divide that fails, then the
    // Constraint itself -- the right side of the predicate (the constant
    // 0 MPa it compares against) is never reached at all.
    REQUIRE(trace.steps.size() == 4);

    auto const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::Constraint);
    CHECK(root.outcome.is_invalid());
    REQUIRE(root.outcome.error().has_value());
    CHECK(root.outcome.error() == formula::ArithmeticError::DivisionByZero);
    // One operand, not two: the Divide that failed, claimed as the
    // predicate's left side. Nothing was ever compared.
    REQUIRE(root.operands.size() == 1);
    CHECK(root.operands[0] == 2);
}

TEST_CASE("a Constraint step records Invalid with two operands when the predicate's right side errors", "[trace]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const outcome = formula::check(rightSideErrors, strengthOf(formula::Rational { 60 }), sink);

    CHECK(outcome.is_invalid());
    // The left side's own Variable, then the right side's Variable, Constant
    // 0 and the Divide that fails, then the Constraint itself.
    REQUIRE(trace.steps.size() == 5);

    auto const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::Constraint);
    CHECK(root.outcome.is_invalid());
    REQUIRE(root.outcome.error().has_value());
    CHECK(root.outcome.error() == formula::ArithmeticError::DivisionByZero);
    // Two operands: the left side succeeded and so was dispatched before the
    // right side's own failure, unlike the one-operand case above.
    REQUIRE(root.operands.size() == 2);
    CHECK(root.operands[0] == 0);
    CHECK(root.operands[1] == 3);
}

TEST_CASE("a trace records one Constraint step per constraint checked via check_all, "
          "each claiming only its own operands",
          "[trace]")
{
    // Two independent constraints over two independent quantities, checked
    // into one shared trace -- so a Constraint step that wrongly claims
    // steps outside its own mark (its predecessor's, or its own again) is
    // visible regardless of which constraint runs first. Declared both ways
    // round, the same reason constraint_tests.cpp's own set tests are.
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 20 } },
                                                   formula::Measured<Diameter> { formula::Rational { 150 } });

    {
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        auto const outcomes =
            formula::check_all(formula::constraints(atLeastThirty, diameterAtMost100), environment, sink);

        REQUIRE(outcomes[0].is_violated());
        REQUIRE(outcomes[1].is_violated());
        // f, 30 MPa, Constraint; d, 100 mm, Constraint.
        REQUIRE(trace.steps.size() == 6);
        CHECK(trace.steps[2].kind == formula::StepKind::Constraint);
        REQUIRE(trace.steps[2].operands.size() == 2);
        CHECK(trace.steps[2].operands[0] == 0);
        CHECK(trace.steps[2].operands[1] == 1);
        CHECK(trace.steps[5].kind == formula::StepKind::Constraint);
        REQUIRE(trace.steps[5].operands.size() == 2);
        CHECK(trace.steps[5].operands[0] == 3);
        CHECK(trace.steps[5].operands[1] == 4);
    }

    {
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        auto const outcomes =
            formula::check_all(formula::constraints(diameterAtMost100, atLeastThirty), environment, sink);

        REQUIRE(outcomes[0].is_violated());
        REQUIRE(outcomes[1].is_violated());
        // Same shape, reversed: d, 100 mm, Constraint; f, 30 MPa, Constraint.
        REQUIRE(trace.steps.size() == 6);
        CHECK(trace.steps[2].kind == formula::StepKind::Constraint);
        REQUIRE(trace.steps[2].operands.size() == 2);
        CHECK(trace.steps[2].operands[0] == 0);
        CHECK(trace.steps[2].operands[1] == 1);
        CHECK(trace.steps[5].kind == formula::StepKind::Constraint);
        REQUIRE(trace.steps[5].operands.size() == 2);
        CHECK(trace.steps[5].operands[0] == 3);
        CHECK(trace.steps[5].operands[1] == 4);
    }
}

// ------------------------------------------------------- phase 10: lookups

namespace
{
using formula::band;
using formula::BandTable;
using formula::banded_lookup;
using formula::breakpoint;
using formula::BreakpointTable;
using formula::exact_lookup;
using formula::interpolating_lookup;
using formula::KeyTable;

[[nodiscard]] constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

[[nodiscard]] auto diameterOf(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::environment(formula::Measured<Diameter> { rat(numerator, denominator) });
}

/// Three bands declared in **centimetres**, which is deliberately neither of
/// the two units a recorder could reach for by accident: not millimetres, the
/// unit `Diameter` is declared in, and not metres, the coherent SI unit every
/// step's value is actually stored in. Locating a value against this table
/// without first converting it into the table's own key unit puts metres
/// against centimetre bounds, and every assertion below then reports a miss
/// instead of the band it names.
///
/// Non-degenerate on every axis this file can name, because a mutation
/// survives whenever *any* axis of a fixture is degenerate and not only the
/// one that caught the last defect:
///
///  - the widths are 3/2, 5/2 and 4 cm -- unequal, so a recorder taking a
///    width from the first row is visible on the others;
///  - no bound equals its own row's index, so a bound cannot be confused with
///    an index;
///  - the two shared boundaries (5/2 and 5) differ from each other, so
///    "always report the first band" is visible on every row;
///  - one bound is declared **unreduced** (`10/2`), so that reducing it is a
///    decision a reader can see being made rather than one no fixture can
///    tell was taken;
///  - the three corrections are distinct and equal to no index and no bound.
inline constexpr BandTable<3> SizeBands {
    band(1, 1, 5, 2),  // 1 to under 5/2 cm
    band(5, 2, 10, 2), // 5/2 to under 5 cm -- 10/2 declared, so reduction shows
    band(5, 1, 9, 1),  // 5 to under 9 cm
};

/// The corrections are stated in **percent**, again not the coherent SI unit
/// of their own dimension, so the result side of the table is converted too
/// rather than passed through.
[[nodiscard]] constexpr auto sizeLookup()
{
    return banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(var<Diameter>, { rat(95), rat(112), rat(105) });
}

/// Underlying type `std::int16_t` with a **negative** enumerator, both
/// deliberately. A step that recorded a key's bit pattern without recording
/// that it is to be read as signed would report 65533 where `render()` writes
/// -3, and a fixture whose keys were all non-negative could not tell the two
/// apart. Numbered by hand and declared out of numeric order for
/// `render_tests.cpp`'s own reasons: a recorder reading a row's *index*
/// rather than its key, or reading the enumeration rather than the table, is
/// invisible against an enumeration left to default.
enum class SpecimenShape : std::int16_t
{
    Undercut = -3,
    Cube = 4,
    Cylinder = 7,
    Beam = 11,
};

inline constexpr KeyTable<SpecimenShape, 3> ShapeKeys {
    SpecimenShape::Cube,     // key 4
    SpecimenShape::Undercut, // key -3 -- the middle row, and negative
    SpecimenShape::Cylinder, // key 7
};

[[nodiscard]] constexpr auto shapeLookup(SpecimenShape shape)
{
    return exact_lookup<ShapeKeys, unit::Megapascal>(shape, { rat(31, 25), rat(4), rat(13, 10) });
}

/// The unsigned half of the same question: an underlying type whose top half
/// no signed type can hold, with a key sitting up there.
enum class ApparatusVariant : unsigned long long
{
    Modern = 2,
    Legacy = 18446744073709551615ULL,
};

inline constexpr KeyTable<ApparatusVariant, 2> ApparatusKeys { ApparatusVariant::Modern, ApparatusVariant::Legacy };

/// Three breakpoints in centimetres, unequally spaced (5/2 then 9/2), with
/// the middle key neither whole nor reduced -- every reason `SizeBands` above
/// gives, unchanged.
inline constexpr BreakpointTable<3> CurvePoints {
    breakpoint(1),
    breakpoint(14, 4), // 7/2 cm -- declared unreduced, and in the middle
    breakpoint(8),
};

[[nodiscard]] constexpr auto curveLookup()
{
    return interpolating_lookup<unit::Centimetre, CurvePoints, unit::Percent>(var<Diameter>,
                                                                              { rat(90), rat(-115), rat(120) });
}

/// `2^62`, an ordinary representable `Rational` used where the scale rather
/// than the arithmetic is the point -- `lookup_tests.cpp`'s own constant, for
/// the same purpose.
constexpr std::int64_t Huge = std::int64_t { 1 } << 62;

/// Keys 0 and 4 mm against values 0 and 2^62 - 1: probed at 3 mm the exact
/// answer is 3(2^62 - 1)/4, whose reduced numerator is above `Rational`'s
/// maximum, so the **interpolation itself** overflows. The same table
/// `lookup_tests.cpp` pins the behaviour of.
inline constexpr BreakpointTable<2> UnrepresentableAnswer { breakpoint(0), breakpoint(4) };

/// One band wide enough to hit, whose correction is stated in **kilometres**,
/// so that a hit still has an arithmetic step left to fail at: 2^62 km is a
/// perfectly representable `Rational` that does not survive being multiplied
/// by 1000 on the way to metres. The band was found, so this is emphatically
/// not a miss.
inline constexpr BandTable<1> WideBand { band(0, 1, 100, 1) };

/// An inner table whose corrections are **lengths**, so that it can stand
/// where the outer table's operand stands and a lookup can be nested inside a
/// lookup -- the one arrangement in which two steps of a derivation carry the
/// identical `DomainError` for entirely different reasons.
inline constexpr BandTable<2> InnerBands {
    band(1, 1, 3, 1), // 1 to under 3 cm
    band(3, 1, 6, 1), // 3 to under 6 cm
};

/// A consumer's own node kind, written against the two-parameter extension
/// point (`sink.hpp`) exactly as `sink_tests.cpp`'s `LegacyNode` is. The
/// library never hands it to a sink, so it contributes no step at all -- and
/// a lookup above it therefore has nothing recorded anywhere to read, which
/// is the one case in which whose failure it is carrying genuinely cannot be
/// determined.
struct UntracedLength: formula::NodeBase
{
    static constexpr formula::Dimension dimension = formula::dim::Length;
};

/// The same extension point, succeeding rather than failing. A lookup above
/// this one **hits** -- and still cannot name the band it hit, because the
/// value that selected it was never recorded anywhere. Saying nothing is the
/// only honest answer there; naming the first band, or whichever one a scan
/// with no value to scan for happened to land on, would be inventing a fact
/// the derivation does not have.
struct UntracedThreeCentimetres: formula::NodeBase
{
    static constexpr formula::Dimension dimension = formula::dim::Length;
};
} // namespace

namespace formula
{
template <typename Rep = Rational, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(UntracedLength const&, Env const&) noexcept
{
    return std::unexpected { ArithmeticError::DivisionByZero };
}

template <typename Rep = Rational, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(UntracedThreeCentimetres const&, Env const&) noexcept
{
    return detail::in_si<Rep>(Rational { 3 }, unit::Centimetre);
}
} // namespace formula

TEST_CASE("a banded lookup step records the band its value fell in", "[trace][lookup]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    // 30 mm == 3 cm, inside the MIDDLE band [5/2, 5) cm -- the position a
    // defect is hardest to see from either end.
    auto const result = formula::checked_evaluate_si<formula::Rational>(sizeLookup(), diameterOf(30), sink);

    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    REQUIRE(trace.steps.size() == 2);

    auto const& step = trace.steps[1];
    CHECK(step.kind == formula::StepKind::BandedLookup);
    CHECK(step.lookupFailure == formula::LookupFailure::None);

    // The band the value fell in, exactly as the table declared it -- 10/2 and
    // not 5, because reducing it is the renderer's decision and not this
    // recorder's to take on its behalf.
    REQUIRE(step.selectedBand.has_value());
    CHECK(*step.selectedBand == band(5, 2, 10, 2));
    // Nothing about the table's extent is claimed on a hit.
    CHECK(!step.coveredRange.has_value());

    // Three units, all different, and a step that confused any two of them
    // would state a number in a scale nobody declared it in: the value is
    // stored in coherent SI (dimensionless), it was *declared* in percent, and
    // the band bounds it was compared against are in centimetres.
    CHECK(step.unit == unit::Percent);
    CHECK(step.sourceUnit == unit::Centimetre);
    CHECK(step.value == rat(112, 100));

    REQUIRE(step.operands.size() == 1);
    CHECK(step.operands[0] == 0);
    CHECK(trace.steps[0].value == rat(3, 100)); // 30 mm, in metres
}

TEST_CASE("a banded lookup step that missed records the miss and what its bands cover", "[trace][lookup]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    // 95 mm == 9.5 cm, past the last band, which ends at 9 cm.
    auto const result = formula::checked_evaluate_si<formula::Rational>(sizeLookup(), diameterOf(95), sink);

    REQUIRE(!result.has_value());
    REQUIRE(trace.steps.size() == 2);

    auto const& step = trace.steps[1];
    CHECK(step.error == formula::ArithmeticError::DomainError);
    CHECK(step.lookupFailure == formula::LookupFailure::Missed);
    CHECK(!step.selectedBand.has_value());

    // The union of the bands, which is one interval because
    // `RequireValidBandTable` has already refused a gap and an overlap -- and
    // which is neither the first band nor the last, so a recorder reporting
    // either of those instead is visible here.
    REQUIRE(step.coveredRange.has_value());
    CHECK(*step.coveredRange == formula::LookupRange { 1, 1, 9, 1 });
    CHECK(step.sourceUnit == unit::Centimetre);
}

TEST_CASE("a banded lookup step tells its own miss apart from an operand's", "[trace][lookup]")
{
    // The whole point of `LookupFailure`. Both derivations below end in a step
    // carrying the IDENTICAL error -- `DomainError` -- for entirely different
    // reasons, and nothing in the error channel separates them: a recorder
    // that set the field from the error alone would answer "missed" for both,
    // and a derivation would then claim a table did not reach a specimen in a
    // case where that table was never consulted at all.
    constexpr auto inner =
        banded_lookup<unit::Centimetre, InnerBands, unit::Millimetre>(var<Diameter>, { rat(950), rat(35) });
    constexpr auto nested =
        banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(inner, { rat(95), rat(112), rat(105) });

    // 15 mm == 1.5 cm: the inner table HITS its first band and answers
    // 950 mm == 95 cm, which the outer table -- covering 1 to under 9 cm --
    // does not reach. So the outer step missed on its own.
    formula::Trace<> ownMiss {};
    {
        formula::RecordingSink<> sink { ownMiss };
        (void) formula::checked_evaluate_si<formula::Rational>(nested, diameterOf(15), sink);
    }
    REQUIRE(ownMiss.steps.size() == 3);
    CHECK(ownMiss.steps[1].lookupFailure == formula::LookupFailure::None);
    CHECK(ownMiss.steps[2].error == formula::ArithmeticError::DomainError);
    CHECK(ownMiss.steps[2].lookupFailure == formula::LookupFailure::Missed);
    CHECK(ownMiss.steps[2].coveredRange.has_value());

    // 95 mm == 9.5 cm: the INNER table misses, and the outer one relays its
    // error untouched. Same enumerator on the outer step, and nothing about
    // the outer table went wrong at all -- so it claims nothing about it.
    formula::Trace<> relayed {};
    {
        formula::RecordingSink<> sink { relayed };
        (void) formula::checked_evaluate_si<formula::Rational>(nested, diameterOf(95), sink);
    }
    REQUIRE(relayed.steps.size() == 3);
    CHECK(relayed.steps[1].lookupFailure == formula::LookupFailure::Missed);
    CHECK(relayed.steps[2].error == formula::ArithmeticError::DomainError);
    CHECK(relayed.steps[2].lookupFailure == formula::LookupFailure::Propagated);
    CHECK(!relayed.steps[2].coveredRange.has_value());
    CHECK(!relayed.steps[2].selectedBand.has_value());
}

TEST_CASE("an exact lookup step records the key it selected with, which no other step carries",
          "[trace][lookup]")
{
    // A hit on the MIDDLE row, whose key is negative.
    formula::Trace<> hit {};
    {
        formula::RecordingSink<> sink { hit };
        (void) formula::checked_evaluate_si<formula::Rational>(shapeLookup(SpecimenShape::Undercut),
                                                               formula::environment(),
                                                               sink);
    }
    // One step and one only: an exact lookup has no operand, so there is no
    // step below it the key could have been recovered from.
    REQUIRE(hit.steps.size() == 1);
    CHECK(hit.steps[0].kind == formula::StepKind::ExactLookup);
    CHECK(hit.steps[0].operands.empty());
    CHECK(hit.steps[0].lookupKeyIsSigned);
    CHECK(static_cast<long long>(hit.steps[0].lookupKey) == -3);
    CHECK(hit.steps[0].lookupFailure == formula::LookupFailure::None);
    CHECK(hit.steps[0].unit == unit::Megapascal);
    CHECK(hit.steps[0].value == rat(4000000)); // 4 MPa, in pascals
    // No key unit: a category key is a discriminator, not a quantity.
    CHECK(hit.steps[0].sourceUnit == formula::Unit {});

    // And a miss, on a key that is a legitimate enumerator of the author's own
    // enumeration and simply names no row of this table.
    formula::Trace<> miss {};
    {
        formula::RecordingSink<> sink { miss };
        (void) formula::checked_evaluate_si<formula::Rational>(shapeLookup(SpecimenShape::Beam),
                                                               formula::environment(),
                                                               sink);
    }
    REQUIRE(miss.steps.size() == 1);
    CHECK(miss.steps[0].error == formula::ArithmeticError::DomainError);
    CHECK(miss.steps[0].lookupFailure == formula::LookupFailure::Missed);
    CHECK(static_cast<long long>(miss.steps[0].lookupKey) == 11);
    // Nothing to cover: an exact table's domain is a set of keys, not an
    // interval, so there is no range to report and none is invented.
    CHECK(!miss.steps[0].coveredRange.has_value());
}

TEST_CASE("an exact lookup step records an unsigned key that no signed type could hold", "[trace][lookup]")
{
    // `render.hpp` spells its two casts separately because an enumeration's
    // underlying type may be `unsigned long long`; a step that stored the key
    // as a signed integer would report -1 for this one.
    constexpr auto node = exact_lookup<ApparatusKeys, unit::One>(ApparatusVariant::Legacy, { rat(3), rat(2) });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, formula::environment(), sink);

    REQUIRE(trace.steps.size() == 1);
    CHECK(!trace.steps[0].lookupKeyIsSigned);
    CHECK(trace.steps[0].lookupKey == 18446744073709551615ULL);
    CHECK(trace.steps[0].lookupFailure == formula::LookupFailure::None);
}

TEST_CASE("an interpolating lookup step tells its own overflow apart from an operand's", "[trace][lookup]")
{
    // The second ambiguity, and NOT the same case as the first: only this kind
    // computes, so only this kind can overflow of its own accord. Both
    // derivations below end in a step carrying `Overflow`.
    constexpr auto own = interpolating_lookup<unit::Millimetre, UnrepresentableAnswer, unit::One>(
        var<Diameter>, { rat(0), rat(Huge - 1) });

    formula::Trace<> ownOverflow {};
    {
        formula::RecordingSink<> sink { ownOverflow };
        (void) formula::checked_evaluate_si<formula::Rational>(own, diameterOf(3), sink);
    }
    REQUIRE(ownOverflow.steps.size() == 2);
    CHECK(ownOverflow.steps[1].error == formula::ArithmeticError::Overflow);
    CHECK(ownOverflow.steps[1].lookupFailure == formula::LookupFailure::Computation);

    // The same enumerator, produced below the lookup instead: 2^62 mm times
    // 2^62 is not representable, and the curve is never consulted.
    constexpr auto overflowingLength = formula::constant<unit::Millimetre>(rat(Huge)) * formula::number(rat(Huge));
    constexpr auto relayed = interpolating_lookup<unit::Millimetre, UnrepresentableAnswer, unit::One>(
        overflowingLength, { rat(0), rat(Huge - 1) });

    formula::Trace<> relayedOverflow {};
    {
        formula::RecordingSink<> sink { relayedOverflow };
        (void) formula::checked_evaluate_si<formula::Rational>(relayed, formula::environment(), sink);
    }
    REQUIRE(relayedOverflow.steps.size() == 4);
    CHECK(relayedOverflow.steps[2].error == formula::ArithmeticError::Overflow);
    CHECK(relayedOverflow.steps[3].error == formula::ArithmeticError::Overflow);
    CHECK(relayedOverflow.steps[3].lookupFailure == formula::LookupFailure::Propagated);
}

TEST_CASE("an interpolating lookup step that missed records the closed range its curve runs over",
          "[trace][lookup]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    // 100 mm == 10 cm, past the curve's last row at 8 cm. No extrapolation and
    // no clamp -- a miss, exactly as a banded lookup's is.
    (void) formula::checked_evaluate_si<formula::Rational>(curveLookup(), diameterOf(100), sink);

    REQUIRE(trace.steps.size() == 2);
    CHECK(trace.steps[1].error == formula::ArithmeticError::DomainError);
    CHECK(trace.steps[1].lookupFailure == formula::LookupFailure::Missed);
    REQUIRE(trace.steps[1].coveredRange.has_value());
    CHECK(*trace.steps[1].coveredRange == formula::LookupRange { 1, 1, 8, 1 });
    CHECK(trace.steps[1].sourceUnit == unit::Centimetre);

    // A value inside the curve is not a miss, and an interpolating lookup
    // selects no band on the way: it computes a number that appears in no row
    // of its own table.
    formula::Trace<> inside {};
    {
        formula::RecordingSink<> insideSink { inside };
        (void) formula::checked_evaluate_si<formula::Rational>(curveLookup(), diameterOf(60), insideSink);
    }
    REQUIRE(inside.steps.size() == 2);
    CHECK(inside.steps[1].lookupFailure == formula::LookupFailure::None);
    CHECK(!inside.steps[1].selectedBand.has_value());
    CHECK(!inside.steps[1].coveredRange.has_value());
}

TEST_CASE("a lookup whose own unit conversion failed is not recorded as a miss", "[trace][lookup]")
{
    // Own failures are not all misses, and a recorder answering "missed" for
    // every error of its own would be as wrong as one answering it for every
    // relayed error.

    // The result side: 30 mm is comfortably inside [0, 100) mm, so the band IS
    // found -- and the correction it selects, 2^62 km, then does not survive
    // the conversion into metres.
    constexpr auto wide = banded_lookup<unit::Millimetre, WideBand, unit::Kilometre>(var<Diameter>, { rat(Huge) });
    formula::Trace<> resultSide {};
    {
        formula::RecordingSink<> sink { resultSide };
        (void) formula::checked_evaluate_si<formula::Rational>(wide, diameterOf(30), sink);
    }
    REQUIRE(resultSide.steps.size() == 2);
    CHECK(resultSide.steps[1].error == formula::ArithmeticError::Overflow);
    CHECK(resultSide.steps[1].lookupFailure == formula::LookupFailure::Conversion);
    CHECK(!resultSide.steps[1].selectedBand.has_value());
    CHECK(!resultSide.steps[1].coveredRange.has_value());

    // The key side: the operand succeeds, and converting its 2^62 metres into
    // the table's own centimetres overflows before any band is looked at.
    constexpr auto farTooLong = banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(
        formula::constant<unit::Metre>(rat(Huge)), { rat(95), rat(112), rat(105) });
    formula::Trace<> keySide {};
    {
        formula::RecordingSink<> sink { keySide };
        (void) formula::checked_evaluate_si<formula::Rational>(farTooLong, formula::environment(), sink);
    }
    REQUIRE(keySide.steps.size() == 2);
    CHECK(!keySide.steps[0].error.has_value()); // the operand itself was fine
    CHECK(keySide.steps[1].error == formula::ArithmeticError::Overflow);
    CHECK(keySide.steps[1].lookupFailure == formula::LookupFailure::Conversion);
}

TEST_CASE("a lookup whose operand recorded no step cannot say whose failure it is", "[trace][lookup]")
{
    // The honest fourth answer. `UntracedLength` fails, but it is a consumer's
    // own node evaluated through the two-parameter extension point, so it
    // contributes no step -- and with nothing recorded below, "this lookup
    // missed" and "something below me failed" are genuinely indistinguishable.
    // Recording `Propagated` here would be a plausible answer to a question
    // the recorder cannot answer, which is the defect this field exists to
    // close.
    constexpr auto node =
        banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(UntracedLength {}, { rat(95), rat(112), rat(105) });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, formula::environment(), sink);

    REQUIRE(trace.steps.size() == 1);
    CHECK(trace.steps[0].operands.empty());
    CHECK(trace.steps[0].error == formula::ArithmeticError::DivisionByZero);
    CHECK(trace.steps[0].lookupFailure == formula::LookupFailure::Undetermined);
}

TEST_CASE("a lookup whose operand recorded no step names no band even when it hits", "[trace][lookup]")
{
    // The other half of the same honesty, and the easier one to get wrong: the
    // lookup succeeded, so there is a band -- and the recorder still has no
    // value to locate it with, because the operand contributed no step. The
    // answer is silence, not the first band and not whichever one a scan with
    // nothing to scan for would land on.
    constexpr auto node = banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(UntracedThreeCentimetres {},
                                                                                    { rat(95), rat(112), rat(105) });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(node, formula::environment(), sink);

    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    // 3 cm really is inside the middle band, so this is a hit and not a
    // degenerate case dressed up as one.
    CHECK(**result == rat(112, 100));

    REQUIRE(trace.steps.size() == 1);
    CHECK(trace.steps[0].operands.empty());
    CHECK(trace.steps[0].lookupFailure == formula::LookupFailure::None);
    CHECK(!trace.steps[0].selectedBand.has_value());
}

TEST_CASE("a lookup whose operand was never measured records absence, not a failure", "[trace][lookup]")
{
    // Absence is not an error anywhere else in this library and must not
    // become one here: nothing was looked up, so nothing missed.
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const environment = formula::environment(formula::Measured<Diameter>::absent());
    auto const result = formula::checked_evaluate_si<formula::Rational>(sizeLookup(), environment, sink);

    REQUIRE(result.has_value());
    CHECK(!result->has_value());
    REQUIRE(trace.steps.size() == 2);
    CHECK(!trace.steps[1].value.has_value());
    CHECK(!trace.steps[1].error.has_value());
    CHECK(trace.steps[1].lookupFailure == formula::LookupFailure::None);
    CHECK(!trace.steps[1].selectedBand.has_value());
}
