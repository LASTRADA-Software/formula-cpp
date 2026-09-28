// SPDX-License-Identifier: Apache-2.0
// EXPECT: this lookup table was given a different number of corrections than it has rows
//
// Five sizes and four values, through the factory. A valid table, so the
// arity check is live: the forgotten row would otherwise read 0.
// This must not compile.
#include <formula-cpp/critical_value.hpp>

namespace
{
struct Specimens: formula::Quantity<Specimens, "n", "number of determinations", formula::unit::One>
{
};

inline constexpr formula::SampleSizeTable<5> Sizes { 3, 4, 5, 6, 8 };
} // namespace

int main()
{
    constexpr auto broken = formula::critical_value<Sizes, formula::unit::One>(
        formula::var<Specimens>,
        { formula::Rational { 10 }, formula::Rational { 30 }, formula::Rational { 20 }, formula::Rational { 50 } });
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
