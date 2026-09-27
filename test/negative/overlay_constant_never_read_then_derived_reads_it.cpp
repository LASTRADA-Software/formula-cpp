// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay fixes a quantity that nothing read where the constant is listed, and an operation listed after it reads the quantity from the environment
// REJECT: formula: this overlay overrides a quantity that no variant or constraint of the method uses
// REJECT: formula: this overlay fixes a quantity, and an operation listed after the constant removed
//
// A constant for a quantity the method never reads, then a definition in
// the same overlay of the ratio -- read by the method's own constraint -- in
// terms of it. The constant met no use where it applied; the definition
// puts in a plain one. Refused as listed before that use; the REJECTs pin
// that it is refused neither as read by nothing nor as bypassed.
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
struct Ratio: formula::Quantity<Ratio, "r", "a dimensionless ratio", formula::unit::One>
{
};
struct Other: formula::Quantity<Other, "o", "a ratio the method never reads", formula::unit::One>
{
};

using formula::var;

// Only the Cube variant reads the shape factor, and only the method's own
// constraint reads the ratio.
inline constexpr auto m =
    formula::method(formula::variants(formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                    formula::rounding_rule<formula::unit::Megapascal,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero>(),
                    formula::constraints(formula::constraint(var<Ratio> <= formula::number(formula::Rational { 137, 100 }),
                                                             formula::Verdict { "the ratio exceeds 1.37" })));

} // namespace

int main()
{
    constexpr auto overlaid = formula::apply(
        formula::overlay(formula::with_constant<Other>(formula::Rational { 431, 1000 },
                                                       formula::Citation { .reference = "Example Standard 12:2021 NA" }),
                         formula::add_derived<Ratio>(var<Other> * formula::number(formula::Rational { 217, 100 }),
                                                     formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 2 ? 0 : 1;
}
