// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method's rounding rule rounds in a unit that does not measure the dimension its variants report
// REJECT: formula: this rounding node names a unit that does not measure
//
// The same method as `method_rounding_rule_dimension_mismatch.cpp`, declared
// AND evaluated. The declaration is refused; the evaluation must add nothing
// to that. Before `evaluate_method` gated its body on the method being well
// formed, clang-cl, clang++ and g++ went on to build the rounding node, which
// refused the same mistake a second time in its own words -- two library
// messages for one mistake. The REJECT refuses that second message. cl
// reported only the first even without the gate.
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
                    formula::rounding_rule<formula::unit::Millimetre,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero>(),
                    formula::constraints());

inline constexpr auto inputs = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                    formula::Measured<EdgeX> { formula::Rational { 150 } },
                                                    formula::Measured<EdgeY> { formula::Rational { 100 } });
} // namespace

int main()
{
    return formula::evaluate_method<Cube>(m, inputs).has_value() ? 0 : 1;
}
