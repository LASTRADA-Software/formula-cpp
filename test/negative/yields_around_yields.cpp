// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this formula is bound to its result quantity already
// REJECT: no viable conversion
// REJECT: no matching
//
// A formula bound to the water/cement ratio, bound again to an air content.
// A bound formula is the top of a formula, not a part of one, so the second
// binding is refused where it is written, once. Evaluated, traced, defined,
// rendered and documented, it adds nothing: each verb given the refused
// binding asks nothing more of it. Without that, each verb would forward to
// the inner binding, whose answer is for the water/cement ratio, where an
// air content was promised.
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>

using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", formula::unit::Litre>;
using CementVolume = formula::Quantity<struct CementVolumeTag, "V_c", "cement content", formula::unit::Litre>;
using WaterCementRatio = formula::Quantity<struct RatioTag, "w/c", "ratio of water to cement", formula::unit::One>;
using AirContent = formula::Quantity<struct AirTag, "a", "air content", formula::unit::One>;

inline constexpr auto inputs =
    formula::environment(formula::Measured<WaterVolume> { 163 }, formula::Measured<CementVolume> { 307 });

int main()
{
    constexpr auto ratio = formula::yields<WaterCementRatio>(formula::var<WaterVolume> / formula::var<CementVolume>);
    constexpr auto rebound = formula::yields<AirContent>(ratio);
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
