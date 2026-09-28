// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this critical value's count is not dimensionless
//
// A mass where the number of determinations belongs. This must not compile.
#include <formula-cpp/critical_value.hpp>

namespace
{
struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "specimen mass", formula::unit::Gram>
{
};

inline constexpr formula::SampleSizeTable<2> Sizes { 3, 4 };
} // namespace

int main()
{
    constexpr auto broken = formula::critical_value<Sizes, formula::unit::One>(
        formula::var<SpecimenMass>, { formula::Rational { 70 }, formula::Rational { 20 } });
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
