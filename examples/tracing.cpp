// SPDX-License-Identifier: Apache-2.0
//
// Explaining a derivation, not just evaluating one.
//
// Every earlier example calls formula::evaluate() or formula::checked_evaluate()
// and gets back a number. This one calls formula::explain() on the same kind of
// formula and gets back the number *and* a formula::Trace: one step per node the
// evaluator visited, each naming the earlier steps it consumed, printed with
// formula::render_trace().
//
// The formula is the same water/cement ratio examples/citations.cpp evaluates,
// with the same invented citation -- tracing does not change what a formula is,
// only what the evaluator is willing to tell you about how it got its answer.
//
// Every citation here is invented: naming a real standard would put copyrighted
// material in a public repository. See docs/tracing.md.

#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <print>

namespace
{
namespace unit = formula::unit;
using formula::var;
using namespace formula::literals;

using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", unit::Litre>;
using CementVolume = formula::Quantity<struct CementVolumeTag, "V_c", "cement content", unit::Litre>;
using WaterCementRatio = formula::Quantity<struct WaterCementRatioTag, "w/c", "ratio of water to cement", unit::One>;

// The formula and its citation, declared together, exactly as in
// examples/citations.cpp -- documented() attaches the citation, and attaching
// it changes nothing this file evaluates or traces.
constexpr auto ratio = formula::documented(var<WaterVolume> / var<CementVolume>,
                                           { .title = "Water/cement ratio",
                                             .reference = "Example Standard 1:2020",
                                             .section = "5.4.2",
                                             .equation = "(3)",
                                             .text = "Ratio of water content to cement content." });

} // namespace

int main()
{
    auto const inputs = formula::environment(formula::Measured<WaterVolume> { 180 },
                                             formula::Measured<CementVolume> { 300 });

    // explain<Result>(expression, environment) returns exactly what
    // evaluate<Result>(expression, environment) would have -- an
    // Outcome<Result> -- plus a Trace of every step the evaluator took to
    // reach it. Tracing observes; it does not change the answer.
    auto const explained = formula::explain<WaterCementRatio>(ratio, inputs);

    // render_trace has no default for maxSteps: TraceRenderOptions::maxSteps
    // is a StepLimit, which has no default constructor, so a caller who
    // writes render_trace(explained.trace, {}) does not compile, rather than
    // risking an unbounded dump of a derivation many times this size.
    std::print("{}", formula::render_trace(explained.trace, { .maxSteps = 10 }));

    // An Outcome may also be Empty, Verdict or Invalid, and number_of is
    // empty for each of those, so comparing it is a complete check. Nothing in
    // this program can make it anything but a value; an example is teaching
    // material, and the check costs nothing to show.
    bool const evaluatedCorrectly = formula::number_of(explained.outcome) == 0.6_r;
    // One step per node: the two variables, the division, and the documented
    // wrapper around it.
    bool const tracedCorrectly = explained.trace.steps.size() == 4;

    bool const allChecksPassed = evaluatedCorrectly && tracedCorrectly;
    std::println("all checks passed: {}", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
