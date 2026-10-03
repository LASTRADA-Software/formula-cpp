// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include "forwarding_nodes.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
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
struct Share: formula::Quantity<Share, "p", "an invented share", unit::Percent>
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
struct SampleMass: formula::Quantity<SampleMass, "m_s", "sample mass", unit::Gram>
{
};
struct StartTemperature: formula::Quantity<StartTemperature, "T_0", "start temperature", unit::Celsius>
{
};
struct EndTemperature: formula::Quantity<EndTemperature, "T_1", "end temperature", unit::Celsius>
{
};
struct Strength: formula::Quantity<Strength, "f", "measured strength", unit::Megapascal>
{
};
struct HeaterPower: formula::Quantity<HeaterPower, "P", "heater power", unit::Kilowatt>
{
};
struct RunTime: formula::Quantity<RunTime, "t", "run time", unit::Hour>
{
};

// Author text that would forge a trace line printed as written: each spells a
// provenance clause only the library may state, or opens a line of its own.
// First, the declared symbol.
struct ForgingFactor: formula::Quantity<ForgingFactor, "k] [fixed by jurisdiction overlay: Example Standard 9:2022 NA", "factor", unit::One>
{
};

// The one variant of the method the verdict test checks.
struct PlainDensity
{
};

// A unit of the author's own whose symbol closes the value's clause.
inline constexpr formula::Unit ForgingNewton { .dimension = formula::dim::Force,
                                               .symbolText = formula::symbol("N] [x"),
                                               .decimals = 1 };
struct ForgingLoad: formula::Quantity<ForgingLoad, "P", "load in the author's unit", ForgingNewton>
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
             "2. #1^2 = 36 kg^2\n"
             "3. V = 3 m3\n"
             "4. #2 / #3 = 12 kg^2/m^3\n");
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

TEST_CASE("a power times a time reads in the coherent unit, not in kilowatt-hours", "[trace-render]")
{
    // The two leaves read in the units they were entered in, kW and h. Their
    // product is 3/2 kW * 4 h = 1500 W * 14400 s = 21600000 J, a computed
    // value with no declared unit, so it reads in the unlabelled coherent unit
    // -- joules -- as every computed step does, and not as 6 kWh.
    auto const heater = formula::environment(formula::Measured<HeaterPower> { formula::Rational { 3, 2 } },
                                             formula::Measured<RunTime> { formula::Rational { 4 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(var<HeaterPower> * var<RunTime>, heater, sink);

    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
          == "1. P = 3/2 kW\n"
             "2. t = 4 h\n"
             "3. #1 * #2 = 21600000 m^2 kg/s^2\n");
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

namespace
{
/// A gram squared, for a variance of masses in grams.
inline constexpr formula::Unit GramSquared { .dimension = formula::dim::Mass * formula::dim::Mass,
                                             .magnitudeNumerator = 1,
                                             .magnitudeDenominator = 1'000'000,
                                             .symbolText = formula::symbol("g2"),
                                             .decimals = 4 };
struct MassVariance: formula::Quantity<MassVariance, "s2", "variance of the determinations", GramSquared>
{
};
} // namespace

TEST_CASE("a derivation renders a RoundedRoot step as one rounding of a root, in its unit and mode", "[trace-render]")
{
    // Fixture A's variance, 427/125 g^2. Its root, 1.84824... g, appears on no
    // line: the radicand's step is exact, and so is the rounded result. The
    // two modes give 1.85 g and 1.84 g, and the suffix is the only text on the
    // line that says why.
    auto const traceOf = [](auto const& node) {
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        (void) formula::checked_evaluate_si<formula::Rational>(
            node, formula::environment(formula::Measured<MassVariance> { formula::Rational { 427, 125 } }), sink);
        return formula::render_trace(trace, { .maxSteps = 10 });
    };

    CHECK(traceOf(formula::rounded_sqrt<unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
              var<MassVariance>))
          == "1. s2 = 427/125 g2\n"
             "2. round(sqrt(#1), to 2 dp of g) = 37/20 g [nearest, ties away from zero]\n");
    CHECK(traceOf(formula::rounded_sqrt<unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::Floor>(
              var<MassVariance>))
          == "1. s2 = 427/125 g2\n"
             "2. round(sqrt(#1), to 2 dp of g) = 46/25 g [toward negative infinity]\n");
}

TEST_CASE("a derivation writes a logarithm or an exponential as a call on its argument's step", "[trace-render]")
{
    auto const traceOf = [](auto const& node, auto const& inputs) {
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        (void) formula::checked_evaluate_si<formula::Rational>(node, inputs, sink);
        return formula::render_trace(trace, { .maxSteps = 10 });
    };
    auto const ratioAt = [](formula::Rational ratioValue) {
        return formula::environment(formula::Measured<Ratio> { ratioValue });
    };
    CHECK(traceOf(formula::log10(var<Ratio>), ratioAt(formula::Rational { 1000 })) == "1. r = 1000\n2. log10(#1) = 3\n");
    CHECK(traceOf(formula::exp(formula::ln(var<Ratio> / var<Ratio>)), ratioAt(formula::Rational { 7 }))
          == "1. r = 7\n2. r = 7\n3. #1 / #2 = 1\n4. ln(#3) = 0\n5. exp(#4) = 1\n");
    // No exact value: the step says so, and so does every step it reaches. The irrational number
    // appears nowhere.
    CHECK(traceOf(formula::exp(formula::ln(var<Ratio>)), ratioAt(formula::Rational { 2 }))
          == "1. r = 2\n2. ln(#1) = no exact rational result exists\n3. exp(#2) = no exact rational result exists\n");
    CHECK(traceOf(formula::ln(var<Ratio>), ratioAt(formula::Rational { 0 }))
          == "1. r = 0\n2. ln(#1) = argument outside the domain of the operation\n");
    // A percentage is read as the number it is: 1000 % is 10.
    CHECK(traceOf(formula::log10(var<Share>), formula::environment(formula::Measured<Share> { formula::Rational { 1000 } }))
          == "1. p = 1000 %\n2. log10(#1) = 1\n");
}

TEST_CASE("a derivation writes a rounded logarithm or exponential as one step in its places and mode", "[trace-render]")
{
    auto const traceOf = [](auto const& node, formula::Rational ratioValue, formula::NumberStyle numberStyle) {
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        (void) formula::checked_evaluate_si<formula::Rational>(
            node, formula::environment(formula::Measured<Ratio> { ratioValue }), sink);
        return formula::render_trace(trace, { .maxSteps = 10, .numbers = numberStyle });
    };
    auto const fractions = formula::NumberStyle::fraction();
    CHECK(traceOf(formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfAwayFromZero>(var<Ratio>),
                  formula::Rational { 2 },
                  fractions)
          == "1. r = 2\n2. round(ln(#1), to 4 dp) = 6931/10000 [nearest, ties away from zero]\n");
    CHECK(traceOf(formula::rounded_log10<formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfEven>(var<Ratio>),
                  formula::Rational { 2 },
                  fractions)
          == "1. r = 2\n2. round(log10(#1), to 3 dp) = 301/1000 [nearest, ties to even]\n");
    CHECK(traceOf(formula::rounded_exp<formula::DecimalPlaces { 3 }, formula::RoundingMode::Floor>(var<Ratio>),
                  formula::Rational { -1 },
                  fractions)
          == "1. r = -1\n2. round(exp(#1), to 3 dp) = 367/1000 [toward negative infinity]\n");
    // A failure reads like any step's, and still names the mode.
    CHECK(traceOf(formula::rounded_exp<formula::DecimalPlaces { 6 }, formula::RoundingMode::HalfAwayFromZero>(var<Ratio>),
                  formula::Rational { 50 },
                  fractions)
          == "1. r = 50\n2. round(exp(#1), to 6 dp) = overflow in exact arithmetic [nearest, ties away from zero]\n");
    // In exact decimals the rounded value is a decimal like any other, with no approximation mark: it is exact.
    CHECK(traceOf(formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfAwayFromZero>(var<Ratio>),
                  formula::Rational { 2 },
                  formula::NumberStyle::exact_decimal())
          == "1. r = 2\n2. round(ln(#1), to 4 dp) = 0.6931 [nearest, ties away from zero]\n");
    // The display guide quotes this line: ln 0.05 = -2.99573..., negative, so the sign is written.
    CHECK(traceOf(formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(var<Ratio>),
                  formula::Rational { 1, 20 },
                  formula::NumberStyle::exact_decimal())
          == "1. r = 0.05\n2. round(ln(#1), to 4 dp) = -2.9957 [nearest, ties to even]\n");
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
    constexpr auto overThreshold = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 473, 10 });
    constexpr auto chosen = formula::when(overThreshold, var<Strength>, var<Strength> * formula::Rational { 2 });
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
             "2. 473/10 MPa\n"
             "3. f = 60 MPa\n"
             "4. if #1 > #2 then #3 = 60000000 kg/(m s^2)\n");
    CHECK(text.find("when(") == std::string::npos);
    CHECK(text.find("[then]") == std::string::npos);
}

TEST_CASE("a derivation renders a Conditional step's else branch", "[trace-render]")
{
    constexpr auto overThreshold = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 473, 10 });
    constexpr auto chosen = formula::when(overThreshold, var<Strength>, var<Strength> * formula::Rational { 2 });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 40 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(chosen, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    CHECK(text
          == "1. f = 40 MPa\n"
             "2. 473/10 MPa\n"
             "3. f = 40 MPa\n"
             "4. 2\n"
             "5. #3 * #4 = 80000000 kg/(m s^2)\n"
             "6. if #1 > #2 else #5 = 80000000 kg/(m s^2)\n");
    CHECK(text.find("[else]") == std::string::npos);
}

TEST_CASE("a derivation renders a Conditional step with no branch when the predicate is absent",
          "[trace-render]")
{
    constexpr auto overThreshold = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 473, 10 });
    constexpr auto chosen = formula::when(overThreshold, var<Strength>, var<Strength> * formula::Rational { 2 });
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
             "2. 473/10 MPa\n"
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
    constexpr auto over = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 473, 10 });
    constexpr auto under = var<Strength> < formula::constant<unit::Megapascal>(formula::Rational { 473, 10 });
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
             "2. 473/10 MPa\n"
             "3. f = 60 MPa\n"
             "4. if #1 > #2 then #3 = 60000000 kg/(m s^2)\n");
    CHECK(less
          == "1. f = 60 MPa\n"
             "2. 473/10 MPa\n"
             "3. f = 60 MPa\n"
             "4. if #1 < #2 else #3 = 60000000 kg/(m s^2)\n");
}

TEST_CASE("a derivation spells a comparison the way render() does", "[trace-render]")
{
    // trace_render.hpp keeps its own six-token table, because it has no
    // Dialect parameter and render.hpp's spelling lives inside a function
    // that does. Two tables can drift, and a reader checking a derivation
    // against the formula it derives must not meet two notations for one
    // comparison -- so the agreement is pinned here, on both surfaces at
    // once, for all six operators rather than the two the cases above reach.
    constexpr auto threshold = formula::constant<unit::Megapascal>(formula::Rational { 473, 10 });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 60 } });

    auto const bothSurfaces = [&environment](auto const& predicate, std::string_view token) {
        auto const node = formula::when(predicate, var<Strength>, var<Strength>);

        // render(): `if f <token> 473/10 MPa then f else f`.
        CHECK(formula::render(node) == "if f " + std::string { token } + " 473/10 MPa then f else f");

        // The trace: `if #1 <token> #2 <branch> #3`.
        formula::Trace<> trace {};
        formula::RecordingSink<> sink { trace };
        (void) formula::checked_evaluate_si<formula::Rational>(node, environment, sink);
        std::string const text = formula::render_trace(trace, { .maxSteps = 10 });
        CHECK(text.find("if #1 " + std::string { token } + " #2 ") != std::string::npos);
    };

    bothSurfaces(var<Strength> < threshold, "<");
    bothSurfaces(var<Strength> <= threshold, "<=");
    bothSurfaces(var<Strength> > threshold, ">");
    bothSurfaces(var<Strength> >= threshold, ">=");
    bothSurfaces(var<Strength> == threshold, "==");
    bothSurfaces(var<Strength> != threshold, "!=");

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
    // -- both "require f >= 473/10 MPa" (render) and "require #1 >= #2 [...]"
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

    constexpr auto atLeastThreshold =
        formula::constraint(var<Strength> >= threshold, formula::Verdict { "reject the specimen" });
    std::string const renderedConstraint = formula::render(atLeastThreshold);

    formula::Trace<> constraintTrace {};
    formula::RecordingSink<> constraintSink { constraintTrace };
    (void) formula::check(atLeastThreshold, environment, constraintSink);
    std::string const fullTrace = formula::render_trace(constraintTrace, { .maxSteps = 10 });

    // render_trace() returns every step, numbered ("1. f = 60 MPa\n2. 473/10
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
    constexpr auto atLeastMinimum =
        formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 273, 10 }),
                            formula::Verdict { "reject the specimen" });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 45 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::check(atLeastMinimum, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    // `require #1 >= #2`, not `constraint(#1, #2)`: the public factory is
    // `constraint(predicate, verdict, citation)`, and a call-shaped spelling
    // here would let a reader map its slots onto the wrong meaning, the same
    // mistake `Conditional`'s withdrawn `when(#1, #2, #3)` made.
    CHECK(text
          == "1. f = 45 MPa\n"
             "2. 273/10 MPa\n"
             "3. require #1 >= #2 [satisfied]\n");
    CHECK(text.find("constraint(") == std::string::npos);
}

TEST_CASE("a derivation renders a violated Constraint step, carrying the verdict", "[trace-render]")
{
    constexpr auto atLeastMinimum =
        formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 273, 10 }),
                            formula::Verdict { "reject the specimen" });
    auto const environment = formula::environment(formula::Measured<Strength> { formula::Rational { 20 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::check(atLeastMinimum, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    CHECK(text
          == "1. f = 20 MPa\n"
             "2. 273/10 MPa\n"
             "3. require #1 >= #2 [reject the specimen]\n");
}

TEST_CASE("a derivation renders a Constraint step as not checked when the predicate is absent -- not satisfied",
          "[trace-render]")
{
    // A satisfied and a not-checked constraint must not read the same way:
    // that would be exactly the safety property `ConstraintOutcome` exists
    // to protect, silently lost at the one surface an inspector actually
    // reads.
    constexpr auto atLeastMinimum =
        formula::constraint(var<Strength> >= formula::constant<unit::Megapascal>(formula::Rational { 273, 10 }),
                            formula::Verdict { "reject the specimen" });
    auto const environment = formula::environment(formula::Measured<Strength>::absent());

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::check(atLeastMinimum, environment, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });

    CHECK(text
          == "1. f = (not measured)\n"
             "2. 273/10 MPa\n"
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
    band(254, 200, 241, 100),  // 127/100 to under 241/100 cm -- 254/200 declared, and the table's low end
    band(241, 100, 946, 200),  // 241/100 to under 473/100 cm -- 946/200 declared, so reduction shows
    band(473, 100, 1754, 200), // 473/100 to under 877/100 cm -- 1754/200 declared, and the table's high end
};

[[nodiscard]] constexpr auto sizeLookup()
{
    return banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(var<Diameter>,
                                                                     { rat(863, 10), rat(1127, 10), rat(1043, 10) });
}

/// A signed underlying type with two negative enumerators: `Undercut`, a row
/// of the table and so rendered by name, and `Overcut`, which the table leaves
/// out and so is rendered by its value -- where a renderer reading a recorded
/// key as unsigned writes 65531 and `render()` writes -5. Declared out of
/// numeric order for `render_tests.cpp`'s reason.
///
/// **Named differently from `trace_tests.cpp`'s otherwise identical enumeration
/// on purpose, and it must stay that way.** Both are internal-linkage types in
/// an anonymous namespace, so `KeyTable<..., 3>` in each file is a *different*
/// specialisation -- but clang spells an anonymous namespace `_GLOBAL__N_1`
/// with no per-translation-unit discriminator, so the template parameter object
/// for both mangles to one name and lands in a COMDAT group keyed by it. The
/// linker keeps one group, discards the other, and the local symbol left in the
/// discarded translation unit points at a section that is no longer there:
///
///     `.rodata._ZTAXtlSt5arrayIN12_GLOBAL__N_113SpecimenShapeELm3EE...'
///     referenced in section `.text' ... defined in discarded section
///
/// Measured on clang 20.1.8, and it is an outright link failure rather than
/// anything subtler: while the two enumerations shared a name and their tables
/// shared their values, `formula-cpp-tests` would not link on either posix
/// clang preset, though cl, clang-cl and g++ 14.2 all linked it without
/// complaint. Renaming this one turned both clang legs green.
///
/// **The trigger is narrower than "two files spell it the same", and knowing
/// which part of it is not a rule is the point.** `nm` over the four objects
/// that declare a key enumeration, measured while this was fixed:
/// `lookup_tests.cpp.o` carried no `_ZTA` symbol at all, so its tables could
/// collide with nothing; and the mangled name encodes the element values as
/// well as the type name and the size, so `render_tests.cpp`'s `{3, 7, 5}`
/// never met `trace_tests.cpp`'s `{4, -3, 7}`. Both of those spelled theirs
/// `SpecimenShape` too, and got away with it because of what they happened to
/// contain rather than because of anything enforced -- an author copying an
/// existing table's values into a new test would have brought the link failure
/// back, with a dangling relocation and no diagnostic naming the key type.
///
/// So they were renamed as well, to `MouldShape` and `SpecimenVariant`, and the
/// rule now holds by construction rather than by coincidence: **each
/// translation unit's key enumeration carries a name of its own.** Follow it in
/// a new test file. `lookup.hpp`'s file comment says the same thing to a
/// consumer, who has no clang leg of their own to catch them.
enum class RenderedShape : std::int16_t
{
    Overcut = -5,
    Undercut = -3,
    Cube = 4,
    Cylinder = 7,
    Beam = 11,
};

inline constexpr KeyTable<RenderedShape, 3> ShapeKeys {
    RenderedShape::Cube,     // key 4
    RenderedShape::Undercut, // key -3 -- the middle row, and negative
    RenderedShape::Cylinder, // key 7
};

[[nodiscard]] constexpr auto shapeLookup(RenderedShape shape)
{
    return exact_lookup<ShapeKeys, unit::Megapascal>(shape, { rat(2791, 1000), rat(43), rat(1373, 1000) });
}

/// Three breakpoints in centimetres, unequally spaced, none of them reduced --
/// the outer two because they are the only rows a covered-range rendering
/// reads, the middle one because it is the row a segment rendering reads.
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

/// The two domains ending on the same number, so that a renderer spelling
/// them the same way fails here. `lookup_tests.cpp` pins the two *behaviours*
/// against each other and `render_tests.cpp` the two spellings inside a
/// formula; a derivation is the third surface. Each pins them on one shared
/// number of its own, for the same reason.
/// Both top ends are declared unreduced (`586/20`), so that a rendering which
/// stopped reducing a covered range or a row would print `586/20 mm` here
/// instead of `293/10 mm` rather than passing unchanged.
inline constexpr BandTable<2> TopBands { band(973, 100, 209, 10), band(209, 10, 586, 20) };
inline constexpr BreakpointTable<2> TopPoints { breakpoint(209, 10), breakpoint(586, 20) };

/// The degenerate tables: one that covers nothing at all, and one whose only
/// row is simultaneously its first and its last.
inline constexpr BandTable<0> NoBands {};
inline constexpr BreakpointTable<0> NoPoints {};
inline constexpr BreakpointTable<1> OnePoint { breakpoint(1474, 200) }; // 737/100 cm

constexpr std::int64_t Huge = std::int64_t { 1 } << 62;

/// Keys 0 and 4 mm against values 0 and 2^62 - 1: at 3 mm the exact answer
/// does not exist inside `Rational`, so the interpolation itself overflows.
inline constexpr BreakpointTable<2> UnrepresentableAnswer { breakpoint(0), breakpoint(4) };

/// A band that is hit, whose correction is stated in kilometres and does not
/// survive the conversion into metres -- an own failure that is not a miss.
inline constexpr BandTable<1> WideBand { band(0, 1, 103, 1) };

/// The inner table of the nested pair, whose corrections are lengths so that
/// a lookup can stand where another lookup's operand stands.
inline constexpr BandTable<2> InnerBands {
    band(218, 200, 307, 100),  // 109/100 to under 307/100 cm
    band(307, 100, 1226, 200), // 307/100 to under 613/100 cm
};

/// An exact table whose corrections are stated in kilometres, so that a row
/// that IS found still fails converting out of the result unit.
inline constexpr KeyTable<RenderedShape, 2> FarKeys { RenderedShape::Cube, RenderedShape::Cylinder };

/// A key enumeration its author spells, for the cross-surface test: the
/// middle row is customized and the others are not, and the customized
/// spelling holds characters Markdown and LaTeX would read as markup, which a
/// derivation -- plain text only -- must show exactly as `render()`'s plain
/// dialect does.
enum class RenderedFinish : std::uint8_t
{
    Rough = 1,
    Polished = 2,
    Oiled = 3,
};

inline constexpr KeyTable<RenderedFinish, 3> FinishKeys {
    RenderedFinish::Rough,
    RenderedFinish::Polished,
    RenderedFinish::Oiled,
};

[[nodiscard]] constexpr auto finishLookup(RenderedFinish finish)
{
    return exact_lookup<FinishKeys, unit::One>(finish, { rat(1127, 1000), rat(863, 1000), rat(1043, 1000) });
}

/// Two rows in centimetres whose values are stated in kilometres: 0 cm sits
/// exactly on the first row, so the interpolation does no arithmetic and the
/// failure that follows belongs to the conversion alone.
inline constexpr BreakpointTable<2> FarValues { breakpoint(0), breakpoint(437, 100) };

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

template <>
struct EnumeratorName<RenderedFinish>
{
    static constexpr std::string_view of(RenderedFinish finish) noexcept
    {
        return finish == RenderedFinish::Polished ? "polished *A*" : "";
    }
};
} // namespace formula

TEST_CASE("a derivation names the band a banded lookup's value fell in", "[trace-render][lookup]")
{
    // A step reading only `= 1127/10 %` explains nothing. What a reader
    // checking a number needs is that 1127/10 % came from the band containing
    // the input --
    // and the band is stated in the unit the table declared it in, which is
    // neither the unit the operand was entered in nor the one the result is
    // shown in.
    CHECK(derivationOf(sizeLookup(), diameterOf(30))
          == "1. d = 30 mm\n"
             "2. lookup(#1) = 1127/10 % [241/100 to under 473/100 cm]\n");
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
             " [in no band; the bands cover 127/100 to under 877/100 cm]\n");
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
        banded_lookup<unit::Centimetre, InnerBands, unit::Millimetre>(var<Diameter>, { rat(947), rat(373, 10) });
    constexpr auto nested =
        banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(inner, { rat(863, 10), rat(1127, 10), rat(1043, 10) });

    // The inner table hits and answers 947 mm == 947/10 cm, which the outer table
    // does not reach: the outer lookup missed on its own.
    std::string const ownMiss = derivationOf(nested, diameterOf(15));
    CHECK(ownMiss
          == "1. d = 15 mm\n"
             "2. lookup(#1) = 947 mm [109/100 to under 307/100 cm]\n"
             "3. lookup(#2) = argument outside the domain of the operation"
             " [in no band; the bands cover 127/100 to under 877/100 cm]\n");

    // The inner table misses and the outer one relays its error untouched.
    // The outer line claims nothing about the outer table, and points at the
    // line that does carry the failure.
    std::string const relayed = derivationOf(nested, diameterOf(95));
    CHECK(relayed
          == "1. d = 95 mm\n"
             "2. lookup(#1) = argument outside the domain of the operation"
             " [in no band; the bands cover 109/100 to under 613/100 cm]\n"
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
    //
    // By name, and the name of the MIDDLE row: kills a recorder that names
    // the first or the last row whatever the key, and a renderer that ignores
    // the recorded name and prints the value.
    CHECK(derivationOf(shapeLookup(RenderedShape::Undercut), formula::environment())
          == "1. lookup(key Undercut) = 43 MPa\n");

    // A key that is a perfectly legitimate enumerator of the author's own
    // enumeration, and simply names no row of this table. It has a name in
    // the author's source, but not among this table's keys, so the step
    // carries none and the value is shown instead.
    CHECK(derivationOf(shapeLookup(RenderedShape::Beam), formula::environment())
          == "1. lookup(key 11) = argument outside the domain of the operation [no row has this key]\n");

    // The same on a negative value: the fallback reads the recorded bit
    // pattern as signed, which kills one that reads every key as unsigned
    // (65531) now that no key in the table is shown by value.
    CHECK(derivationOf(shapeLookup(RenderedShape::Overcut), formula::environment())
          == "1. lookup(key -5) = argument outside the domain of the operation [no row has this key]\n");
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
    // 100 mm == 10 cm, past the curve's last row at 793/100 cm. No extrapolation
    // and no clamp; the range is closed at both ends and says so.
    CHECK(derivationOf(curveLookup(), diameterOf(100))
          == "1. d = 100 mm\n"
             "2. interpolate(#1) = argument outside the domain of the operation"
             " [outside the curve, which runs 139/100 to 793/100 cm]\n");

    // A value inside the curve names the two rows its answer came from --
    // which is what an auditor reconciles against a published curve, and the
    // honest equivalent of "which band" for a table that selects no single
    // row. 6 cm is in the second segment, not the first, so a renderer
    // reaching for a fixed pair is visible; and the rows are reduced on the
    // way out, as every other declared bound in this library is.
    CHECK(derivationOf(curveLookup(), diameterOf(60))
          == "1. d = 60 mm\n"
             "2. interpolate(#1) = 53773/2310 % [between 331/100 and 793/100 cm]\n");

    // The other end of the same axis: 2 cm is in the FIRST segment. A suite
    // that only ever probed the second lets "report the last pair" through in
    // silence, exactly as round 1's fixtures let "report the last band"
    // through by only ever selecting the middle one.
    CHECK(derivationOf(curveLookup(), diameterOf(20))
          == "1. d = 20 mm\n"
             "2. interpolate(#1) = 11221/480 % [between 139/100 and 331/100 cm]\n");

    // A value sitting exactly on a row says so instead. The two clauses mean
    // different things -- between two rows a reader has an interpolation to
    // check, on a row the table stated the number itself.
    CHECK(derivationOf(curveLookup(), diameterOf(331, 10))
          == "1. d = 331/10 mm\n"
             "2. interpolate(#1) = -1139/10 % [on the row at 331/100 cm]\n");

    // And on the curve's LAST row, where it is the only way an answer can be
    // produced at all -- and the row index every other on-a-row probe in this
    // file happens not to be.
    CHECK(derivationOf(curveLookup(), diameterOf(793, 10))
          == "1. d = 793/10 mm\n"
             "2. interpolate(#1) = 1217/10 % [on the row at 793/100 cm]\n");
}

TEST_CASE("a derivation spells a band's excluded top and a curve's included one differently",
          "[trace-render][lookup]")
{
    // Both tables end on 293/10 mm, and the difference is one word. A band's
    // top is excluded -- 293/10 mm falls in no band of it -- and a breakpoint
    // is a row the table states a value at, so 293/10 mm hits the curve
    // exactly.
    // "Harmonising" the two spellings in either direction fails here.
    constexpr auto bands =
        banded_lookup<unit::Millimetre, TopBands, unit::One>(var<Diameter>, { rat(1127, 1000), rat(863, 1000) });
    constexpr auto curve = interpolating_lookup<unit::Millimetre, TopPoints, unit::One>(
        var<Diameter>, { rat(1127, 1000), rat(863, 1000) });

    std::vector<std::string> const banded = lines(derivationOf(bands, diameterOf(293, 10)));
    REQUIRE(banded.size() == 2);
    CHECK(bracketed(banded[1]) == "in no band; the bands cover 973/100 to under 293/10 mm");

    // The same number, reached rather than excluded -- and the line says so:
    // 293/10 mm is a row of this curve, and the clause names it as one.
    CHECK(derivationOf(curve, diameterOf(293, 10))
          == "1. d = 293/10 mm\n"
             "2. interpolate(#1) = 863/1000 [on the row at 293/10 mm]\n");

    // And the curve's own extent, spelled without the word that makes a band
    // half-open -- on the same number the band table excluded.
    std::vector<std::string> const past = lines(derivationOf(curve, diameterOf(35)));
    REQUIRE(past.size() == 2);
    CHECK(bracketed(past[1]) == "outside the curve, which runs 209/10 to 293/10 mm");
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
    // the trace -- by name when the key names a row, by the author's own
    // spelling when there is one, and by value when it names no row. Each of
    // the three is its own branch on each surface, so each is compared.
    auto const keysAgree = [](auto const& node) {
        std::string const renderedKey = callSubject(formula::render(node));
        std::vector<std::string> const tracedKey = lines(derivationOf(node, formula::environment()));
        REQUIRE(tracedKey.size() == 1);
        CHECK(callSubject(tracedKey[0]) == renderedKey);
    };
    keysAgree(shapeLookup(RenderedShape::Undercut));
    keysAgree(finishLookup(RenderedFinish::Polished));
    // A negative key naming no row is what separates the two casts
    // `key_text` spells separately from one that reads every key as
    // unsigned.
    keysAgree(shapeLookup(RenderedShape::Overcut));
    std::vector<std::string> const tracedKey =
        lines(derivationOf(shapeLookup(RenderedShape::Undercut), formula::environment()));
    REQUIRE(tracedKey.size() == 1);

    // The head names, all three: the two selecting kinds share one and the
    // computing kind has its own, and a reader checking a derivation against
    // the formula it derives must meet one name per kind rather than two.
    auto const tracedHead = [](std::string const& line) {
        std::size_t const afterStepNumber = line.find(". ");
        REQUIRE(afterStepNumber != std::string::npos);
        return headName(line.substr(afterStepNumber + 2));
    };

    CHECK(tracedHead(traced[1]) == headName(formula::render(sizeLookup())));
    CHECK(tracedHead(tracedKey[0]) == headName(formula::render(shapeLookup(RenderedShape::Undercut))));

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
        banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(UntracedLength {},
                                                                  { rat(863, 10), rat(1127, 10), rat(1043, 10) });

    CHECK(derivationOf(node, formula::environment())
          == "1. lookup() = division by zero"
             " [this lookup or something below it: the operand recorded no step]\n");
}

TEST_CASE("a documented step shows its value as the step it documents does", "[trace-render][citation]")
{
    // The citation wraps a value declared in grams, and in millimetres below,
    // neither of which is its dimension's coherent SI unit -- the case the
    // water/cement ratio above cannot catch, since a ratio's declared unit and
    // its SI unit coincide. The documented step adds a citation and nothing
    // else: its value reads exactly as the line it names, never as the same
    // number rescaled into kilograms or metres and stripped of its unit.
    constexpr formula::Citation cited { .title = "Sample mass", .reference = "Example Standard 1:2020", .section = "4.1" };

    constexpr auto documentedVariable = formula::documented(var<SampleMass>, cited);
    CHECK(derivationOf(documentedVariable, formula::environment(formula::Measured<SampleMass> { formula::Rational { 139 } }))
          == "1. m_s = 139 g\n"
             "2. #1 = 139 g [Sample mass, Example Standard 1:2020, 4.1]\n");

    // A rounding step declares the unit it rounds in; the citation over it
    // states the rounded value in that unit.
    constexpr auto documentedRounding = formula::documented(
        formula::rounded<unit::Gram, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(var<SampleMass>),
        cited);
    CHECK(derivationOf(documentedRounding,
                       formula::environment(formula::Measured<SampleMass> { formula::Rational { 163, 10 } }))
          == "1. m_s = 163/10 g\n"
             "2. round(#1, to 0 dp of g) = 16 g [nearest, ties away from zero]\n"
             "3. #2 = 16 g [Sample mass, Example Standard 1:2020, 4.1]\n");

    // A citation over a citation: each reads as the line below it, down to
    // the variable's own millimetres.
    constexpr formula::Citation measured { .title = "Specimen diameter",
                                           .reference = "Example Standard 1:2020",
                                           .section = "3.2" };
    constexpr formula::Citation adopted { .title = "Diameter as adopted",
                                          .reference = "Example Standard 2:2021",
                                          .section = "1.4" };
    constexpr auto twiceDocumented = formula::documented(formula::documented(var<Diameter>, measured), adopted);
    CHECK(derivationOf(twiceDocumented, formula::environment(formula::Measured<Diameter> { formula::Rational { 277 } }))
          == "1. d = 277 mm\n"
             "2. #1 = 277 mm [Specimen diameter, Example Standard 1:2020, 3.2]\n"
             "3. #2 = 277 mm [Diameter as adopted, Example Standard 2:2021, 1.4]\n");
}

TEST_CASE("a documented Celsius reading reads in Celsius, and a documented difference of two does not",
          "[trace-render][citation]")
{
    // Celsius is affine: a reading converts with its offset and a difference
    // of two readings must not. A citation over the reading states it as the
    // reading's line does; a citation over the library's own subtraction
    // states the difference in kelvins' coherent spelling, as the
    // subtraction's line does, never as a Celsius reading.
    constexpr formula::Citation cited { .title = "Temperature rise",
                                        .reference = "Example Standard 1:2020",
                                        .section = "6.3" };
    auto const temperatures = formula::environment(formula::Measured<StartTemperature> { formula::Rational { 163, 10 } },
                                                   formula::Measured<EndTemperature> { formula::Rational { 277, 10 } });

    std::string const degreesCelsius = "\xc2\xb0" "C";
    CHECK(derivationOf(formula::documented(var<EndTemperature>, cited), temperatures)
          == "1. T_1 = 277/10 " + degreesCelsius + "\n"
             + "2. #1 = 277/10 " + degreesCelsius + " [Temperature rise, Example Standard 1:2020, 6.3]\n");
    CHECK(derivationOf(formula::documented(var<EndTemperature> - var<StartTemperature>, cited), temperatures)
          == "1. T_1 = 277/10 " + degreesCelsius + "\n"
             + "2. T_0 = 163/10 " + degreesCelsius + "\n"
             + "3. #1 - #2 = 57/5 K\n"
               "4. #3 = 57/5 K [Temperature rise, Example Standard 1:2020, 6.3]\n");
}

TEST_CASE("a documented step over a node that recorded no step keeps the coherent SI unit", "[trace-render][citation]")
{
    // With no line below to take a unit from, the documented step says what
    // any computed step says -- its value in coherent SI -- rather than
    // guessing one.
    constexpr auto node = formula::documented(UntracedLength {}, { .title = "Untraced length" });
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(node, formula::environment(), sink);
    REQUIRE(trace.steps.size() == 1);
    CHECK(trace.steps[0].unit == formula::coherent(formula::dim::Length));
}

TEST_CASE("a documented step over a consumer node that forwards the sink keeps the coherent SI unit",
          "[trace-render][citation]")
{
    // The consumer's node records no step of its own, so the documented step
    // claims the node's operands -- the two readings -- and neither of them
    // is the value it documents. Taking a unit from one would state a 57/5 K
    // rise as a Celsius reading of it, and a density in litres, which the
    // renderer refuses. The documented step says what any computed step
    // says instead: its value in coherent SI.
    constexpr formula::Citation cited { .title = "Consumer formula",
                                        .reference = "Example Standard 1:2020",
                                        .section = "6.4" };
    auto const temperatures = formula::environment(formula::Measured<StartTemperature> { formula::Rational { 163, 10 } },
                                                   formula::Measured<EndTemperature> { formula::Rational { 277, 10 } });

    std::string const rise = derivationOf(
        formula::documented(forwarding::difference(var<EndTemperature>, var<StartTemperature>), cited), temperatures);
    CHECK(rise.find(" = 57/5 K [Consumer formula, Example Standard 1:2020, 6.4]\n") != std::string::npos);

    constexpr auto density = formula::documented(forwarding::quotient(var<SampleMass>, var<WaterVolume>), cited);
    auto const specimen = formula::environment(formula::Measured<SampleMass> { formula::Rational { 139 } },
                                               formula::Measured<WaterVolume> { formula::Rational { 277 } });
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(density, specimen, sink);
    REQUIRE(trace.steps.size() == 3);
    CHECK(trace.steps.back().unit == formula::coherent(formula::dim::Mass / formula::dim::Volume));
    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
              .find(" = 139/277 kg/m^3 [Consumer formula, Example Standard 1:2020, 6.4]\n")
          != std::string::npos);
}

TEST_CASE("a documented step over a consumer node with one operand of its dimension keeps the coherent SI unit",
          "[trace-render][citation]")
{
    // The consumer's node has a single operand, a Celsius reading, and
    // returns its rise above 16.3 degrees C: the documented step claims
    // exactly one step, of the right dimension, so nothing at run time tells
    // it from the node's own. Only the node's kind does -- a consumer's node
    // records no step -- and the rise stays in coherent SI rather than being
    // stated as a Celsius reading of it.
    constexpr formula::Citation cited { .title = "Rise above the reference",
                                        .reference = "Example Standard 1:2020",
                                        .section = "6.5" };
    constexpr auto rise =
        formula::documented(forwarding::rise_above(var<EndTemperature>, formula::Rational { 5789, 20 }), cited);
    std::string const degreesCelsius = "\xc2\xb0" "C";
    CHECK(derivationOf(rise, formula::environment(formula::Measured<EndTemperature> { formula::Rational { 277, 10 } }))
          == "1. T_1 = 277/10 " + degreesCelsius + "\n"
             + "2. #1 = 57/5 K [Rise above the reference, Example Standard 1:2020, 6.5]\n");
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
    constexpr auto far = exact_lookup<FarKeys, unit::Kilometre>(RenderedShape::Cylinder, { rat(1127, 1000), rat(Huge) });
    CHECK(derivationOf(far, formula::environment())
          == "1. lookup(key Cylinder) = overflow in exact arithmetic"
             " [this lookup's own unit conversion failed, not anything below it]\n");

    // And on the interpolating kind, where it is one enumerator away from
    // claiming "the interpolation itself overflowed" about an interpolation
    // that did no arithmetic at all: 0 cm sits exactly on the first row.
    constexpr auto afterCurve =
        interpolating_lookup<unit::Centimetre, FarValues, unit::Kilometre>(var<Diameter>, { rat(Huge), rat(1127, 1000) });
    CHECK(derivationOf(afterCurve, diameterOf(0))
          == "1. d = 0 mm\n"
             "2. interpolate(#1) = overflow in exact arithmetic"
             " [this lookup's own unit conversion failed, not anything below it]\n");

    // And on the other side of the curve, where it is one enumerator away
    // from claiming a three-row curve declares no rows: converting 2^62 metres
    // into centimetres overflows before any row is looked at.
    constexpr auto beforeCurve = interpolating_lookup<unit::Centimetre, CurvePoints, unit::Percent>(
        formula::constant<unit::Metre>(rat(Huge)), { rat(873, 10), rat(-1139, 10), rat(1217, 10) });
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
    // last at once. "Runs 737/100 to 737/100 cm" would describe it as a range it is
    // not, so it is named as the point it is.
    constexpr auto onePoint =
        interpolating_lookup<unit::Centimetre, OnePoint, unit::Percent>(var<Diameter>, { rat(873, 10) });
    std::vector<std::string> const single = lines(derivationOf(onePoint, diameterOf(30)));
    REQUIRE(single.size() == 2);
    CHECK(bracketed(single[1]) == "outside the curve, whose only row is at 737/100 cm");
}

// ---------------------------------------------------------------------------
// Which variant a method selected
//
// Spec section 9.1 makes this the phase's acceptance criterion: the trace
// records which variant fired and on what discriminator. The fixture below is
// written so that the name of a variant NOT taken cannot appear in a
// derivation by any other route -- no quantity symbol, unit, constant or
// citation in it contains "Cube", "Cylinder" or "Prism" -- so that a check for
// its absence tests the step and nothing else.
// ---------------------------------------------------------------------------

namespace
{
// Nested in a namespace of their own, inside the anonymous one, so that every
// derivation below also shows that neither qualifier reaches the trace.
namespace specimen
{
    struct Cube;
    struct Cylinder;
    struct Prism;
} // namespace specimen

struct MaximumLoad: formula::Quantity<MaximumLoad, "F", "maximum load", unit::Kilonewton>
{
};

// An invented method: a 139 mm cube, a 163 mm diameter cylinder, a 197 mm
// square prism. The areas are what those shapes have; nothing here is taken
// from any published standard.
inline constexpr auto compressiveStrength = formula::method(
    formula::variants(
        formula::variant<specimen::Cube>(var<MaximumLoad> / formula::constant<unit::SquareMillimetre>(19'321)),
        formula::variant<specimen::Cylinder>(
            var<MaximumLoad> / (formula::pi * formula::constant<unit::SquareMillimetre>(formula::Rational { 26'569, 4 }))),
        formula::variant<specimen::Prism>(var<MaximumLoad> / formula::constant<unit::SquareMillimetre>(38'809))),
    formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());

inline constexpr auto inputs = formula::environment(formula::Measured<MaximumLoad> { formula::Rational { 562 } });

using specimen::Cylinder;

/// A derivation of @p m under tag `Tag`, rendered.
template <typename Tag, typename M>
[[nodiscard]] std::string methodDerivation(M const& m)
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::evaluate_method<Tag>(m, inputs, sink);
    return formula::render_trace(trace, { .maxSteps = 20 });
}

/// The line of a hand-built `VariantSelected` step over a constant 1, the
/// line numbered 2.
[[nodiscard]] std::string variantLine(std::string_view tag, std::size_t index, std::size_t count)
{
    formula::Trace<> trace {};
    formula::Step<> operand {};
    operand.kind = formula::StepKind::Constant;
    operand.value = formula::Rational { 1 };
    trace.steps.push_back(std::move(operand));

    formula::Step<> step {};
    step.kind = formula::StepKind::VariantSelected;
    step.variantTag = tag;
    step.variantIndex = index;
    step.variantCount = count;
    step.value = formula::Rational { 1 };
    step.operands = { 0 };
    trace.steps.push_back(std::move(step));

    std::string const text = formula::render_trace(trace, { .maxSteps = 2 });
    return text.substr(text.find('\n') + 1);
}
} // namespace

TEST_CASE("the trace names which variant fired and on what discriminator", "[trace][method]")
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::evaluate_method<Cylinder>(compressiveStrength, inputs, sink);

    std::string const rendered = formula::render_trace(trace, { .maxSteps = 20 });
    // Names the variant AND the discriminator, not merely that selection happened.
    CHECK(rendered.find("variant Cylinder") != std::string::npos);
    CHECK(rendered.find("Cube") == std::string::npos); // the one NOT taken is not claimed
    CHECK(rendered.find("Prism") == std::string::npos);
}

TEST_CASE("a variant step reads as its operand, with the variant and its position in brackets",
          "[trace-render][method]")
{
    // The FIRST variant, deliberately: the test above is the one that must
    // notice a recorder naming the first variant whatever was selected, and
    // this one must not share that job, or a mutation of the tag would be
    // killed twice and prove nothing about either. What this one pins is
    // the shape of the whole line and the ordinal.
    //
    // 562 kN over 19 321 mm2 is 29.087... MPa, which rounds to 29.1.
    CHECK(methodDerivation<specimen::Cube>(compressiveStrength)
          == "1. F = 562 kN\n"
             "2. 19321 mm2\n"
             "3. #1 / #2 = 562000000000/19321 kg/(m s^2)\n"
             "4. round(#3, in MPa) = 291/10 MPa [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "5. #4 = 291/10 MPa [variant Cube (1st of 3), selected by tag]\n");
}

TEST_CASE("a variant's position is an ordinal counted from one", "[trace-render][method]")
{
    // One-based in the text, zero-based in the step, as every position this
    // library reports in a diagnostic is.
    CHECK(variantLine("Core", 0, 1) == "2. #1 = 1 [variant Core (1st of 1), selected by tag]\n");
    CHECK(variantLine("Core", 1, 4) == "2. #1 = 1 [variant Core (2nd of 4), selected by tag]\n");
    CHECK(variantLine("Core", 2, 4) == "2. #1 = 1 [variant Core (3rd of 4), selected by tag]\n");
    CHECK(variantLine("Core", 3, 4) == "2. #1 = 1 [variant Core (4th of 4), selected by tag]\n");

    // The teens take `th` whatever their last digit, in every hundred; the
    // numbers either side of them do not.
    std::string const expected[] = { "11th", "12th", "13th", "21st", "22nd", "23rd", "101st", "111th", "112th", "113th" };
    std::size_t const positions[] = { 11, 12, 13, 21, 22, 23, 101, 111, 112, 113 };
    for (std::size_t i = 0; i < std::size(positions); ++i)
    {
        std::string const line = variantLine("Core", positions[i] - 1, 200);
        CHECK(line.find("(" + expected[i] + " of 200)") != std::string::npos);
    }
}

TEST_CASE("a variant whose tag could not be named is still identified by its position", "[trace-render][method]")
{
    // An empty name is what the recorder is handed when the compiler's
    // signature was not in the shape the library reads. The position is the
    // one thing still known, and it is said; no name is invented.
    CHECK(variantLine("", 1, 3)
          == "2. #1 = 1 [the 2nd of 3 variants, selected by a tag whose name could not be read]\n");
}

TEST_CASE("a rounding step whose provenance is no known value says so rather than guess", "[trace-render][method]")
{
    // A hand-built step may hold any value of the underlying type. Naming
    // either provenance for it would claim a rule was the method's, or a
    // jurisdiction's, on no evidence.
    formula::Trace<> trace {};
    formula::Step<> operand {};
    operand.kind = formula::StepKind::Constant;
    operand.value = formula::Rational { 1 };
    trace.steps.push_back(std::move(operand));

    formula::Step<> step {};
    step.kind = formula::StepKind::RoundingRuleApplied;
    step.granularity = 1;
    step.roundingProvenance = static_cast<formula::RoundingProvenance>(7);
    step.dimension = formula::dim::Pressure;
    step.unit = unit::Megapascal;
    step.value = formula::Rational { 1'000'000 };
    step.operands = { 0 };
    trace.steps.push_back(std::move(step));

    std::string const text = formula::render_trace(trace, { .maxSteps = 2 });
    CHECK(text.substr(text.find('\n') + 1)
          == "2. round(#1, in MPa) = 1 MPa [rounded to 1 dp (unknown provenance); nearest, ties away from zero]\n");
}

TEST_CASE("a declared symbol cannot write a provenance clause into a trace line", "[trace-render][escape]")
{
    // Printed as written this line read `1. k] [fixed by jurisdiction
    // overlay: Example Standard 9:2022 NA = 1`, for a quantity no overlay fixed.
    auto const environment = formula::environment(formula::Measured<ForgingFactor> { formula::Rational { 1 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(var<ForgingFactor>, environment, sink);

    CHECK(formula::render_trace(trace, { .maxSteps = 10 }) == "1. k\\] \\[fixed by jurisdiction overlay: Example Standard 9:2022 NA = 1\n");
}

TEST_CASE("a citation cannot close its clause, open another, or start a line", "[trace-render][escape]")
{
    // A clause-closing citation, with a newline, another control character and a backslash
    // added: every one escaped, and the line stays one line.
    constexpr auto ratio =
        formula::documented(var<WaterVolume> / var<CementVolume>,
                            { .title = "Strength] [derived by jurisdiction overlay: Example Standard 9:2022 NA\n9. a\\b\x1f" });
    auto const environment = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                                  formula::Measured<CementVolume> { formula::Rational { 300 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(ratio, environment, sink);

    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
          == "1. V_w = 180 l\n"
             "2. V_c = 300 l\n"
             "3. #1 / #2 = 3/5\n"
             "4. #3 = 3/5 [Strength\\] \\[derived by jurisdiction overlay: Example Standard 9:2022 NA\\n9. a\\\\b\\x1f]\n");
}

TEST_CASE("a verdict's label cannot name a second owner for its constraint", "[trace-render][escape]")
{
    // Printed as written, the clause named the method's own
    // constraint a jurisdiction's as well.
    constexpr auto limit =
        formula::constraint(var<Mass> >= formula::constant<unit::Kilogram>(formula::Rational { 973, 100 }),
                            formula::Verdict { "reject; jurisdiction overlay: Example Standard 9:2022 NA" });
    auto const m = formula::method(formula::variants(formula::variant<PlainDensity>(var<Mass> / var<Volume>)),
                                   formula::rounding_rule<unit::KilogramPerCubicMetre,
                                                          formula::DecimalPlaces { 0 },
                                                          formula::RoundingMode::HalfAwayFromZero>(),
                                   formula::constraints(limit));
    auto const environment = formula::environment(formula::Measured<Mass> { formula::Rational { 6 } },
                                                  formula::Measured<Volume> { formula::Rational { 3 } });

    formula::Trace<> trace {};
    (void) formula::check_method(m, environment, formula::RecordingSink<> { trace });

    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
          == "1. m = 6 kg\n"
             "2. 973/100 kg\n"
             "3. require #1 >= #2 [reject\\; jurisdiction overlay: Example Standard 9:2022 NA; the method's own constraint]\n"
             "4. acceptance(#3) [the method's own constraints]\n");
}

TEST_CASE("a unit's symbol cannot close the clause it stands in", "[trace-render][escape]")
{
    auto const environment = formula::environment(formula::Measured<ForgingLoad> { formula::Rational { 4 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(var<ForgingLoad>, environment, sink);

    CHECK(formula::render_trace(trace, { .maxSteps = 10 }) == "1. P = 4 N\\] \\[x\n");
}

namespace
{
// Author text the compile-time rules let through -- a semicolon and a
// backslash are refused nowhere -- in a variant's tag and a lookup key's name.
// Final re-review of phase 11, L3: each escaped today, and no test said so.
struct EscapedTag
{
};

enum class EscapedGrade : std::uint8_t
{
    Plain,
    Forging,
};

inline constexpr formula::KeyTable<EscapedGrade, 2> EscapedGradeKeys { EscapedGrade::Plain, EscapedGrade::Forging };

inline constexpr formula::BandTable<1> ForgingLoadBands { formula::band(127, 100, 973, 100) };
} // namespace

template <>
struct formula::TagName<EscapedTag>
{
    static constexpr std::string_view of() noexcept
    {
        return "steel; y \\";
    }
};

template <>
struct formula::EnumeratorName<EscapedGrade>
{
    static constexpr std::string_view of(EscapedGrade grade) noexcept
    {
        return grade == EscapedGrade::Forging ? "steel; y \\" : "plain";
    }
};

TEST_CASE("a justification cannot close the clause it stands in", "[trace-render][escape]")
{
    constexpr auto read = formula::numeric_value_of<unit::Newton, "why; not] [x">(var<ForgingLoad>);
    auto const environment = formula::environment(formula::Measured<ForgingLoad> { formula::Rational { 4 } });

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(read, environment, sink);

    CHECK(formula::render_trace(trace, { .maxSteps = 10 }).ends_with("2. numeric(#1, in N) = 4 (why\\; not\\] \\[x)\n"));
}

TEST_CASE("a unit's symbol is escaped in a numeric value, a method's rounding and a lookup", "[trace-render][escape]")
{
    auto const environment = formula::environment(formula::Measured<ForgingLoad> { formula::Rational { 4 } });

    formula::Trace<> numericTrace {};
    (void) formula::checked_evaluate_si<formula::Rational>(
        formula::numeric_value_of<ForgingNewton, "the annex states it in N">(var<ForgingLoad>),
        environment,
        formula::RecordingSink<> { numericTrace });
    CHECK(formula::render_trace(numericTrace, { .maxSteps = 10 }).find("2. numeric(#1, in N\\] \\[x) = 4 ")
          != std::string::npos);

    auto const m = formula::method(
        formula::variants(formula::variant<PlainDensity>(var<ForgingLoad>)),
        formula::rounding_rule<ForgingNewton, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    formula::Trace<> roundingTrace {};
    (void) formula::evaluate_method<PlainDensity>(m, environment, formula::RecordingSink<> { roundingTrace });
    CHECK(formula::render_trace(roundingTrace, { .maxSteps = 10 }).find("2. round(#1, in N\\] \\[x) = 4 N\\] \\[x [rounded")
          != std::string::npos);

    formula::Trace<> lookupTrace {};
    (void) formula::checked_evaluate_si<formula::Rational>(
        formula::banded_lookup<ForgingNewton, ForgingLoadBands, unit::One>(var<ForgingLoad>,
                                                                           { formula::Rational { 1127, 1000 } }),
        environment,
        formula::RecordingSink<> { lookupTrace });
    CHECK(formula::render_trace(lookupTrace, { .maxSteps = 10 }).ends_with(" [127/100 to under 973/100 N\\] \\[x]\n"));
}

TEST_CASE("a variant's tag and a lookup key's name are escaped", "[trace-render][escape]")
{
    auto const environment = formula::environment(formula::Measured<Mass> { formula::Rational { 6 } },
                                                  formula::Measured<Volume> { formula::Rational { 3 } });
    auto const m = formula::method(formula::variants(formula::variant<EscapedTag>(var<Mass> / var<Volume>)),
                                   formula::rounding_rule<unit::KilogramPerCubicMetre,
                                                          formula::DecimalPlaces { 0 },
                                                          formula::RoundingMode::HalfAwayFromZero>(),
                                   formula::constraints());
    formula::Trace<> variantTrace {};
    (void) formula::evaluate_method<EscapedTag>(m, environment, formula::RecordingSink<> { variantTrace });
    CHECK(formula::render_trace(variantTrace, { .maxSteps = 10 })
              .find(" [variant steel\\; y \\\\ (1st of 1), selected by tag]\n")
          != std::string::npos);

    formula::Trace<> keyTrace {};
    (void) formula::checked_evaluate_si<formula::Rational>(
        formula::exact_lookup<EscapedGradeKeys, unit::One>(
            EscapedGrade::Forging, { formula::Rational { 1127, 1000 }, formula::Rational { 863, 1000 } }),
        environment,
        formula::RecordingSink<> { keyTrace });
    CHECK(formula::render_trace(keyTrace, { .maxSteps = 10 }) == "1. lookup(key steel\\; y \\\\) = 863/1000\n");
}

TEST_CASE("a rounding or a numeric value in a unit with no symbol adds no unit clause to its line", "[trace-render]")
{
    // `unit::One`'s symbol is empty, and a method's rounding step once read
    // `round(#4, in )`, a numeric value `numeric(#3, in )`. The clause is
    // dropped, as the value's own unit is after a dimensionless number; a
    // named unit keeps it.
    constexpr auto ratio = var<WaterVolume> / var<CementVolume>;
    auto const environment = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                                  formula::Measured<CementVolume> { formula::Rational { 300 } });

    auto const dimensionless = formula::method(
        formula::variants(formula::variant<PlainDensity>(ratio)),
        formula::rounding_rule<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    formula::Trace<> ruleTrace {};
    (void) formula::evaluate_method<PlainDensity>(dimensionless, environment, formula::RecordingSink<> { ruleTrace });
    CHECK(formula::render_trace(ruleTrace, { .maxSteps = 10 })
              .find("4. round(#3) = 3/5 [rounded to 2 dp (method default); nearest, ties away from zero]\n")
          != std::string::npos);

    formula::Trace<> placesTrace {};
    (void) formula::checked_evaluate_si<formula::Rational>(
        formula::rounded<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(ratio),
        environment,
        formula::RecordingSink<> { placesTrace });
    CHECK(formula::render_trace(placesTrace, { .maxSteps = 10 }).find("4. round(#3, to 2 dp) = 3/5 ") != std::string::npos);

    formula::Trace<> numericTrace {};
    (void) formula::checked_evaluate_si<formula::Rational>(
        formula::numeric_value_of<unit::One, "the fit is stated over the bare ratio">(ratio),
        environment,
        formula::RecordingSink<> { numericTrace });
    CHECK(formula::render_trace(numericTrace, { .maxSteps = 10 })
              .ends_with("4. numeric(#3) = 3/5 (the fit is stated over the bare ratio)\n"));

    // A named unit keeps its clause, on the same two lines.
    auto const inMegapascals = formula::method(
        formula::variants(formula::variant<PlainDensity>(var<Strength>)),
        formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    auto const strength = formula::environment(formula::Measured<Strength> { formula::Rational { 30 } });
    formula::Trace<> namedRuleTrace {};
    (void) formula::evaluate_method<PlainDensity>(inMegapascals, strength, formula::RecordingSink<> { namedRuleTrace });
    CHECK(formula::render_trace(namedRuleTrace, { .maxSteps = 10 }).find("2. round(#1, in MPa) = 30 MPa [")
          != std::string::npos);

    formula::Trace<> namedNumericTrace {};
    (void) formula::checked_evaluate_si<formula::Rational>(
        formula::numeric_value_of<unit::Megapascal, "the fit is stated in MPa">(var<Strength>),
        strength,
        formula::RecordingSink<> { namedNumericTrace });
    CHECK(formula::render_trace(namedNumericTrace, { .maxSteps = 10 })
              .ends_with("2. numeric(#1, in MPa) = 30 (the fit is stated in MPa)\n"));
}

// ---- A series on the trace (phase 12) ----

namespace
{
namespace series_trace
{
    // The shared fixture of the phase 12 plan: an invented screen analysis.
    // Every element differs, and the middle one was not measured.
    struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", unit::Gram>
    {
    };
    struct Sieved: formula::Quantity<Sieved, "m_s", "mass passing a screen", unit::Gram>
    {
    };
    struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", unit::Gram>
    {
    };
    struct Stockpile: formula::Quantity<Stockpile, "m_p", "stockpile mass", unit::Tonne>
    {
    };
    /// A reading on an offset scale: each is a point on the Celsius scale,
    /// while a sum or a difference of them is none.
    struct Reading: formula::Quantity<Reading, "T_r", "a temperature reading", unit::Celsius>
    {
    };
    /// The same kind of reading on the Fahrenheit scale, the other offset unit.
    struct FahrenheitReading: formula::Quantity<FahrenheitReading, "T_f", "a temperature reading", unit::Fahrenheit>
    {
    };
    /// A thousandth of a metre the author gave no symbol: a value shown in it
    /// could not say what scale it is on.
    inline constexpr formula::Unit UnnamedMillimetre { .dimension = formula::dim::Length,
                                                       .magnitudeNumerator = 1,
                                                       .magnitudeDenominator = 1000,
                                                       .decimals = 1 };
    struct Gap: formula::Quantity<Gap, "w", "a gap in an unnamed unit", UnnamedMillimetre>
    {
    };
    /// Money: a named base dimension of its own, never a bare ratio.
    inline constexpr formula::Unit Euro { .dimension = formula::base_dimension("EUR"),
                                          .symbolText = formula::symbol("EUR"),
                                          .decimals = 2 };
    struct Price: formula::Quantity<Price, "p", "a unit price", Euro>
    {
    };

    [[nodiscard]] constexpr formula::Measured<Retained> retained(std::int64_t grams)
    {
        return formula::Measured<Retained> { formula::Rational { grams } };
    }

    /// 23.7, 41.3 and 37.9 degrees Celsius.
    inline constexpr auto readings = formula::environment(
        formula::measured_series<Reading>(formula::Measured<Reading> { formula::Rational { 237, 10 } },
                                          formula::Measured<Reading> { formula::Rational { 413, 10 } },
                                          formula::Measured<Reading> { formula::Rational { 379, 10 } }));

    /// 50 and 51 degrees Fahrenheit.
    inline constexpr auto fahrenheitReadings = formula::environment(formula::measured_series<FahrenheitReading>(
        formula::Measured<FahrenheitReading> { formula::Rational { 50 } },
        formula::Measured<FahrenheitReading> { formula::Rational { 51 } }));

    inline constexpr auto inputs = formula::environment(
        formula::measured_series<Retained>(
            retained(130), retained(210), formula::Measured<Retained>::absent(), retained(340), retained(28)),
        formula::Measured<TotalMass> { formula::Rational { 1250 } });

    inline constexpr auto allPresent = formula::environment(
        formula::measured_series<Retained>(retained(130), retained(210), retained(95), retained(340), retained(28)));

    /// The series step, then a second step of its own walk: `m_t`, read as a
    /// single value into the same trace.
    [[nodiscard]] inline formula::Trace<> series_then_total()
    {
        formula::Trace<> trace {};
        (void) formula::detail::dispatch_series<formula::Rational>(
            formula::series<Retained, 5>, inputs, formula::RecordingSink<> { trace });
        (void) formula::checked_evaluate_si<formula::Rational>(
            formula::var<TotalMass>, inputs, formula::RecordingSink<> { trace });
        return trace;
    }
} // namespace series_trace
} // namespace

TEST_CASE("a series step names its quantity in the sink's vocabulary and lists every element", "[series][trace]")
{
    using series_trace::Retained;
    using series_trace::Sieved;
    formula::Trace<> trace {};
    formula::RecordingSink sink { trace, formula::vocabulary(formula::renames<Retained>("R")) };
    (void) formula::detail::dispatch_series<formula::Rational>(formula::series<Retained, 5>, series_trace::inputs, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 20 });
    CHECK(text.find("R = ") != std::string::npos);
    CHECK(text.find("m_r") == std::string::npos);            // the declared symbol, dropped
    CHECK(text.find("130 g") != std::string::npos);          // shown in the unit entered, not kg
    CHECK(text.find("(not measured)") != std::string::npos); // the absent element, as absence
    CHECK(text.find("28 g") != std::string::npos);           // the last element, not cut
    // In order, one line, every element in its own place.
    CHECK(text == "1. R = 130 g; 210 g; (not measured); 340 g; 28 g\n");

    // Crossed over: the other jurisdiction writes Retained as S and Sieved
    // as R, so a sink that looked the symbol up by the wrong quantity, or
    // ignored the vocabulary, writes the wrong letter.
    formula::Trace<> crossed {};
    formula::RecordingSink crossedSink {
        crossed, formula::vocabulary(formula::renames<Retained>("S"), formula::renames<Sieved>("R"))
    };
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::series<Retained, 5>, series_trace::inputs, crossedSink);
    CHECK(formula::render_trace(crossed, { .maxSteps = 20 }) == "1. S = 130 g; 210 g; (not measured); 340 g; 28 g\n");
}

TEST_CASE("a series step never renders as a single absent value", "[series][trace]")
{
    // Step::value is empty for a series step. A renderer that consulted it
    // would print "(not measured)" for a fully present series.
    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::series<series_trace::Retained, 5>, series_trace::allPresent, formula::RecordingSink<> { trace });
    REQUIRE(trace.steps.size() == 1);
    CHECK(!trace.steps[0].value.has_value());
    std::string const text = formula::render_trace(trace, { .maxSteps = 20 });
    CHECK(text.find("(not measured)") == std::string::npos);
    CHECK(text == "1. m_r = 130 g; 210 g; 95 g; 340 g; 28 g\n");
}

TEST_CASE("a long series shares the one maxSteps budget and says how much it left out", "[series][trace]")
{
    // Each line costs one unit of maxSteps and each element shown one more.
    // The count left out is exact, so it is pinned at several budgets: a
    // count hard-coded, or computed from the wrong side, fails at least one.
    //
    // render_trace is the only surface that shows element values, so this is where the count is pinned. render() and
    // document() take no environment and print a formula, never values, so
    // there is nothing for them to truncate and no test of theirs could
    // fail. The gallery (tools/gallery) copies render_trace's output
    // verbatim into a fenced block, so it carries this count by
    // construction, and its test pins the generated page.
    formula::Trace<> const trace = series_trace::series_then_total();
    REQUIRE(trace.steps.size() == 2);

    CHECK(formula::render_trace(trace, { .maxSteps = 3 })
          == "1. m_r = 130 g; 210 g; ... 3 more\n"
             "... 1 further step not shown\n");
    CHECK(formula::render_trace(trace, { .maxSteps = 5 })
          == "1. m_r = 130 g; 210 g; (not measured); 340 g; ... 1 more\n"
             "... 1 further step not shown\n");
    // Exactly enough for the series, and none left for the next line.
    CHECK(formula::render_trace(trace, { .maxSteps = 6 })
          == "1. m_r = 130 g; 210 g; (not measured); 340 g; 28 g\n"
             "... 1 further step not shown\n");
    CHECK(formula::render_trace(trace, { .maxSteps = 7 })
          == "1. m_r = 130 g; 210 g; (not measured); 340 g; 28 g\n"
             "2. m_t = 1250 g\n");
    // The line itself and nothing else: every element is left out, and said so.
    CHECK(formula::render_trace(trace, { .maxSteps = 1 })
          == "1. m_r = ... 5 more\n"
             "... 1 further step not shown\n");
}

TEST_CASE("a series that failed at an element names that element, counted from one", "[series][trace]")
{
    // Positions are zero-based in the API and one-based in every text the
    // library writes. The failure is at zero-based 2, so the line says 3.
    constexpr std::int64_t tooLarge = std::numeric_limits<std::int64_t>::max() / 100;
    constexpr auto overflowing = formula::environment(formula::measured_series<series_trace::Stockpile>(
        formula::Measured<series_trace::Stockpile> { formula::Rational { 1 } },
        formula::Measured<series_trace::Stockpile> { formula::Rational { 2 } },
        formula::Measured<series_trace::Stockpile> { formula::Rational { tooLarge } },
        formula::Measured<series_trace::Stockpile> { formula::Rational { 3 } }));

    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::series<series_trace::Stockpile, 4>, overflowing, formula::RecordingSink<> { trace });
    REQUIRE(trace.steps.size() == 1);
    CHECK(trace.steps[0].failedElement == std::optional<std::size_t> { 2 });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 }) == "1. m_p = overflow in exact arithmetic at element 3\n");
}

TEST_CASE("an elementwise step names its operands, and a broadcast scalar appears once", "[series][trace]")
{
    // The point of a broadcast: m_t is evaluated once and appears once in the
    // derivation, as an operand of the one elementwise step, however long
    // the series.
    constexpr auto screens =
        formula::environment(formula::measured_series<series_trace::Retained>(series_trace::retained(130),
                                                                              series_trace::retained(210),
                                                                              series_trace::retained(95),
                                                                              series_trace::retained(340),
                                                                              series_trace::retained(28)),
                             formula::Measured<series_trace::TotalMass> { formula::Rational { 1250 } });
    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(formula::series<series_trace::Retained, 5>
                                                                   / formula::var<series_trace::TotalMass>,
                                                               screens,
                                                               formula::RecordingSink<> { trace });
    // A computed step has no declared unit, as a scalar quotient has none, so
    // the fraction is shown in the coherent unit, exactly.
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
          == "1. m_r = 130 g; 210 g; 95 g; 340 g; 28 g\n"
             "2. m_t = 1250 g\n"
             "3. #1 / #2 = 13/125; 21/125; 19/250; 34/125; 14/625\n");
    REQUIRE(trace.steps.size() == 3);
    CHECK(trace.steps[2].kind == formula::StepKind::ElementwiseDivide);
    CHECK(trace.steps[2].operands == std::vector<std::size_t> { 0, 1 });
}

TEST_CASE("a series scaled by a pure number reads in the series' unit", "[series][trace]")
{
    // 137, 213 and 293 g, times 3/2 and divided by 3/2: each still a mass in
    // grams, and shown in grams rather than in unlabelled kilograms --
    // whichever side the number stands on.
    constexpr auto masses =
        formula::environment(formula::measured_series<series_trace::Retained>(
            series_trace::retained(137), series_trace::retained(213), series_trace::retained(293)));
    constexpr auto retained = formula::series<series_trace::Retained, 3>;
    constexpr auto factor = formula::number(formula::Rational { 3, 2 });
    auto const derivation = [&](auto const& seriesNode) {
        formula::Trace<> trace {};
        (void) formula::detail::dispatch_series<formula::Rational>(seriesNode, masses, formula::RecordingSink<> { trace });
        return formula::render_trace(trace, { .maxSteps = 30 });
    };
    CHECK(derivation(retained * factor)
          == "1. m_r = 137 g; 213 g; 293 g\n"
             "2. 3/2\n"
             "3. #1 * #2 = 411/2 g; 639/2 g; 879/2 g\n");
    CHECK(derivation(factor * retained)
          == "1. 3/2\n"
             "2. m_r = 137 g; 213 g; 293 g\n"
             "3. #1 * #2 = 411/2 g; 639/2 g; 879/2 g\n");
    CHECK(derivation(retained / factor)
          == "1. m_r = 137 g; 213 g; 293 g\n"
             "2. 3/2\n"
             "3. #1 / #2 = 274/3 g; 142 g; 586/3 g\n");
    // A number divided by a series is no mass; it keeps the coherent unit.
    CHECK(derivation(factor / retained)
          == "1. 3/2\n"
             "2. m_r = 137 g; 213 g; 293 g\n"
             "3. #1 / #2 = 1500/137 1/kg; 500/71 1/kg; 1500/293 1/kg\n");

    // Celsius readings doubled are no readings: 593.7 K is not 2 x 23.7 degC.
    formula::Trace<> doubled {};
    (void) formula::detail::dispatch_series<formula::Rational>(formula::series<series_trace::Reading, 3>
                                                                   * formula::number(formula::Rational { 2 }),
                                                               series_trace::readings,
                                                               formula::RecordingSink<> { doubled });
    REQUIRE(doubled.steps.size() == 3);
    CHECK(formula::render_trace(doubled, { .maxSteps = 30 }).ends_with("3. #1 * #2 = 5937/10 K; 6289/10 K; 6221/10 K\n"));
}

TEST_CASE("a series of prices scaled by a pure number reads in euros", "[series][trace]")
{
    // Euros are a named base dimension, so of a price and a ratio only the
    // ratio is dimensionless, and the product is a price in euros. Declared
    // as a bare ratio, as money had to be, the euros would be a pure number
    // too, and the product of two pure numbers keeps no unit.
    constexpr auto prices = formula::environment(
        formula::measured_series<series_trace::Price>(formula::Measured<series_trace::Price> { formula::Rational { 137 } },
                                                      formula::Measured<series_trace::Price> { formula::Rational { 213 } },
                                                      formula::Measured<series_trace::Price> { formula::Rational { 293 } }));
    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(formula::series<series_trace::Price, 3>
                                                                   * formula::number(formula::Rational { 3, 2 }),
                                                               prices,
                                                               formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
          == "1. p = 137 EUR; 213 EUR; 293 EUR\n"
             "2. 3/2\n"
             "3. #1 * #2 = 411/2 EUR; 639/2 EUR; 879/2 EUR\n");
}

TEST_CASE("a per-element constant and a negation each record one step with every element", "[series][trace]")
{
    // Grams, not the coherent kilogram: a line that printed the stored SI
    // values beside the constant's symbol would read 1/1000 g.
    constexpr auto factors =
        formula::series_constant<unit::Gram>(formula::Rational { 1 }, formula::Rational { 2 }, formula::Rational { 3 });
    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        -factors, formula::environment(), formula::RecordingSink<> { trace });
    // The constant's line is its values alone, as a scalar constant's is, in
    // the unit it was written in; the negation, computed, names its operand
    // and reads in the coherent unit.
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
          == "1. 1 g; 2 g; 3 g\n"
             "2. -#1 = -1/1000 kg; -1/500 kg; -3/1000 kg\n");
    REQUIRE(trace.steps.size() == 2);
    CHECK(trace.steps[0].kind == formula::StepKind::SeriesConstant);
    CHECK(trace.steps[1].kind == formula::StepKind::ElementwiseNegate);
}

TEST_CASE("a failing scalar operand is reported without a position", "[series][trace]")
{
    constexpr auto screens = formula::environment(
        formula::measured_series<series_trace::Retained>(series_trace::retained(130), series_trace::retained(210)),
        formula::Measured<series_trace::TotalMass> { formula::Rational { 1250 } });
    constexpr auto total = formula::var<series_trace::TotalMass>;
    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::series<series_trace::Retained, 2> * (total / (total - total)), screens, formula::RecordingSink<> { trace });
    std::string const text = formula::render_trace(trace, { .maxSteps = 30 });
    // The elementwise line says what failed and names no element: the
    // failure was the scalar's, before any element was computed.
    CHECK(text.find("7. #1 * #6 = division by zero\n") != std::string::npos);
    CHECK(text.find("at element") == std::string::npos);
}

TEST_CASE("an elementwise step whose left operand failed says its right one was not evaluated", "[series][trace]")
{
    // As for the scalar operators (binary_expression): the right side is
    // never evaluated, so the line names the left operand, #5, which failed,
    // in its place, and says what stands in the right one's -- never `* #5`,
    // which reads as something unseen times #5.
    constexpr auto screens = formula::environment(
        formula::measured_series<series_trace::Retained>(series_trace::retained(130), series_trace::retained(210)),
        formula::Measured<series_trace::TotalMass> { formula::Rational { 1250 } });
    constexpr auto s = formula::series<series_trace::Retained, 2>;
    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        s / (s - s) * formula::var<series_trace::TotalMass>, screens, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
          == "1. m_r = 130 g; 210 g\n"
             "2. m_r = 130 g; 210 g\n"
             "3. m_r = 130 g; 210 g\n"
             "4. #2 - #3 = 0 kg; 0 kg\n"
             "5. #1 / #4 = division by zero at element 1\n"
             "6. #5 * (not evaluated) = division by zero at element 1\n");
}

TEST_CASE("a binary step names the side that failed, the side never evaluated and a side that recorded no step",
          "[trace-render]")
{
    // The left side fails, so the right is never evaluated: `#4 / (not
    // evaluated)`, the failed operand where it stood.
    constexpr auto sample = var<SampleMass>;
    CHECK(derivationOf(sample / (sample - sample) / sample,
                       formula::environment(formula::Measured<SampleMass> { formula::Rational { 137 } }))
          == "1. m_s = 137 g\n"
             "2. m_s = 137 g\n"
             "3. m_s = 137 g\n"
             "4. #2 - #3 = 0 kg\n"
             "5. #1 / #4 = division by zero\n"
             "6. #5 / (not evaluated) = division by zero\n");

    // A consumer's node that records no step: on the right, after a left
    // operand that was evaluated; on the left, where its failure left the
    // right never evaluated.
    auto const diameter = formula::environment(formula::Measured<Diameter> { formula::Rational { 263 } });
    CHECK(derivationOf(var<Diameter> + UntracedLength {}, diameter)
          == "1. d = 263 mm\n"
             "2. #1 + (untraced) = division by zero\n");
    CHECK(derivationOf(UntracedLength {} + var<Diameter>, diameter)
          == "1. (untraced) + (not evaluated) = division by zero\n");
    // A recorded left that failed, before an untraced right: the right was
    // never evaluated, which is not the same as evaluated untraced.
    constexpr auto d = var<Diameter>;
    CHECK(lines(derivationOf(d / (d - d) * d + UntracedLength {}, diameter)).back()
          == "7. #6 + (not evaluated) = division by zero");

    // A side set by hand renders as set, but never a side "not evaluated"
    // under a value that was computed: such a step names its operands as
    // claimed.
    formula::Trace<> forged {};
    formula::Step<> three {};
    three.kind = formula::StepKind::Constant;
    three.unit = formula::coherent(formula::dim::Scalar);
    three.value = formula::Rational { 3 };
    formula::Step<> quotient = three;
    quotient.kind = formula::StepKind::Divide;
    quotient.operands = { 0 };
    quotient.rightOperand = formula::OperandSide::NotEvaluated;
    forged.steps = { three, quotient };
    CHECK(formula::render_trace(forged, { .maxSteps = 10 }) == "1. 3\n2. / #1 = 3\n");
}

TEST_CASE("a running total is one step naming its end, in the operand's unit", "[series][trace]")
{
    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<series_trace::Retained, 5>),
        series_trace::allPresent,
        formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
          == "1. m_r = 130 g; 210 g; 95 g; 340 g; 28 g\n"
             "2. cumulative(#1, from last) = 803 g; 673 g; 463 g; 368 g; 28 g\n");
    REQUIRE(trace.steps.size() == 2);
    CHECK(trace.steps[1].kind == formula::StepKind::CumulativeSum);
    CHECK(trace.steps[1].cumulativeDirection == formula::CumulativeDirection::FromLast);
    CHECK(trace.steps[1].operands == std::vector<std::size_t> { 0 });

    // The other end, over the series with its middle element unmeasured:
    // every total from there on is absent, and says so.
    formula::Trace<> fromFirst {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::cumulative<formula::CumulativeDirection::FromFirst>(formula::series<series_trace::Retained, 5>),
        series_trace::inputs,
        formula::RecordingSink<> { fromFirst });
    CHECK(formula::render_trace(fromFirst, { .maxSteps = 30 })
          == "1. m_r = 130 g; 210 g; (not measured); 340 g; 28 g\n"
             "2. cumulative(#1, from first) = 130 g; 340 g; (not measured); (not measured); (not measured)\n");
    REQUIRE(fromFirst.steps.size() == 2);
    CHECK(fromFirst.steps[1].cumulativeDirection == formula::CumulativeDirection::FromFirst);
}

TEST_CASE("a sum is a single-value step whose operand is the series step", "[series][trace]")
{
    auto const explained = formula::explain<series_trace::TotalMass>(
        formula::sum(formula::series<series_trace::Retained, 5>), series_trace::allPresent);
    CHECK(explained.outcome.measurement().value() == formula::Rational { 803 });
    CHECK(formula::render_trace(explained.trace, { .maxSteps = 30 })
          == "1. m_r = 130 g; 210 g; 95 g; 340 g; 28 g\n"
             "2. sum(#1) = 803 g\n");
    REQUIRE(explained.trace.steps.size() == 2);
    CHECK(explained.trace.steps[1].kind == formula::StepKind::SeriesSum);
    CHECK(explained.trace.steps[1].value == formula::Rational { 803, 1000 }); // one value, in coherent SI
    CHECK(explained.trace.steps[1].elements.empty());
    CHECK(explained.trace.steps[1].operands == std::vector<std::size_t> { 0 });
}

TEST_CASE("a sum, a range and a running total of Celsius readings read in the coherent unit, not as readings",
          "[series][trace]")
{
    // 296.85, 314.45 and 311.05 K. Their sum, 922.35 K, and their range,
    // 17.6 K, are no points on the Celsius scale: shown in degrees Celsius
    // they would read 649.2 and -255.55 degC, each off by the offset. A mean
    // is a point on the scale, 307.45 K, and reads as the readings do.
    std::string const degreesCelsius = "\xc2\xb0" "C";
    std::string const readingsLine = "1. T_r = 237/10 " + degreesCelsius + "; 413/10 " + degreesCelsius + "; 379/10 "
                                     + degreesCelsius + "\n";
    constexpr auto readings = formula::series<series_trace::Reading, 3>;
    CHECK(derivationOf(formula::sum(readings), series_trace::readings) == readingsLine + "2. sum(#1) = 18447/20 K\n");
    CHECK(derivationOf(formula::sample_range(readings), series_trace::readings)
          == readingsLine + "2. sample_range(#1) = 88/5 K\n");
    CHECK(derivationOf(formula::sample_mean(readings), series_trace::readings)
          == readingsLine + "2. sample_mean(#1) = 343/10 " + degreesCelsius + "\n");

    formula::Trace<> running {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::cumulative<formula::CumulativeDirection::FromFirst>(readings),
        series_trace::readings,
        formula::RecordingSink<> { running });
    CHECK(formula::render_trace(running, { .maxSteps = 30 })
          == readingsLine + "2. cumulative(#1, from first) = 5937/20 K; 6113/10 K; 18447/20 K\n");
    REQUIRE(running.steps.size() == 2);
    CHECK(running.steps[1].unit.offsetNumerator == 0);
}

TEST_CASE("a sum, a range and a mean of Fahrenheit readings read as those of Celsius ones do", "[series][trace]")
{
    // 50 and 51 degrees Fahrenheit are 50967/180 and 51067/180 K. Their sum,
    // 51017/90 K, and their range, 5/9 K, are no points on the Fahrenheit
    // scale and read in the unlabelled coherent unit -- kelvin -- where the
    // 5/9 K is a difference of one degree. Their mean is a point on the scale,
    // 101/2 degrees, and reads as the readings do.
    std::string const degreesFahrenheit = "\xc2\xb0" "F";
    std::string const readingsLine = "1. T_f = 50 " + degreesFahrenheit + "; 51 " + degreesFahrenheit + "\n";
    constexpr auto readings = formula::series<series_trace::FahrenheitReading, 2>;
    CHECK(derivationOf(formula::sum(readings), series_trace::fahrenheitReadings)
          == readingsLine + "2. sum(#1) = 51017/90 K\n");
    CHECK(derivationOf(formula::sample_range(readings), series_trace::fahrenheitReadings)
          == readingsLine + "2. sample_range(#1) = 5/9 K\n");
    CHECK(derivationOf(formula::sample_mean(readings), series_trace::fahrenheitReadings)
          == readingsLine + "2. sample_mean(#1) = 101/2 " + degreesFahrenheit + "\n");
}

TEST_CASE("a sum and a mean of a series in a unit with no symbol read in the coherent unit", "[series][trace]")
{
    // 0.137 and 0.263 m, entered as 137 and 263 of an unnamed thousandth of
    // a metre. The sum borrows no unit it cannot name: 0.4 m, in the coherent
    // unit every unlabelled computed value is shown in.
    constexpr auto gaps = formula::environment(
        formula::measured_series<series_trace::Gap>(formula::Measured<series_trace::Gap> { formula::Rational { 137 } },
                                                    formula::Measured<series_trace::Gap> { formula::Rational { 263 } }));
    CHECK(derivationOf(formula::sum(formula::series<series_trace::Gap, 2>), gaps)
          == "1. w = 137/1000 m; 263/1000 m\n"
             "2. sum(#1) = 2/5 m\n");
    // Nor does a mean, which would borrow an offset unit: 0.2 m.
    CHECK(derivationOf(formula::sample_mean(formula::series<series_trace::Gap, 2>), gaps)
          == "1. w = 137/1000 m; 263/1000 m\n"
             "2. sample_mean(#1) = 1/5 m\n");
}

TEST_CASE("a series with nothing measured traces as absence at every element, and never as zero", "[series][trace]")
{
    // A running total or a sum that started from zero would
    // print 0 g somewhere. Nothing here may.
    constexpr auto noneMeasured = formula::environment(
        formula::measured_series<series_trace::Retained>(formula::Measured<series_trace::Retained>::absent(),
                                                         formula::Measured<series_trace::Retained>::absent(),
                                                         formula::Measured<series_trace::Retained>::absent(),
                                                         formula::Measured<series_trace::Retained>::absent(),
                                                         formula::Measured<series_trace::Retained>::absent()));
    auto const explained =
        formula::explain<series_trace::TotalMass>(formula::sum(formula::cumulative<formula::CumulativeDirection::FromLast>(
                                                      formula::series<series_trace::Retained, 5>)),
                                                  noneMeasured);
    CHECK(explained.outcome.measurement().is_absent());
    std::string const text = formula::render_trace(explained.trace, { .maxSteps = 40 });
    CHECK(text
          == "1. m_r = (not measured); (not measured); (not measured); (not measured); (not measured)\n"
             "2. cumulative(#1, from last) = (not measured); (not measured); (not measured); (not measured); "
             "(not measured)\n"
             "3. sum(#2) = (not measured)\n");
    CHECK(text.find('0') == std::string::npos);
}

TEST_CASE("a running total that overflowed names its element, counted from one", "[series][trace]")
{
    // Stated in tonnes and read into kilograms, each large element just over
    // half of Rational's limit, at zero-based 1 and 2 -- off the centre, so
    // that from the last the position (1) is not the number of additions
    // made (3). From the first the total overflows at zero-based 2, element 3
    // in the text; from the last at zero-based 1, element 2.
    constexpr std::int64_t halfOfLimitInKg = std::numeric_limits<std::int64_t>::max() / 2000 + 1;
    constexpr auto heavy = formula::environment(formula::measured_series<series_trace::Stockpile>(
        formula::Measured<series_trace::Stockpile> { formula::Rational { 1 } },
        formula::Measured<series_trace::Stockpile> { formula::Rational { halfOfLimitInKg } },
        formula::Measured<series_trace::Stockpile> { formula::Rational { halfOfLimitInKg } },
        formula::Measured<series_trace::Stockpile> { formula::Rational { 2 } },
        formula::Measured<series_trace::Stockpile> { formula::Rational { 3 } }));
    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::cumulative<formula::CumulativeDirection::FromFirst>(formula::series<series_trace::Stockpile, 5>),
        heavy,
        formula::RecordingSink<> { trace });
    REQUIRE(trace.steps.size() == 2);
    CHECK(trace.steps[1].failedElement == std::optional<std::size_t> { 2 });
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
              .find("2. cumulative(#1, from first) = overflow in exact arithmetic at element 3\n")
          != std::string::npos);

    formula::Trace<> fromLast {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<series_trace::Stockpile, 5>),
        heavy,
        formula::RecordingSink<> { fromLast });
    REQUIRE(fromLast.steps.size() == 2);
    CHECK(fromLast.steps[1].failedElement == std::optional<std::size_t> { 1 });
    CHECK(formula::render_trace(fromLast, { .maxSteps = 30 })
              .find("2. cumulative(#1, from last) = overflow in exact arithmetic at element 2\n")
          != std::string::npos);
}

TEST_CASE("a per-element rounding records each element's granularity and its mode", "[series][trace]")
{
    struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", unit::Percent>
    {
    };
    static constexpr formula::PlacesTable<5> places { formula::DecimalPlaces { 0 },
                                                      formula::DecimalPlaces { 0 },
                                                      formula::DecimalPlaces { 0 },
                                                      formula::DecimalPlaces { 1 },
                                                      formula::DecimalPlaces { 1 } };
    constexpr auto screens = formula::environment(
        formula::measured_series<Passing>(formula::Measured<Passing> { formula::Rational { 125, 2 } },
                                          formula::Measured<Passing> { formula::Rational { 127, 2 } },
                                          formula::Measured<Passing> { formula::Rational { 486, 5 } },
                                          formula::Measured<Passing> { formula::Rational { 165, 4 } },
                                          formula::Measured<Passing> { formula::Rational { 161, 20 } }));
    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        formula::rounded_elementwise<unit::Percent, places, formula::RoundingMode::HalfAwayFromZero>(
            formula::series<Passing, 5>),
        screens,
        formula::RecordingSink<> { trace });
    // Shown in percent, the unit rounded in; the mode in the bracket at the
    // end, as a scalar rounding step writes it.
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
          == "1. p = 125/2 %; 127/2 %; 486/5 %; 165/4 %; 161/20 %\n"
             "2. round(#1, to 0/0/0/1/1 dp of %) = 63 %; 64 %; 97 %; 413/10 %; 81/10 % "
             "[nearest, ties away from zero]\n");
    REQUIRE(trace.steps.size() == 2);
    CHECK(trace.steps[1].kind == formula::StepKind::ElementwiseRound);
    CHECK(trace.steps[1].elementGranularities == std::vector<int> { 0, 0, 0, 1, 1 });
    CHECK(trace.steps[1].mode == formula::RoundingMode::HalfAwayFromZero);

    // Cut short by the budget, the bracket still ends the line.
    CHECK(formula::render_trace(trace, { .maxSteps = 8 })
          == "1. p = 125/2 %; 127/2 %; 486/5 %; 165/4 %; 161/20 %\n"
             "2. round(#1, to 0/0/0/1/1 dp of %) = 63 %; ... 4 more [nearest, ties away from zero]\n");

    // A dimensionless unit has no symbol, and the line then names none, as a
    // scalar rounding's does and as `render()` writes the node:
    // never a dangling "dp of )".
    struct Share: formula::Quantity<Share, "s", "share passing a screen", unit::One>
    {
    };
    static constexpr formula::PlacesTable<2> sharePlaces { formula::DecimalPlaces { 0 }, formula::DecimalPlaces { 1 } };
    constexpr auto shares = formula::environment(formula::measured_series<Share>(
        formula::Measured<Share> { formula::Rational { 7, 8 } }, formula::Measured<Share> { formula::Rational { 3, 8 } }));
    constexpr auto roundedShares =
        formula::rounded_elementwise<unit::One, sharePlaces, formula::RoundingMode::HalfAwayFromZero>(
            formula::series<Share, 2>);
    formula::Trace<> shareTrace {};
    (void) formula::detail::dispatch_series<formula::Rational>(
        roundedShares, shares, formula::RecordingSink<> { shareTrace });
    CHECK(formula::render_trace(shareTrace, { .maxSteps = 30 })
          == "1. s = 7/8; 3/8\n"
             "2. round(#1, to 0/1 dp) = 1; 2/5 [nearest, ties away from zero]\n");
    CHECK(formula::render(roundedShares) == "round(s(i), to 0/1 dp)");
}

TEST_CASE("a series and a curve escape their symbols and units, as a scalar step does", "[trace-render][escape][series]")
{
    // Phase 11's escaping reaches the series lines through `step_line`, the
    // one entry point: the declared symbol that spells a jurisdiction's
    // clause, and the author's unit that closes the value's clause, are
    // escaped on every element, every pair and an interpolation's value.
    auto const factors = formula::environment(
        formula::measured_series<ForgingFactor>(formula::Measured<ForgingFactor> { formula::Rational { 1 } },
                                                formula::Measured<ForgingFactor> { formula::Rational { 2 } }));
    formula::Trace<> factorTrace {};
    (void) formula::checked_evaluate_series<ForgingFactor>(
        formula::series<ForgingFactor, 2>, factors, formula::RecordingSink<> { factorTrace });
    CHECK(formula::render_trace(factorTrace, { .maxSteps = 10 })
          == "1. k\\] \\[fixed by jurisdiction overlay: Example Standard 9:2022 NA = 1; 2\n");

    constexpr formula::BreakpointTable<2> points { formula::breakpoint(103), formula::breakpoint(127) };
    auto const loads = formula::environment(
        formula::measured_series<ForgingLoad>(formula::Measured<ForgingLoad> { formula::Rational { 4 } },
                                              formula::Measured<ForgingLoad> { formula::Rational { 5 } }));
    formula::Trace<> curveTrace {};
    (void) formula::checked_evaluate<ForgingLoad>(
        formula::interpolate_at(formula::curve(formula::domain<unit::Metre, points>, formula::series<ForgingLoad, 2>),
                                formula::constant<unit::Metre>(formula::Rational { 113 })),
        loads,
        formula::RecordingSink<> { curveTrace });
    CHECK(formula::render_trace(curveTrace, { .maxSteps = 20 })
          == "1. 103 m; 127 m\n"
             "2. P = 4 N\\] \\[x; 5 N\\] \\[x\n"
             "3. curve(#1, #2) = 103 m: 4 N\\] \\[x; 127 m: 5 N\\] \\[x\n"
             "4. 113 m\n"
             "5. interpolate(#3, at #4) = 53/12 N\\] \\[x [between 103 and 127 m]\n");

    // Raw observations, and a binning's miss, whose clause names the
    // observation and the classes in the author's unit.
    auto const observedLoads = formula::environment(
        formula::MeasuredObservations<ForgingLoad, 2>(formula::Rational { 4 }, formula::Rational { 12 }));
    formula::Trace<> binningTrace {};
    (void) formula::checked_evaluate_series_si(
        formula::binned<ForgingNewton, ForgingLoadBands>(formula::observations<ForgingLoad, 2>),
        observedLoads,
        formula::RecordingSink<> { binningTrace });
    CHECK(formula::render_trace(binningTrace, { .maxSteps = 20 })
          == "1. P = 4 N\\] \\[x; 12 N\\] \\[x\n"
             "2. bin(#1) = argument outside the domain of the operation at observation 2 "
             "[12 N\\] \\[x in no class; the classes cover 127/100 to under 973/100 N\\] \\[x]\n");
}

namespace
{
struct Determinations: formula::Quantity<Determinations, "n", "number of determinations", unit::One>
{
};

/// The shared fixtures' critical-value table. **Invented, and deliberately
/// unrealistic -- no published table holds values like these -- so that
/// nobody mistakes it for one or "corrects" it toward one.** No row for 7.
inline constexpr formula::SampleSizeTable<5> DeviationSizes { 3, 4, 5, 6, 8 };

/// A unit no conversion out of can fit: one of it is 2^63 - 1 coherent units.
inline constexpr formula::Unit Enormous { .dimension = formula::dim::Scalar,
                                          .magnitudeNumerator = 9'223'372'036'854'775'807,
                                          .magnitudeDenominator = 1,
                                          .symbolText = formula::symbol("E"),
                                          .decimals = 0 };

/// The critical value at @p countExpression, traced.
template <formula::Unit ResultUnit = unit::One,
          formula::SampleSizeTable Sizes = DeviationSizes,
          typename Count,
          typename Env>
[[nodiscard]] formula::Trace<> criticalTraceOf(Count countExpression, Env const& countInputs)
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    if constexpr (Sizes.size() == 5)
        (void) formula::checked_evaluate_si<formula::Rational>(
            formula::critical_value<Sizes, ResultUnit>(countExpression,
                                                       { formula::Rational { 10 },
                                                         formula::Rational { 30 },
                                                         formula::Rational { 20 },
                                                         formula::Rational { 50 },
                                                         formula::Rational { 40 } }),
            countInputs,
            sink);
    else
        (void) formula::checked_evaluate_si<formula::Rational>(
            formula::critical_value<Sizes, ResultUnit>(countExpression, {}), countInputs, sink);
    return trace;
}

/// The trace of the table's critical value at @p count, or of an absent count
/// when @p count is empty, rendered.
[[nodiscard]] std::string criticalTraceAt(std::optional<formula::Rational> count)
{
    auto const measured =
        count.has_value() ? formula::Measured<Determinations> { *count } : formula::Measured<Determinations>::absent();
    return formula::render_trace(criticalTraceOf(var<Determinations>, formula::environment(measured)), { .maxSteps = 10 });
}
} // namespace

TEST_CASE("a derivation names the sample size a critical value was read at", "[trace-render][critical-value]")
{
    CHECK(criticalTraceAt(formula::Rational { 6 })
          == "1. n = 6\n"
             "2. critical(#1) = 50 [critical value at n = 6]\n");
}

TEST_CASE("a derivation of a critical-value miss names the count and every size the table declares",
          "[trace-render][critical-value]")
{
    // The hole at 7, and the table's whole list of sizes, so that the reader
    // sees which counts would have hit rather than a bare domain error.
    std::string const missed = criticalTraceAt(formula::Rational { 7 });
    CHECK(missed.starts_with("1. n = 7\n2. critical(#1) = "));
    CHECK(missed.ends_with(" [no row for n = 7 (declared: 3, 4, 5, 6, 8)]\n"));

    // Zero is a whole, non-negative count that no row declares: a miss, and
    // never called "not a whole, non-negative number".
    CHECK(criticalTraceAt(formula::Rational { 0 }).ends_with(" [no row for n = 0 (declared: 3, 4, 5, 6, 8)]\n"));

    // A count far above any row is still a count, and misses; it is never
    // narrowed onto a row. 2^32 + 3 is the count that wraps onto the row for
    // 3 wherever the table's size type is 32 bits wide.
    CHECK(criticalTraceAt(formula::Rational { (std::int64_t { 1 } << 32) + 3 })
              .ends_with(" [no row for n = 4294967299 (declared: 3, 4, 5, 6, 8)]\n"));

    // A count that is no number of determinations names no row either, and
    // the line says why, pointing at the step that holds the value.
    std::string const fractional = criticalTraceAt(formula::Rational { 11, 2 });
    CHECK(fractional.starts_with("1. n = 11/2\n2. critical(#1) = "));
    CHECK(
        fractional.ends_with(" [no row for n = #1, which is not a whole, non-negative number (declared: 3, 4, 5, 6, 8)]\n"));

    // An absent count is not a miss and never reads n = 0.
    std::string const absent = criticalTraceAt(std::nullopt);
    CHECK(absent.find("n = 0") == std::string::npos);
    CHECK(absent.find("no row") == std::string::npos);
}

TEST_CASE("a derivation of a critical value says whose failure it carries", "[trace-render][critical-value]")
{
    auto const sixSpecimens = formula::environment(formula::Measured<Determinations> { formula::Rational { 6 } });

    // The count itself fails, dividing by zero: the lookup relays it and says
    // so, never "no row".
    std::string const relayed = formula::render_trace(
        criticalTraceOf(var<Determinations> / (var<Determinations> - var<Determinations>), sixSpecimens),
        { .maxSteps = 10 });
    CHECK(relayed.ends_with(" [carried up from #5]\n"));

    // The row is found, and converting its value out of the table's unit
    // overflows: the lookup's own conversion, not a miss.
    std::string const overflowed =
        formula::render_trace(criticalTraceOf<Enormous>(var<Determinations>, sixSpecimens), { .maxSteps = 10 });
    CHECK(overflowed.ends_with(" [this lookup's own unit conversion failed, not anything below it]\n"));

    // A table of no sizes misses every count, and says it declares none.
    std::string const empty = formula::render_trace(
        criticalTraceOf<unit::One, formula::SampleSizeTable<0> {}>(var<Determinations>, sixSpecimens), { .maxSteps = 10 });
    CHECK(empty.ends_with(" [no row for n = 6; the table declares no sizes]\n"));
}

TEST_CASE("a critical-value step records the count and whose failure it carries, and the trace keeps the sizes",
          "[trace][critical-value]")
{
    auto const recordAt = [](formula::Rational count) {
        return criticalTraceOf(var<Determinations>, formula::environment(formula::Measured<Determinations> { count }));
    };

    auto const hitTrace = recordAt(formula::Rational { 8 });
    auto const& hit = hitTrace.steps.back();
    CHECK(hit.kind == formula::StepKind::SampleSizeLookup);
    CHECK(hit.lookupFailure == formula::LookupFailure::None);
    CHECK(hit.lookupKey == 8);
    // The declared sizes live in the trace's side table, keyed by the step's
    // index, and the step itself carries nothing for them.
    REQUIRE(hitTrace.sampleSizeRecords.size() == 1);
    CHECK(hitTrace.sampleSizeRecords[0].step == hitTrace.steps.size() - 1);
    CHECK(hitTrace.sampleSizeRecords[0].declaredSizes == "3, 4, 5, 6, 8");

    auto const missTrace = recordAt(formula::Rational { 2 });
    CHECK(missTrace.steps.back().lookupFailure == formula::LookupFailure::Missed);
    CHECK(missTrace.steps.back().lookupKey == 2);

    auto const notACountTrace = recordAt(formula::Rational { -3 });
    CHECK(notACountTrace.steps.back().lookupFailure == formula::LookupFailure::NotACount);
    CHECK(notACountTrace.steps.back().lookupKey == 0);

    auto const relayedTrace =
        criticalTraceOf(var<Determinations> / (var<Determinations> - var<Determinations>),
                        formula::environment(formula::Measured<Determinations> { formula::Rational { 6 } }));
    CHECK(relayedTrace.steps.back().lookupFailure == formula::LookupFailure::Propagated);

    // A hand-built trace whose record names another step: the line says the
    // record is missing rather than print sizes it does not have.
    auto forged = recordAt(formula::Rational { 7 });
    forged.sampleSizeRecords[0].step = 0;
    CHECK(formula::render_trace(forged, { .maxSteps = 10 })
              .ends_with(" [no row for n = 7 (the table's sizes were not recorded)]\n"));
}

namespace
{
/// An environment of a consumer's own that works its values out and says,
/// at run time, where each came from: 180 l of water, and cement it has no
/// value for -- or, when @p cementFails, whose read fails with
/// `DomainError`. Each source is whatever it is told.
struct CalculatingEnvironment
{
    formula::ValueSource waterSource;
    formula::ValueSource cementSource;
    bool cementFails;

    template <formula::Described Q>
    [[nodiscard]] constexpr formula::Measured<Q> get() const noexcept
    {
        if constexpr (std::is_same_v<Q, WaterVolume>)
            return formula::Measured<Q> { formula::Rational { 180 } };
        else
            return formula::Measured<Q>::absent();
    }

    template <formula::Described Q>
    [[nodiscard]] constexpr std::expected<formula::Measured<Q>, formula::ArithmeticError> checked_get() const noexcept
    {
        if (std::is_same_v<Q, CementVolume> && cementFails)
            return std::unexpected { formula::ArithmeticError::DomainError };
        return get<Q>();
    }

    template <formula::Described Q>
    [[nodiscard]] constexpr formula::ValueSource source_of() const noexcept
    {
        return std::is_same_v<Q, WaterVolume> ? waterSource : cementSource;
    }
};

[[nodiscard]] formula::Trace<> traced_sum(CalculatingEnvironment const& calculating)
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_si<formula::Rational>(
        var<WaterVolume> + var<CementVolume>, calculating, formula::RecordingSink<> { trace });
    return trace;
}
} // namespace

TEST_CASE("a value the environment calculated says so, and one it has no value for says that", "[trace-render]")
{
    constexpr auto derived = formula::ValueSource::Derived;
    formula::Trace<> const calculated = traced_sum({ derived, derived, false });
    CHECK(calculated.steps[0].inputSource == derived);
    CHECK(calculated.steps[1].inputSource == derived);
    CHECK(formula::render_trace(calculated, { .maxSteps = 10 })
          == "1. V_w = 180 l, calculated\n"
             "2. V_c = (no value), calculated\n"
             "3. #1 + #2 = (not measured)\n");

    // The run-time answer decides the other wordings too: a typed-in input,
    // and a measured one never measured, read as they always have.
    formula::Trace<> const measured =
        traced_sum({ formula::ValueSource::ManuallyEntered, formula::ValueSource::Measured, false });
    CHECK(formula::render_trace(measured, { .maxSteps = 10 })
          == "1. V_w = 180 l, entered by hand\n"
             "2. V_c = (not measured)\n"
             "3. #1 + #2 = (not measured)\n");
    formula::Trace<> const typedEmpty =
        traced_sum({ formula::ValueSource::Measured, formula::ValueSource::ManuallyEntered, false });
    CHECK(formula::render_trace(typedEmpty, { .maxSteps = 10 })
          == "1. V_w = 180 l\n"
             "2. V_c = (entered by hand as empty)\n"
             "3. #1 + #2 = (not measured)\n");
}

TEST_CASE("a calculated value whose read failed is that variable's failure, and says it was calculated",
          "[trace-render]")
{
    constexpr auto derived = formula::ValueSource::Derived;
    formula::Trace<> const failed = traced_sum({ derived, derived, true });
    REQUIRE(failed.steps.size() == 3);
    CHECK(failed.steps[1].kind == formula::StepKind::Variable);
    CHECK(failed.steps[1].error == formula::ArithmeticError::DomainError);
    CHECK(failed.steps[1].inputSource == derived);
    CHECK(failed.steps[2].error == formula::ArithmeticError::DomainError);
    CHECK(formula::render_trace(failed, { .maxSteps = 10 })
          == "1. V_w = 180 l, calculated\n"
             "2. V_c = argument outside the domain of the operation, calculated\n"
             "3. #1 + #2 = argument outside the domain of the operation\n");
}

// ------------------------------------------------------- decimals in a trace

namespace
{
/// A decimal wherever that is the exact value, and otherwise the value rounded
/// half to even at its unit's declared decimals, marked `≈`. The tests below
/// that use it state values whose decimals never end, beside a computed value
/// that is rounded, so that a number shown exact and one shown rounded can be
/// told apart -- and a style that rounded everything, or nothing, fails.
inline constexpr formula::NumberStyle approximately =
    formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven);

/// @p trace with every number spelled in @p numberStyle.
[[nodiscard]] std::string renderedIn(formula::Trace<> const& trace, formula::NumberStyle numberStyle)
{
    return formula::render_trace(trace, { .maxSteps = 40, .numbers = numberStyle });
}

/// The trace of @p node evaluated against @p environment.
template <typename Node, typename Env>
[[nodiscard]] formula::Trace<> tracedValue(Node const& node, Env const& environment)
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_si<formula::Rational>(node, environment, formula::RecordingSink<> { trace });
    return trace;
}

struct Aperture: formula::Quantity<Aperture, "a", "screen aperture", unit::Metre>
{
};
struct PassingShare: formula::Quantity<PassingShare, "p", "share passing a screen", unit::Percent>
{
};
struct Coefficient: formula::Quantity<Coefficient, "c", "a dimensionless coefficient", unit::One>
{
};

/// A unit whose declared decimals no `DecimalPlaces` can hold: 19. Invented,
/// to make a padded or rounded spelling fail.
inline constexpr formula::Unit OverPreciseUnit { .dimension = formula::dim::Length,
                                                 .symbolText = formula::symbol("u"),
                                                 .decimals = 19 };
struct OverPreciseLength: formula::Quantity<OverPreciseLength, "l_u", "a length in an over-precise unit", OverPreciseUnit>
{
};

/// Thirds of a metre, whose decimals never end: as a snap's permitted values
/// and as a curve's declared domain.
inline constexpr formula::BreakpointTable<2> ThirdsPoints { formula::breakpoint(1, 3), formula::breakpoint(2, 3) };

/// Two classes of aperture, 0 to under 1/3 m and 1/3 to under 2/3 m.
inline constexpr formula::BandTable<2> ThirdsClasses { formula::band(0, 1, 1, 3), formula::band(1, 3, 2, 3) };

/// Two invented rows in percent: at least 100/3 %, and from 0 to 200/3 %.
inline constexpr formula::Envelope<2> ThirdsEnvelope {
    formula::LimitRow { formula::limit(rat(100, 3)), formula::unbounded },
    formula::LimitRow { formula::limit(rat(0)), formula::limit(rat(200, 3)) },
};

/// 100/3 % and 500/7 % passing -- invented, and decimals that never end.
[[nodiscard]] auto passingShares()
{
    return formula::environment(formula::measured_series<PassingShare>(formula::Measured<PassingShare> { rat(100, 3) },
                                                                       formula::Measured<PassingShare> { rat(500, 7) }));
}
} // namespace

TEST_CASE("a trace spells its numbers as exact decimals when asked, and only where they are exact",
          "[trace-render][decimals]")
{
    constexpr auto ratio =
        formula::documented(var<WaterVolume> / var<CementVolume>,
                            { .title = "Water/cement ratio", .reference = "Example Standard 1:2020", .section = "5.2" });
    formula::Trace<> const trace =
        tracedValue(ratio,
                    formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                         formula::Measured<CementVolume> { formula::Rational { 300 } }));
    CHECK(renderedIn(trace, formula::NumberStyle::exact_decimal())
          == "1. V_w = 180 l\n"
             "2. V_c = 300 l\n"
             "3. #1 / #2 = 0.6\n"
             "4. #3 = 0.6 [Water/cement ratio, Example Standard 1:2020, 5.2]\n");
    // Fractions by default, as a trace has always read.
    CHECK(formula::render_trace(trace, { .maxSteps = 40 }) == renderedIn(trace, formula::NumberStyle::fraction()));
    CHECK(renderedIn(trace, formula::NumberStyle::fraction()).find("3. #1 / #2 = 3/5\n") != std::string::npos);
}

TEST_CASE("a value whose decimal never ends is a fraction unless an approximation is asked for, and then says so",
          "[trace-render][decimals]")
{
    constexpr auto density = var<Mass> / var<Volume>;
    formula::Trace<> const third = tracedValue(density,
                                               formula::environment(formula::Measured<Mass> { formula::Rational { 1 } },
                                                                    formula::Measured<Volume> { formula::Rational { 3 } }));
    CHECK(renderedIn(third, formula::NumberStyle::fraction()) == "1. m = 1 kg\n2. V = 3 m3\n3. #1 / #2 = 1/3 kg/m^3\n");
    CHECK(renderedIn(third, formula::NumberStyle::exact_decimal()) == "1. m = 1 kg\n2. V = 3 m3\n3. #1 / #2 = 1/3 kg/m^3\n");
    CHECK(renderedIn(third, approximately)
          == "1. m = 1 kg\n2. V = 3 m3\n3. #1 / #2 = \xe2\x89\x88"
             "0.333 kg/m^3\n");

    // Padded to the declared decimals of each unit: 3 for kilograms, 4 for
    // cubic metres. The quotient's unit is one nobody declared -- the
    // coherent kg/m3, with no symbol -- so it is never padded, and rounded it
    // uses that unit's 3 places.
    CHECK(renderedIn(third, formula::NumberStyle::exact_decimal(formula::DecimalPadding::Padded))
          == "1. m = 1.000 kg\n2. V = 3.0000 m3\n3. #1 / #2 = 1/3 kg/m^3\n");
    CHECK(renderedIn(third,
                     formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven,
                                                               formula::DecimalPadding::Padded))
          == "1. m = 1.000 kg\n2. V = 3.0000 m3\n3. #1 / #2 = \xe2\x89\x88"
             "0.333 kg/m^3\n");
    // An unlabelled value that is an exact decimal shows that it is not
    // padded: 1.5, not 1.500.
    formula::Trace<> const threeHalves =
        tracedValue(density,
                    formula::environment(formula::Measured<Mass> { formula::Rational { 3 } },
                                         formula::Measured<Volume> { formula::Rational { 2 } }));
    CHECK(renderedIn(threeHalves, formula::NumberStyle::exact_decimal(formula::DecimalPadding::Padded))
          == "1. m = 3.000 kg\n2. V = 2.0000 m3\n3. #1 / #2 = 1.5 kg/m^3\n");
}

TEST_CASE("a value the style cannot spell in its unit is not shown, and says why", "[trace-render][decimals]")
{
    formula::Trace<> const trace =
        tracedValue(var<OverPreciseLength>,
                    formula::environment(formula::Measured<OverPreciseLength> { formula::Rational { 3, 5 } }));
    // Fractions and unpadded exact decimals never read the unit's decimals.
    CHECK(renderedIn(trace, formula::NumberStyle::fraction()) == "1. l_u = 3/5 u\n");
    CHECK(renderedIn(trace, formula::NumberStyle::exact_decimal()) == "1. l_u = 0.6 u\n");
    // Padding or rounding does, and 19 is more than any `DecimalPlaces` holds.
    CHECK(renderedIn(trace, formula::NumberStyle::exact_decimal(formula::DecimalPadding::Padded))
          == "1. l_u = (not shown: overflow in exact arithmetic)\n");
    CHECK(renderedIn(trace, approximately) == "1. l_u = (not shown: overflow in exact arithmetic)\n");
}

TEST_CASE("a rejection's statistic and limit are shown exact beside the comparison, whatever the style",
          "[trace-render][decimals][rejection]")
{
    // 40, 40 and 47 g: the mean is 127/3 g, the third determination lies
    // 14/3 g from it, and the limit is a tenth of the mean, 127/30 g. Two
    // values rounded to one decimal each could read as equal beside the `>`
    // that separated them; these are shown as they are.
    constexpr auto rejection =
        formula::without_outliers<formula::PerPass::MostExtreme,
                                  formula::OnLimit::Keep,
                                  formula::AtMost<1>,
                                  formula::KeepAtLeast<2>>(formula::series<SampleMass, 3>,
                                                           formula::deviation_from_mean(rat(1, 10)
                                                                                        * formula::pass_mean<SampleMass>),
                                                           formula::Verdict { "repeat the determination" },
                                                           formula::Citation { .reference = "Example Standard 7:2020" });
    auto const determinations = formula::environment(
        formula::measured_series<SampleMass>(formula::Measured<SampleMass> { rat(40) },
                                             formula::Measured<SampleMass> { rat(40) },
                                             formula::Measured<SampleMass> { rat(47) }));
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_rejection<SampleMass>(rejection, determinations, formula::RecordingSink<> { trace });

    CHECK(renderedIn(trace, approximately)
          == "1. m_s = 40 g; 40 g; 47 g\n"
             "2. 0.1\n"
             "3. pass mean = \xe2\x89\x88"
             "42.3 g\n"
             "4. #2 * #3 = \xe2\x89\x88"
             "0.004 kg\n"
             "5. pass 1: 3 values, mean \xe2\x89\x88"
             "42.3 g\n"
             "6. rejected element 3 of 3 (47 g) in pass 1: abs(x - mean) = 14/3 g > 127/30 g (deviation from mean)\n"
             "7. 0.1\n"
             "8. pass mean = 40 g\n"
             "9. #7 * #8 = 0.004 kg\n"
             "10. pass 2: 2 values, mean 40 g\n"
             "11. settled: 1 rejected, 2 remain\n");

    // Squared, as a deviation in standard deviations compares them, over six
    // determinations -- 40 g five times and 47 g, a mean of 247/6 g:
    // 1225/36 g2 against 49/6 g2, shown exact.
    constexpr auto inDeviations =
        formula::without_outliers<formula::PerPass::MostExtreme,
                                  formula::OnLimit::Keep,
                                  formula::AtMost<1>,
                                  formula::KeepAtLeast<3>>(formula::series<SampleMass, 6>,
                                                           formula::deviation_in_stddevs(formula::number(rat(1))),
                                                           formula::Verdict { "repeat the determination" },
                                                           formula::Citation { .reference = "Example Standard 7:2020" });
    auto const sixOf = [](std::int64_t last, std::int64_t fifth) {
        return formula::environment(formula::measured_series<SampleMass>(formula::Measured<SampleMass> { rat(40) },
                                                                         formula::Measured<SampleMass> { rat(40) },
                                                                         formula::Measured<SampleMass> { rat(40) },
                                                                         formula::Measured<SampleMass> { rat(40) },
                                                                         formula::Measured<SampleMass> { rat(fifth) },
                                                                         formula::Measured<SampleMass> { rat(last) }));
    };
    formula::Trace<> squared {};
    (void) formula::checked_evaluate_rejection<SampleMass>(inDeviations,
                                                           sixOf(47, 40),
                                                           formula::RecordingSink<> { squared });
    CHECK(renderedIn(squared, approximately)
          == "1. m_s = 40 g; 40 g; 40 g; 40 g; 40 g; 47 g\n"
             "2. 1\n"
             "3. pass 1: 6 values, mean \xe2\x89\x88"
             "41.2 g\n"
             "4. rejected element 6 of 6 (47 g) in pass 1: (x - mean)^2 = 1225/36 g2 > limit^2 * s^2 = 49/6 g2 "
             "(deviation in standard deviations)\n"
             "5. 1\n"
             "6. pass 2: 5 values, mean 40 g\n"
             "7. settled: 1 rejected, 5 remain\n");

    // Padded, a square is still never padded -- it declares no decimals of
    // its own -- where the rejected value in grams is: with 46 g, a mean of
    // 41 g, 25 g2 against 6 g2.
    formula::Trace<> padded {};
    (void) formula::checked_evaluate_rejection<SampleMass>(inDeviations, sixOf(46, 40), formula::RecordingSink<> { padded });
    CHECK(renderedIn(padded, formula::NumberStyle::exact_decimal(formula::DecimalPadding::Padded))
              .find("rejected element 6 of 6 (46.0 g) in pass 1: (x - mean)^2 = 25 g2 > limit^2 * s^2 = 6 g2 "
                    "(deviation in standard deviations)\n")
          != std::string::npos);

    // By gap to range, against a critical value read at the pass's n from a
    // table invented for this test -- 2/3 at n = 6, 3/2 at n = 5 -- over 40 g
    // four times, 41 g and 47 g: a gap of 6 g over a range of 7 g, 6/7 against
    // 2/3, both a pure number.
    constexpr auto byGap =
        formula::without_outliers<formula::PerPass::MostExtreme,
                                  formula::OnLimit::Keep,
                                  formula::AtMost<1>,
                                  formula::KeepAtLeast<3>>(
            formula::series<SampleMass, 6>,
            formula::gap_to_range(formula::critical_value<DeviationSizes, unit::One>(
                formula::pass_count, { rat(9), rat(9), rat(3, 2), rat(2, 3), rat(9) })),
            formula::Verdict { "repeat the determination" },
            formula::Citation { .reference = "Example Standard 7:2020" });
    formula::Trace<> gapTrace {};
    (void) formula::checked_evaluate_rejection<SampleMass>(byGap, sixOf(47, 41), formula::RecordingSink<> { gapTrace });
    std::string const gapText = renderedIn(gapTrace, approximately);
    CHECK(gapText.find("rejected element 6 of 6 (47 g) in pass 1: gap / range = 6/7 > 2/3 (gap to range)\n")
          != std::string::npos);
    CHECK(gapText.find("pass 1: 6 values, mean \xe2\x89\x88"
                       "41.3 g\n")
          != std::string::npos);
}

TEST_CASE("a conformity element's value and its row are shown exact, whatever the style", "[trace-render][decimals]")
{
    constexpr auto passingCheck = formula::conformity<unit::Percent>(
        formula::series<PassingShare, 2>, ThirdsEnvelope, formula::Verdict { "reject the specimen" });
    formula::Trace<> trace {};
    (void) formula::check_conformity(passingCheck, passingShares(), formula::RecordingSink<> { trace });

    // Rounded, the first element would read 33.3 % against a row of at least
    // 100/3 % -- as if it had failed the row it met exactly.
    CHECK(renderedIn(trace, approximately)
          == "1. p = \xe2\x89\x88"
             "33.3 %; \xe2\x89\x88"
             "71.4 %\n"
             "2. conform(#1) [1 satisfied, 100/3 % (at least 100/3 %); "
             "2 violated, 500/7 % (from 0 to 200/3 %): reject the specimen]\n");
}

TEST_CASE("a binning's missed observation and the classes it missed are shown exact, whatever the style",
          "[trace-render][decimals]")
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_series_si(
        formula::binned<unit::Metre, ThirdsClasses>(formula::observations<Aperture, 2>),
        formula::environment(formula::MeasuredObservations<Aperture, 2>(rat(1, 7), rat(2, 3))),
        formula::RecordingSink<> { trace });

    CHECK(renderedIn(trace, approximately)
          == "1. a = \xe2\x89\x88"
             "0.143 m; \xe2\x89\x88"
             "0.667 m\n"
             "2. bin(#1) = argument outside the domain of the operation at observation 2 "
             "[2/3 m in no class; the classes cover 0 to under 2/3 m]\n");
}

TEST_CASE("a snap's value and the neighbour it names are the permitted values as typed, whatever the style",
          "[trace-render][decimals]")
{
    formula::Trace<> const trace =
        tracedValue(formula::snapped<unit::Metre, ThirdsPoints, formula::SnapTie::TowardLower>(var<Aperture>),
                    formula::environment(formula::Measured<Aperture> { rat(3, 7) }));
    CHECK(renderedIn(trace, approximately)
          == "1. a = \xe2\x89\x88"
             "0.429 m\n"
             "2. snap(#1) = 1/3 m [1/3 m to 2/3 m; nearer 1/3 m]\n");
}

TEST_CASE("a number typed rather than computed is shown exact, whatever the style", "[trace-render][decimals]")
{
    // Each beside a product computed from it, which is rounded: so a style
    // that never rounded, or rounded everything, fails here as well.
    CHECK(renderedIn(tracedValue(formula::constant<unit::One>(rat(1, 3)) * var<Mass>,
                                 formula::environment(formula::Measured<Mass> { rat(2, 7) })),
                     approximately)
          == "1. 1/3\n"
             "2. m = \xe2\x89\x88"
             "0.286 kg\n"
             "3. #1 * #2 = \xe2\x89\x88"
             "0.095 kg\n");

    // The library's pi is a rational it states itself, and has no decimal
    // that ends.
    CHECK(renderedIn(tracedValue(formula::pi * var<Mass>, formula::environment(formula::Measured<Mass> { rat(1) })),
                     approximately)
          == "1. pi = 245850922/78256779\n"
             "2. m = 1 kg\n"
             "3. #1 * #2 = \xe2\x89\x88"
             "3.142 kg\n");

    // A table's row, which each of the three table lookups reads: the banded
    // one's bounds are shown as typed too, as exact decimals.
    CHECK(renderedIn(tracedValue(banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(
                                     var<Diameter>, { rat(100, 3), rat(200, 3), rat(500, 7) })
                                     * rat(1, 7),
                                 diameterOf(30)),
                     approximately)
          == "1. d = 30 mm\n"
             "2. lookup(#1) = 200/3 % [2.41 to under 4.73 cm]\n"
             "3. 1/7\n"
             "4. #2 * #3 = \xe2\x89\x88"
             "0.095\n");
    CHECK(renderedIn(tracedValue(exact_lookup<ShapeKeys, unit::One>(RenderedShape::Cylinder,
                                                                    { rat(1, 3), rat(2, 3), rat(1, 7) })
                                     * rat(2, 7),
                                 formula::environment()),
                     approximately)
          == "1. lookup(key Cylinder) = 1/7\n"
             "2. 2/7\n"
             "3. #1 * #2 = \xe2\x89\x88"
             "0.041\n");
    CHECK(renderedIn(tracedValue(formula::critical_value<DeviationSizes, unit::One>(
                                     var<Determinations>, { rat(1, 3), rat(2, 3), rat(1, 7), rat(2, 7), rat(3, 7) })
                                     * rat(1, 11),
                                 formula::environment(formula::Measured<Determinations> { rat(5) })),
                     approximately)
          == "1. n = 5\n"
             "2. critical(#1) = 1/7 [critical value at n = 5]\n"
             "3. 1/11\n"
             "4. #2 * #3 = \xe2\x89\x88"
             "0.013\n");
}

TEST_CASE("a constant an overlay fixed or derived is shown as typed, whatever the style", "[trace-render][decimals]")
{
    constexpr auto scaledMass = formula::method(
        formula::variants(formula::variant<PlainDensity>(var<Coefficient> * var<Mass>)),
        formula::rounding_rule<unit::Kilogram, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    constexpr formula::Citation annex { .reference = "Example Standard 12:2021 NA", .section = "NA.4" };
    auto const twoKilograms = formula::environment(formula::Measured<Mass> { rat(2) });

    constexpr auto fixed =
        formula::apply(formula::overlay(formula::with_constant<Coefficient>(rat(1, 3), annex)), scaledMass);
    formula::Trace<> fixedTrace {};
    (void) formula::evaluate_method<PlainDensity>(fixed, twoKilograms, formula::RecordingSink<> { fixedTrace });
    CHECK(renderedIn(fixedTrace, approximately)
          == "1. c = 1/3 [fixed by jurisdiction overlay: Example Standard 12:2021 NA, NA.4]\n"
             "2. m = 2 kg\n"
             "3. #1 * #2 = \xe2\x89\x88"
             "0.667 kg\n"
             "4. round(#3, in kg) = 0.667 kg [rounded to 3 dp (method default); nearest, ties away from zero]\n"
             "5. #4 = 0.667 kg [variant PlainDensity (1st of 1), selected by tag]\n");

    // A derived quantity passes its definition's value on: a constant typed
    // as 1/3 reads 1/3 on both lines.
    constexpr auto derived = formula::apply(
        formula::overlay(formula::add_derived<Coefficient>(formula::constant<unit::One>(rat(1, 3)), annex)), scaledMass);
    formula::Trace<> derivedTrace {};
    (void) formula::evaluate_method<PlainDensity>(derived, twoKilograms, formula::RecordingSink<> { derivedTrace });
    CHECK(renderedIn(derivedTrace, approximately)
          == "1. 1/3\n"
             "2. c = #1 = 1/3 [derived by jurisdiction overlay: Example Standard 12:2021 NA, NA.4]\n"
             "3. m = 2 kg\n"
             "4. #2 * #3 = \xe2\x89\x88"
             "0.667 kg\n"
             "5. round(#4, in kg) = 0.667 kg [rounded to 3 dp (method default); nearest, ties away from zero]\n"
             "6. #5 = 0.667 kg [variant PlainDensity (1st of 1), selected by tag]\n");
}

TEST_CASE("a step that passes a typed number on shows it as typed, whatever the style", "[trace-render][decimals]")
{
    // A citation over a constant, then a quotient computed from it.
    constexpr auto cited =
        formula::documented(formula::constant<unit::Gram>(rat(1, 3)),
                            { .title = "Tare", .reference = "Example Standard 1:2020", .section = "4.3" })
        / var<SampleMass>;
    CHECK(renderedIn(tracedValue(cited, formula::environment(formula::Measured<SampleMass> { rat(1) })), approximately)
          == "1. 1/3 g\n"
             "2. #1 = 1/3 g [Tare, Example Standard 1:2020, 4.3]\n"
             "3. m_s = 1 g\n"
             "4. #2 / #3 = \xe2\x89\x88"
             "0.333\n");

    // A conditional whose branch that ran is a constant: its value is that
    // constant's. The measured mass it compared is rounded.
    constexpr auto chosen = formula::when(var<Mass> > formula::constant<unit::Kilogram>(rat(1, 7)),
                                          formula::constant<unit::Kilogram>(rat(1, 3)),
                                          var<Mass>);
    CHECK(renderedIn(tracedValue(chosen, formula::environment(formula::Measured<Mass> { rat(2, 7) })), approximately)
          == "1. m = \xe2\x89\x88"
             "0.286 kg\n"
             "2. 1/7 kg\n"
             "3. 1/3 kg\n"
             "4. if #1 > #2 then #3 = 1/3 kg\n");
}

TEST_CASE("a curve's typed points and values are shown as typed, and its computed ones rounded", "[trace-render][decimals]")
{
    // A declared domain paired with measured values: the points as typed,
    // the values rounded, on the domain's line and on the curve's alike.
    constexpr auto measuredCurve =
        formula::curve(formula::domain<unit::Metre, ThirdsPoints>, formula::series<PassingShare, 2>);
    CHECK(renderedIn(tracedValue(formula::interpolate_at(measuredCurve, formula::constant<unit::Metre>(rat(1, 2))),
                                 passingShares()),
                     approximately)
          == "1. 1/3 m; 2/3 m\n"
             "2. p = \xe2\x89\x88"
             "33.3 %; \xe2\x89\x88"
             "71.4 %\n"
             "3. curve(#1, #2) = 1/3 m: \xe2\x89\x88"
             "33.3 %; 2/3 m: \xe2\x89\x88"
             "71.4 %\n"
             "4. 0.5 m\n"
             "5. interpolate(#3, at #4) = \xe2\x89\x88"
             "52.4 % [between 1/3 and 2/3 m]\n");

    // Per-element constants as the values: every pair as typed, and only the
    // interpolation, and the measured aperture it was read at, rounded.
    constexpr auto typedCurve = formula::curve(formula::domain<unit::Metre, ThirdsPoints>,
                                               formula::series_constant<unit::Percent>(rat(100, 3), rat(500, 7)));
    CHECK(renderedIn(tracedValue(formula::interpolate_at(typedCurve, var<Aperture>),
                                 formula::environment(formula::Measured<Aperture> { rat(3, 7) })),
                     approximately)
          == "1. 1/3 m; 2/3 m\n"
             "2. 100/3 %; 500/7 %\n"
             "3. curve(#1, #2) = 1/3 m: 100/3 %; 2/3 m: 500/7 %\n"
             "4. a = \xe2\x89\x88"
             "0.429 m\n"
             "5. interpolate(#3, at #4) = \xe2\x89\x88"
             "44.2 % [between 1/3 and 2/3 m]\n");
}

namespace
{
/// The role another record plays, read from by `from_record`.
struct Reference
{
};

/// Halves and three quarters of a metre, whose decimals end: a domain that
/// shows its padding.
inline constexpr formula::BreakpointTable<2> HalvesPoints { formula::breakpoint(1, 2), formula::breakpoint(3, 4) };

/// Sevenths of a metre, the second domain of a splice: 1/7 m below the thirds
/// and 5/7 m above them.
inline constexpr formula::BreakpointTable<2> SeventhsPoints { formula::breakpoint(1, 7), formula::breakpoint(5, 7) };
} // namespace

TEST_CASE("a record's scope over a typed number shows it as typed, whatever the style", "[trace-render][decimals]")
{
    auto const context = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)),
                                             formula::environment(formula::Measured<Mass> { rat(2, 7) })),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)),
                                   formula::environment()));
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_si<formula::Rational>(
        formula::from_record<Reference>(formula::constant<unit::Kilogram>(rat(1, 3))) * var<Mass>,
        context,
        formula::RecordingSink<> { trace });
    CHECK(renderedIn(trace, approximately)
          == "1. 1/3 kg\n"
             "2. #1 from record Reference (sample 23, test 3) = 1/3 kg\n"
             "3. m = \xe2\x89\x88"
             "0.286 kg\n"
             "4. #2 * #3 = \xe2\x89\x88"
             "0.095 kg^2\n");
}

TEST_CASE("a step over a consumer's node that forwards the sink is not taken for its typed operand",
          "[trace-render][decimals]")
{
    // The consumer's node records no step, so the citation claims the
    // constant under it -- whose value, 1/3 m, is not the rise above 1/7 m
    // the node returns, 4/21 m, which is computed and rounded.
    constexpr auto rise = formula::documented(
        forwarding::rise_above(formula::constant<unit::Metre>(rat(1, 3)), rat(1, 7)),
        { .title = "Rise above the reference", .reference = "Example Standard 1:2020", .section = "6.5" });
    CHECK(renderedIn(tracedValue(rise, formula::environment()), approximately)
          == "1. 1/3 m\n"
             "2. #1 = \xe2\x89\x88"
             "0.19 m [Rise above the reference, Example Standard 1:2020, 6.5]\n");

    // The same for a branch that ran: the branch's step is the constant, and
    // the conditional's value is the node's.
    constexpr auto chosen = formula::when(var<Mass> > formula::constant<unit::Kilogram>(rat(1, 7)),
                                          forwarding::rise_above(formula::constant<unit::Kilogram>(rat(1, 3)), rat(1, 7)),
                                          var<Mass>);
    CHECK(renderedIn(tracedValue(chosen, formula::environment(formula::Measured<Mass> { rat(2, 7) })), approximately)
          == "1. m = \xe2\x89\x88"
             "0.286 kg\n"
             "2. 1/7 kg\n"
             "3. 1/3 kg\n"
             "4. if #1 > #2 then #3 = \xe2\x89\x88"
             "0.19 kg\n");
}

TEST_CASE("a replaced variant whose formula is a typed number shows it as typed, whatever the style",
          "[trace-render][decimals]")
{
    constexpr auto scaledMass = formula::method(
        formula::variants(formula::variant<PlainDensity>(var<Coefficient> * var<Mass>)),
        formula::rounding_rule<unit::Kilogram, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    constexpr formula::Citation annex { .reference = "Example Standard 12:2021 NA", .section = "NA.5" };
    constexpr auto replaced = formula::apply(
        formula::overlay(formula::replace_variant<PlainDensity>(formula::constant<unit::Kilogram>(rat(1, 3)), annex)),
        scaledMass);
    formula::Trace<> trace {};
    (void) formula::evaluate_method<PlainDensity>(replaced, formula::environment(), formula::RecordingSink<> { trace });
    CHECK(renderedIn(trace, approximately)
          == "1. 1/3 kg\n"
             "2. #1 = 1/3 kg [replaced by jurisdiction overlay: Example Standard 12:2021 NA, NA.5]\n"
             "3. round(#2, in kg) = 0.333 kg [rounded to 3 dp (method default); nearest, ties away from zero]\n"
             "4. #3 = 0.333 kg [variant PlainDensity (1st of 1), selected by tag]\n");
}

TEST_CASE("a precision limit's level over a typed number shows it as typed, whatever the style",
          "[trace-render][decimals]")
{
    // Pass 1 states the level expression's value, and the placeholder the
    // level it read: both the constant 1/3 kg. The limit computed from it is
    // rounded.
    constexpr auto limitAtThird = formula::precision_limit<formula::PrecisionKind::Repeatability>(
        formula::constant<unit::Kilogram>(rat(1, 3)),
        formula::constant<unit::Kilogram>(rat(1, 10)) + rat(1, 50) * formula::precision_level<Mass>);
    CHECK(renderedIn(tracedValue(limitAtThird, formula::environment()), approximately)
          == "1. 1/3 kg\n"
             "2. level (pass 1 of 2) = #1 = 1/3 kg\n"
             "3. 0.1 kg\n"
             "4. 0.02\n"
             "5. level = 1/3 kg [bound by #8]\n"
             "6. #4 * #5 = \xe2\x89\x88"
             "0.007 kg\n"
             "7. #3 + #6 = \xe2\x89\x88"
             "0.107 kg\n"
             "8. r at level #2 (pass 2 of 2) = #7 = \xe2\x89\x88"
             "0.107 kg\n");
}

TEST_CASE("a value in a unit nobody declared is never padded, a quantity in unit::One included",
          "[trace-render][decimals]")
{
    // `unit::One` is the coherent unit of a pure number, so a quantity
    // declared in it reads as a computed ratio does: 0.5, not 0.500.
    CHECK(renderedIn(tracedValue(var<Coefficient> * formula::number(rat(1, 2)),
                                 formula::environment(formula::Measured<Coefficient> { rat(1, 2) })),
                     formula::NumberStyle::exact_decimal(formula::DecimalPadding::Padded))
          == "1. c = 0.5\n"
             "2. 0.5\n"
             "3. #1 * #2 = 0.25\n");

    // A curve whose values are computed in no declared unit -- a number over
    // a series of percentages -- shows them in the coherent unit, and its
    // line pads its declared points in metres but not those values.
    constexpr auto inverseCurve =
        formula::curve(formula::domain<unit::Metre, HalvesPoints>, rat(2) / formula::series<PassingShare, 2>);
    CHECK(renderedIn(tracedValue(formula::interpolate_at(inverseCurve, formula::constant<unit::Metre>(rat(5, 8))),
                                 formula::environment(formula::measured_series<PassingShare>(
                                     formula::Measured<PassingShare> { rat(50) },
                                     formula::Measured<PassingShare> { rat(80) }))),
                     formula::NumberStyle::exact_decimal(formula::DecimalPadding::Padded))
          == "1. 0.500 m; 0.750 m\n"
             "2. 2\n"
             "3. p = 50.0 %; 80.0 %\n"
             "4. #2 / #3 = 4; 2.5\n"
             "5. curve(#1, #4) = 0.500 m: 4; 0.750 m: 2.5\n"
             "6. 0.625 m\n"
             "7. interpolate(#5, at #6) = 3.25 [between 0.500 and 0.750 m]\n");
}

TEST_CASE("a splice of typed curves shows its pairs as typed, whatever the style", "[trace-render][decimals]")
{
    constexpr auto thirds = formula::curve(formula::domain<unit::Metre, ThirdsPoints>,
                                           formula::series_constant<unit::Percent>(rat(100, 3), rat(500, 7)));
    constexpr auto sevenths = formula::curve(formula::domain<unit::Metre, SeventhsPoints>,
                                             formula::series_constant<unit::Percent>(rat(100, 7), rat(600, 7)));
    constexpr auto joined = formula::splice<formula::Monotone::NonDecreasing>(thirds, sevenths);
    formula::Trace<> trace = tracedValue(formula::interpolate_at(joined, var<Aperture>),
                                         formula::environment(formula::Measured<Aperture> { rat(3, 7) }));
    CHECK(renderedIn(trace, approximately)
          == "1. 1/3 m; 2/3 m\n"
             "2. 100/3 %; 500/7 %\n"
             "3. curve(#1, #2) = 1/3 m: 100/3 %; 2/3 m: 500/7 %\n"
             "4. 1/7 m; 5/7 m\n"
             "5. 100/7 %; 600/7 %\n"
             "6. curve(#4, #5) = 1/7 m: 100/7 %; 5/7 m: 600/7 %\n"
             "7. splice(#3, #6, non-decreasing) = 1/7 m: 100/7 %; 1/3 m: 100/3 %; 2/3 m: 500/7 %; 5/7 m: 600/7 %\n"
             "8. a = \xe2\x89\x88"
             "0.429 m\n"
             "9. interpolate(#7, at #8) = \xe2\x89\x88"
             "44.2 % [between 1/3 and 2/3 m]\n");

    // A splice whose pairs are not its curves' -- a hand-built trace -- is not
    // taken for typed: its line is spelled as a computed one is.
    formula::Trace<> forged = trace;
    forged.steps[6].elements[0] = rat(1, 11);
    CHECK(renderedIn(forged, approximately)
              .find("7. splice(#3, #6, non-decreasing) = 1/7 m: \xe2\x89\x88"
                    "9.1 %; 1/3 m: \xe2\x89\x88"
                    "33.3 %;")
          != std::string::npos);

    // Nor is a pairing whose points are not its domain's.
    formula::Trace<> forgedPairing = trace;
    forgedPairing.steps[2].domainElements[0] = rat(1, 7);
    CHECK(renderedIn(forgedPairing, approximately).find("3. curve(#1, #2) = \xe2\x89\x88" "0.143 m: 100/3 %;")
          != std::string::npos);
}

TEST_CASE("a precision limit whose limit is a typed number shows it as typed, whatever the style",
          "[trace-render][decimals]")
{
    // Pass 2 states its limit expression's value, the constant 1/7 kg, and
    // the product computed from it is rounded. Both passes read in the
    // coherent unit here: no placeholder names a quantity whose unit the
    // level could borrow.
    constexpr auto limitOfSeventh =
        formula::precision_limit<formula::PrecisionKind::Repeatability>(formula::constant<unit::Kilogram>(rat(1, 3)),
                                                                        formula::constant<unit::Kilogram>(rat(1, 7)))
        * var<Mass>;
    formula::Trace<> const trace =
        tracedValue(limitOfSeventh, formula::environment(formula::Measured<Mass> { rat(2, 7) }));
    CHECK(renderedIn(trace, approximately)
          == "1. 1/3 kg\n"
             "2. level (pass 1 of 2) = #1 = 1/3 kg\n"
             "3. 1/7 kg\n"
             "4. r at level #2 (pass 2 of 2) = #3 = 1/7 kg\n"
             "5. m = \xe2\x89\x88"
             "0.286 kg\n"
             "6. #4 * #5 = \xe2\x89\x88"
             "0.041 kg^2\n");

    // A hand-built trace whose limit step does not hold the value pass 2
    // states is not taken for typed: pass 2's line is spelled as a computed
    // one is, rounded.
    formula::Trace<> forgedLimit = trace;
    forgedLimit.steps[2].value = rat(1, 11);
    CHECK(renderedIn(forgedLimit, approximately).find("4. r at level #2 (pass 2 of 2) = #3 = \xe2\x89\x88" "0.143 kg\n")
          != std::string::npos);
    // Nor one whose pass 2 states a value its limit step does not hold.
    formula::Trace<> forgedPass = trace;
    forgedPass.steps[3].value = rat(1, 11);
    CHECK(renderedIn(forgedPass, approximately).find("4. r at level #2 (pass 2 of 2) = #3 = \xe2\x89\x88" "0.091 kg\n")
          != std::string::npos);
}

TEST_CASE("a trace pads a typed number in a labelled unit, where the formula states it as typed", "[trace-render][decimals]")
{
    // Under a padding style a trace states 5 kJ in kilojoules' one declared
    // decimal, as every value in that unit on its lines; the formula
    // (`RenderOptions`, render.hpp) states the 5 its author typed.
    constexpr auto work = formula::constant<unit::Kilojoule>(rat(5)) * var<Mass>;
    constexpr formula::NumberStyle padding = formula::NumberStyle::exact_decimal(formula::DecimalPadding::Padded);
    CHECK(renderedIn(tracedValue(work, formula::environment(formula::Measured<Mass> { rat(2) })), padding)
              .starts_with("1. 5.0 kJ\n"));
    CHECK(formula::render(work, formula::DefaultVocabulary {}, { .numbers = padding }) == "5 kJ * m");
}

TEST_CASE("a rounding in a unit nobody declared shows the first significant digit, not a zero", "[trace-render][decimals]")
{
    using formula::detail::checked_shown_text;
    constexpr formula::NumberStyle rounded = formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven);
    constexpr formula::NumberStyle ceiling = formula::NumberStyle::approximate_decimal(formula::RoundingMode::Ceiling);

    // A price of 3401/9300 EUR/kWh is 3401/33480000000 in the coherent unit of
    // euros per energy, euros per joule, about 1.0158e-7. The default 3 places
    // round it to 0; extended to its first digit, 7 places, it reads
    // ≈0.0000001, still marked as rounded.
    constexpr formula::Unit perJoule = formula::coherent(formula::base_dimension("EUR") / formula::dim::Energy);
    constexpr formula::Rational tariff { 3401, 33'480'000'000 };
    CHECK(checked_shown_text(tariff, rounded, perJoule).value() == "\xe2\x89\x88" "0.0000001");
    CHECK(checked_shown_text(tariff, rounded, formula::unit::One).value() == "\xe2\x89\x88" "0.0000001");
    // The first significant digit, not the first place a rounding leaves
    // something at: 1/11250000, about 8.9e-8, rounds up to 0.0000001 at 7
    // places, and reads its digit at 8.
    CHECK(checked_shown_text(formula::Rational { 1, 11'250'000 }, rounded, formula::unit::One).value()
          == "\xe2\x89\x88" "0.00000009");
    // A negative value alike: -1/30000000 shows its first digit at 8 places.
    CHECK(checked_shown_text(formula::Rational { -1, 30'000'000 }, rounded, formula::unit::One).value()
          == "\xe2\x89\x88" "-0.00000003");
    // At its first digit's place the value is rounded in the style's own
    // mode, and a carry is written as short as it is: 1/10000001, just under
    // 1e-7, has its first digit at 8 places, where half-even rounds it up to
    // 0.0000001, not 0.00000010.
    CHECK(checked_shown_text(formula::Rational { 1, 10'000'001 }, rounded, formula::unit::One).value()
          == "\xe2\x89\x88" "0.0000001");
    // A mode that rounds toward zero at 3 places is extended too: 1/11250000
    // and the tariff round down at their first digits, and the negative
    // tariff rounds up at its first.
    constexpr formula::NumberStyle flooring = formula::NumberStyle::approximate_decimal(formula::RoundingMode::Floor);
    CHECK(checked_shown_text(formula::Rational { 1, 11'250'000 }, flooring, formula::unit::One).value()
          == "\xe2\x89\x88" "0.00000008");
    CHECK(checked_shown_text(tariff, flooring, formula::unit::One).value() == "\xe2\x89\x88" "0.0000001");
    CHECK(checked_shown_text(-tariff, ceiling, formula::unit::One).value() == "\xe2\x89\x88" "-0.0000001");
    // The 18th place is the last one looked at, and a digit there is shown.
    CHECK(checked_shown_text(formula::Rational { 1, 300'000'000'000'000'000 }, rounded, formula::unit::One).value()
          == "\xe2\x89\x88" "0.000000000000000003");
    // Padding a unit nobody declared stays off at the places extended to.
    CHECK(checked_shown_text(tariff,
                             formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven,
                                                                       formula::DecimalPadding::Padded),
                             perJoule)
              .value()
          == "\xe2\x89\x88" "0.0000001");

    // A value that shows a digit at 3 places keeps them: 1/12 is ≈0.083.
    CHECK(checked_shown_text(formula::Rational { 1, 12 }, rounded, formula::unit::One).value() == "\xe2\x89\x88" "0.083");
    // So does one a rounding mode takes away from zero: the tariff rounded up
    // at 3 places is 0.001, which is no zero.
    CHECK(checked_shown_text(tariff, ceiling, formula::unit::One).value() == "\xe2\x89\x88" "0.001");
    // One that rounds to zero even at 18 places reads ≈0.
    CHECK(checked_shown_text(formula::Rational { 1, 4'000'000'000'000'000'000 }, rounded, formula::unit::One).value()
          == "\xe2\x89\x88" "0");
    // A unit someone declared keeps its declared places, whatever they round
    // to: 1/300000 g at the gram's one decimal.
    CHECK(checked_shown_text(formula::Rational { 1, 300'000 }, rounded, formula::unit::Gram).value()
          == "\xe2\x89\x88" "0");
    // No other style rounds, so none extends: the tariff has no exact decimal.
    CHECK(checked_shown_text(tariff, formula::NumberStyle::exact_decimal(), perJoule).value() == "3401/33480000000");
}
