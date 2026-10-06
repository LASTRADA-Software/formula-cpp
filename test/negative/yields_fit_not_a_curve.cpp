// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: linear_least_squares fits a curve, or points and values read as observations; pair a domain series and a
// value series with curve(domain, values), or read both with observations<Q, Capacity> REJECT: no matching overloaded
// function REJECT: no matching function REJECT: RequireResultDimension
//
// A bound series handed to the fit, which fits a curve: refused once, in the
// words the series it holds draws, and not as an ambiguous or a missing
// overload -- also when the fit's slope is taken and evaluated next.
#include <formula-cpp/least_squares.hpp>

struct Length: formula::Quantity<Length, "L", "an invented length", formula::unit::Millimetre>
{
};
struct Lengths: formula::Quantity<Lengths, "L_s", "invented lengths", formula::unit::Millimetre>
{
};
struct Rate: formula::Quantity<Rate, "v", "an invented rate of change", formula::unit::MillimetrePerMinute>
{
};

inline constexpr auto fitPoints =
    formula::environment(formula::measured_series<Length>(formula::Measured<Length> { formula::Rational { 103, 10 } },
                                                          formula::Measured<Length> { formula::Rational { 139, 10 } }));
inline constexpr auto lengths = formula::yields<Lengths>(formula::series<Length, 2>);
inline constexpr auto fit = formula::linear_least_squares(lengths, { .reference = "Example Standard 12" });

int main()
{
    return formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(fit), fitPoints).has_value() ? 0 : 1;
}
