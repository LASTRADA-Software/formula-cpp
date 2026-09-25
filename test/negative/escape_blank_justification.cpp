// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: numeric_value_of requires a justification saying why this rule is stated
//
// A justification of nothing but spaces satisfies "carries a justification"
// and says nothing at all, which defeats this escape hatch exactly as
// thoroughly as an empty one. The first version of the check counted bytes
// rather than content, so this compiled; a reviewer found it by trying it.
//
// The same applies to a tab, a newline, or a NUL -- `FixedString` is
// byte-oriented and will carry any of them. This case pins the one a person
// would actually type.
#include <formula-cpp/escape.hpp>

struct Strength: formula::Quantity<Strength, "f", "material strength", formula::unit::Megapascal>
{
};

int main()
{
    constexpr auto broken = formula::numeric_value_of<formula::unit::Megapascal, "   ">(formula::var<Strength>);
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
