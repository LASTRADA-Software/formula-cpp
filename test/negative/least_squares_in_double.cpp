// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a curve can only be evaluated with Rep = Rational
// REJECT: must be noexcept
// REJECT: compute cannot be called
//
// A fit evaluated in double: its input is a curve, and a curve compares, so
// it is refused once, in the curve's own words (plan, C3). The opaque call's
// own checks for double pass -- LinearLeastSquares::compute is generic over
// Rep -- and add nothing.
#include <formula-cpp/least_squares.hpp>

struct Elapsed: formula::Quantity<Elapsed, "t", "an invented elapsed time", formula::unit::Second>
{
};
struct Length: formula::Quantity<Length, "L", "an invented length", formula::unit::Millimetre>
{
};

inline constexpr auto fit = formula::linear_least_squares(
    formula::curve(formula::series<Elapsed, 2>, formula::series<Length, 2>), { .reference = "Example Standard 12" });

inline constexpr auto fitPoints =
    formula::environment(formula::measured_series<Elapsed>(formula::Measured<Elapsed> { formula::Rational { 1 } },
                                                           formula::Measured<Elapsed> { formula::Rational { 3 } }),
                         formula::measured_series<Length>(formula::Measured<Length> { formula::Rational { 103, 10 } },
                                                          formula::Measured<Length> { formula::Rational { 139, 10 } }));

int main()
{
    return formula::checked_evaluate_si<double>(formula::opaque_output<"slope">(fit), fitPoints).has_value() ? 0 : 1;
}