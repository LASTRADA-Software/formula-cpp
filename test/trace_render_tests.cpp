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
