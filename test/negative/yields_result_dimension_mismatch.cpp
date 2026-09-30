// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this result quantity does not measure the dimension this expression computes
// REJECT: formula: this definition's expression measures a different dimension
//
// A formula bound to a quantity that does not measure what it computes:
// refused where the formula is written, in checked_evaluate's words. The
// refused formula is then evaluated and defined, and neither adds a second
// message: each verb given a refused Yields asks nothing more.
#include <formula-cpp/formula.hpp>

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

inline constexpr auto inputs =
    formula::environment(formula::Measured<WaterVolume> { 163 }, formula::Measured<CementVolume> { 307 });

int main()
{
    // The expression is dimensionless; `Length` is not.
    constexpr auto refused = formula::yields<Length>(formula::var<WaterVolume> / formula::var<CementVolume>);
    auto const evaluated = formula::checked_evaluate(refused, inputs);
    auto const defined = formula::define(refused);
    return evaluated.has_value() && decltype(defined)::valid ? 0 : 1;
}
