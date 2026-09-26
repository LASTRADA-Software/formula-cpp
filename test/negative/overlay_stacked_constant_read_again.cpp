// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method reads a quantity both where an overlay fixed or derived it and, elsewhere, unsubstituted from
// the environment REJECT: formula: this overlay overrides a quantity that no variant or constraint of the method uses
//
// A method whose two variants both read the shape factor. One overlay fixes
// it; a second, applied to the result, replaces the Cube with a formula that
// reads it again from the environment -- while the Cylinder still reads the
// first overlay's constant. One quantity at two values across two overlays,
// refused as it is within one.
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
    static_cast<void>(m);
    constexpr auto both = formula::method(
        formula::variants(formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeY>) ),
                          formula::variant<Cylinder>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeX>) )),
        formula::rounding_rule<formula::unit::Megapascal,
                               formula::DecimalPlaces { 1 },
                               formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    constexpr auto fixed =
        formula::apply(formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 97, 100 })), both);
    constexpr auto replaced = formula::apply(
        formula::overlay(formula::replace_variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeY> * var<EdgeY>) )), fixed);
    return std::tuple_size_v<decltype(replaced.variantSet.cases)> == 2 ? 0 : 1;
}
