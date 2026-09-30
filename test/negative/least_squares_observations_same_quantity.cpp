// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: linear_least_squares reads one quantity as both its points and its values
// REJECT: RequireResultDimension
// REJECT: has no output of that name
// REJECT: passes an input of another shape
//
// One quantity read as both the points and the values of a line: a line
// through every observation against itself says nothing. Refused once, and
// not also as an operation over inputs of another shape or a wrong result.
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
struct Determination: formula::Quantity<Determination, "R2", "an invented coefficient of determination", formula::unit::One>
{
};

inline constexpr auto twoObserved = formula::environment(
    formula::MeasuredObservations<Elapsed, 4>(formula::Rational { 1 }, formula::Rational { 3 }),
    formula::MeasuredObservations<Length, 4>(formula::Rational { 103, 10 }, formula::Rational { 139, 10 }));
inline constexpr auto fit = formula::linear_least_squares(
    formula::observations<Length, 4>, formula::observations<Length, 4>, { .reference = "Example Standard 12" });

int main()
{
    auto const rSquared = formula::checked_evaluate<Determination>(formula::opaque_output<"r squared">(fit), twoObserved);
    return formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(fit), twoObserved).has_value()
                   && rSquared.has_value()
               ? 0
               : 1;
}
