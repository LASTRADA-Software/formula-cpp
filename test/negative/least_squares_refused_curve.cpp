// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this curve pairs a domain and values of different lengths
// REJECT: RequireResultDimension
// REJECT: output_dimensions
// REJECT: compute cannot be called
//
// A fit over a curve that was refused -- two points and three values -- with
// its slope evaluated: the curve's one message, and nothing from the fit, its
// output or the evaluation over the refused curve's stand-in dimensions.
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
    formula::curve(formula::series<Elapsed, 2>, formula::series<Length, 3>), { .reference = "Example Standard 12" });

int main()
{
    return formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(fit), fitPoints).has_value() ? 0 : 1;
}