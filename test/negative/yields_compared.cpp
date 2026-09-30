// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a bound formula is not an operand; use its .expression
// REJECT: no matching
// REJECT: invalid operands
// REJECT: no match for
//
// A formula bound to the water/cement ratio, compared with a limit as a
// constraint compares, then checked, rendered and documented: refused once, in
// this library's words rather than the compiler's missing operator, and the
// refused comparison asks nothing more of any of the three. The formula it
// holds, .expression, is the one to compare.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>

using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", formula::unit::Litre>;
using CementVolume = formula::Quantity<struct CementVolumeTag, "V_c", "cement content", formula::unit::Litre>;
using WaterCementRatio = formula::Quantity<struct RatioTag, "w/c", "ratio of water to cement", formula::unit::One>;

inline constexpr auto inputs =
    formula::environment(formula::Measured<WaterVolume> { 163 }, formula::Measured<CementVolume> { 307 });

int main()
{
    using namespace formula::literals;
    constexpr auto ratio = formula::yields<WaterCementRatio>(formula::var<WaterVolume> / formula::var<CementVolume>);
    constexpr auto tooWet = formula::constraint(ratio >= formula::constant<formula::unit::One>(0.45_r),
                                                formula::Verdict { "too much water for the cement" });
    auto const shown = formula::render(tooWet);
    auto const written = formula::document(tooWet);
    return formula::check(tooWet, inputs).is_satisfied() && !shown.empty() && !written.formula.empty() ? 0 : 1;
}
