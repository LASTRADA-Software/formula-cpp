// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay overrides a quantity that no variant or constraint of the method uses
//
// The ratio fixed, then the constraints replaced by ones that do not read
// it. Only the method's own constraint read the ratio, and it is gone, so
// the method the overlay produces reads the ratio nowhere -- although it was
// still read at the moment the constant was applied, which is the order a
// check made per operation would miss. See
// `overlay_constant_after_its_constraint_is_replaced` for the other order.
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
                    formula::constraints(formula::constraint(var<Ratio> <= formula::number(formula::Rational { 137, 100 }),
                                                             formula::Verdict { "the ratio exceeds 1.37" })));

// A jurisdiction's constraint reading the ratio plainly.
inline constexpr auto readsRatio = formula::constraints(formula::constraint(
    var<Ratio> <= formula::number(formula::Rational { 217, 100 }), formula::Verdict { "the ratio exceeds 2.17" }));

// A jurisdiction's constraint reading neither the shape factor nor the ratio.
inline constexpr auto readsLoad = formula::constraints(
    formula::constraint(var<Force> >= formula::constant<formula::unit::Newton>(formula::Rational { 46'700 }),
                        formula::Verdict { "the load is below the minimum" }));
} // namespace

int main()
{
    constexpr auto overlaid = formula::apply(
        formula::overlay(
            formula::with_constant<Ratio>(formula::Rational { 431, 1000 },
                                          formula::Citation { .reference = "Example Standard 12:2021 NA" }),
            formula::with_constraints(readsLoad, formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        m);
    return formula::constraint_origin(overlaid).provenance() == formula::ConstraintProvenance::JurisdictionOverlay ? 0 : 1;
}
