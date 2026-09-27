// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay replaces a variant the method does not declare
// REJECT: formula: this overlay overrides a quantity that no variant or constraint of the method uses
//
// A replacement of a Prism the method never declared, whose formula is the
// only reader of the ratio a constant then fixes. The refused replacement
// leaves the method without it, so the constant finds nothing reading the
// ratio; the REJECT pins that the constant's check waits on every
// replacement having been applied, rather than adding "no variant uses it"
// to the message that names the mistaken tag.
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
        formula::overlay(formula::replace_variant<Prism>(var<Ratio> * var<Force> / (var<EdgeX> * var<EdgeY>),
                                                         formula::Citation { .reference = "Example Standard 12:2021 NA" }),
                         formula::with_constant<Ratio>(formula::Rational { 781, 1000 },
                                                       formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 2 ? 0 : 1;
}
