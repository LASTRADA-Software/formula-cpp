// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Mass: formula::Quantity<Mass, "m", "specimen mass", unit::Kilogram>
{
};
struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", unit::One>
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

// The predicate every phase-8 conditional test below shares: strength over an
// invented 473/10 MPa. Kept at namespace scope so the mutation test (further
// down) can name its exact type.
constexpr auto overThreshold = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 473, 10 });
constexpr auto chosen = formula::when(overThreshold, var<Strength>, var<Strength> * formula::Rational { 2 });

[[nodiscard]] auto strengthOf(formula::Rational value)
{
    return formula::environment(formula::Measured<Strength> { value });
}

// The constraint every Constraint-step test below shares: strength at least
// an invented 273/10 MPa, phrased the way a standard's rejection rule reads.
// Kept at namespace scope for the same reason `overThreshold`/`chosen` above
// are.
constexpr auto atLeastMinimum =
    formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 273, 10 }),
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
constexpr auto diameterAtMostLimit =
    formula::constraint(var<Diameter> <= formula::constant<unit::Millimetre>(formula::Rational { 139 }),
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

    // A sum of values in two units has no one unit to borrow, so the coherent
    // SI unit of its dimension is the truthful answer -- not the unit of
    // either operand, which a sum of litres and millilitres shows there is no
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
    // left child -- verified by mutation.
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
    // var<Volume>` used elsewhere in this file for the arena tests,
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
    // that is how a Round step makes the change of value visible.
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

TEST_CASE("a logarithm step records its own kind and the error an irrational value is", "[trace]")
{
    auto const traced = [](auto const& node) {
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        (void) formula::checked_evaluate_si<formula::Rational>(
            node, formula::environment(formula::Measured<Ratio> { formula::Rational { 2 } }), sink);
        return trace;
    };
    formula::Trace<> const natural = traced(formula::ln(var<Ratio>));
    REQUIRE(natural.steps.size() == 2);
    CHECK(natural.steps[1].kind == formula::StepKind::NaturalLogarithm);
    CHECK(natural.steps[1].error == formula::ArithmeticError::Inexact);
    CHECK_FALSE(natural.steps[1].value.has_value());
    CHECK(natural.steps[1].unit == formula::coherent(formula::dim::Scalar));
    REQUIRE(natural.steps[1].operands.size() == 1);
    CHECK(natural.steps[1].operands[0] == 0);
    CHECK(traced(formula::log10(var<Ratio>)).steps[1].kind == formula::StepKind::DecimalLogarithm);
    CHECK(traced(formula::exp(var<Ratio>)).steps[1].kind == formula::StepKind::Exponential);
}

TEST_CASE("a rounded logarithm step records its places and mode and no unit of its own", "[trace]")
{
    constexpr auto node =
        formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfAwayFromZero>(var<Ratio>);
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(
        node, formula::environment(formula::Measured<Ratio> { formula::Rational { 2 } }), sink);
    REQUIRE(trace.steps.size() == 2);
    CHECK(trace.steps[1].kind == formula::StepKind::RoundedNaturalLogarithm);
    CHECK(trace.steps[1].granularity == 4);
    CHECK(trace.steps[1].mode == formula::RoundingMode::HalfAwayFromZero);
    CHECK(trace.steps[1].unit == formula::coherent(formula::dim::Scalar));
    CHECK(trace.steps[1].value == formula::Rational { 6931, 10000 });
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
    CHECK(trace.steps[1].kind == formula::StepKind::Constant);   // predicate rhs: 473/10 MPa
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
    auto const outcome = formula::check(atLeastMinimum, strengthOf(formula::Rational { 45 }), sink);

    CHECK(outcome.is_satisfied());
    REQUIRE(trace.steps.size() == 3);

    auto const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::Constraint);
    CHECK(root.comparison == formula::Comparison::GreaterOrEqual);
    CHECK(root.outcome.is_satisfied());
    CHECK_FALSE(root.outcome.verdict().has_value());
    // The predicate's own two sides -- f, then 273/10 MPa -- exactly as a
    // Conditional step claims its predicate's two sides.
    REQUIRE(root.operands.size() == 2);
    CHECK(root.operands[0] == 0);
    CHECK(root.operands[1] == 1);
}

TEST_CASE("a Constraint step records a violated verdict, carrying it", "[trace]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const outcome = formula::check(atLeastMinimum, strengthOf(formula::Rational { 20 }), sink);

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
        formula::check(atLeastMinimum, formula::environment(formula::Measured<Strength>::absent()), sink);

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
            formula::check_all(formula::constraints(atLeastMinimum, diameterAtMostLimit), environment, sink);

        REQUIRE(outcomes[0].is_violated());
        REQUIRE(outcomes[1].is_violated());
        // f, 273/10 MPa, Constraint; d, 139 mm, Constraint.
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
            formula::check_all(formula::constraints(diameterAtMostLimit, atLeastMinimum), environment, sink);

        REQUIRE(outcomes[0].is_violated());
        REQUIRE(outcomes[1].is_violated());
        // Same shape, reversed: d, 139 mm, Constraint; f, 273/10 MPa, Constraint.
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
///  - the widths are 57/50, 58/25 and 101/25 cm -- unequal, so a recorder
///    taking a width from the first row is visible on the others;
///  - no bound equals its own row's index, so a bound cannot be confused with
///    an index;
///  - the two shared boundaries (241/100 and 473/100) differ from each other, so
///    "always report the first band" is visible on every row;
///  - three bounds are declared **unreduced**, so that reducing them is a
///    decision a reader can see being made rather than one no fixture can
///    tell was taken -- and crucially the table's **outer** bounds (`254/200`
///    and `1754/200`) are among them, because those are the only two a covered-range
///    rendering ever reads. A first revision of this fixture left the outer
///    bounds in lowest terms and put the unreduced ones in the middle, and
///    with that fixture `closed_range_text` could stop reducing altogether
///    and the whole suite still passed;
///  - the three corrections are distinct and equal to no index and no bound.
inline constexpr BandTable<3> SizeBands {
    band(254, 200, 241, 100),  // 127/100 to under 241/100 cm -- 254/200 declared, and it is the table's low end
    band(241, 100, 946, 200),  // 241/100 to under 473/100 cm -- 946/200 declared, so reduction shows
    band(473, 100, 1754, 200), // 473/100 to under 877/100 cm -- 1754/200 declared, and it is the table's high end
};

/// The corrections are stated in **percent**, again not the coherent SI unit
/// of their own dimension, so the result side of the table is converted too
/// rather than passed through.
[[nodiscard]] constexpr auto sizeLookup()
{
    return banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(var<Diameter>,
                                                                     { rat(863, 10), rat(1127, 10), rat(1043, 10) });
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
    return exact_lookup<ShapeKeys, unit::Megapascal>(shape, { rat(2791, 1000), rat(43), rat(1373, 1000) });
}

/// The unsigned half of the same question: an underlying type whose top half
/// no signed type can hold, with a key sitting up there.
enum class ApparatusVariant : unsigned long long
{
    Modern = 2,
    Legacy = 18446744073709551615ULL,
};

inline constexpr KeyTable<ApparatusVariant, 2> ApparatusKeys { ApparatusVariant::Modern, ApparatusVariant::Legacy };

/// Three breakpoints in centimetres, unequally spaced (48/25 then 231/50), none of
/// them reduced -- every reason `SizeBands` above gives, unchanged, including
/// that the **outer** rows are unreduced because they are the only two a
/// covered-range rendering ever reads.
inline constexpr BreakpointTable<3> CurvePoints {
    breakpoint(278, 200),  // 139/100 cm -- the curve's low end, declared unreduced
    breakpoint(662, 200),  // 331/100 cm -- declared unreduced, and in the middle
    breakpoint(2379, 300), // 793/100 cm -- the curve's high end, declared unreduced
};

[[nodiscard]] constexpr auto curveLookup()
{
    return interpolating_lookup<unit::Centimetre, CurvePoints, unit::Percent>(
        var<Diameter>, { rat(873, 10), rat(-1139, 10), rat(1217, 10) });
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
inline constexpr BandTable<1> WideBand { band(0, 1, 103, 1) };

/// An inner table whose corrections are **lengths**, so that it can stand
/// where the outer table's operand stands and a lookup can be nested inside a
/// lookup -- the one arrangement in which two steps of a derivation carry the
/// identical `DomainError` for entirely different reasons.
inline constexpr BandTable<2> InnerBands {
    band(218, 200, 307, 100),  // 109/100 to under 307/100 cm
    band(307, 100, 1226, 200), // 307/100 to under 613/100 cm
};

/// An exact table whose corrections are stated in **kilometres**, so that a
/// row that IS found can still fail on the way out: 2^62 km is a perfectly
/// representable `Rational` that does not survive being multiplied by 1000.
/// That is the one failure an exact lookup can have which is not a miss, and
/// the exact lookup is a kind where no other own-failure state exists to
/// confuse it with -- which is precisely why nothing else pins it.
inline constexpr KeyTable<SpecimenShape, 2> FarKeys { SpecimenShape::Cube, SpecimenShape::Cylinder };

/// Two rows in centimetres whose values are stated in **kilometres**. 0 cm
/// sits exactly on the first row, so the interpolation performs no arithmetic
/// at all and cannot overflow -- and the row's own 2^62 km then does not
/// survive the conversion into metres. The one table that separates "the
/// interpolation overflowed" from "the conversion after it did".
inline constexpr BreakpointTable<2> FarValues { breakpoint(0), breakpoint(437, 100) };

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
    // 30 mm == 3 cm, inside the MIDDLE band [241/100, 473/100) cm -- the position a
    // defect is hardest to see from either end.
    auto const result = formula::checked_evaluate_si<formula::Rational>(sizeLookup(), diameterOf(30), sink);

    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    REQUIRE(trace.steps.size() == 2);

    auto const& step = trace.steps[1];
    CHECK(step.kind == formula::StepKind::BandedLookup);
    CHECK(step.lookupFailure == formula::LookupFailure::None);

    // The band the value fell in, exactly as the table declared it -- 946/200
    // and not 473/100, because reducing it is the renderer's decision and not this
    // recorder's to take on its behalf.
    REQUIRE(step.selectedBand.has_value());
    CHECK(*step.selectedBand == band(241, 100, 946, 200));
    // Nothing about the table's extent is claimed on a hit.
    CHECK(!step.coveredRange.has_value());
    // A banded lookup selects a band and not a segment.
    CHECK(!step.selectedSegment.has_value());

    // Three units, all different, and a step that confused any two of them
    // would state a number in a scale nobody declared it in: the value is
    // stored in coherent SI (dimensionless), it was *declared* in percent, and
    // the band bounds it was compared against are in centimetres.
    CHECK(step.unit == unit::Percent);
    CHECK(step.sourceUnit == unit::Centimetre);
    CHECK(step.value == rat(1127, 1000));

    REQUIRE(step.operands.size() == 1);
    CHECK(step.operands[0] == 0);
    CHECK(trace.steps[0].value == rat(3, 100)); // 30 mm, in metres

    // The first and the last band as well, because a recorder that reported a
    // fixed row would be invisible against the middle one alone in one
    // direction and against either end alone in the other. The last band in
    // particular is where the table's own high bound sits, so it is the row a
    // recorder confusing "the band found" with "what the table covers" would
    // land on by accident.
    // The locals are named apart from the enclosing test's, because GCC's
    // `-Wshadow` -- which this project's Linux leg runs with warnings as
    // errors -- flags a lambda capturing nothing whose own locals share a
    // name with one in the enclosing scope.
    auto const bandAt = [](std::int64_t millimetres) {
        formula::Trace<> probe {};
        formula::RecordingSink<> probeSink { probe };
        (void) formula::checked_evaluate_si<formula::Rational>(sizeLookup(), diameterOf(millimetres), probeSink);
        REQUIRE(probe.steps.size() == 2);
        return probe.steps[1].selectedBand;
    };

    CHECK(bandAt(15) == std::optional { band(254, 200, 241, 100) });  // 3/2 cm, the first band
    CHECK(bandAt(70) == std::optional { band(473, 100, 1754, 200) }); // 7 cm, the last band
}

TEST_CASE("a banded lookup step that missed records the miss and what its bands cover", "[trace][lookup]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    // 95 mm == 19/2 cm, past the last band, which ends at 877/100 cm.
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
    // Repeated back as the table declared them (254/200 and 1754/200), not reduced --
    // reducing is the renderer's decision, made in the one place that already
    // reduces every other declared bound.
    REQUIRE(step.coveredRange.has_value());
    CHECK(*step.coveredRange == formula::LookupRange { 254, 200, 1754, 200 });
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
        banded_lookup<unit::Centimetre, InnerBands, unit::Millimetre>(var<Diameter>, { rat(947), rat(373, 10) });
    constexpr auto nested =
        banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(inner, { rat(863, 10), rat(1127, 10), rat(1043, 10) });

    // 15 mm == 3/2 cm: the inner table HITS its first band and answers
    // 947 mm == 947/10 cm, which the outer table -- covering 127/100 to under
    // 877/100 cm --
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

    // 95 mm == 19/2 cm: the INNER table misses, and the outer one relays its
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
    // The middle row's name, recorded while the key's type was still known.
    // Kills a recorder that leaves the name empty, and one that names the
    // first or last row whatever the key.
    CHECK(hit.steps[0].lookupKeyName == "Undercut");
    CHECK(hit.steps[0].lookupFailure == formula::LookupFailure::None);
    CHECK(hit.steps[0].unit == unit::Megapascal);
    CHECK(hit.steps[0].value == rat(43000000)); // 43 MPa, in pascals
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
    // `Beam` has a name in the author's source, but the table has no row for
    // it, and a step's name comes from matching against the table's own keys.
    // Kills a recorder that names a missed key anyway.
    CHECK(miss.steps[0].lookupKeyName.empty());
    // Nothing to cover: an exact table's domain is a set of keys, not an
    // interval, so there is no range to report and none is invented.
    CHECK(!miss.steps[0].coveredRange.has_value());
}

TEST_CASE("an exact lookup step records an unsigned key that no signed type could hold", "[trace][lookup]")
{
    // `render.hpp` spells its two casts separately because an enumeration's
    // underlying type may be `unsigned long long`; a step that stored the key
    // as a signed integer would report -1 for this one.
    constexpr auto node =
        exact_lookup<ApparatusKeys, unit::One>(ApparatusVariant::Legacy, { rat(1127, 1000), rat(863, 1000) });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, formula::environment(), sink);

    REQUIRE(trace.steps.size() == 1);
    CHECK(!trace.steps[0].lookupKeyIsSigned);
    CHECK(trace.steps[0].lookupKey == 18446744073709551615ULL);
    // The name is matched against the table by the key's value, and this
    // value is the top of `unsigned long long`: the only test in which the
    // recorder names a key no signed type could hold.
    CHECK(trace.steps[0].lookupKeyName == "Legacy");
    CHECK(trace.steps[0].lookupFailure == formula::LookupFailure::None);
}

/// A table that declares a row under a value naming no enumerator -- legal,
/// since a `KeyTable` holds values of the enumeration, not only its
/// enumerators. Its values differ from `ShapeKeys`' so the two cannot share a
/// template parameter object (see `trace_render_tests.cpp`'s `RenderedShape`).
inline constexpr KeyTable<SpecimenShape, 2> UnnamedRowKeys { SpecimenShape::Cube, static_cast<SpecimenShape>(9) };

TEST_CASE("an exact lookup step that hits a row whose key names no enumerator records no name, and no miss",
          "[trace][lookup]")
{
    // An empty `lookupKeyName` is not a miss by itself: this lookup HIT the
    // second row, whose key has no name to record. `lookupFailure` is what
    // says whether it missed. Kills a reading of "empty name" as "missed", and
    // a recorder that invents a name for the row.
    constexpr auto node =
        exact_lookup<UnnamedRowKeys, unit::One>(static_cast<SpecimenShape>(9), { rat(1127, 1000), rat(863, 1000) });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, formula::environment(), sink);

    REQUIRE(trace.steps.size() == 1);
    CHECK(trace.steps[0].lookupFailure == formula::LookupFailure::None);
    CHECK(trace.steps[0].value == rat(863, 1000));
    CHECK(trace.steps[0].lookupKeyName.empty());
    CHECK(static_cast<long long>(trace.steps[0].lookupKey) == 9);
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
    // 100 mm == 10 cm, past the curve's last row at 793/100 cm. No extrapolation and
    // no clamp -- a miss, exactly as a banded lookup's is.
    (void) formula::checked_evaluate_si<formula::Rational>(curveLookup(), diameterOf(100), sink);

    REQUIRE(trace.steps.size() == 2);
    CHECK(trace.steps[1].error == formula::ArithmeticError::DomainError);
    CHECK(trace.steps[1].lookupFailure == formula::LookupFailure::Missed);
    REQUIRE(trace.steps[1].coveredRange.has_value());
    CHECK(*trace.steps[1].coveredRange == formula::LookupRange { 278, 200, 2379, 300 });
    CHECK(trace.steps[1].sourceUnit == unit::Centimetre);

    // A value inside the curve is not a miss, and an interpolating lookup
    // selects no band on the way: it names the two rows its answer came from
    // instead, because between them the answer appears in neither.
    formula::Trace<> inside {};
    {
        formula::RecordingSink<> insideSink { inside };
        (void) formula::checked_evaluate_si<formula::Rational>(curveLookup(), diameterOf(60), insideSink);
    }
    REQUIRE(inside.steps.size() == 2);
    CHECK(inside.steps[1].lookupFailure == formula::LookupFailure::None);
    CHECK(!inside.steps[1].selectedBand.has_value());
    CHECK(!inside.steps[1].coveredRange.has_value());
    // 6 cm sits in the SECOND segment, so a recorder reaching for a fixed
    // pair -- the first, or the one whose index matches -- is visible. The
    // keys come back as the table declared them, unreduced.
    REQUIRE(inside.steps[1].selectedSegment.has_value());
    CHECK(*inside.steps[1].selectedSegment == formula::Segment { breakpoint(662, 200), breakpoint(2379, 300) });

    // And 2 cm sits in the FIRST segment. Both ends of that axis, because
    // "reports the first pair" and "reports the last pair" are two mutations
    // and a fixture that only ever selects one end can see only one of them.
    // A suite probing the second segment alone lets "report the last pair"
    // through in silence -- measured, not supposed.
    formula::Trace<> firstSegment {};
    {
        formula::RecordingSink<> firstSink { firstSegment };
        (void) formula::checked_evaluate_si<formula::Rational>(curveLookup(), diameterOf(20), firstSink);
    }
    REQUIRE(firstSegment.steps.size() == 2);
    REQUIRE(firstSegment.steps[1].selectedSegment.has_value());
    CHECK(*firstSegment.steps[1].selectedSegment == formula::Segment { breakpoint(278, 200), breakpoint(662, 200) });

    // A value sitting exactly ON a row reports that row twice, which is this
    // type's spelling for "the table stated this number directly".
    formula::Trace<> onRow {};
    {
        formula::RecordingSink<> rowSink { onRow };
        (void) formula::checked_evaluate_si<formula::Rational>(curveLookup(), diameterOf(331, 10), rowSink);
    }
    REQUIRE(onRow.steps.size() == 2);
    REQUIRE(onRow.steps[1].selectedSegment.has_value());
    CHECK(*onRow.steps[1].selectedSegment == formula::Segment { breakpoint(662, 200), breakpoint(662, 200) });

    // And on the curve's LAST row, which is the behaviour `lookup.hpp` pins
    // on its own curve -- a breakpoint is a row and not a boundary, so the last one is
    // reached, and interpolating to it is not merely equivalent but
    // impossible because it begins no segment. The same mirror as above: the
    // row hits in this file all landed on row index 1, so "report row 1"
    // survived everything until this probe existed.
    formula::Trace<> lastRow {};
    {
        formula::RecordingSink<> lastSink { lastRow };
        (void) formula::checked_evaluate_si<formula::Rational>(curveLookup(), diameterOf(793, 10), lastSink);
    }
    REQUIRE(lastRow.steps.size() == 2);
    CHECK(lastRow.steps[1].lookupFailure == formula::LookupFailure::None);
    CHECK(lastRow.steps[1].value == rat(1217, 1000));
    REQUIRE(lastRow.steps[1].selectedSegment.has_value());
    CHECK(*lastRow.steps[1].selectedSegment == formula::Segment { breakpoint(2379, 300), breakpoint(2379, 300) });
}

TEST_CASE("an exact lookup that found its row can still fail converting it out", "[trace][lookup]")
{
    // The one own-failure an exact lookup has that is not a miss. Its own kind
    // is where that confusion is hardest to catch -- an exact lookup cannot
    // interpolate, so there is no `Computation` state to mix it up with, and
    // nothing else here would notice the recorder leaving the field alone.
    constexpr auto node =
        exact_lookup<FarKeys, unit::Kilometre>(SpecimenShape::Cylinder, { rat(1127, 1000), rat(Huge) });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, formula::environment(), sink);

    REQUIRE(trace.steps.size() == 1);
    CHECK(trace.steps[0].error == formula::ArithmeticError::Overflow);
    // The row WAS found: `Cylinder` is row 1 of this table, and 2^62 km is a
    // perfectly good `Rational` until it is asked to become metres.
    CHECK(trace.steps[0].lookupFailure == formula::LookupFailure::Conversion);
    CHECK(static_cast<long long>(trace.steps[0].lookupKey) == 7);
}

TEST_CASE("an interpolating lookup separates its own overflow from the conversion after it",
          "[trace][lookup]")
{
    // Both are this node's **own** failures, both carry `Overflow`, and only
    // one of them is the interpolation. Reporting the conversion as
    // `Computation` would print "the interpolation itself overflowed" about an
    // interpolation that performed no arithmetic at all -- the euphemism this
    // whole field exists to refuse, one enumerator to the left of where it was
    // refused.
    constexpr auto node =
        interpolating_lookup<unit::Centimetre, FarValues, unit::Kilometre>(var<Diameter>, { rat(Huge), rat(1127, 1000) });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, diameterOf(0), sink);

    REQUIRE(trace.steps.size() == 2);
    CHECK(trace.steps[1].error == formula::ArithmeticError::Overflow);
    CHECK(trace.steps[1].lookupFailure == formula::LookupFailure::Conversion);
    // The curve answered, so no row is claimed to be missing and no segment is
    // claimed to have been used.
    CHECK(!trace.steps[1].coveredRange.has_value());
    CHECK(!trace.steps[1].selectedSegment.has_value());
}

TEST_CASE("an interpolating lookup whose key conversion failed never consulted its curve",
          "[trace][lookup]")
{
    // The third own-failure, on the other side of the curve: converting 2^62
    // metres into the table's centimetres overflows before any row is looked
    // at. Reporting it as a miss would print "the curve declares no rows"
    // about a three-row curve.
    constexpr auto node = interpolating_lookup<unit::Centimetre, CurvePoints, unit::Percent>(
        formula::constant<unit::Metre>(rat(Huge)), { rat(873, 10), rat(-1139, 10), rat(1217, 10) });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, formula::environment(), sink);

    REQUIRE(trace.steps.size() == 2);
    CHECK(!trace.steps[0].error.has_value()); // the operand itself was fine
    CHECK(trace.steps[1].error == formula::ArithmeticError::Overflow);
    CHECK(trace.steps[1].lookupFailure == formula::LookupFailure::Conversion);
    CHECK(!trace.steps[1].coveredRange.has_value());
}

TEST_CASE("a lookup whose own unit conversion failed is not recorded as a miss", "[trace][lookup]")
{
    // Own failures are not all misses, and a recorder answering "missed" for
    // every error of its own would be as wrong as one answering it for every
    // relayed error.

    // The result side: 30 mm is comfortably inside [0, 103) mm, so the band IS
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
        formula::constant<unit::Metre>(rat(Huge)), { rat(863, 10), rat(1127, 10), rat(1043, 10) });
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
        banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(UntracedLength {},
                                                                  { rat(863, 10), rat(1127, 10), rat(1043, 10) });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, formula::environment(), sink);

    REQUIRE(trace.steps.size() == 1);
    CHECK(trace.steps[0].operands.empty());
    CHECK(trace.steps[0].error == formula::ArithmeticError::DivisionByZero);
    CHECK(trace.steps[0].lookupFailure == formula::LookupFailure::Undetermined);

    // The interpolating kind reaches the same state by the same route, and it
    // is a separate block of code rather than a shared one -- so asserting it
    // only for the banded kind would leave a copy nothing enters.
    constexpr auto curve = interpolating_lookup<unit::Centimetre, CurvePoints, unit::Percent>(
        UntracedLength {}, { rat(873, 10), rat(-1139, 10), rat(1217, 10) });

    formula::Trace<> curveTrace {};
    formula::RecordingSink<> curveSink { curveTrace };
    (void) formula::checked_evaluate_si<formula::Rational>(curve, formula::environment(), curveSink);

    REQUIRE(curveTrace.steps.size() == 1);
    CHECK(curveTrace.steps[0].error == formula::ArithmeticError::DivisionByZero);
    CHECK(curveTrace.steps[0].lookupFailure == formula::LookupFailure::Undetermined);
}

TEST_CASE("a lookup whose operand recorded no step names no band even when it hits", "[trace][lookup]")
{
    // The other half of the same honesty, and the easier one to get wrong: the
    // lookup succeeded, so there is a band -- and the recorder still has no
    // value to locate it with, because the operand contributed no step. The
    // answer is silence, not the first band and not whichever one a scan with
    // nothing to scan for would land on.
    constexpr auto node = banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(
        UntracedThreeCentimetres {}, { rat(863, 10), rat(1127, 10), rat(1043, 10) });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::checked_evaluate_si<formula::Rational>(node, formula::environment(), sink);

    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    // 3 cm really is inside the middle band, so this is a hit and not a
    // degenerate case dressed up as one.
    CHECK(**result == rat(1127, 1000));

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

// ---------------------------------------------------------------------------
// A method's variant selection, recorded
//
// The step's NAME is pinned in `trace_render_tests.cpp` -- "the trace names
// which variant fired and on what discriminator" -- and, apart from the
// customized spelling below, these tests select the FIRST variant whenever
// they read `variantTag`, so that a recorder naming the first variant
// whatever was selected is killed by that one test alone. Everything else the
// step carries is pinned here.
// ---------------------------------------------------------------------------

namespace
{
struct Plate;
struct Disc;
struct Ring;

struct Load: formula::Quantity<Load, "F", "applied load", unit::Kilonewton>
{
};
struct Side: formula::Quantity<Side, "a", "loaded side", unit::Millimetre>
{
};

// A spelling of the author's own for the first variant's tag, as a published
// method might word it.
struct Core;
} // namespace

template <>
struct formula::TagName<Core>
{
    static constexpr std::string_view of() noexcept { return "core drilled 103 mm"; }
};

namespace
{
inline constexpr auto rule =
    formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>();

inline constexpr auto bearing = formula::method(
    formula::variants(formula::variant<Plate>(var<Load> / (var<Side> * var<Side>)),
                      formula::variant<Disc>(var<Load> / (formula::pi * var<Side> * var<Side>)),
                      formula::variant<Ring>(var<Load> / (var<Side> * var<Side> * formula::Rational { 2 }))),
    rule,
    formula::constraints());

[[nodiscard]] auto loadOn(long long load, long long side)
{
    return formula::environment(formula::Measured<Load> { formula::Rational { load } },
                                formula::Measured<Side> { formula::Rational { side } });
}

/// A sink that defines only half of the variant pair, and counts it.
struct HalfVariantSink
{
    int* told;

    template <formula::Node N>
    constexpr void entered(N const&) noexcept
    {
    }

    template <formula::Node N, typename V>
    constexpr void produced(N const&, V const&) noexcept
    {
    }

    void variant_entered(formula::VariantSelection const&) noexcept { ++*told; }
};
} // namespace

TEST_CASE("a method's selection is the root step, and claims the rounded variant as its operand", "[trace][method]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::evaluate_method<Disc>(bearing, loadOn(100, 50), sink);

    REQUIRE(!trace.empty());
    formula::Step<> const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::VariantSelected);

    // The second of three, zero-based.
    CHECK(root.variantIndex == 1);
    CHECK(root.variantCount == 3);

    // One operand: the rounding step, which is the variant that ran. The
    // selection is the walk's one root, and nothing is left unclaimed.
    REQUIRE(root.operands.size() == 1);
    formula::Step<> const& variant = trace.steps[root.operands.front()];
    CHECK(variant.kind == formula::StepKind::RoundingRuleApplied);
    CHECK(trace.unclaimed.size() == 1);
    CHECK(trace.marks.empty());

    // The step's value is exactly what the method returned, in the unit the
    // variant was rounded in.
    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    REQUIRE(root.value.has_value());
    CHECK(*root.value == **result);
    CHECK(*root.value == *variant.value);
    CHECK(root.dimension == formula::dim::Pressure);
    CHECK(root.unit == unit::Megapascal);
}

TEST_CASE("a selection records the tag's name, or the author's spelling of it", "[trace][method]")
{
    // The first variant, whose tag is named the author's way.
    constexpr auto cored = formula::method(
        formula::variants(formula::variant<Core>(var<Load> / (var<Side> * var<Side>)),
                          formula::variant<Plate>(var<Load> / (var<Side> * var<Side> * formula::Rational { 2 }))),
        rule,
        formula::constraints());

    formula::Trace<> customized {};
    (void) formula::evaluate_method<Core>(cored, loadOn(100, 50), formula::RecordingSink<> { customized });
    CHECK(customized.steps[customized.root()].variantTag == "core drilled 103 mm");

    // And a tag nobody customized, by its own name: unqualified, with the
    // anonymous namespace it is declared in nowhere in sight.
    formula::Trace<> reflected {};
    (void) formula::evaluate_method<Plate>(bearing, loadOn(100, 50), formula::RecordingSink<> { reflected });
    CHECK(reflected.steps[reflected.root()].variantTag == "Plate");
}

TEST_CASE("a selected variant that fails is recorded with its failure, not a value", "[trace][method]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::evaluate_method<Plate>(bearing, loadOn(100, 0), sink);

    REQUIRE(!result.has_value());
    formula::Step<> const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::VariantSelected);
    REQUIRE(root.error.has_value());
    CHECK(*root.error == result.error());
    CHECK(!root.value.has_value());
    // Still one operand, which carries the same failure, and still one root.
    REQUIRE(root.operands.size() == 1);
    CHECK(trace.steps[root.operands.front()].error == root.error);
    CHECK(trace.unclaimed.size() == 1);
}

TEST_CASE("a selected variant with an absent input is recorded as absent, not as failed", "[trace][method]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const result = formula::evaluate_method<Plate>(
        bearing,
        formula::environment(formula::Measured<Load>::absent(), formula::Measured<Side> { formula::Rational { 50 } }),
        sink);

    REQUIRE(result.has_value());
    CHECK(!result->has_value());
    formula::Step<> const& root = trace.steps[trace.root()];
    CHECK(root.kind == formula::StepKind::VariantSelected);
    CHECK(!root.value.has_value());
    CHECK(!root.error.has_value());
}

TEST_CASE("a sink that defines half of the variant pair is told nothing", "[trace][method]")
{
    // Both or neither, asked in one `requires`: a sink told of an entry it
    // will never see closed would leave its own bookkeeping unbalanced.
    int told = 0;
    auto const result = formula::evaluate_method<Plate>(bearing, loadOn(100, 50), HalfVariantSink { &told });

    REQUIRE(result.has_value());
    CHECK(result->has_value());
    CHECK(told == 0);
}

TEST_CASE("an overlaid method's selection is counted in the method as published", "[trace][method][overlay]")
{
    // A reader counts back in the only `variants(...)` in the source, the
    // published one; the overlay that pinned or pruned is elsewhere. So the
    // position a trace reports must not move when a variant before it is
    // removed. Tags are not read here -- see the note above this section.
    constexpr auto pruned = formula::apply(
        formula::overlay(formula::prune_variant<Plate>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        bearing);
    formula::Trace<> afterPrune {};
    (void) formula::evaluate_method<Disc>(pruned, loadOn(100, 50), formula::RecordingSink<> { afterPrune });
    formula::Step<> const& prunedRoot = afterPrune.steps[afterPrune.root()];
    CHECK(prunedRoot.variantIndex == 1);
    CHECK(prunedRoot.variantCount == 3);

    // The last variant, with the first pruned: 3rd of 3, not 2nd of 2.
    formula::Trace<> lastAfterPrune {};
    (void) formula::evaluate_method<Ring>(pruned, loadOn(100, 50), formula::RecordingSink<> { lastAfterPrune });
    CHECK(lastAfterPrune.steps[lastAfterPrune.root()].variantIndex == 2);
    CHECK(lastAfterPrune.steps[lastAfterPrune.root()].variantCount == 3);

    // A pin leaves one variant, which is still the 2nd of 3, not the 1st of 1.
    constexpr auto pinned = formula::apply(
        formula::overlay(formula::pin_variant<Disc>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        bearing);
    formula::Trace<> afterPin {};
    (void) formula::evaluate_method<Disc>(pinned, loadOn(100, 50), formula::RecordingSink<> { afterPin });
    CHECK(afterPin.steps[afterPin.root()].variantIndex == 1);
    CHECK(afterPin.steps[afterPin.root()].variantCount == 3);

    // And through an operation that rewrites every variant, after the prune
    // that moved them: the positions survive the rewrite too.
    constexpr auto rewritten = formula::apply(
        formula::overlay(formula::prune_variant<Plate>(formula::Citation { .reference = "Example Standard 12:2021 NA" }),
                         formula::with_constant<Side>(formula::Rational { 473, 10 },
                                                      formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        bearing);
    formula::Trace<> afterRewrite {};
    (void) formula::evaluate_method<Ring>(rewritten, loadOn(100, 7), formula::RecordingSink<> { afterRewrite });
    CHECK(afterRewrite.steps[afterRewrite.root()].variantIndex == 2);
    CHECK(afterRewrite.steps[afterRewrite.root()].variantCount == 3);
}

TEST_CASE("an overlay applied at run time still counts in the method as published", "[trace][method][overlay]")
{
    // Nothing here is a constant expression: each method is an ordinary local
    // built from the one before it, so the layout each overlay starts from is
    // run time data. The second prune starts from a layout the first one
    // moved, `{ 1, 2 }` of 3, so a layout rebuilt from the tags alone -- or
    // from the pack's own order -- would report the Ring as the 1st of 1.
    auto const base = bearing;
    auto const withoutPlate = formula::apply(
        formula::overlay(formula::prune_variant<Plate>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        base);
    auto const ringOnly = formula::apply(
        formula::overlay(formula::prune_variant<Disc>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        withoutPlate);

    formula::Trace<> afterTwoPrunes {};
    (void) formula::evaluate_method<Ring>(ringOnly, loadOn(100, 50), formula::RecordingSink<> { afterTwoPrunes });
    CHECK(afterTwoPrunes.steps[afterTwoPrunes.root()].variantIndex == 2);
    CHECK(afterTwoPrunes.steps[afterTwoPrunes.root()].variantCount == 3);

    // A pin over the moved layout: the Disc is still the 2nd of 3.
    auto const discOnly = formula::apply(
        formula::overlay(formula::pin_variant<Disc>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        withoutPlate);
    formula::Trace<> afterPin {};
    (void) formula::evaluate_method<Disc>(discOnly, loadOn(100, 50), formula::RecordingSink<> { afterPin });
    CHECK(afterPin.steps[afterPin.root()].variantIndex == 1);
    CHECK(afterPin.steps[afterPin.root()].variantCount == 3);
}

TEST_CASE("overlay steps compose, and the position still counts in the method as published", "[trace][method][overlay]")
{
    // Two prunes in ONE overlay: the second is applied to the method the
    // first produced, whose layout is already `{ 1, 2 }` of 3.
    constexpr auto twoPrunes = formula::apply(
        formula::overlay(formula::prune_variant<Plate>(formula::Citation { .reference = "Example Standard 12:2021 NA" }),
                         formula::prune_variant<Disc>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        bearing);
    formula::Trace<> afterTwoPrunes {};
    (void) formula::evaluate_method<Ring>(twoPrunes, loadOn(100, 50), formula::RecordingSink<> { afterTwoPrunes });
    CHECK(afterTwoPrunes.steps[afterTwoPrunes.root()].variantIndex == 2);
    CHECK(afterTwoPrunes.steps[afterTwoPrunes.root()].variantCount == 3);

    // A prune, then a pin: two `apply` calls, since one overlay refuses both.
    // The pin picks from the pruned layout, and keeps the Ring's own place.
    constexpr auto pruneThenPin = formula::apply(
        formula::overlay(formula::pin_variant<Ring>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        formula::apply(formula::overlay(
                           formula::prune_variant<Plate>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                       bearing));
    formula::Trace<> afterPruneThenPin {};
    (void) formula::evaluate_method<Ring>(pruneThenPin, loadOn(100, 50), formula::RecordingSink<> { afterPruneThenPin });
    CHECK(afterPruneThenPin.steps[afterPruneThenPin.root()].variantIndex == 2);
    CHECK(afterPruneThenPin.steps[afterPruneThenPin.root()].variantCount == 3);

    // Two prunes by two `apply` calls, in the order that removes the MIDDLE
    // variant first, so the second prune acts on the layout `{ 0, 2 }`.
    constexpr auto twoApplies = formula::apply(
        formula::overlay(formula::prune_variant<Plate>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        formula::apply(
            formula::overlay(formula::prune_variant<Disc>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
            bearing));
    formula::Trace<> afterTwoApplies {};
    (void) formula::evaluate_method<Ring>(twoApplies, loadOn(100, 50), formula::RecordingSink<> { afterTwoApplies });
    CHECK(afterTwoApplies.steps[afterTwoApplies.root()].variantIndex == 2);
    CHECK(afterTwoApplies.steps[afterTwoApplies.root()].variantCount == 3);
}

TEST_CASE("a result told without its entry is dropped, never read off an empty stack", "[trace]")
{
    // A consumer's own evaluator that calls `produced` but forgot `entered`:
    // there is no mark to claim from. Reading one off the empty stack was
    // undefined behaviour -- cl's debug library aborts the program -- so the
    // sink drops the step instead. The same for each of the other three
    // pairs a sink is told.
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const value = formula::Evaluated<formula::Rational> { std::optional { formula::Rational { 1 } } };

    sink.produced(var<Mass>, value);
    constexpr auto limit =
        formula::constraint(var<Mass> >= formula::constant<unit::Kilogram>(formula::Rational { 973, 1000 }),
                            formula::Verdict { "too light" });
    sink.constraint_produced(limit, formula::ConstraintOutcome::satisfied());
    sink.variant_produced(formula::VariantSelection { "Cube", 0, 1 }, value);
    sink.acceptance_produced(formula::ConstraintOrigin {});

    CHECK(trace.steps.empty());
    CHECK(trace.marks.empty());

    // And the sink still records a walk that follows, whole.
    (void) formula::checked_evaluate_si<formula::Rational>(
        var<Mass>, formula::environment(formula::Measured<Mass> { formula::Rational { 6 } }), sink);
    CHECK(trace.steps.size() == 1);
}

TEST_CASE("a branch told with no when() entered is dropped, never read off an empty stack", "[trace]")
{
    // The same consumer mistake as above, for the pending branch a `when()`
    // pushes in `entered`: `branch_taken` with nothing entered, and a
    // `when()`'s `produced` after only some other node's `entered`. Both read
    // `back()` of an empty stack, which is undefined behaviour.
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    auto const value = formula::Evaluated<formula::Rational> { std::optional { formula::Rational { 1 } } };

    sink.branch_taken(chosen, true);
    CHECK(trace.branchStack.empty());

    sink.entered(var<Strength>);
    sink.produced(chosen, value);
    CHECK(trace.steps.empty());
    CHECK(trace.branchStack.empty());
    // The mark `var<Strength>` pushed is left for it, and it claims it.
    sink.produced(var<Strength>, value);
    CHECK(trace.steps.size() == 1);
    CHECK(trace.marks.empty());
}

// ---- A series on the trace (phase 12) ----

namespace
{
namespace series_recording
{
    struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", unit::Gram>
    {
    };
    struct Stockpile: formula::Quantity<Stockpile, "m_p", "stockpile mass", unit::Tonne>
    {
    };

    [[nodiscard]] constexpr formula::Measured<Retained> retained(std::int64_t grams)
    {
        return formula::Measured<Retained> { formula::Rational { grams } };
    }

    inline constexpr auto inputs = formula::environment(formula::measured_series<Retained>(
        retained(130), retained(210), formula::Measured<Retained>::absent(), retained(340), retained(28)));

    /// A sink defining only one of the two series hooks: told nothing, since
    /// the evaluator asks for both in one `requires`.
    struct HalfSeriesSink: formula::NullSink
    {
        int* calls;

        template <formula::SeriesNode S>
        void series_entered(S const&) const
        {
            ++*calls;
        }
    };

    /// The other half: a sink defining only `series_produced`, told nothing
    /// either -- so both halves of the rule are pinned.
    struct ProducedOnlySink: formula::NullSink
    {
        int* calls;

        template <formula::SeriesNode S, typename R>
        void series_produced(S const&, R const&) const
        {
            ++*calls;
        }
    };

    /// A sink defining both: told both, once each, and nothing else.
    struct BothSeriesHooks: formula::NullSink
    {
        int* entered;
        int* produced;

        template <formula::SeriesNode S>
        void series_entered(S const&) const
        {
            ++*entered;
        }
        template <formula::SeriesNode S, typename R>
        void series_produced(S const&, R const&) const
        {
            ++*produced;
        }
    };
} // namespace series_recording
} // namespace

TEST_CASE("a series step records every element in coherent SI, and no single value", "[series][trace]")
{
    using series_recording::Retained;
    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::series<Retained, 5>, series_recording::inputs, formula::RecordingSink<> { trace });

    REQUIRE(trace.steps.size() == 1);
    formula::Step<> const& step = trace.steps[0];
    CHECK(step.kind == formula::StepKind::SeriesVariable);
    CHECK(step.symbol == "m_r");
    CHECK(step.unit == formula::unit::Gram); // declared unit, for the renderer to convert back to
    CHECK(step.dimension == formula::unit::Gram.dimension);
    CHECK(!step.value.has_value()); // a series has no single value
    CHECK(!step.error.has_value());
    CHECK(!step.failedElement.has_value());
    // In order, in kilograms, the absent one absent and not zero.
    REQUIRE(step.elements.size() == 5);
    CHECK(step.elements[0] == formula::Rational { 13, 100 });
    CHECK(step.elements[1] == formula::Rational { 21, 100 });
    CHECK(!step.elements[2].has_value());
    CHECK(step.elements[3] == formula::Rational { 17, 50 });
    CHECK(step.elements[4] == formula::Rational { 7, 250 });
    // The walk's root, unclaimed and alone.
    CHECK(trace.unclaimed == std::vector<std::size_t> { 0 });
    CHECK(trace.marks.empty());
}

TEST_CASE("a series step that failed records the error and the element, and no elements", "[series][trace]")
{
    using series_recording::Stockpile;
    constexpr std::int64_t tooLarge = std::numeric_limits<std::int64_t>::max() / 100;
    constexpr auto overflowing = formula::environment(
        formula::measured_series<Stockpile>(formula::Measured<Stockpile> { formula::Rational { 1 } },
                                            formula::Measured<Stockpile> { formula::Rational { 2 } },
                                            formula::Measured<Stockpile> { formula::Rational { tooLarge } },
                                            formula::Measured<Stockpile> { formula::Rational { 3 } }));

    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::series<Stockpile, 4>, overflowing, formula::RecordingSink<> { trace });
    REQUIRE(trace.steps.size() == 1);
    CHECK(trace.steps[0].error == formula::ArithmeticError::Overflow);
    CHECK(trace.steps[0].failedElement == std::optional<std::size_t> { 2 });
    // No partial series: the elements before the failure are not shown as
    // though they were a result.
    CHECK(trace.steps[0].elements.empty());
}

TEST_CASE("a sink hears about a series through both hooks or neither", "[series][trace]")
{
    using series_recording::Retained;
    int halfCalls = 0;
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::series<Retained, 5>, series_recording::inputs, series_recording::HalfSeriesSink { {}, &halfCalls });
    CHECK(halfCalls == 0);

    int producedOnlyCalls = 0;
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::series<Retained, 5>,
        series_recording::inputs,
        series_recording::ProducedOnlySink { {}, &producedOnlyCalls });
    CHECK(producedOnlyCalls == 0);

    int entered = 0;
    int produced = 0;
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::series<Retained, 5>,
        series_recording::inputs,
        series_recording::BothSeriesHooks { {}, &entered, &produced });
    CHECK(entered == 1);
    CHECK(produced == 1);

    // NullSink defines neither, and a consumer's sink written before series
    // existed -- entered/produced only, constrained on Node -- still
    // compiles and is told nothing.
    auto const untraced = formula::detail::dispatch_series<formula::Rational>(
        formula::series<Retained, 5>, series_recording::inputs, formula::NullSink {});
    CHECK(untraced.has_value());
}

TEST_CASE("explain_series returns the outcome and the derivation that produced it", "[series][trace]")
{
    using series_recording::Retained;
    auto const explained = formula::explain_series<Retained>(
        formula::series<Retained, 5>, series_recording::inputs, formula::vocabulary(formula::renames<Retained>("R")));
    // Exactly what checked_evaluate_series returns.
    REQUIRE(explained.outcome.has_value());
    CHECK(explained.outcome
          == formula::checked_evaluate_series<Retained>(formula::series<Retained, 5>, series_recording::inputs));
    CHECK(explained.outcome->element(3).value() == formula::Rational { 340 });
    REQUIRE(explained.trace.steps.size() == 1);
    CHECK(explained.trace.steps[0].symbol == "R"); // in the vocabulary given
    CHECK(explained.trace.steps[0].kind == formula::StepKind::SeriesVariable);

    // A typed-in series was not derived, so there is nothing to trace.
    constexpr auto typedIn = formula::environment(
        formula::entered(formula::measured_series<Retained>(series_recording::retained(1), series_recording::retained(2))));
    auto const overridden = formula::explain_series<Retained>(formula::series<Retained, 2>, typedIn);
    REQUIRE(overridden.outcome.has_value());
    CHECK(overridden.outcome->is_overridden());
    CHECK(overridden.trace.empty());
}

TEST_CASE("explain_series keeps a failure and its element, and the step that failed", "[series][trace]")
{
    // A series has no throwing spelling, so explain_series carries the
    // failure in its outcome rather than throwing it away.
    using series_recording::Stockpile;
    constexpr std::int64_t tooLarge = std::numeric_limits<std::int64_t>::max() / 100;
    constexpr auto overflowing = formula::environment(
        formula::measured_series<Stockpile>(formula::Measured<Stockpile> { formula::Rational { 1 } },
                                            formula::Measured<Stockpile> { formula::Rational { tooLarge } }));
    auto const explained = formula::explain_series<Stockpile>(formula::series<Stockpile, 2>, overflowing);
    REQUIRE(!explained.outcome.has_value());
    CHECK(explained.outcome.error() == formula::SeriesFailure { formula::ArithmeticError::Overflow, 1 });
    REQUIRE(explained.trace.steps.size() == 1);
    CHECK(explained.trace.steps[0].failedElement == std::optional<std::size_t> { 1 });
}

TEST_CASE("traced returns what the evaluation returned with the steps it recorded", "[trace]")
{
    constexpr auto density = var<Mass> / var<Volume>;
    auto const environmentGiven = environmentOf(6, 3);

    auto const recordedRun = formula::traced([&](auto recordingSink)
                                             { return formula::checked_evaluate<Density>(density, environmentGiven, recordingSink); });
    REQUIRE(recordedRun.outcome.has_value());
    CHECK(recordedRun.outcome == formula::checked_evaluate<Density>(density, environmentGiven));
    CHECK(recordedRun.outcome->measurement().value() == formula::Rational { 2 });

    // The steps are the ones a hand-built sink records: m, V, then the division.
    formula::Trace<> handBuilt {};
    (void) formula::checked_evaluate<Density>(density, environmentGiven, formula::RecordingSink<> { handBuilt });
    REQUIRE(recordedRun.trace.steps.size() == 3);
    CHECK(recordedRun.trace.steps[2].kind == formula::StepKind::Divide);
    CHECK(formula::render_trace(recordedRun.trace, { .maxSteps = 100 }) == formula::render_trace(handBuilt, { .maxSteps = 100 }));

    // Another evaluation gives another value and another trace.
    auto const other = formula::traced([&](auto recordingSink)
                                       { return formula::checked_evaluate<Density>(density, environmentOf(9, 3), recordingSink); });
    CHECK(other.outcome != recordedRun.outcome);
    CHECK(formula::render_trace(other.trace, { .maxSteps = 100 }) != formula::render_trace(recordedRun.trace, { .maxSteps = 100 }));

    // Every symbol is written as the vocabulary given says, the default one otherwise.
    auto const renamed = formula::traced(
        [&](auto recordingSink) { return formula::checked_evaluate<Density>(density, environmentGiven, recordingSink); },
        formula::vocabulary(formula::renames<Mass>("M")));
    CHECK(renamed.trace.steps[0].symbol == "M");
    CHECK(recordedRun.trace.steps[0].symbol == "m");
}

TEST_CASE("traced keeps a failure in the outcome and the steps up to it in the trace", "[trace]")
{
    constexpr auto bad = var<Mass> / formula::number(formula::Rational { 0 });
    auto const environmentGiven = environmentOf(6, 3);

    auto const failed = formula::traced([&](auto recordingSink)
                                        { return formula::checked_evaluate<Mass>(bad, environmentGiven, recordingSink); });
    REQUIRE(!failed.outcome.has_value());
    CHECK(failed.outcome.error() == formula::ArithmeticError::DivisionByZero);
    REQUIRE(!failed.trace.empty());
    CHECK(failed.trace.steps[failed.trace.root()].kind == formula::StepKind::Divide);
    CHECK(failed.trace.steps[failed.trace.root()].error == formula::ArithmeticError::DivisionByZero);
}

namespace
{
/// Doubled strength, a formula whose result is also a quantity it reads.
constexpr auto doubledStrength = var<Strength> * formula::Rational { 2 };
constexpr auto boundDoubled = formula::yields<Strength>(doubledStrength);

[[nodiscard]] std::string shown(formula::Trace<> const& recorded)
{
    return formula::render_trace(recorded, { .maxSteps = 100 });
}
} // namespace

TEST_CASE("trace_of is the trace traced records, for a success and for a failure", "[trace]")
{
    auto const environmentGiven = strengthOf(formula::Rational { 30 });
    auto const viaTraced = formula::traced([&](auto recordingSink)
                                           { return formula::checked_evaluate<Strength>(doubledStrength, environmentGiven, recordingSink); })
                               .trace;
    auto const direct = formula::trace_of<Strength>(doubledStrength, environmentGiven);
    CHECK(!direct.empty());
    CHECK(shown(direct) == shown(viaTraced));
    // Another environment, another trace: a stub returning a fixed trace fails here.
    CHECK(shown(formula::trace_of<Strength>(doubledStrength, strengthOf(formula::Rational { 31 }))) != shown(direct));

    // A failure still gives the trace, and the failing step is its last.
    constexpr auto bad = var<Mass> / formula::number(formula::Rational { 0 });
    auto const massGiven = environmentOf(6, 3);
    auto const failedVia = formula::traced([&](auto recordingSink)
                                           { return formula::checked_evaluate<Mass>(bad, massGiven, recordingSink); })
                               .trace;
    auto const failed = formula::trace_of<Mass>(bad, massGiven);
    REQUIRE(!failed.empty());
    CHECK(shown(failed) == shown(failedVia));
    CHECK(failed.steps.back().kind == formula::StepKind::Divide);
    CHECK(failed.steps.back().error == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("trace_of a bound formula is trace_of for the quantity it names, named or not", "[trace][yields]")
{
    auto const environmentGiven = strengthOf(formula::Rational { 30 });
    auto const expected = shown(formula::trace_of<Strength>(doubledStrength, environmentGiven));
    CHECK(!expected.empty());
    CHECK(shown(formula::trace_of(boundDoubled, environmentGiven)) == expected);
    CHECK(shown(formula::trace_of<Strength>(boundDoubled, environmentGiven)) == expected);
}

TEST_CASE("trace_of_si is the trace of the evaluation in SI units, whatever was entered for a result", "[trace]")
{
    constexpr auto density = var<Mass> / var<Volume>;
    auto const derived = environmentOf(6, 3);
    auto const viaTraced = formula::traced([&](auto recordingSink)
                                           { return formula::checked_evaluate_si<formula::Rational>(density, derived, recordingSink); })
                               .trace;
    auto const inSi = formula::trace_of_si(density, derived);
    REQUIRE(!inSi.empty());
    CHECK(shown(inSi) == shown(viaTraced));
    CHECK(shown(inSi) == shown(formula::trace_of<Density>(density, derived)));

    // A density typed in for the result is not derived: the evaluation for
    // `Density` returns it and records nothing, where the SI evaluation has no
    // result to read it for and still derives the quotient. That tells the two
    // verbs apart, which a trace_of_si that named a result would not.
    auto const overridden = formula::environment(formula::Measured<Mass> { formula::Rational { 6 } },
                                                 formula::Measured<Volume> { formula::Rational { 3 } },
                                                 formula::entered(formula::Measured<Density> { formula::Rational { 999 } }));
    CHECK(formula::trace_of<Density>(density, overridden).empty());
    auto const stillDerived = formula::trace_of_si(density, overridden);
    CHECK(shown(stillDerived) == shown(inSi));
}

TEST_CASE("trace_of writes every symbol as the vocabulary it is given says", "[trace][vocabulary]")
{
    constexpr auto south = formula::vocabulary(formula::renames<Strength>("f_s"));
    auto const environmentGiven = strengthOf(formula::Rational { 30 });
    auto const plain = shown(formula::trace_of<Strength>(doubledStrength, environmentGiven));
    CHECK(plain.find("f_s") == std::string::npos);

    auto const named = shown(formula::trace_of<Strength>(doubledStrength, environmentGiven, south));
    CHECK(named.starts_with("1. f_s = "));
    CHECK(shown(formula::trace_of(boundDoubled, environmentGiven, south)) == named);
    CHECK(shown(formula::trace_of_si(doubledStrength, environmentGiven, south)).starts_with("1. f_s = "));
    CHECK(shown(formula::trace_of_si(doubledStrength, environmentGiven)).find("f_s") == std::string::npos);
}
