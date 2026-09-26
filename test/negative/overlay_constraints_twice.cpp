// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay lists the same operation twice
//
// Two replacements of the constraints in one overlay, holding different
// constraints -- two different types, which a repeat rule comparing types
// alone lets through, and the second then silently discards the first,
// because a method has one set of constraints. Every `with_constraints` has
// the same identity for the repeat rule.
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
        formula::overlay(
            formula::with_constraints(readsLoad, formula::Citation { .reference = "Example Standard 12:2021 NA" }),
            formula::with_constraints(formula::constraints(),
                                      formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        m);
    return formula::constraint_origin(overlaid).provenance() == formula::ConstraintProvenance::JurisdictionOverlay ? 0 : 1;
}
