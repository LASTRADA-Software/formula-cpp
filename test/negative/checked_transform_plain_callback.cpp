// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a checked_transform callback must return std::expected
//
// checked_transform's callback reports failure through std::expected; one returning a bare Rational belongs to
// transform. This must not compile.
#include <formula-cpp/measured.hpp>

struct Reading: formula::Quantity<Reading, "V", "a volume reading", formula::unit::Litre>
{
};

int main()
{
    auto const plain = [](formula::Rational gauged) { return gauged; };
    auto const refused = formula::checked_transform(formula::Measured<Reading> { formula::Rational { 1 } }, plain);
    return refused.has_value() ? 0 : 1;
}
