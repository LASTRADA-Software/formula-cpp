// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/trace.hpp>

#include <catch2/catch_test_macros.hpp>

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

[[nodiscard]] auto environmentOf(long long mass, long long volume)
{
    return formula::environment(formula::Measured<Mass> { formula::Rational { mass } },
                                formula::Measured<Volume> { formula::Rational { volume } });
}
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

TEST_CASE("a short-circuited operand leaves the parent with one operand, not two", "[trace]")
{
    // Dividing by zero fails in the right operand of the outer division, so
    // that division's own right operand never produces a step.
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

TEST_CASE("a deep tree is destroyed without recursing", "[trace]")
{
    // Build a long left-leaning chain at runtime by recording many steps
    // directly: the concern is the arena's destructor, not the expression
    // template, which is bounded by what a compiler will instantiate.
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
    // Destruction happens at the end of this scope. A tree of owning nodes
    // would recurse 200'000 deep here and overflow the stack; a flat vector
    // of index-referencing steps cannot.
}
