// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a checked_combine callback must return std::expected
//
// checked_combine's callback reports failure through std::expected; one returning a bare Rational belongs to
// combine. This must not compile.
#include <formula-cpp/measured.hpp>

struct Reading: formula::Quantity<Reading, "V", "a volume reading", formula::unit::Litre>
{
};

int main()
{
    auto const plain = [](formula::Rational augend, formula::Rational addend) { return augend + addend; };
    auto const refused = formula::checked_combine<Reading>(formula::Measured<Reading> { formula::Rational { 1 } },
                                                           formula::Measured<Reading> { formula::Rational { 2 } }, plain);
    return refused.has_value() ? 0 : 1;
}
