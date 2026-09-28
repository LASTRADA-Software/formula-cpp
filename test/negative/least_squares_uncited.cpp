// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: linear_least_squares needs a citation, the reason the method fits a line here; pass {} when it gives none
// REJECT: no matching overloaded function
// REJECT: no matching function
// REJECT: RequireResultDimension
//
// A fit with no citation at all: refused in this library's words, which say
// that {} is allowed, and not as a call that matches nothing. The slope is
// taken and evaluated, and asks nothing again.
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
inline constexpr auto fit =
    formula::linear_least_squares(formula::curve(formula::series<Elapsed, 2>, formula::series<Length, 2>));

int main()
{
    return formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(fit), fitPoints).has_value() ? 0 : 1;
}