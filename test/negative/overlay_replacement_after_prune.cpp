// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay replaces a variant that the method it produces does not hold
// REJECT: formula: this overlay replaces a variant the method does not declare
//
// The Cylinder pruned, then replaced by the same overlay. Asked where the
// replacement is applied, this would read "does not declare" -- false of the
// method the author wrote -- and the two orders of one mistake would get two
// messages. Judged against the produced method, both get the same one.
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
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::prune_variant<Cylinder>(),
                                        formula::replace_variant<Cylinder>(var<Force> / (var<EdgeY> * var<EdgeY>) )),
                       m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 1 ? 0 : 1;
}
