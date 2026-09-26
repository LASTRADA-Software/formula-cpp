// SPDX-License-Identifier: Apache-2.0
// EXPECT: this series constant was given a different number of elements than its length
//
// A per-element constant whose length is stated, five, given three values.
// Stating the length ties the constant to the method's domain; a value
// forgotten is refused naming both counts, never padded with zeros.
#include <formula-cpp/series.hpp>

inline constexpr auto factors = formula::series_constant<formula::unit::One, 5>(
    formula::Rational { 1 }, formula::Rational { 2 }, formula::Rational { 3 });

int main()
{
    return static_cast<int>(decltype(factors)::length);
}
