// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a checked_transform callback must be callable with a Rational
//
// A checked_transform callback that cannot be called with a Rational at all is refused in the library's own words,
// not with the standard library's complaint about a missing invoke_result type. This must not compile.
#include <formula-cpp/measured.hpp>

#include <string_view>

struct Reading: formula::Quantity<Reading, "V", "a volume reading", formula::unit::Litre>
{
};

int main()
{
    auto const uncallable = [](std::string_view spelled) {
        return std::expected<formula::Rational, formula::ArithmeticError> { formula::Rational { spelled.size() } };
    };
    auto const refused =
        formula::checked_transform(formula::Measured<Reading> { formula::Rational { 1 } }, uncallable);
    return refused.has_value() ? 0 : 1;
}
