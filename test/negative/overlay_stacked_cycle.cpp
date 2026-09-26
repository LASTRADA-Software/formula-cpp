// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method reads a quantity both where an overlay fixed or derived it and, elsewhere, unsubstituted from
// the environment REJECT: formula: this overlay derives a quantity that no variant or constraint of the method uses
//
// A definition cycle built by two overlays applied in turn: the first
// defines the shape factor by the ratio, the second the ratio by the shape
// factor. The rule that a substitution be in effect wherever the method reads
// its quantity is judged against the whole produced method, whichever overlay
// left each substitution, so the second overlay is refused for the plain
// shape factor it puts back inside the first overlay's definition.
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
    constexpr auto first = formula::apply(formula::overlay(formula::add_derived<ShapeFactor>(var<Ratio>)), m);
    constexpr auto second = formula::apply(formula::overlay(formula::add_derived<Ratio>(var<ShapeFactor>)), first);
    return std::tuple_size_v<decltype(second.variantSet.cases)> == 2 ? 0 : 1;
}
