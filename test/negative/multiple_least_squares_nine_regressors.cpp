// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: multiple_least_squares fits at most 8 regressors
// REJECT: reads each regressor
// REJECT: has no output of that name
// REJECT: constraints not satisfied
// REJECT: MultipleLeastSquares<9>
//
// Nine distinct observed quantities as regressors: refused once, and not also as
// operands that are not observations, a missing output, or the operation's own
// constraint on K.
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
struct Factor1: formula::Quantity<Factor1, "f_1", "an invented factor", formula::unit::One>
{
};
struct Factor2: formula::Quantity<Factor2, "f_2", "an invented factor", formula::unit::One>
{
};
struct Factor3: formula::Quantity<Factor3, "f_3", "an invented factor", formula::unit::One>
{
};
struct Factor4: formula::Quantity<Factor4, "f_4", "an invented factor", formula::unit::One>
{
};
struct Factor5: formula::Quantity<Factor5, "f_5", "an invented factor", formula::unit::One>
{
};
struct Factor6: formula::Quantity<Factor6, "f_6", "an invented factor", formula::unit::One>
{
};
struct Factor7: formula::Quantity<Factor7, "f_7", "an invented factor", formula::unit::One>
{
};
struct Factor8: formula::Quantity<Factor8, "f_8", "an invented factor", formula::unit::One>
{
};
struct Factor9: formula::Quantity<Factor9, "f_9", "an invented factor", formula::unit::One>
{
};

inline constexpr auto sample = formula::environment(formula::MeasuredObservations<Temperature, 8>(),
                                                     formula::MeasuredObservations<Content, 8>(),
                                                     formula::MeasuredObservations<Length, 8>());
inline constexpr auto fit = formula::multiple_least_squares(
    formula::regressors(formula::observations<Factor1, 8>, formula::observations<Factor2, 8>,
                        formula::observations<Factor3, 8>, formula::observations<Factor4, 8>,
                        formula::observations<Factor5, 8>, formula::observations<Factor6, 8>,
                        formula::observations<Factor7, 8>, formula::observations<Factor8, 8>,
                        formula::observations<Factor9, 8>),
    formula::observations<Length, 8>, { .reference = "Example Standard 12" });

int main()
{
    auto const rSquared = formula::checked_evaluate_si(formula::opaque_output<"r squared">(fit), sample);
    auto const firstCoefficient = formula::checked_evaluate_si(formula::opaque_output<"coefficient 1">(fit), sample);
    return firstCoefficient.has_value() && rSquared.has_value() ? 0 : 1;
}
