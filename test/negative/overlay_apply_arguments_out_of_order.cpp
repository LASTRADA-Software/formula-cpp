// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method's variants are not a variants pack
// REJECT: apply_operation
//
// An overlay applied to a method whose variants and rounding rule were passed
// in each other's place. The method's declaration is refused, and `apply`
// must add nothing to that refusal: it gates its body on the method being
// well formed, as `evaluate_method` does. Without the gate, `apply` goes on
// to hand a rounding rule to `apply_operation` as though it were a variants
// pack, and the compiler reports that in its own words; the REJECT refuses
// any output naming it.
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

inline constexpr auto m = formula::method(formula::rounding_rule<formula::unit::Megapascal,
                                                                 formula::DecimalPlaces { 1 },
                                                                 formula::RoundingMode::HalfAwayFromZero>(),
                                          formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                                          formula::constraints());
} // namespace

int main()
{
    constexpr auto overlaid = formula::apply(
        formula::overlay(formula::pin_variant<Cube>(formula::Citation { .reference = "Example Standard 12:2021 NA" })), m);
    static_cast<void>(overlaid);
    return 0;
}
