// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this result quantity does not measure the dimension this expression computes
//
// The trace of a dimensionless gradient asked for as a length: refused in
// checked_evaluate's words, once, although trace_of reaches it through a
// lambda that traced() both names in its return type and runs.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

struct Rise: formula::Quantity<Rise, "h", "height gained", formula::unit::Millimetre>
{
};
struct Run: formula::Quantity<Run, "L", "horizontal distance covered", formula::unit::Millimetre>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

int main()
{
    auto const inputs = formula::environment(formula::Measured<Rise> { 163 }, formula::Measured<Run> { 307 });
    // The expression is dimensionless; `Length` is not.
    auto const recorded = formula::trace_of<Length>(formula::var<Rise> / formula::var<Run>, inputs);
    return recorded.empty() ? 1 : 0;
}
