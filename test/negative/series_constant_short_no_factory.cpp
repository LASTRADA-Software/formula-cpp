// SPDX-License-Identifier: Apache-2.0
// EXPECT: this series constant was given a different number of elements than its length
//
// The node built by aggregate initialisation, with no factory call: two values
// for a constant of five. `Elements<N>` is the member's own type, so the short
// braced list reaches its wrong-count constructor and is refused naming both
// counts, where a raw std::array member would have padded three zeros.
#include <formula-cpp/series.hpp>

inline constexpr formula::SeriesConstantNode<formula::unit::One, 5> factors {
    {}, { formula::Rational { 1 }, formula::Rational { 2 } }
};

int main()
{
    return static_cast<int>(decltype(factors)::length);
}
