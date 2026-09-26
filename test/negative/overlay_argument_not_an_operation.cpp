// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this argument of overlay(...) is not an overlay operation
// REJECT: RequireOperationListedOnce
// REJECT: apply_operation
//
// `overlay(42, 42)`: two arguments that are not operations, and alike. Two
// REJECTs pin two gates. The repeat rule is asked only once every argument is
// an operation, so it must not also report the same operation twice; and
// `apply` is not instantiated over a malformed overlay, so it must not add the
// compiler's own words for an `apply_operation` that takes no `int`.
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
    constexpr auto overlaid = formula::apply(formula::overlay(42, 42), m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 0 ? 1 : 0;
}
