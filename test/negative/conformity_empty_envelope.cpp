// SPDX-License-Identifier: Apache-2.0
// EXPECT: this envelope was given a different number of rows than its series has elements
// REJECT: no matching
// REJECT: could not convert
//
// An envelope written as {}: no row for any of the five points. Refused naming
// both counts, zero and five, as every other wrong count is -- never five rows
// nobody wrote.
#include <formula-cpp/conformity.hpp>

struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
{
};

inline constexpr auto check =
    formula::conformity<formula::unit::Percent>(formula::series<Passing, 5>, {}, formula::Verdict { "reject the specimen" });

int main()
{
    return static_cast<int>(decltype(check)::length);
}
