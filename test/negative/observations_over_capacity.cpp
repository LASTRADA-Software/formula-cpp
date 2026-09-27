// SPDX-License-Identifier: Apache-2.0
// EXPECT: more observations were listed than this set's capacity holds
//
// Three observations listed for a capacity of two: refused once, naming
// both counts, never truncated.
#include <formula-cpp/environment.hpp>

struct Size: formula::Quantity<Size, "d", "particle size", formula::unit::Metre>
{
};

inline constexpr auto overfull =
    formula::MeasuredObservations<Size, 2>(formula::Rational { 103 }, formula::Rational { 127 }, formula::Rational { 241 });

int main()
{
    return overfull.size() == 2 ? 0 : 1;
}
