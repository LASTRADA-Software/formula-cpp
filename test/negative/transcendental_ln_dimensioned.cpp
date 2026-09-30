// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the argument of this logarithm or exponential is not dimensionless
// A logarithm or an exponential of a length: ln(2 m) is not a number, it changes with the unit.
// One mistake, one message.
// This must not compile.

#include <formula-cpp/formula.hpp>

namespace
{
struct Height: formula::Quantity<Height, "h", "an invented height", formula::unit::Metre>
{
};
} // namespace

int main()
{
    constexpr auto logarithm = formula::ln(formula::var<Height>);
    return decltype(logarithm)::dimension == formula::dim::Scalar ? 0 : 1;
}
