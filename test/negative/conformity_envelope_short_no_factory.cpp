// SPDX-License-Identifier: Apache-2.0
// EXPECT: this envelope was given a different number of rows than its series has elements
// REJECT: no matching
//
// Four limit rows for a series of five, aggregate-initialising the check
// with no factory: the envelope is the check's own member type, so the same
// refusal holds.
#include <formula-cpp/conformity.hpp>

struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
{
};

using formula::limit;
using formula::LimitRow;

inline constexpr formula::Conformity<formula::unit::Percent, formula::SeriesVarNode<Passing, 5>> check {
    formula::series<Passing, 5>,
    { LimitRow { limit(formula::Rational { 30 }), limit(formula::Rational { 40 }) },
    LimitRow { limit(formula::Rational { 50 }), limit(formula::Rational { 60 }) },
    LimitRow { limit(formula::Rational { 60 }), formula::unbounded },
    LimitRow { limit(formula::Rational { 0 }), limit(formula::Rational { 95 }) } },
    formula::Verdict { "reject the specimen" }
};

int main()
{
    return static_cast<int>(decltype(check)::length);
}
