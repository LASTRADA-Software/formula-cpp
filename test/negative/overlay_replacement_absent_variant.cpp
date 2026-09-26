// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay replaces a variant the method does not declare
// REJECT: formula: this overlay replaces a variant that the method it produces does not hold
//
// Replacing a Prism the method never declared. The REJECT pins that one
// mistake gets one message: a tag the method never declared is a mistaken
// name, and only a tag it did declare can be one the produced method no
// longer holds.
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
struct Ratio: formula::Quantity<Ratio, "r", "a dimensionless ratio", formula::unit::One>
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
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::replace_variant<Prism>(var<Force> / (var<EdgeY> * var<EdgeY>) )), m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 2 ? 0 : 1;
}
