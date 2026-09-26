// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this sample-size table declares a size of zero
//
// A row for a sample of no determinations. The sizes do ascend, so only the
// zero is wrong, and only its message may appear. This must not compile.
#include <formula-cpp/critical_value.hpp>

namespace
{
struct Specimens: formula::Quantity<Specimens, "n", "number of determinations", formula::unit::One>
{
};

inline constexpr formula::SampleSizeTable<3> FromZero { 0, 3, 4 };
} // namespace

int main()
{
    constexpr auto broken = formula::critical_value<FromZero, formula::unit::One>(
        formula::var<Specimens>, { formula::Rational { 70 }, formula::Rational { 20 }, formula::Rational { 90 } });
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
