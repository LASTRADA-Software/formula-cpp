// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay replaces a variant with a formula of a different dimension from the method's
// REJECT: formula: this overlay replaces a variant that the method it produces does not hold
//
// The other order of `overlay_replacement_dimension_then_pruned`: the
// prune first, so the replacement meets no Cylinder where it is applied.
// Judged against the method the overlay was applied to, it is still the
// dimension that is wrong, and that is the one message.
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
    constexpr auto overlaid = formula::apply(
        formula::overlay(
            formula::prune_variant<Cylinder>(formula::Citation { .reference = "Example Standard 12:2021 NA" }),
            formula::replace_variant<Cylinder>(var<EdgeX> * var<EdgeY> / var<EdgeX>,
                                               formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 1 ? 0 : 1;
}
