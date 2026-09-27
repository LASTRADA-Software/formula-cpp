// SPDX-License-Identifier: Apache-2.0
// EXPECT: this environment holds raw observations for this quantity, which are neither a single value nor a series
// REJECT: provides no value for this quantity
// REJECT: holds a single value for this quantity
// REJECT: cannot convert
// REJECT: no viable conversion
// REJECT: could not convert
//
// Raw observations read as a series of three: refused once -- an
// observation is at no point of a domain.
#include <formula-cpp/binning.hpp>

struct Size: formula::Quantity<Size, "d", "particle size", formula::unit::Metre>
{
};
inline constexpr auto sized = formula::environment(
    formula::MeasuredObservations<Size, 3>(formula::Rational { 103 }, formula::Rational { 127 }, formula::Rational { 241 }));

inline constexpr auto asSeries = formula::series<Size, 3>;

int main()
{
    return formula::checked_evaluate_series<Size>(asSeries, sized).has_value() ? 0 : 1;
}
