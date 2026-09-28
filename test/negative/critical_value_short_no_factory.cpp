// SPDX-License-Identifier: Apache-2.0
// EXPECT: this lookup table was given a different number of corrections than it has rows
//
// The same mistake as `critical_value_short.cpp`, on the route it cannot
// reach: the node aggregate-initialised directly, with no factory call. The
// member itself is `Corrections<K>` for a valid table, so the short list is
// refused here too. This must not compile.
#include <formula-cpp/critical_value.hpp>

namespace
{
struct Specimens: formula::Quantity<Specimens, "n", "number of determinations", formula::unit::One>
{
};

inline constexpr formula::SampleSizeTable<5> Sizes { 3, 4, 5, 6, 8 };

inline constexpr formula::SampleSizeLookupNode<Sizes, formula::unit::One, formula::VarNode<Specimens>> broken {
    {},
    { formula::Rational { 10 }, formula::Rational { 30 }, formula::Rational { 20 }, formula::Rational { 50 } },
    formula::var<Specimens>
};
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
