// SPDX-License-Identifier: Apache-2.0
// EXPECT: a binning can only be evaluated with Rep = Rational
//
// A binning evaluated in double: refused in the library's words.
#include <formula-cpp/binning.hpp>

struct Size: formula::Quantity<Size, "d", "particle size", formula::unit::Metre>
{
};
inline constexpr formula::BandTable<3> sizeClasses { formula::band(0, 1, 127, 1),
                                                     formula::band(127, 1, 197, 1),
                                                     formula::band(197, 1, 331, 1) };
inline constexpr auto sized = formula::environment(
    formula::MeasuredObservations<Size, 3>(formula::Rational { 103 }, formula::Rational { 127 }, formula::Rational { 241 }));

inline constexpr auto counted = formula::binned<formula::unit::Metre, sizeClasses>(formula::observations<Size, 3>);

int main()
{
    return formula::checked_evaluate_series_si<double>(counted, sized).has_value() ? 0 : 1;
}
