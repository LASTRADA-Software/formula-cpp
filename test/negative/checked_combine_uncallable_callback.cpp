// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a checked_combine callback must be callable with two Rationals
//
// A checked_combine callback that cannot be called with two Rationals at all is refused in the library's own words,
// not with the standard library's complaint about a missing invoke_result type. This must not compile.
#include <formula-cpp/measured.hpp>

#include <string_view>

struct Reading: formula::Quantity<Reading, "V", "a volume reading", formula::unit::Litre>
{
};

int main()
{
    auto const uncallable = [](std::string_view spelled, formula::Rational addend) {
        return formula::checked_add(formula::Rational { spelled.size() }, addend);
    };
    auto const refused = formula::checked_combine<Reading>(formula::Measured<Reading> { formula::Rational { 1 } },
                                                           formula::Measured<Reading> { formula::Rational { 2 } },
                                                           uncallable);
    return refused.has_value() ? 0 : 1;
}
