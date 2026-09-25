// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
#include <string_view>

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
}
