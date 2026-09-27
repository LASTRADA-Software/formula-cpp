// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a curve can only be evaluated with Rep = Rational
//
// `checked_evaluate_si<double>` on an interpolation along a curve. Ordering
// its points, locating a point between two and judging a splice's direction
// all compare, and a comparison a few ULPs off picks the wrong answer
// silently. Refused once -- the curve beneath is not evaluated at all, so it
// adds no second message. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
    struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
    {
    };

    inline constexpr formula::BreakpointTable<2> Points { formula::breakpoint(103), formula::breakpoint(127) };

    inline constexpr auto node = formula::interpolate_at(
        formula::curve(formula::domain<formula::unit::Metre, Points>(), formula::series<Passing, 2>),
        formula::constant<formula::unit::Metre>(formula::Rational { 113 }));
} // namespace

int main()
{
    auto const environment = formula::environment(formula::measured_series<Passing>(
        formula::Measured<Passing> { formula::Rational { 1 } }, formula::Measured<Passing> { formula::Rational { 2 } }));
    auto const computed = formula::checked_evaluate_si<double>(node, environment);
    return computed.has_value() ? 0 : 1;
}
