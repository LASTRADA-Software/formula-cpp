// SPDX-License-Identifier: Apache-2.0
// EXPECT: this binning declares no classes
// REJECT: this binning's key unit
//
// A binning into no classes: every observation would miss. Refused once,
// with the key check -- a gram key for sizes in metres -- gated off. The
// empty count series evaluates without a further message on every compiler
// measured, so nothing else is rejected here.
#include <formula-cpp/binning.hpp>

struct Size: formula::Quantity<Size, "d", "particle size", formula::unit::Metre>
{
};
struct Count: formula::Quantity<Count, "n", "particles in a class", formula::unit::One>
{
};
inline constexpr auto sized = formula::environment(
    formula::MeasuredObservations<Size, 3>(formula::Rational { 4 }, formula::Rational { 11 }, formula::Rational { 47 }));

inline constexpr formula::BandTable<0> noClasses {};

inline constexpr auto counted = formula::binned<formula::unit::Gram, noClasses>(formula::observations<Size, 3>);

int main()
{
    return formula::checked_evaluate_series_si(counted, sized).has_value() ? 0 : 1;
}
