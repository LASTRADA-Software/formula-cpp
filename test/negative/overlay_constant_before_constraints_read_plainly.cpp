// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method reads a quantity both where an overlay fixed or derived it and, elsewhere, unsubstituted from the environment
// REJECT: formula: this overlay overrides a quantity that no variant or constraint of the method uses
//
// The ratio fixed -- which only the method's own constraint reads -- then
// the constraints replaced by one that reads it plainly. Operations apply in
// order, so the constant never reaches the new constraint, which would check
// the specimen's ratio under an overlay that claims to fix it. The REJECT
// pins that this is not reported as "nothing uses it": the new constraint
// does, although the one the constant was applied to is gone.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>

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

using formula::var;

// Only the Cube variant reads the shape factor, and only the method's own
// constraint reads the ratio.
inline constexpr auto m =
    formula::method(formula::variants(formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                    formula::rounding_rule<formula::unit::Megapascal,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero>(),
                    formula::constraints(formula::constraint(var<Ratio> <= formula::number(formula::Rational { 1 }),
                                                             formula::Verdict { "the ratio exceeds one" })));

// A jurisdiction's constraint reading the ratio plainly.
inline constexpr auto readsRatio = formula::constraints(formula::constraint(
    var<Ratio> <= formula::number(formula::Rational { 2 }), formula::Verdict { "the ratio exceeds two" }));

// A jurisdiction's constraint reading neither the shape factor nor the ratio.
inline constexpr auto readsLoad = formula::constraints(
    formula::constraint(var<Force> >= formula::constant<formula::unit::Newton>(formula::Rational { 50'000 }),
                        formula::Verdict { "the load is below the minimum" }));
} // namespace

int main()
{
    constexpr auto overlaid = formula::apply(
        formula::overlay(formula::with_constant<Ratio>(formula::Rational { 1, 2 }), formula::with_constraints(readsRatio)),
        m);
    return overlaid.constraintOrigin.provenance() == formula::ConstraintProvenance::JurisdictionOverlay ? 0 : 1;
}
