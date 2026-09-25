// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method declares no variant for that tag
//
// A `Prism` selected from a method that declares only `Cube` and `Cylinder`.
//
// There is no fallback variant and no "first match wins": a specimen that
// matches no variant has no result, so the selection is refused where the
// author wrote it rather than quietly answered with some other variant's
// number.
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

using formula::var;

inline constexpr auto m = formula::method(
    formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
    formula::rounding_rule<formula::unit::Megapascal, formula::DecimalPlaces { 1 },
                           formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());

inline constexpr auto inputs = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                    formula::Measured<EdgeX> { formula::Rational { 150 } },
                                                    formula::Measured<EdgeY> { formula::Rational { 100 } });
} // namespace

int main()
{
    return formula::evaluate_method<Prism>(m, inputs).has_value() ? 0 : 1;
}
