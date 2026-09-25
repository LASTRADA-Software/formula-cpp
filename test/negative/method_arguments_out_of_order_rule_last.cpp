// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method's rounding rule is not a rounding rule
// REJECT: RequireRoundingRuleMeasuresVariants
//
// `method(variants(...), constraints(), rounding_rule<...>())` -- the rounding
// rule and the constraints passed in each other's place.
//
// The variants are well formed and agree, so there IS a dimension to compare
// against, and only the shape rule stands between the dimension rule and a
// `ConstraintSet`, which has no `unit` to read. The REJECT pins that gate: the
// dimension rule's name appears in the output only if it is asked.
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

inline constexpr auto m = formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                                          formula::constraints(),
                                          formula::rounding_rule<formula::unit::Megapascal,
                                                                 formula::DecimalPlaces { 1 },
                                                                 formula::RoundingMode::HalfAwayFromZero>());
} // namespace

int main()
{
    return static_cast<int>(sizeof(m));
}
