// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this curve is read at a point that does not measure the dimension of its domain
// A logarithm over an interpolation that is refused already, whose stand-in dimension is a length:
// one message, the interpolation's; the logarithm stays silent about an operand refused for another reason.
// This must not compile.

#include <formula-cpp/formula.hpp>

namespace
{
struct Height: formula::Quantity<Height, "h", "an invented height", formula::unit::Metre>
{
};
struct Opening: formula::Quantity<Opening, "d", "an invented screen opening", formula::unit::Metre>
{
};
} // namespace

inline constexpr auto misread = formula::interpolate_at(
    formula::curve(formula::series<Opening, 3>, formula::series<Height, 3>),
    formula::constant<formula::unit::Percent>(formula::Rational { 50 }));
inline constexpr auto logarithm = formula::ln(misread);

int main()
{
    return decltype(logarithm)::dimension == formula::dim::Scalar ? 0 : 1;
}
