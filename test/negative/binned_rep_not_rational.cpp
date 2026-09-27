// SPDX-License-Identifier: Apache-2.0
// EXPECT: a binning can only be evaluated with Rep = Rational
//
// A binning evaluated in double: refused in the library's words.
#include <formula-cpp/binning.hpp>

struct Size: formula::Quantity<Size, "d", "particle size", formula::unit::Metre>
{
};
inline constexpr formula::BandTable<3> sizeClasses { formula::band(0, 1, 11, 1),
                                                     formula::band(11, 1, 29, 1),
                                                     formula::band(29, 1, 83, 1) };
inline constexpr auto sized = formula::environment(
    formula::MeasuredObservations<Size, 3>(formula::Rational { 4 }, formula::Rational { 11 }, formula::Rational { 47 }));

inline constexpr auto counted = formula::binned<formula::unit::Metre, sizeClasses>(formula::observations<Size, 3>);

int main()
{
    return formula::checked_evaluate_series_si<double>(counted, sized).has_value() ? 0 : 1;
}
