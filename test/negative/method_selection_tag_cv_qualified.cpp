// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this tag is not a plain class type
// REJECT: formula: this method declares no variant for that tag
//
// `evaluate_method<const Cube>` on a method that declares a `Cube` variant.
//
// A selection tag is held to the rule a declared tag is held to. Both
// refusals would be true here -- `const Cube` is not a plain class type, and
// no variant is tagged `const Cube` -- but only the first names the mistake:
// the second would send the author to add a variant they already have. So
// the case also refuses any output carrying the no-match text, which is what
// pins the order the two rules are asked in.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/method.hpp>

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

using formula::var;

inline constexpr auto m =
    formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                    formula::rounding_rule<formula::unit::Megapascal,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero>(),
                    formula::constraints());

inline constexpr auto inputs = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                    formula::Measured<EdgeX> { formula::Rational { 139 } },
                                                    formula::Measured<EdgeY> { formula::Rational { 103 } });
} // namespace

int main()
{
    return formula::evaluate_method<Cube const>(m, inputs).has_value() ? 0 : 1;
}
