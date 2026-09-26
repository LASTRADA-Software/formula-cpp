// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this tag is not a plain class type
// REJECT: formula: this overlay overrides a quantity that no variant or constraint of the method uses
// REJECT: formula: this overlay replaces a variant the method does not declare
//
// The same with a replacement tagged `const Cylinder`: refused by the tag
// rule, where the operation is written. The replacement is never applied,
// so the constant's check waits on it, and the declared-variant rule is not
// asked of a tag the tag rule refused -- neither adds a message.
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
        formula::overlay(formula::replace_variant<Cylinder const>(var<Ratio> * var<Force> / (var<EdgeX> * var<EdgeY>) ),
                         formula::with_constant<Ratio>(formula::Rational { 4, 5 })),
        m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 2 ? 0 : 1;
}
