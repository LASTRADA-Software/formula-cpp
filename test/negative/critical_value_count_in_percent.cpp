// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this critical value's count is stated in a unit other than a bare number
//
// A count of determinations declared in percent. 300 % is 3 in the coherent
// unit, so this would read the row for 3 -- at a number nobody wrote. The
// count is dimensionless, so the scalar check passes and only this one
// speaks. This must not compile.
#include <formula-cpp/critical_value.hpp>

namespace
{
struct SpecimensInPercent: formula::Quantity<SpecimensInPercent, "n_p", "number of determinations", formula::unit::Percent>
{
};

inline constexpr formula::SampleSizeTable<2> Sizes { 3, 4 };
} // namespace

int main()
{
    constexpr auto broken = formula::critical_value<Sizes, formula::unit::One>(
        formula::var<SpecimensInPercent>, { formula::Rational { 70 }, formula::Rational { 20 } });
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
