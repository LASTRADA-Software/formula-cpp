// SPDX-License-Identifier: Apache-2.0
// EXPECT: rounded_elementwise rounds each element of a series, and this is a single value, not a series
// REJECT: no matching
// REJECT: this expression is a series, not a single value
//
// A per-element rounding given a single value: there are no elements to
// round one by one. Refused in this library's words, as sum and cumulative
// are, through an overload that takes a Node for no other purpose; the
// refused series it returns draws no second message from
// checked_evaluate_series.
#include <formula-cpp/series.hpp>

struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};

inline constexpr formula::PlacesTable<1> places { formula::DecimalPlaces { 0 } };
inline constexpr auto inputs = formula::environment(formula::Measured<TotalMass> { formula::Rational { 1249 } });

int main()
{
    return formula::checked_evaluate_series<TotalMass>(
               formula::rounded_elementwise<formula::unit::Gram, places, formula::RoundingMode::HalfEven>(
                   formula::var<TotalMass>),
               inputs)
                   .has_value()
               ? 0
               : 1;
}
