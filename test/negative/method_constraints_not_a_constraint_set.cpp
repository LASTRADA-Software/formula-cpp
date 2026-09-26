// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method's constraints are not a constraint set
//
// `method(variants(...), rounding_rule<...>(), rounding_rule<...>())` -- the
// rounding rule given twice and the constraints forgotten.
//
// The variants and the rounding rule are both well formed, so only the
// constraint-set rule has anything to say here. The two out-of-order cases
// cannot stand in for this one: each has a wrong part ahead of the
// constraints and expects that part's message, so they pass whether or not
// the constraint-set rule exists.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/method.hpp>

namespace
{
struct Cube
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

inline constexpr auto rule = formula::
    rounding_rule<formula::unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>();

inline constexpr auto m =
    formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )), rule, rule);
} // namespace

int main()
{
    return static_cast<int>(sizeof(m));
}
