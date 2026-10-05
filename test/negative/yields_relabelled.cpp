// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this formula names its result quantity with yields
// REJECT: no matching
//
// A formula bound to the road gradient, evaluated for another
// dimensionless quantity. The dimensions agree, so nothing else could refuse
// it: the formula says which quantity it computes, and the call names
// another. Refused by the overload that takes a bound formula, in this
// library's words, rather than as an overload nobody matched.
#include <formula-cpp/formula.hpp>

struct Rise: formula::Quantity<Rise, "h", "height gained", formula::unit::Millimetre>
{
};
struct Run: formula::Quantity<Run, "L", "horizontal distance covered", formula::unit::Millimetre>
{
};
struct Gradient: formula::Quantity<Gradient, "s", "road gradient", formula::unit::One>
{
};
struct Efficiency: formula::Quantity<Efficiency, "eta", "drivetrain efficiency", formula::unit::One>
{
};

inline constexpr auto ratio = formula::yields<Gradient>(formula::var<Rise> / formula::var<Run>);
inline constexpr auto inputs =
    formula::environment(formula::Measured<Rise> { 163 }, formula::Measured<Run> { 307 });

int main()
{
    return formula::checked_evaluate<Efficiency>(ratio, inputs).has_value() ? 0 : 1;
}
