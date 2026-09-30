// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this result quantity does not measure the dimension this expression computes
//
// The trace of a dimensionless ratio asked for as a length: refused in
// checked_evaluate's words, once, although trace_of reaches it through a
// lambda that traced() both names in its return type and runs.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

int main()
{
    auto const inputs = formula::environment(formula::Measured<WaterVolume> { 163 }, formula::Measured<CementVolume> { 307 });
    // The expression is dimensionless; `Length` is not.
    auto const recorded = formula::trace_of<Length>(formula::var<WaterVolume> / formula::var<CementVolume>, inputs);
    return recorded.empty() ? 1 : 0;
}
