// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this formula is bound to its result quantity already
// REJECT: no viable conversion
// REJECT: no matching
//
// A formula bound to the road gradient, bound again to an efficiency.
// A bound formula is the top of a formula, not a part of one, so the second
// binding is refused where it is written, once. Evaluated, traced, defined,
// rendered and documented, it adds nothing: each verb given the refused
// binding asks nothing more of it. Without that, each verb would forward to
// the inner binding, whose answer is for the road gradient, where an
// efficiency was promised.
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>

using Rise = formula::Quantity<struct RiseTag, "h", "height gained", formula::unit::Millimetre>;
using Run = formula::Quantity<struct RunTag, "L", "horizontal distance covered", formula::unit::Millimetre>;
using Gradient = formula::Quantity<struct GradientTag, "s", "road gradient", formula::unit::One>;
using Efficiency = formula::Quantity<struct EfficiencyTag, "eta", "drivetrain efficiency", formula::unit::One>;

inline constexpr auto inputs =
    formula::environment(formula::Measured<Rise> { 163 }, formula::Measured<Run> { 307 });

int main()
{
    constexpr auto ratio = formula::yields<Gradient>(formula::var<Rise> / formula::var<Run>);
    constexpr auto rebound = formula::yields<Efficiency>(ratio);
    auto const evaluated = formula::evaluate(rebound, inputs);
    auto const checked = formula::checked_evaluate(rebound, inputs);
    auto const explained = formula::explain(rebound, inputs);
    auto const checkedExplained = formula::checked_explain(rebound, inputs);
    auto const traced = formula::trace_of(rebound, inputs);
    auto const defined = formula::define(rebound);
    auto const written = formula::render(rebound) + formula::document(rebound).formula;
    return evaluated.is_value() && checked.has_value() && explained.outcome.is_value() && checkedExplained.has_value()
                   && !traced.empty() && decltype(defined)::valid && !written.empty()
               ? 0
               : 1;
}
