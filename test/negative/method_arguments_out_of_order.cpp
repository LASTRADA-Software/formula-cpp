// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method's variants are not a variants pack
//
// `method(rounding_rule<...>(), variants(...), constraints())` -- the variants
// and the rounding rule passed in each other's place.
//
// `method(...)` takes three arguments of unrelated types, so this compiled
// before the shape rules existed, and failed only when `evaluate_method` was
// called, with the compiler's own words for an undefined template. Nothing
// here evaluates the method: the refusal has to come from declaring it.
//
// Both of the first two parts are wrong here, and the rounding-rule message
// fires too; this case expects the first. The gate in front of the dimension
// rule is pinned by `method_arguments_out_of_order_rule_last.cpp`, not here:
// with a rounding rule where the variants belong there is no agreed dimension,
// so the dimension rule stays quiet whether that gate exists or not.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/method.hpp>

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
    return static_cast<int>(sizeof(m));
}
