// SPDX-License-Identifier: Apache-2.0
// EXPECT: this conformity check states its limits in a unit that does not measure the dimension of its subject
// REJECT: this envelope was given a different number of rows than its series has elements
//
// A percentage judged against limits "in millimetres": refused once, in the
// library's words, and the envelope -- of the right length -- adds nothing.
#include <formula-cpp/conformity.hpp>

struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", formula::unit::Percent>
{
};

using formula::limit;
using formula::LimitRow;

inline constexpr auto check = formula::conformity<formula::unit::Millimetre>(
    formula::series<Passing, 4>,
    { LimitRow { limit(formula::Rational { 30 }), limit(formula::Rational { 40 }) },
      LimitRow { limit(formula::Rational { 50 }), limit(formula::Rational { 60 }) },
      LimitRow { limit(formula::Rational { 60 }), formula::unbounded },
      LimitRow { limit(formula::Rational { 0 }), limit(formula::Rational { 95 }) } },
    formula::Verdict { "reject the specimen" });

int main()
{
    return static_cast<int>(decltype(check)::length);
}
