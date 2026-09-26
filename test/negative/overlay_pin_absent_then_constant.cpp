// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay pins or prunes a variant the method does not declare
// REJECT: formula: this overlay overrides a quantity that no variant or constraint of the method uses
//
// Pin `Prism`, which the method does not declare, then fix the shape factor,
// which the method's SECOND variant reads.
//
// The pin is refused, and the refused pin must leave the method as it was.
// Were it to carry on as `variant_index` falls back -- with a method holding
// only the variant at position 0, `Cylinder`, which never reads the shape
// factor -- the constant would then be refused as well, as a quantity nothing
// reads: false of the author's method, whose `Cube` reads it. The REJECT
// refuses that second message. `Cube` sits second on purpose: with it first,
// the fallback method would read the constant and hide the defect.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>

#include <tuple>

namespace
{
struct Cube
{
};
struct Cylinder
{
};
struct Prism
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", formula::unit::Millimetre>
{
};
struct ShapeFactor: formula::Quantity<ShapeFactor, "k_s", "shape factor", formula::unit::One>
{
};

using formula::var;

// Only the second variant, Cube, reads the shape factor.
inline constexpr auto m =
    formula::method(formula::variants(formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ),
                                      formula::variant<Cube>(var<ShapeFactor>* var<Force> / (var<EdgeX> * var<EdgeY>) )),
                    formula::rounding_rule<formula::unit::Megapascal,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero>(),
                    formula::constraints());
} // namespace

int main()
{
    constexpr auto overlaid = formula::apply(
        formula::overlay(formula::pin_variant<Prism>(formula::Citation { .reference = "Example Standard 12:2021 NA" }),
                         formula::with_constant<ShapeFactor>(formula::Rational { 97, 100 })),
        m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 0 ? 1 : 0;
}
