// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method's rounding rule rounds in a unit that does not measure the dimension its variants report
// REJECT: OverriddenConstantNode
//
// A rounding override in millimetres on a pressure method, then a constant
// the method reads. The rule is refused where the overlay applies it, and
// the method is carried on unchanged: the constant rewrites a method with
// the method's own rule, so the refusal is not raised again by the method
// the constant builds. g++ 13.3 and clang++ 20.1.8 printed it twice before,
// once for each method holding the rule, and print it once now; cl 19.51
// printed it once either way. The count is not something this test can
// assert (it was counted by hand); the REJECT pins the second refusal away
// by what it would name -- the rewritten variants, which hold the constant's
// `OverriddenConstantNode`.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>

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
struct ShapeFactor: formula::Quantity<ShapeFactor, "k_s", "shape factor", formula::unit::One>
{
};

using formula::var;

inline constexpr auto m =
    formula::method(formula::variants(formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeX>) )),
                    formula::rounding_rule<formula::unit::Megapascal,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero>(),
                    formula::constraints());
} // namespace

int main()
{
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::with_rounding<formula::unit::Millimetre,
                                                               formula::DecimalPlaces { 1 },
                                                               formula::RoundingMode::HalfAwayFromZero>(),
                                        formula::with_constant<ShapeFactor>(formula::Rational { 1, 2 })),
                       m);
    return overlaid.rounding.places.value == 1 ? 0 : 1;
}
