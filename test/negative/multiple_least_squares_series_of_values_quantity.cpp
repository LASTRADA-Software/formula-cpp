// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: multiple_least_squares reads each regressor and the values as raw observations
// REJECT: reads one quantity twice
// REJECT: has no output of that name
// REJECT: passes an input of another shape
//
// A series of the very quantity the values observe, read as a regressor: one
// mistake, refused once as a set that is not raw observations, and not also as
// a repeated quantity -- the check for that is off for operands that are not
// observations -- a missing output or an operation over inputs of another shape.
#include <formula-cpp/least_squares.hpp>

struct Temperature: formula::Quantity<Temperature, "T", "an invented temperature", formula::unit::Celsius>
{
};
struct Content: formula::Quantity<Content, "w", "an invented content", formula::unit::Percent>
{
};
struct Length: formula::Quantity<Length, "L", "an invented length", formula::unit::Millimetre>
{
};

inline constexpr auto sample = formula::environment(formula::MeasuredObservations<Temperature, 8>(),
                                                     formula::MeasuredObservations<Content, 8>(),
                                                     formula::MeasuredObservations<Length, 8>());
inline constexpr auto fit = formula::multiple_least_squares(
    formula::regressors(formula::series<Length, 6>, formula::observations<Content, 8>),
    formula::observations<Length, 8>, { .reference = "Example Standard 12" });

int main()
{
    auto const rSquared = formula::checked_evaluate_si(formula::opaque_output<"r squared">(fit), sample);
    auto const firstCoefficient = formula::checked_evaluate_si(formula::opaque_output<"coefficient 1">(fit), sample);
    return firstCoefficient.has_value() && rSquared.has_value() ? 0 : 1;
}
