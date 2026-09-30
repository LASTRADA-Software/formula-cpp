// SPDX-License-Identifier: Apache-2.0
// EXPECT: rounded_elementwise rounds each element of a series, and this is a single value, not a series
// REJECT: no matching
// REJECT: this expression is a series, not a single value
//
// A per-element rounding, named once as a DecimalRounding, given a single
// value: refused in the same words as the three-argument spelling, through the
// overload that takes a Node for no other purpose, and not by an overload list.
#include <formula-cpp/series.hpp>

struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};

inline constexpr formula::DecimalRounding wholeGrams { formula::unit::Gram,
                                                       formula::DecimalPlaces { 0 },
                                                       formula::RoundingMode::HalfEven };
inline constexpr auto inputs = formula::environment(formula::Measured<TotalMass> { formula::Rational { 1249 } });

int main()
{
    return formula::checked_evaluate_series<TotalMass>(formula::rounded_elementwise<wholeGrams>(formula::var<TotalMass>),
                                                       inputs)
                   .has_value()
               ? 0
               : 1;
}
