// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: numeric_value_of requires a justification saying why this rule is stated
//
// An empty justification defeats the only safeguard numeric_value_of has: a
// caller could otherwise satisfy "must carry a justification" with nothing at
// all. This must not compile.
#include <formula-cpp/escape.hpp>

struct Strength: formula::Quantity<Strength, "f", "material strength", formula::unit::Megapascal>
{
};

int main()
{
    constexpr auto broken = formula::numeric_value_of<formula::unit::Megapascal, "">(formula::var<Strength>);
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
