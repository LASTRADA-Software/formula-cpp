// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay fixes or derives a quantity that an operation listed after it reads again, unsubstituted
//
// A definition cycle across two quantities: the shape factor defined by the
// ratio, then the ratio defined by the shape factor. The second definition
// rewrites the ratio inside the first, and so puts a plain shape factor back
// into the first definition -- a use of it that no substitution reaches, which
// would read the specimen's value under an overlay that claims to define it.
// Refused by the rule that a substitution be in effect wherever the produced
// method reads its quantity. Neither definition is unused -- each quantity's
// node is still in the produced method -- so no second refusal applies.
//
// Judged within the one overlay, against the method it produces, as every
// result check of an overlay is.
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
    constexpr auto overlaid = formula::apply(
        formula::overlay(formula::add_derived<ShapeFactor>(var<Ratio>), formula::add_derived<Ratio>(var<ShapeFactor>)), m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 2 ? 0 : 1;
}
