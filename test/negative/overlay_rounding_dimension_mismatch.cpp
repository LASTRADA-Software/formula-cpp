// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method's rounding rule rounds in a unit that does not measure the dimension its variants report
//
// An overlay that rounds a pressure method in millimetres. `apply` holds
// the overlay's rule to the check the method's own rule was held to, in the
// same words -- the overlay does not get to ship a rule the method itself
// could not have been declared with.
//
// Applied only, never evaluated: evaluating would build the rounding node,
// whose own check would refuse the rule in its own words whether or not the
// method had.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>

namespace
{
struct Cube
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

inline constexpr auto m = formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                                          formula::rounding_rule<formula::unit::Megapascal,
                                                                 formula::DecimalPlaces { 1 },
                                                                 formula::RoundingMode::HalfAwayFromZero>(),
                                          formula::constraints());
} // namespace

int main()
{
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::with_rounding<formula::unit::Millimetre,
                                                               formula::DecimalPlaces { 1 },
                                                               formula::RoundingMode::HalfAwayFromZero>(
                           formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                       m);
    return overlaid.rounding.places.value == 1 ? 0 : 1;
}
