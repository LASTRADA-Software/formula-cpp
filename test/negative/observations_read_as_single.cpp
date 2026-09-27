// SPDX-License-Identifier: Apache-2.0
// EXPECT: this environment holds raw observations for this quantity, which are neither a single value nor a series
// REJECT: provides no value for this quantity
// REJECT: holds a series for this quantity
// REJECT: cannot convert
// REJECT: no viable conversion
// REJECT: could not convert
//
// Raw observations read as one number: refused once, in words true of
// observations.
#include <formula-cpp/binning.hpp>

struct Size: formula::Quantity<Size, "d", "particle size", formula::unit::Metre>
{
};
inline constexpr auto sized = formula::environment(
    formula::MeasuredObservations<Size, 3>(formula::Rational { 103 }, formula::Rational { 127 }, formula::Rational { 241 }));

inline constexpr auto doubled = formula::var<Size> * formula::Rational { 2 };

int main()
{
    return formula::checked_evaluate<Size>(doubled, sized).has_value() ? 0 : 1;
}
