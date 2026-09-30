// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the argument of this logarithm or exponential is not dimensionless
// A rounded logarithm of a length: the plain node's dimension check fires once, through the factory.
// One mistake, one message; the rounding node names no unit, so nothing else can fire.
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
    constexpr auto logarithm =
        formula::rounded_ln<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(formula::var<Height>);
    return decltype(logarithm)::dimension == formula::dim::Scalar ? 0 : 1;
}
