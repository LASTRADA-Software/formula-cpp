// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a bound formula is not an operand; use its .expression
// REJECT: no matching
// REJECT: invalid operands
// REJECT: no match for
// REJECT: RequireResultDimension
//
// A formula bound to the road gradient, used as an operand of another
// formula, then evaluated, rendered and documented: refused once, in this
// library's words, and the refused value asks nothing more of any of the
// three. The formula it holds, .expression, is the operand to use.
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>

using Rise = formula::Quantity<struct RiseTag, "h", "height gained", formula::unit::Millimetre>;
using Run = formula::Quantity<struct RunTag, "L", "horizontal distance covered", formula::unit::Millimetre>;
using Gradient = formula::Quantity<struct GradientTag, "s", "road gradient", formula::unit::One>;

inline constexpr auto inputs =
    formula::environment(formula::Measured<Rise> { 163 }, formula::Measured<Run> { 307 });

int main()
{
    constexpr auto ratio = formula::yields<Gradient>(formula::var<Rise> / formula::var<Run>);
    constexpr auto misused = formula::var<Rise> * ratio;
    auto const shown = formula::render(misused);
    auto const written = formula::document(misused);
    return formula::checked_evaluate<Rise>(misused, inputs).has_value() && !shown.empty() && !written.formula.empty()
               ? 0
               : 1;
}
