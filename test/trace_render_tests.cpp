// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>

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

TEST_CASE("a derivation renders a Round step as round[to N dp of unit](...)", "[trace-render]")
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
             "2. round[to 1 dp of mm](#1) = 123/10 mm\n");
}

TEST_CASE("a derivation renders a RoundSignificant step as round[to N sf of unit](...)", "[trace-render]")
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
             "2. round[to 2 sf of mm](#1) = 12 mm\n");
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
    // rendered, not merely carried, in the numeric[in ...] prefix.
    CHECK(text
          == "1. f = 70 MPa\n"
             "2. numeric[in MPa](#1) = 70 (empirical fit only valid in MPa)\n");
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

    // Every recorded operand is named -- the predicate's two sides, #1 and
    // #2, and the branch that ran, #3 -- and which branch is a trailing
    // clause, the same way a citation trails a Documented step.
    CHECK(text
          == "1. f = 60 MPa\n"
             "2. 50 MPa\n"
             "3. f = 60 MPa\n"
             "4. when(#1, #2, #3) = 60000000 [then]\n");
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
             "6. when(#1, #2, #5) = 80000000 [else]\n");
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

    // Only the predicate's own two operands were ever recorded, and the
    // suffix says plainly that neither branch ran -- not which one, and not
    // "false", which would misreport a predicate that never resolved at all.
    CHECK(text
          == "1. f = (not measured)\n"
             "2. 50 MPa\n"
             "3. when(#1, #2) = (not measured) [no branch]\n");
}
