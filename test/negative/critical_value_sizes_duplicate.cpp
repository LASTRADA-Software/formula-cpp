// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this sample-size table's sizes do not strictly ascend
//
// The size 4 declared twice: the second row, and the value typed for it, could
// never be selected. This must not compile.
#include <formula-cpp/critical_value.hpp>

namespace
{
struct Specimens: formula::Quantity<Specimens, "n", "number of determinations", formula::unit::One>
{
};

inline constexpr formula::SampleSizeTable<4> Repeated { 3, 4, 4, 6 };
} // namespace

int main()
{
    constexpr auto broken = formula::critical_value<Repeated, formula::unit::One>(
        formula::var<Specimens>,
        { formula::Rational { 70 }, formula::Rational { 20 }, formula::Rational { 90 }, formula::Rational { 60 } });
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
