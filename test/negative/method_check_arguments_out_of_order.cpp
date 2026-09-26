// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method's rounding rule is not a rounding rule
// REJECT: check_all
//
// `method(variants(...), constraints(), rounding_rule<...>())` -- the rounding
// rule and the constraints in each other's place -- declared AND checked. The
// declaration is refused; checking the method must add nothing to that.
// Ungated, `check_method` would hand the rounding rule, sitting where the
// constraints belong, to `check_all`, which takes only a constraint set, and
// the compiler would add its own error naming it. The REJECT refuses that
// name.
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

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

inline constexpr auto m = formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                                          formula::constraints(),
                                          formula::rounding_rule<formula::unit::Megapascal,
                                                                 formula::DecimalPlaces { 1 },
                                                                 formula::RoundingMode::HalfAwayFromZero>());

inline constexpr auto inputs = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                    formula::Measured<EdgeX> { formula::Rational { 150 } });
} // namespace

int main()
{
    return formula::check_method(m, inputs).size() == 0 ? 0 : 1;
}
