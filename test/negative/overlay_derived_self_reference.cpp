// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay derives a quantity from an expression that reads the quantity itself
// REJECT: formula: this method reads a quantity both where an overlay fixed or derived it and, elsewhere, unsubstituted from
// the environment REJECT: formula: this overlay derives a quantity that no variant or constraint of the method uses
//
// Defining the shape factor by an expression that reads the shape factor:
// that use would keep the specimen's value while every other use evaluates
// the definition. Refused where the operation is written.
//
// Applied as well as written, and the REJECTs pin that applying it adds
// nothing: an operation its own class body refuses is part of what `apply`
// asks before instantiating its body, so the definition is never also judged
// against the method it would have produced.
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
    constexpr auto deriving = formula::add_derived<ShapeFactor>(
        var<ShapeFactor> * var<EdgeY> / var<EdgeX>, formula::Citation { .reference = "Example Standard 12:2021 NA" });
    constexpr auto overlaid = formula::apply(formula::overlay(deriving), m);
    return deriving.source.title.empty() && std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 2 ? 0 : 1;
}
