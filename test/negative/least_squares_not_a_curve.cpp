// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: linear_least_squares fits a curve; pair the domain and the values with curve(domain, values)
// REJECT: no matching overloaded function
// REJECT: no matching function
// REJECT: RequireResultDimension
//
// Two loose series handed to the fit: a curve already pairs a domain with
// values of one length, which two series would have to re-derive. Refused
// once, in this library's words, and not in the compiler's -- also when the
// fit's slope is taken and evaluated next, as anyone writing a fit does: the
// refusal returns a fit of a refused curve, whose outputs ask nothing again.
#include <formula-cpp/least_squares.hpp>

struct Elapsed: formula::Quantity<Elapsed, "t", "an invented elapsed time", formula::unit::Second>
{
};
struct Length: formula::Quantity<Length, "L", "an invented length", formula::unit::Millimetre>
{
};
struct Rate: formula::Quantity<Rate, "v", "an invented rate of change", formula::unit::MillimetrePerMinute>
{
};

inline constexpr auto fitPoints =
    formula::environment(formula::measured_series<Elapsed>(formula::Measured<Elapsed> { formula::Rational { 1 } },
                                                           formula::Measured<Elapsed> { formula::Rational { 3 } }),
                         formula::measured_series<Length>(formula::Measured<Length> { formula::Rational { 103, 10 } },
                                                          formula::Measured<Length> { formula::Rational { 139, 10 } }));
inline constexpr auto fit = formula::linear_least_squares(
    formula::series<Elapsed, 2>, formula::series<Length, 2>, { .reference = "Example Standard 12" });

int main()
{
    return formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(fit), fitPoints).has_value() ? 0 : 1;
}