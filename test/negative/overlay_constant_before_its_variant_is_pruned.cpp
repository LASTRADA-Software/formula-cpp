// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay overrides a quantity that no variant or constraint of the method uses
//
// Fix the shape factor, then prune `Cube`, the only variant that reads it. When
// the constant is applied, `Cube` still reads it; in the method the overlay
// produces, nothing does. The rule is judged of that method, so this is refused
// exactly as the opposite order is -- the case that pins it, since a check made
// per operation accepts this order.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/environment.hpp>
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

// Only the Cube variant reads the shape factor.
inline constexpr auto m =
    formula::method(formula::variants(formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                    formula::rounding_rule<formula::unit::Megapascal,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero>(),
                    formula::constraints());
} // namespace

int main()
{
    constexpr auto overlaid = formula::apply(
        formula::overlay(
            formula::with_constant<ShapeFactor>(formula::Rational { 863, 1000 },
                                                formula::Citation { .reference = "Example Standard 12:2021 NA" }),
            formula::prune_variant<Cube>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 0 ? 1 : 0;
}
