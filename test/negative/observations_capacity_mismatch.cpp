// SPDX-License-Identifier: Apache-2.0
// EXPECT: this environment holds observations of a different capacity for this quantity than the one read
// REJECT: provides no value for this quantity
// REJECT: not raw observations
// REJECT: cannot convert
// REJECT: no viable conversion
// REJECT: could not convert
//
// Observations supplied with a capacity of three and read with one of
// five: refused once, naming both.
#include <formula-cpp/binning.hpp>

struct Size: formula::Quantity<Size, "d", "particle size", formula::unit::Metre>
{
};
struct Count: formula::Quantity<Count, "n", "particles in a class", formula::unit::One>
{
};
inline constexpr formula::BandTable<3> sizeClasses { formula::band(0, 1, 11, 1),
                                                     formula::band(11, 1, 29, 1),
                                                     formula::band(29, 1, 83, 1) };
inline constexpr auto sized = formula::environment(
    formula::MeasuredObservations<Size, 3>(formula::Rational { 4 }, formula::Rational { 11 }, formula::Rational { 47 }));

inline constexpr auto counted = formula::binned<formula::unit::Metre, sizeClasses>(formula::observations<Size, 5>);

int main()
{
    return formula::checked_evaluate_series<Count>(counted, sized).has_value() ? 0 : 1;
}
