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
// The formula is the same road gradient examples/citations.cpp evaluates, with
// the same invented citation -- tracing does not change what a formula is, only
// what the evaluator is willing to tell you about how it got its answer.
//
// The run is entered in kilometres, which is not the coherent unit of a length,
// and its step in the trace shows it as entered: 3 km, not 3000 m.
//
// Every citation here is invented: naming a real standard would put copyrighted
// material in a public repository. See docs/tracing.md.

#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <print>
#include <string>

namespace
{
namespace unit = formula::unit;
using formula::var;
using namespace formula::literals;

using Rise = formula::Quantity<struct RiseTag, "h", "height gained", unit::Metre>;
using Run = formula::Quantity<struct RunTag, "L", "horizontal distance covered", unit::Kilometre>;
using Gradient = formula::Quantity<struct GradientTag, "s", "road gradient", unit::One>;

// The formula and its citation, declared together, exactly as in
// examples/citations.cpp -- documented() attaches the citation, and attaching
// it changes nothing this file evaluates or traces.
constexpr auto gradient = formula::documented(var<Rise> / var<Run>,
                                              { .title = "Road gradient",
                                                .reference = "Example Standard 1:2020",
                                                .section = "5.4.2",
                                                .equation = "(3)",
                                                .text = "Height a road gains over the horizontal distance it covers." });

} // namespace

int main()
{
    auto const inputs = formula::environment(formula::Measured<Rise> { 90 }, formula::Measured<Run> { 3 });

    // explain<Result>(expression, environment) returns exactly what
    // evaluate<Result>(expression, environment) would have -- an
    // Outcome<Result> -- plus a Trace of every step the evaluator took to
    // reach it. Tracing observes; it does not change the answer.
    auto const explained = formula::explain<Gradient>(gradient, inputs);

    // render_trace has no default for maxSteps: TraceRenderOptions::maxSteps
    // is a StepLimit, which has no default constructor, so a caller who
    // writes render_trace(explained.trace, {}) does not compile, rather than
    // risking an unbounded dump of a derivation many times this size.
    std::string const rendered = formula::render_trace(explained.trace, { .maxSteps = 10 });
    std::print("{}", rendered);

    // An Outcome may also be Empty, Verdict or Invalid, and number_of is
    // empty for each of those, so comparing it is a complete check. Nothing in
    // this program can make it anything but a value; an example is teaching
    // material, and the check costs nothing to show.
    bool const evaluatedCorrectly = formula::number_of(explained.outcome) == 0.03_r;
    // One step per node: the two variables, the division, and the documented
    // wrapper around it.
    bool const tracedCorrectly = explained.trace.steps.size() == 4;
    // The run's step states it as it was entered, in kilometres, while the
    // division beneath it worked in metres.
    bool const inputShownAsEntered = rendered.contains("2. L = 3 km\n");

    bool const allChecksPassed = evaluatedCorrectly && tracedCorrectly && inputShownAsEntered;
    std::println("all checks passed: {}", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
