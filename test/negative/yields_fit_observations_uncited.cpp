// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: linear_least_squares needs a citation, the reason the method fits a line here; pass {} when it gives none
// REJECT: no matching overloaded function
// REJECT: no matching function
// REJECT: RequireResultDimension
// REJECT: has no output of that name
//
// A line through bound raw observations with no citation: refused once, in
// the words the observations they hold draw, and not as a missing overload,
// a missing output or a wrong result dimension.
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

inline constexpr auto twoObserved = formula::environment(
    formula::MeasuredObservations<Elapsed, 4>(formula::Rational { 1 }, formula::Rational { 3 }),
    formula::MeasuredObservations<Length, 4>(formula::Rational { 103, 10 }, formula::Rational { 139, 10 }));
inline constexpr auto times = formula::yields<Elapsed>(formula::observations<Elapsed, 4>);
inline constexpr auto lengths = formula::yields<Length>(formula::observations<Length, 4>);
inline constexpr auto fit = formula::linear_least_squares(times, lengths);

int main()
{
    return formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(fit), twoObserved).has_value() ? 0 : 1;
}
