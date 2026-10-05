// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a bound formula is not an operand; use its .expression
// REJECT: no matching
// REJECT: invalid operands
// REJECT: no match for
//
// A formula bound to the road gradient, compared with a limit as a
// constraint compares, then checked, rendered and documented: refused once, in
// this library's words rather than the compiler's missing operator, and the
// refused comparison asks nothing more of any of the three. The formula it
// holds, .expression, is the one to compare.
#include <formula-cpp/constraint.hpp>
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
    using namespace formula::literals;
    constexpr auto ratio = formula::yields<Gradient>(formula::var<Rise> / formula::var<Run>);
    constexpr auto tooShallow = formula::constraint(ratio >= formula::constant<formula::unit::One>(0.45_r),
                                                    formula::Verdict { "too shallow a gradient for the climb" });
    auto const shown = formula::render(tooShallow);
    auto const written = formula::document(tooShallow);
    return formula::check(tooShallow, inputs).is_satisfied() && !shown.empty() && !written.formula.empty() ? 0 : 1;
}
