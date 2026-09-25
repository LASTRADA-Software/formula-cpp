// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
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
struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "water volume", unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement volume", unit::Litre>
{
};
struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", unit::Millimetre>
{
};
struct Strength: formula::Quantity<Strength, "f", "measured strength", unit::Megapascal>
{
};
} // namespace

TEST_CASE("a derivation renders one line per step, in order", "[trace-render]")
{
    constexpr auto density = formula::pow<2>(var<Mass>) / var<Volume>;
    auto const environment = formula::environment(formula::Measured<Mass> { formula::Rational { 6 } },
                                                  formula::Measured<Volume> { formula::Rational { 3 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(density, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    // Steps are numbered from one and name their operands by number. The two
    // leaves carry the symbol of the unit they were declared in; the squared
    // mass and the quotient carry none, because a computed value has no
    // declared unit and `kg2` and `kg2/m3` are not units this library writes.
    CHECK(text
          == "1. m = 6 kg\n"
             "2. #1^2 = 36\n"
             "3. V = 3 m3\n"
             "4. #2 / #3 = 12\n");
}

TEST_CASE("a value renders in the unit it was entered in, not in coherent SI", "[trace-render]")
{
    constexpr auto ratio =
        formula::documented(var<WaterVolume> / var<CementVolume>,
                            { .title = "Water/cement ratio", .reference = "Example Standard 1:2020", .section = "5.2" });
    auto const environment = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                                  formula::Measured<CementVolume> { formula::Rational { 300 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(ratio, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    // The whole point of `Step::unit`. Both volumes are stored as 9/50 and
    // 3/10 cubic metres, which is what the arithmetic needs and is not what
    // anybody entered. A person auditing this report typed 180 litres.
    CHECK(text
          == "1. V_w = 180 l\n"
             "2. V_c = 300 l\n"
             "3. #1 / #2 = 3/5\n"
             "4. #3 = 3/5 [Water/cement ratio, Example Standard 1:2020, 5.2]\n");
}

TEST_CASE("a derivation longer than the limit is cut, and says so", "[trace-render]")
{
    formula::Trace<> trace {};
    for (std::size_t i = 0; i < 100; ++i)
    {
        formula::Step<> step {};
        step.kind = formula::StepKind::Constant;
        step.value = formula::Rational { static_cast<long long>(i) };
        trace.steps.push_back(std::move(step));
    }

    std::string const text = formula::render_trace(trace, { .maxSteps = 3 });

    // Exactly three steps, then an honest count of what was left out -- not a
    // silent truncation and not a hundred lines.
    CHECK(text
          == "1. 0\n"
             "2. 1\n"
             "3. 2\n"
             "... 97 further steps not shown\n");
}

TEST_CASE("a failing step renders its error, and an absent one renders absence", "[trace-render]")
{
    formula::Trace<> trace {};

    formula::Step<> failed {};
    failed.kind = formula::StepKind::Divide;
    failed.error = formula::ArithmeticError::DivisionByZero;
    trace.steps.push_back(std::move(failed));

    formula::Step<> absent {};
    absent.kind = formula::StepKind::Variable;
    absent.symbol = "m";
    trace.steps.push_back(std::move(absent));

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    // The division recorded no operands at all, so there is no step number to
    // name and none is invented: the operator stands alone.
    CHECK(text
          == "1. / = division by zero\n"
             "2. m = (not measured)\n");
}

TEST_CASE("an empty trace renders nothing rather than a stray header", "[trace-render]")
{
    formula::Trace<> const trace {};
    CHECK(formula::render_trace(trace, { .maxSteps = 10 }).empty());
}

TEST_CASE("a value that cannot be shown in its recorded unit is refused, not restated", "[trace-render]")
{
    // Only a hand-built `Step` can reach this: the recorder always records a
    // unit of the step's own dimension. A length recorded as if it were a
    // mass has no conversion, and printing the raw coherent-SI number beside
    // a `kg` would be a wrong number dressed as a right one.
    formula::Trace<> trace {};

    formula::Step<> mismatched {};
    mismatched.kind = formula::StepKind::Variable;
    mismatched.symbol = "L";
    mismatched.dimension = formula::dim::Length;
    mismatched.unit = unit::Kilogram;
    mismatched.value = formula::Rational { 2 };
    trace.steps.push_back(std::move(mismatched));

    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
          == "1. L = (not shown: argument outside the domain of the operation)\n");
}

TEST_CASE("a citation carrying only an equation number still identifies itself", "[trace-render]")
{
    // Every identifying field is optional. A step cited with nothing but an
    // equation number used to render with no citation clause at all, which is
    // indistinguishable from an uncited step -- the one thing a provenance
    // feature must never be.
    formula::Trace<> trace {};

    formula::Step<> equationOnly {};
    equationOnly.kind = formula::StepKind::Documented;
    equationOnly.value = formula::Rational { 3 };
    equationOnly.unit = formula::unit::One;
    equationOnly.dimension = formula::dim::Scalar;
    equationOnly.citation = formula::Citation { .equation = "(7)" };
    trace.steps.push_back(std::move(equationOnly));

    formula::Step<> uncited {};
    uncited.kind = formula::StepKind::Documented;
    uncited.value = formula::Rational { 3 };
    uncited.unit = formula::unit::One;
    uncited.dimension = formula::dim::Scalar;
    trace.steps.push_back(std::move(uncited));

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    CHECK(text.find("[(7)]") != std::string::npos);
    // The genuinely uncited step still adds no trailing noise.
    CHECK(text.find("2.  = 3 []") == std::string::npos);
}

TEST_CASE("the bound's exact boundaries render without an elision line", "[trace-render]")
{
    // A trace of exactly `maxSteps` must render whole, with no "further steps"
    // line -- the off-by-one that would show "... 0 further steps not shown"
    // is invisible until someone hits the boundary exactly.
    formula::Trace<> trace {};
    for (std::size_t i = 0; i < 3; ++i)
    {
        formula::Step<> step {};
        step.kind = formula::StepKind::Constant;
        step.value = formula::Rational { static_cast<long long>(i) };
        step.unit = formula::unit::One;
        step.dimension = formula::dim::Scalar;
        trace.steps.push_back(std::move(step));
    }

    SECTION("exactly the limit")
    {
        std::string const text = formula::render_trace(trace, { .maxSteps = 3 });
        CHECK(text == "1. 0\n2. 1\n3. 2\n");
        CHECK(text.find("further steps") == std::string::npos);
    }

    SECTION("one under the limit")
    {
        std::string const text = formula::render_trace(trace, { .maxSteps = 4 });
        CHECK(text == "1. 0\n2. 1\n3. 2\n");
        CHECK(text.find("further steps") == std::string::npos);
    }

    SECTION("one over the limit")
    {
        std::string const text = formula::render_trace(trace, { .maxSteps = 2 });
        CHECK(text == "1. 0\n2. 1\n... 1 further step not shown\n");
    }
}

TEST_CASE("an explicit limit of zero is allowed, and says what it hid", "[trace-render]")
{
    // Only the *implicit* zero from `render_trace(trace, {})` is forbidden --
    // that one is a caller who forgot. A caller who writes 0 deliberately gets
    // what they asked for, and is still told what was left out rather than
    // silently handed an empty string.
    formula::Trace<> trace {};
    formula::Step<> step {};
    step.kind = formula::StepKind::Constant;
    step.value = formula::Rational { 7 };
    step.unit = formula::unit::One;
    step.dimension = formula::dim::Scalar;
    trace.steps.push_back(std::move(step));

    std::string const text = formula::render_trace(trace, { .maxSteps = 0 });
    CHECK(text == "... 1 further step not shown\n");
}

// --------------------------------------------------------------- phase 8

TEST_CASE("a derivation renders a Round step as round(..., to N dp of unit)", "[trace-render]")
{
    // 12.34 mm to one decimal place, half away from zero, is 12.3 mm -- the
    // same example rounding_node_tests.cpp verifies directly.
    constexpr auto node =
        formula::rounded<unit::Millimetre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Diameter>);
    auto const environment = formula::environment(formula::Measured<Diameter> { formula::Rational { 1234, 100 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    // The pre-rounding value (12.34 mm) is right there on #1 -- adjacency is
    // this library's answer to "how does a Round step make the change of
    // value visible", not a duplicated field on the Round step itself.
    CHECK(text
          == "1. d = 617/50 mm\n"
             "2. round(#1, to 1 dp of mm) = 123/10 mm [nearest, ties away from zero]\n");
}

TEST_CASE("a derivation renders a RoundSignificant step as round(..., to N sf of unit)", "[trace-render]")
{
    // 12.34 mm to two significant digits is 12 mm.
    constexpr auto node = formula::rounded_to_digits<unit::Millimetre, formula::SignificantDigits { 2 },
                                                     formula::RoundingMode::HalfAwayFromZero>(var<Diameter>);
    auto const environment = formula::environment(formula::Measured<Diameter> { formula::Rational { 1234, 100 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    CHECK(text
          == "1. d = 617/50 mm\n"
             "2. round(#1, to 2 sf of mm) = 12 mm [nearest, ties away from zero]\n");
}

TEST_CASE("a derivation names the rounding mode, which is the whole reason two runs differ",
          "[trace-render]")
{
    // Two rounding nodes identical but for the mode, on a value that lands
    // exactly on a tie. They produce 13 mm and 12 mm. Before the step carried
    // the mode, every human-readable output this library has -- render, LaTeX,
    // document() and the trace -- was character-for-character identical for
    // both, so a reader checking a report against a method that says "round
    // half to even" had nothing to check against.
    constexpr auto awayFromZero =
        formula::rounded<unit::Millimetre, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Diameter>);
    constexpr auto toEven =
        formula::rounded<unit::Millimetre, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfEven>(
            var<Diameter>);
    auto const environment = formula::environment(formula::Measured<Diameter> { formula::Rational { 25, 2 } });

    auto const traceOf = [&environment](auto const& node) {
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        (void) formula::checked_evaluate_si<formula::Rational>(node, environment, sink);
        return formula::render_trace(trace, { .maxSteps = 10 });
    };

    CHECK(traceOf(awayFromZero)
          == "1. d = 25/2 mm\n"
             "2. round(#1, to 0 dp of mm) = 13 mm [nearest, ties away from zero]\n");
    CHECK(traceOf(toEven)
          == "1. d = 25/2 mm\n"
             "2. round(#1, to 0 dp of mm) = 12 mm [nearest, ties to even]\n");
}

TEST_CASE("a derivation renders a NumericValue step's justification, and the unit it read from",
          "[trace-render]")
{
    constexpr auto node =
        formula::numeric_value_of<unit::Megapascal, "empirical fit only valid in MPa">(var<Strength>);
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 70 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    // The bare number (70) carries no unit suffix of its own -- it is
    // dimensionless by construction -- but "in MPa" is not lost: it is
    // rendered, not merely carried, in the numeric(..., in ...) suffix.
    CHECK(text
          == "1. f = 70 MPa\n"
             "2. numeric(#1, in MPa) = 70 (empirical fit only valid in MPa)\n");
}

TEST_CASE("a derivation renders a Conditional step's then branch", "[trace-render]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto chosen = formula::when(overFifty, var<Strength>, var<Strength> * formula::Rational { 2 });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 60 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(chosen, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    // The step is spelled the way render() spells the node it came from --
    // `if <lhs> <comparison> <rhs> then <branch>` -- with the predicate's two
    // sides as #1 and #2 and the branch that ran as #3. It must NOT read
    // `when(#1, #2, #3)`: that is positionally identical to the public
    // when(predicate, then, else) and means something else, so a reader who
    // knows the API reads the wrong value out of it. And no `[then]` suffix:
    // the keyword in the body already names the branch.
    CHECK(text
          == "1. f = 60 MPa\n"
             "2. 50 MPa\n"
             "3. f = 60 MPa\n"
             "4. if #1 > #2 then #3 = 60000000\n");
    CHECK(text.find("when(") == std::string::npos);
    CHECK(text.find("[then]") == std::string::npos);
}

TEST_CASE("a derivation renders a Conditional step's else branch", "[trace-render]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto chosen = formula::when(overFifty, var<Strength>, var<Strength> * formula::Rational { 2 });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 40 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(chosen, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    CHECK(text
          == "1. f = 40 MPa\n"
             "2. 50 MPa\n"
             "3. f = 40 MPa\n"
             "4. 2\n"
             "5. #3 * #4 = 80000000\n"
             "6. if #1 > #2 else #5 = 80000000\n");
    CHECK(text.find("[else]") == std::string::npos);
}

TEST_CASE("a derivation renders a Conditional step with no branch when the predicate is absent",
          "[trace-render]")
{
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto chosen = formula::when(overFifty, var<Strength>, var<Strength> * formula::Rational { 2 });
    auto const environment = formula::environment(formula::Measured<Strength>::absent());

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(chosen, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    // Only the predicate's own two operands were ever recorded, so the step
    // states the comparison and stops -- no branch keyword at all -- and the
    // one suffix that survives says plainly that neither branch ran: not
    // which one, and not "false", which would misreport a predicate that
    // never resolved at all. This is the clause the body cannot express, and
    // it must stay distinct from the else branch's `else #5` above.
    CHECK(text
          == "1. f = (not measured)\n"
             "2. 50 MPa\n"
             "3. if #1 > #2 = (not measured) [no branch]\n");
}

TEST_CASE("a derivation renders a Conditional step whose predicate raised an arithmetic error",
          "[trace-render]")
{
    // The one arity below two that arises in practice: the predicate's left
    // side fails, so the evaluator never dispatches the right one and no step
    // is ever recorded for it. The line must still say what was being
    // compared and must not pretend the recorded operand was both sides.
    constexpr auto overZero = (var<Strength> / formula::number(formula::Rational { 0 }))
                              > formula::constant<unit::Megapascal>(formula::Rational { 0 });
    constexpr auto guarded = formula::when(overZero, var<Strength>, var<Strength> * formula::Rational { 2 });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 60 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(guarded, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    // One operand, and no comparison token: the left side failed before the
    // right was ever dispatched, so nothing was compared. Writing `#3 >`
    // beside a side that does not exist would claim a comparison that never
    // happened.
    CHECK(text
          == "1. f = 60 MPa\n"
             "2. 0\n"
             "3. #1 / #2 = division by zero\n"
             "4. if #3 = division by zero [no branch]\n");
}

TEST_CASE("a derivation renders the comparison a conditional actually made", "[trace-render]")
{
    // Two conditionals identical but for the comparison operator. Before the
    // step carried a Comparison, both rendered `when(#1, #2, #3)` -- a trace
    // that says two values were compared but never which way is not an audit
    // trail, because the trace is the artefact that survives on its own.
    constexpr auto over = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto under = var<Strength> < formula::constant<unit::Megapascal>(formula::Rational { 50 });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 60 } });

    auto const traceOf = [&environment](auto const& node) {
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        (void) formula::checked_evaluate_si<formula::Rational>(node, environment, sink);
        return formula::render_trace(trace, { .maxSteps = 10 });
    };

    std::string const greater = traceOf(formula::when(over, var<Strength>, var<Strength>));
    std::string const less = traceOf(formula::when(under, var<Strength>, var<Strength>));

    CHECK(greater
          == "1. f = 60 MPa\n"
             "2. 50 MPa\n"
             "3. f = 60 MPa\n"
             "4. if #1 > #2 then #3 = 60000000\n");
    CHECK(less
          == "1. f = 60 MPa\n"
             "2. 50 MPa\n"
             "3. f = 60 MPa\n"
             "4. if #1 < #2 else #3 = 60000000\n");
}

TEST_CASE("a derivation spells a comparison the way render() does", "[trace-render]")
{
    // trace_render.hpp keeps its own six-token table, because it has no
    // Dialect parameter and render.hpp's spelling lives inside a function
    // that does. Two tables can drift, and a reader checking a derivation
    // against the formula it derives must not meet two notations for one
    // comparison -- so the agreement is pinned here, on both surfaces at
    // once, for all six operators rather than the two the cases above reach.
    constexpr auto fifty = formula::constant<unit::Megapascal>(formula::Rational { 50 });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 60 } });

    auto const bothSurfaces = [&environment](auto const& predicate, std::string_view token) {
        auto const node = formula::when(predicate, var<Strength>, var<Strength>);

        // render(): `if f <token> 50 MPa then f else f`.
        CHECK(formula::render(node) == "if f " + std::string { token } + " 50 MPa then f else f");

        // The trace: `if #1 <token> #2 <branch> #3`.
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        (void) formula::checked_evaluate_si<formula::Rational>(node, environment, sink);
        std::string const text = formula::render_trace(trace, { .maxSteps = 10 });
        CHECK(text.find("if #1 " + std::string { token } + " #2 ") != std::string::npos);
    };

    bothSurfaces(var<Strength> < fifty, "<");
    bothSurfaces(var<Strength> <= fifty, "<=");
    bothSurfaces(var<Strength> > fifty, ">");
    bothSurfaces(var<Strength> >= fifty, ">=");
    bothSurfaces(var<Strength> == fifty, "==");
    bothSurfaces(var<Strength> != fifty, "!=");

    // A Constraint's two surfaces must agree the same way a WhenNode's do
    // above, and for the same reason nothing here has checked it yet:
    // constraint_expression (trace_render.hpp) and render_node(Constraint
    // ...) (render.hpp) are two independent functions that each spell
    // "require <lhs> <comparison> <rhs>" from scratch, and nothing but this
    // assertion ties them together. Phase 8 shipped exactly this shape of
    // defect -- render() and the trace renderer disagreeing about a
    // rounding spelling -- for several commits, each internally consistent
    // and fully tested, caught only by a whole-branch review because no
    // test compared the two surfaces to each other.
    //
    // Extracts just the keyword and the comparison token from each surface
    // -- both "require f >= 30 MPa" (render) and "require #1 >= #2 [...]"
    // (trace) are shaped "<keyword> <operand> <comparison> <operand> ...",
    // differing only in how the operand is spelled (a variable's symbol vs.
    // a step reference), which is expected and not what this checks -- and
    // compares the two surfaces directly to each other rather than each to
    // its own hardcoded literal, so a mismatch's failure message shows both
    // actual surfaces side by side instead of only naming which literal
    // stopped matching.
    auto const keywordAndComparison = [](std::string_view text) {
        std::size_t const firstSpace = text.find(' ');
        std::size_t const secondSpace = text.find(' ', firstSpace + 1);
        std::size_t const thirdSpace = text.find(' ', secondSpace + 1);
        return std::string { text.substr(0, firstSpace) } + " "
               + std::string { text.substr(secondSpace + 1, thirdSpace - secondSpace - 1) };
    };

    constexpr auto atLeastFifty =
        formula::constraint(var<Strength> >= fifty, formula::Verdict { "reject the specimen" });
    std::string const renderedConstraint = formula::render(atLeastFifty);

    formula::Trace<> constraintTrace {};
    formula::RecordingSink<> constraintSink { constraintTrace };
    (void) formula::check(atLeastFifty, environment, constraintSink);
    std::string const fullTrace = formula::render_trace(constraintTrace, { .maxSteps = 10 });

    // render_trace() returns every step, numbered ("1. f = 60 MPa\n2. 50
    // MPa\n3. require #1 >= #2 [...]\n"), not the constraint line alone --
    // its line is the last one here, because the constraint step is the
    // outermost and so the last claimed. Found by position, not by
    // searching for either surface's own keyword: searching for "require "
    // would itself assume the very keyword this test exists to check, and
    // would find nothing -- not a mismatch, a `std::string::npos` -- the
    // moment a mutation changed it, hiding the two-surfaces-disagree
    // failure this test exists to show behind an unrelated one.
    std::string_view remaining { fullTrace };
    if (!remaining.empty() && remaining.back() == '\n')
        remaining.remove_suffix(1);
    std::size_t const lastNewline = remaining.find_last_of('\n');
    std::string_view lastLine = lastNewline == std::string_view::npos ? remaining : remaining.substr(lastNewline + 1);

    // Every line also opens with its own step number ("3. "), which
    // keywordAndComparison must not mistake for the keyword -- strip it the
    // same way, by position, before extracting.
    std::size_t const afterStepNumber = lastLine.find(". ");
    REQUIRE(afterStepNumber != std::string_view::npos);
    std::string const tracedConstraint { lastLine.substr(afterStepNumber + 2) };

    CHECK(keywordAndComparison(renderedConstraint) == keywordAndComparison(tracedConstraint));
}

// ------------------------------------------------------- Constraint steps

TEST_CASE("a derivation renders a satisfied Constraint step", "[trace-render]")
{
    constexpr auto atLeastThirty =
        formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 30 }),
                            formula::Verdict { "reject the specimen" });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 45 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::check(atLeastThirty, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    // `require #1 >= #2`, not `constraint(#1, #2)`: the public factory is
    // `constraint(predicate, verdict, citation)`, and a call-shaped spelling
    // here would let a reader map its slots onto the wrong meaning, the same
    // mistake `Conditional`'s withdrawn `when(#1, #2, #3)` made.
    CHECK(text
          == "1. f = 45 MPa\n"
             "2. 30 MPa\n"
             "3. require #1 >= #2 [satisfied]\n");
    CHECK(text.find("constraint(") == std::string::npos);
}

TEST_CASE("a derivation renders a violated Constraint step, carrying the verdict", "[trace-render]")
{
    constexpr auto atLeastThirty =
        formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 30 }),
                            formula::Verdict { "reject the specimen" });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 20 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::check(atLeastThirty, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    CHECK(text
          == "1. f = 20 MPa\n"
             "2. 30 MPa\n"
             "3. require #1 >= #2 [reject the specimen]\n");
}

TEST_CASE("a derivation renders a Constraint step as not checked when the predicate is absent -- not satisfied",
          "[trace-render]")
{
    // A satisfied and a not-checked constraint must not read the same way:
    // that would be exactly the safety property `ConstraintOutcome` exists
    // to protect, silently lost at the one surface an inspector actually
    // reads.
    constexpr auto atLeastThirty =
        formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 30 }),
                            formula::Verdict { "reject the specimen" });
    auto const environment = formula::environment(formula::Measured<Strength>::absent());

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::check(atLeastThirty, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    CHECK(text
          == "1. f = (not measured)\n"
             "2. 30 MPa\n"
             "3. require #1 >= #2 [not checked]\n");
}

TEST_CASE("a derivation renders a Constraint step with one operand when the predicate's left side errors",
          "[trace-render]")
{
    // The one arity below two that arises in practice, the same shape
    // trace_render_tests.cpp already pins for Conditional: the predicate's
    // left side fails, so the evaluator never dispatches the right one and
    // no step is ever recorded for it. The line must still say what was
    // being checked and must not pretend the recorded operand was both
    // sides.
    constexpr auto leftSideErrors =
        formula::constraint((var<Strength> / formula::number(formula::Rational { 0 }))
                                 > formula::constant<unit::Megapascal>(formula::Rational { 0 }),
                            formula::Verdict { "result is unusable" });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 60 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::check(leftSideErrors, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    // One operand, and no comparison token: the left side failed before the
    // right was ever dispatched, so nothing was compared. Writing
    // `require #3 >` beside a side that does not exist would claim a
    // comparison that never happened.
    CHECK(text
          == "1. f = 60 MPa\n"
             "2. 0\n"
             "3. #1 / #2 = division by zero\n"
             "4. require #3 [division by zero]\n");
}

TEST_CASE("a derivation renders a Constraint step with two operands when the predicate's right side errors",
          "[trace-render]")
{
    // The other arity that reaches Invalid: the left side resolves, so the
    // right side is dispatched, and it is the right side that fails. Two
    // operands, unlike the case above.
    constexpr auto rightSideErrors =
        formula::constraint(var<Strength> > (var<Strength> / formula::number(formula::Rational { 0 })),
                            formula::Verdict { "result is unusable" });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 60 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::check(rightSideErrors, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    // Two operands, and the comparison token is shown even though it was
    // never actually evaluated -- `comparison` is a compile-time property of
    // the predicate's type, recorded regardless of whether the runtime
    // comparison ever ran, exactly as `conditional_expression`'s own
    // documented exception is the *one*-operand case only, not this one.
    CHECK(text
          == "1. f = 60 MPa\n"
             "2. f = 60 MPa\n"
             "3. 0\n"
             "4. #2 / #3 = division by zero\n"
             "5. require #1 > #4 [division by zero]\n");
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

/// The same table `trace_tests.cpp` records against, and non-degenerate for
/// the same reasons: bands of unequal width, stated in a key unit that is
/// neither the operand's declared unit nor the coherent SI one, no bound equal
/// to its own index, three distinct corrections, and three bounds declared
/// unreduced so that reducing them is visibly a decision -- including the
/// table's **outer** two, which are the only bounds a covered-range rendering
/// ever reads.
inline constexpr BandTable<3> SizeBands {
    band(2, 2, 5, 2),  // 1 to under 5/2 cm -- 2/2 declared, and the table's low end
    band(5, 2, 10, 2), // 5/2 to under 5 cm -- 10/2 declared, so reduction shows
    band(5, 1, 18, 2), // 5 to under 9 cm -- 18/2 declared, and the table's high end
};

[[nodiscard]] constexpr auto sizeLookup()
{
    return banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(var<Diameter>, { rat(95), rat(112), rat(105) });
}

/// A signed underlying type with a negative enumerator, so that a renderer
/// reading a recorded key as unsigned writes 65533 where `render()` writes
/// -3. Declared out of numeric order for `render_tests.cpp`'s reason.
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

/// Three breakpoints in centimetres, unequally spaced, none of them reduced --
/// the outer two because they are the only rows a covered-range rendering
/// reads, the middle one because it is the row a segment rendering reads.
inline constexpr BreakpointTable<3> CurvePoints {
    breakpoint(4, 4),  // 1 cm -- the curve's low end, declared unreduced
    breakpoint(14, 4), // 7/2 cm -- declared unreduced, and in the middle
    breakpoint(24, 3), // 8 cm -- the curve's high end, declared unreduced
};

[[nodiscard]] constexpr auto curveLookup()
{
    return interpolating_lookup<unit::Centimetre, CurvePoints, unit::Percent>(var<Diameter>,
                                                                              { rat(90), rat(-115), rat(120) });
}

/// The two domains ending on the same number, so that a renderer spelling
/// them the same way fails here. `lookup.hpp` pins the two *behaviours*
/// against each other at 30 mm and `render_tests.cpp` pins the two spellings
/// inside a formula; a derivation is the third surface, and it is pinned on
/// the same number for the same reason.
/// Both top ends are declared unreduced (`60/2`), so that a rendering which
/// stopped reducing a covered range or a row would print `60/2 mm` here
/// instead of `30 mm` rather than passing unchanged.
inline constexpr BandTable<2> TopBands { band(9, 1, 20, 1), band(20, 1, 60, 2) };
inline constexpr BreakpointTable<2> TopPoints { breakpoint(20), breakpoint(60, 2) };

/// The degenerate tables: one that covers nothing at all, and one whose only
/// row is simultaneously its first and its last.
inline constexpr BandTable<0> NoBands {};
inline constexpr BreakpointTable<0> NoPoints {};
inline constexpr BreakpointTable<1> OnePoint { breakpoint(30, 4) }; // 15/2 cm

constexpr std::int64_t Huge = std::int64_t { 1 } << 62;

/// Keys 0 and 4 mm against values 0 and 2^62 - 1: at 3 mm the exact answer
/// does not exist inside `Rational`, so the interpolation itself overflows.
inline constexpr BreakpointTable<2> UnrepresentableAnswer { breakpoint(0), breakpoint(4) };

/// A band that is hit, whose correction is stated in kilometres and does not
/// survive the conversion into metres -- an own failure that is not a miss.
inline constexpr BandTable<1> WideBand { band(0, 1, 100, 1) };

/// The inner table of the nested pair, whose corrections are lengths so that
/// a lookup can stand where another lookup's operand stands.
inline constexpr BandTable<2> InnerBands {
    band(2, 2, 3, 1),  // 1 to under 3 cm
    band(3, 1, 12, 2), // 3 to under 6 cm
};

/// An exact table whose corrections are stated in kilometres, so that a row
/// that IS found still fails converting out of the result unit.
inline constexpr KeyTable<SpecimenShape, 2> FarKeys { SpecimenShape::Cube, SpecimenShape::Cylinder };

/// Two rows in centimetres whose values are stated in kilometres: 0 cm sits
/// exactly on the first row, so the interpolation does no arithmetic and the
/// failure that follows belongs to the conversion alone.
inline constexpr BreakpointTable<2> FarValues { breakpoint(0), breakpoint(5) };

/// A consumer's own node kind, written against the two-parameter extension
/// point, so the library never hands it to a sink and it contributes no step.
struct UntracedLength: formula::NodeBase
{
    static constexpr formula::Dimension dimension = formula::dim::Length;
};

/// Every line of a rendered derivation, with the trailing newline of each
/// already removed.
[[nodiscard]] std::vector<std::string> lines(std::string const& text)
{
    std::vector<std::string> result;
    std::size_t start = 0;
    for (std::size_t at = text.find('\n'); at != std::string::npos; at = text.find('\n', start))
    {
        result.push_back(text.substr(start, at - start));
        start = at + 1;
    }
    return result;
}

/// One rendered derivation, for a node evaluated against @p environment.
template <typename Node, typename Env>
[[nodiscard]] std::string derivationOf(Node const& node, Env const& environment)
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, environment, sink);
    return formula::render_trace(trace, { .maxSteps = 10 });
}

/// The head name of a call -- everything before its first `(`. Located by
/// position and by nothing else, so that the cross-surface test below
/// compares the two surfaces to each other rather than each to a literal of
/// its own.
[[nodiscard]] std::string headName(std::string const& text)
{
    return text.substr(0, text.find('('));
}

/// The subject of a call -- its first argument, up to the first `, ` or the
/// closing `)`. Valid only where the subject itself contains neither.
[[nodiscard]] std::string callSubject(std::string const& text)
{
    std::size_t const open = text.find('(');
    REQUIRE(open != std::string::npos);
    std::size_t const end = std::min(text.find(", ", open), text.find(')', open));
    REQUIRE(end != std::string::npos);
    return text.substr(open + 1, end - open - 1);
}

/// The fields of a rendered call's argument list, split on `, ` -- the Plain
/// dialect's own separator.
[[nodiscard]] std::vector<std::string> callFields(std::string const& text)
{
    std::size_t const open = text.find('(');
    std::size_t const close = text.rfind(')');
    REQUIRE(open != std::string::npos);
    REQUIRE(close != std::string::npos);

    std::string const inside = text.substr(open + 1, close - open - 1);
    std::vector<std::string> fields;
    std::size_t start = 0;
    for (std::size_t at = inside.find(", "); at != std::string::npos; at = inside.find(", ", start))
    {
        fields.push_back(inside.substr(start, at - start));
        start = at + 2;
    }
    fields.push_back(inside.substr(start));
    return fields;
}

/// The contents of a step line's one bracketed clause, without the brackets.
[[nodiscard]] std::string bracketed(std::string const& line)
{
    std::size_t const open = line.rfind('[');
    std::size_t const close = line.rfind(']');
    REQUIRE(open != std::string::npos);
    REQUIRE(close != std::string::npos);
    REQUIRE(open < close);
    return line.substr(open + 1, close - open - 1);
}
} // namespace

namespace formula
{
template <typename Rep = Rational, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(UntracedLength const&, Env const&) noexcept
{
    return std::unexpected { ArithmeticError::DivisionByZero };
}
} // namespace formula

TEST_CASE("a derivation names the band a banded lookup's value fell in", "[trace-render][lookup]")
{
    // A step reading only `= 112 %` explains nothing. What a reader checking
    // a number needs is that 112 % came from the band containing the input --
    // and the band is stated in the unit the table declared it in, which is
    // neither the unit the operand was entered in nor the one the result is
    // shown in.
    CHECK(derivationOf(sizeLookup(), diameterOf(30))
          == "1. d = 30 mm\n"
             "2. lookup(#1) = 112 % [5/2 to under 5 cm]\n");
}

TEST_CASE("a derivation renders a banded miss as a miss, never as a value", "[trace-render][lookup]")
{
    // Phase 9's `[not checked]` against `[else]` is the precedent: a reader
    // must never confuse "no row matched" with "the matched row held zero".
    // The bands are named too, because "outside the domain" is not something
    // a reader can check without knowing what the domain was.
    CHECK(derivationOf(sizeLookup(), diameterOf(95))
          == "1. d = 95 mm\n"
             "2. lookup(#1) = argument outside the domain of the operation"
             " [in no band; the bands cover 1 to under 9 cm]\n");
}

TEST_CASE("a derivation renders a lookup's own miss differently from one it is relaying",
          "[trace-render][lookup]")
{
    // The heart of the matter. Both derivations below end in a lookup step
    // carrying the IDENTICAL error, and a renderer reading that step alone
    // would say "lookup failed: argument outside the domain of the operation"
    // for both -- true-sounding, and in the second case describing a table
    // that was never consulted.
    constexpr auto inner =
        banded_lookup<unit::Centimetre, InnerBands, unit::Millimetre>(var<Diameter>, { rat(950), rat(35) });
    constexpr auto nested =
        banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(inner, { rat(95), rat(112), rat(105) });

    // The inner table hits and answers 950 mm == 95 cm, which the outer table
    // does not reach: the outer lookup missed on its own.
    std::string const ownMiss = derivationOf(nested, diameterOf(15));
    CHECK(ownMiss
          == "1. d = 15 mm\n"
             "2. lookup(#1) = 950 mm [1 to under 3 cm]\n"
             "3. lookup(#2) = argument outside the domain of the operation"
             " [in no band; the bands cover 1 to under 9 cm]\n");

    // The inner table misses and the outer one relays its error untouched.
    // The outer line claims nothing about the outer table, and points at the
    // line that does carry the failure.
    std::string const relayed = derivationOf(nested, diameterOf(95));
    CHECK(relayed
          == "1. d = 95 mm\n"
             "2. lookup(#1) = argument outside the domain of the operation"
             " [in no band; the bands cover 1 to under 6 cm]\n"
             "3. lookup(#2) = argument outside the domain of the operation [carried up from #2]\n");

    // And the two outermost lines are compared to each other rather than only
    // to the literals above, so that a change making them agree fails here
    // even if both literals were updated to match.
    REQUIRE(lines(ownMiss).size() == 3);
    REQUIRE(lines(relayed).size() == 3);
    CHECK(lines(ownMiss)[2] != lines(relayed)[2]);
}

TEST_CASE("a derivation renders an exact lookup's key, which is its whole subject",
          "[trace-render][lookup]")
{
    // The key sits where the other two kinds' operand reference sits, because
    // it plays that part -- and it has to be on this step, since an exact
    // lookup has no operand and so no step below it that could carry the key.
    CHECK(derivationOf(shapeLookup(SpecimenShape::Undercut), formula::environment())
          == "1. lookup(key -3) = 4 MPa\n");

    // A key that is a perfectly legitimate enumerator of the author's own
    // enumeration, and simply names no row of this table.
    CHECK(derivationOf(shapeLookup(SpecimenShape::Beam), formula::environment())
          == "1. lookup(key 11) = argument outside the domain of the operation [no row has this key]\n");
}

TEST_CASE("a derivation renders an interpolation's own overflow differently from one it is relaying",
          "[trace-render][lookup]")
{
    // The second ambiguity, and not the first one twice: only this kind
    // computes, so only this kind can overflow of its own accord. A renderer
    // covering the miss and the relay but not this one would render case two
    // as a euphemism.
    constexpr auto own = interpolating_lookup<unit::Millimetre, UnrepresentableAnswer, unit::One>(
        var<Diameter>, { rat(0), rat(Huge - 1) });
    CHECK(derivationOf(own, diameterOf(3))
          == "1. d = 3 mm\n"
             "2. interpolate(#1) = overflow in exact arithmetic"
             " [the interpolation itself overflowed, not anything below it]\n");

    // The same error enumerator, produced below the lookup instead.
    constexpr auto overflowingLength = formula::constant<unit::Millimetre>(rat(Huge)) * formula::number(rat(Huge));
    constexpr auto relayed = interpolating_lookup<unit::Millimetre, UnrepresentableAnswer, unit::One>(
        overflowingLength, { rat(0), rat(Huge - 1) });

    std::vector<std::string> const relayedLines = lines(derivationOf(relayed, formula::environment()));
    REQUIRE(relayedLines.size() == 4);
    CHECK(relayedLines[2] == "3. #1 * #2 = overflow in exact arithmetic");
    CHECK(relayedLines[3] == "4. interpolate(#3) = overflow in exact arithmetic [carried up from #3]");
}

TEST_CASE("a derivation renders an interpolating miss as outside the curve, not as no band",
          "[trace-render][lookup]")
{
    // 100 mm == 10 cm, past the curve's last row at 8 cm. No extrapolation
    // and no clamp; the range is closed at both ends and says so.
    CHECK(derivationOf(curveLookup(), diameterOf(100))
          == "1. d = 100 mm\n"
             "2. interpolate(#1) = argument outside the domain of the operation"
             " [outside the curve, which runs 1 to 8 cm]\n");

    // A value inside the curve names the two rows its answer came from --
    // which is what an auditor reconciles against a published curve, and the
    // honest equivalent of "which band" for a table that selects no single
    // row. 6 cm is in the second segment, not the first, so a renderer
    // reaching for a fixed pair is visible; and the rows are reduced on the
    // way out, as every other declared bound in this library is.
    CHECK(derivationOf(curveLookup(), diameterOf(60))
          == "1. d = 60 mm\n"
             "2. interpolate(#1) = 140/9 % [between 7/2 and 8 cm]\n");

    // The other end of the same axis: 2 cm is in the FIRST segment. A suite
    // that only ever probed the second lets "report the last pair" through in
    // silence, exactly as round 1's fixtures let "report the last band"
    // through by only ever selecting the middle one.
    CHECK(derivationOf(curveLookup(), diameterOf(20))
          == "1. d = 20 mm\n"
             "2. interpolate(#1) = 8 % [between 1 and 7/2 cm]\n");

    // A value sitting exactly on a row says so instead. The two clauses mean
    // different things -- between two rows a reader has an interpolation to
    // check, on a row the table stated the number itself.
    CHECK(derivationOf(curveLookup(), diameterOf(35))
          == "1. d = 35 mm\n"
             "2. interpolate(#1) = -115 % [on the row at 7/2 cm]\n");

    // And on the curve's LAST row, where it is the only way an answer can be
    // produced at all -- and the row index every other on-a-row probe in this
    // file happens not to be.
    CHECK(derivationOf(curveLookup(), diameterOf(80))
          == "1. d = 80 mm\n"
             "2. interpolate(#1) = 120 % [on the row at 8 cm]\n");
}

TEST_CASE("a derivation spells a band's excluded top and a curve's included one differently",
          "[trace-render][lookup]")
{
    // Both tables end on 30 mm, and the difference is one word. A band's top
    // is excluded -- 30 mm falls in no band of it -- and a breakpoint is a
    // row the table states a value at, so 30 mm hits the curve exactly.
    // "Harmonising" the two spellings in either direction fails here.
    constexpr auto bands = banded_lookup<unit::Millimetre, TopBands, unit::One>(var<Diameter>, { rat(1), rat(2) });
    constexpr auto curve =
        interpolating_lookup<unit::Millimetre, TopPoints, unit::One>(var<Diameter>, { rat(1), rat(2) });

    std::vector<std::string> const banded = lines(derivationOf(bands, diameterOf(30)));
    REQUIRE(banded.size() == 2);
    CHECK(bracketed(banded[1]) == "in no band; the bands cover 9 to under 30 mm");

    // The same number, reached rather than excluded -- and the line says so:
    // 30 mm is a row of this curve, and the clause names it as one.
    CHECK(derivationOf(curve, diameterOf(30))
          == "1. d = 30 mm\n"
             "2. interpolate(#1) = 2 [on the row at 30 mm]\n");

    // And the curve's own extent, spelled without the word that makes a band
    // half-open -- on the same number the band table excluded.
    std::vector<std::string> const past = lines(derivationOf(curve, diameterOf(35)));
    REQUIRE(past.size() == 2);
    CHECK(bracketed(past[1]) == "outside the curve, which runs 20 to 30 mm");
    CHECK(bracketed(past[1]).find("under") == std::string::npos);
}

TEST_CASE("a derivation spells a lookup the way render() does", "[trace-render][lookup]")
{
    // `render.hpp` and `trace_render.hpp` compose a band, a key and a head
    // name from scratch, independently of each other, and nothing but this
    // assertion ties them together. Phase 8 shipped exactly this shape of
    // defect for several commits -- two surfaces each internally consistent
    // and fully tested, disagreeing with each other -- caught only by a
    // whole-branch review because no test compared them.
    //
    // Each surface is compared to the other and never to a literal here, so a
    // failure shows both actual spellings side by side rather than naming
    // whichever literal stopped matching.

    // The band: render() writes it as a row's selector, the trace as the
    // clause on the step that selected it. 30 mm falls in the MIDDLE band,
    // which is render()'s field 2 (field 0 is the operand).
    std::vector<std::string> const renderedBands = callFields(formula::render(sizeLookup()));
    REQUIRE(renderedBands.size() == 4);
    std::string const renderedBand = renderedBands[2].substr(0, renderedBands[2].find(" gives "));

    std::vector<std::string> const traced = lines(derivationOf(sizeLookup(), diameterOf(30)));
    REQUIRE(traced.size() == 2);
    CHECK(bracketed(traced[1]) == renderedBand);

    // The key: render() writes it as the exact lookup's subject, and so does
    // the trace. A negative key is what separates the two casts `key_text`
    // spells separately from one that reads every key as unsigned.
    std::string const renderedKey = callSubject(formula::render(shapeLookup(SpecimenShape::Undercut)));
    std::vector<std::string> const tracedKey =
        lines(derivationOf(shapeLookup(SpecimenShape::Undercut), formula::environment()));
    REQUIRE(tracedKey.size() == 1);
    CHECK(callSubject(tracedKey[0]) == renderedKey);

    // The head names, all three: the two selecting kinds share one and the
    // computing kind has its own, and a reader checking a derivation against
    // the formula it derives must meet one name per kind rather than two.
    auto const tracedHead = [](std::string const& line) {
        std::size_t const afterStepNumber = line.find(". ");
        REQUIRE(afterStepNumber != std::string::npos);
        return headName(line.substr(afterStepNumber + 2));
    };

    CHECK(tracedHead(traced[1]) == headName(formula::render(sizeLookup())));
    CHECK(tracedHead(tracedKey[0]) == headName(formula::render(shapeLookup(SpecimenShape::Undercut))));

    std::vector<std::string> const tracedCurve = lines(derivationOf(curveLookup(), diameterOf(60)));
    REQUIRE(tracedCurve.size() == 2);
    CHECK(tracedHead(tracedCurve[1]) == headName(formula::render(curveLookup())));
    // And the two head names really are different, so that the check above
    // would not pass merely because both surfaces had collapsed to one name.
    CHECK(headName(formula::render(curveLookup())) != headName(formula::render(sizeLookup())));
}

TEST_CASE("a derivation says when it cannot tell whose failure a lookup is carrying",
          "[trace-render][lookup]")
{
    // The operand is a consumer's own node evaluated through the
    // two-parameter extension point, so it contributes no step -- and with
    // nothing recorded below, the line says exactly that rather than picking
    // whichever of the two answers sounds better.
    constexpr auto node =
        banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(UntracedLength {}, { rat(95), rat(112), rat(105) });

    CHECK(derivationOf(node, formula::environment())
          == "1. lookup() = division by zero"
             " [this lookup or something below it: the operand recorded no step]\n");
}

TEST_CASE("a derivation renders a lookup's own conversion failure as neither a miss nor a relay",
          "[trace-render][lookup]")
{
    // The band IS found, and the correction it selects then does not survive
    // being converted out of the table's own result unit. Nothing missed and
    // nothing below failed.
    constexpr auto wide = banded_lookup<unit::Millimetre, WideBand, unit::Kilometre>(var<Diameter>, { rat(Huge) });
    CHECK(derivationOf(wide, diameterOf(30))
          == "1. d = 30 mm\n"
             "2. lookup(#1) = overflow in exact arithmetic"
             " [this lookup's own unit conversion failed, not anything below it]\n");

    // The same state on the exact kind, which has no operand and no
    // interpolation -- so nothing else in this file would notice the clause
    // going missing entirely.
    constexpr auto far = exact_lookup<FarKeys, unit::Kilometre>(SpecimenShape::Cylinder, { rat(1), rat(Huge) });
    CHECK(derivationOf(far, formula::environment())
          == "1. lookup(key 7) = overflow in exact arithmetic"
             " [this lookup's own unit conversion failed, not anything below it]\n");

    // And on the interpolating kind, where it is one enumerator away from
    // claiming "the interpolation itself overflowed" about an interpolation
    // that did no arithmetic at all: 0 cm sits exactly on the first row.
    constexpr auto afterCurve =
        interpolating_lookup<unit::Centimetre, FarValues, unit::Kilometre>(var<Diameter>, { rat(Huge), rat(1) });
    CHECK(derivationOf(afterCurve, diameterOf(0))
          == "1. d = 0 mm\n"
             "2. interpolate(#1) = overflow in exact arithmetic"
             " [this lookup's own unit conversion failed, not anything below it]\n");

    // And on the other side of the curve, where it is one enumerator away
    // from claiming a three-row curve declares no rows: converting 2^62 metres
    // into centimetres overflows before any row is looked at.
    constexpr auto beforeCurve = interpolating_lookup<unit::Centimetre, CurvePoints, unit::Percent>(
        formula::constant<unit::Metre>(rat(Huge)), { rat(90), rat(-115), rat(120) });
    std::vector<std::string> const keySide = lines(derivationOf(beforeCurve, formula::environment()));
    REQUIRE(keySide.size() == 2);
    CHECK(bracketed(keySide[1]) == "this lookup's own unit conversion failed, not anything below it");
}

TEST_CASE("a derivation renders a miss against a table that covers nothing at all",
          "[trace-render][lookup]")
{
    // An empty table validates and always misses -- see `band.hpp` -- so
    // there is no interval to name, and none is invented.
    constexpr auto noBands = banded_lookup<unit::Centimetre, NoBands, unit::Percent>(var<Diameter>, {});
    std::vector<std::string> const banded = lines(derivationOf(noBands, diameterOf(30)));
    REQUIRE(banded.size() == 2);
    CHECK(bracketed(banded[1]) == "the table declares no bands");

    constexpr auto noPoints = interpolating_lookup<unit::Centimetre, NoPoints, unit::Percent>(var<Diameter>, {});
    std::vector<std::string> const empty = lines(derivationOf(noPoints, diameterOf(30)));
    REQUIRE(empty.size() == 2);
    CHECK(bracketed(empty[1]) == "the curve declares no rows");

    // The other degenerate shape: a curve whose only row is its first and its
    // last at once. "Runs 15/2 to 15/2 cm" would describe it as a range it is
    // not, so it is named as the point it is.
    constexpr auto onePoint = interpolating_lookup<unit::Centimetre, OnePoint, unit::Percent>(var<Diameter>, { rat(90) });
    std::vector<std::string> const single = lines(derivationOf(onePoint, diameterOf(30)));
    REQUIRE(single.size() == 2);
    CHECK(bracketed(single[1]) == "outside the curve, whose only row is at 15/2 cm");
}
