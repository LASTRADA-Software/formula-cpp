// SPDX-License-Identifier: Apache-2.0
// EXPECT: a conformity check judges each element of a series against its own row, and this subject is a single value
// REJECT: no matching
// REJECT: this conformity check states its limits in a unit that does not measure
//
// A single value given a conformity check: there is no element to give each
// row. Refused in this library's words, through an overload that takes a Node
// for no other purpose, rather than as a constraint nobody satisfied. The
// unit is wrong for the subject too; that check is gated off behind the
// refusal, which is the one mistake reported.
#include <formula-cpp/conformity.hpp>

struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};

using formula::limit;
using formula::LimitRow;

inline constexpr auto check = formula::conformity<formula::unit::Percent>(
    formula::var<TotalMass>,
    { LimitRow { limit(formula::Rational { 30 }), limit(formula::Rational { 40 }) } },
    formula::Verdict { "reject the specimen" });

int main()
{
    return static_cast<int>(decltype(check)::length);
}
