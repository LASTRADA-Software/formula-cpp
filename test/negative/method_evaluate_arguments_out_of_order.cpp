// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method's variants are not a variants pack
// REJECT: SelectVariant
//
// The same swapped method as `method_arguments_out_of_order.cpp`, declared
// AND evaluated. The declaration is refused; the evaluation must add nothing
// to that. Before `evaluate_method` gated its body on the method being well
// formed, clang-cl, clang++ and g++ went on to name
// `SelectVariant<Cube, RoundingRule<...>>`, which has no definition, and
// reported that in their own words on top of ours. The REJECT refuses that
// template's name. cl reported only our messages even without the gate.
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

inline constexpr auto m = formula::method(formula::rounding_rule<formula::unit::Megapascal,
                                                                 formula::DecimalPlaces { 1 },
                                                                 formula::RoundingMode::HalfAwayFromZero>(),
                                          formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                                          formula::constraints());

inline constexpr auto inputs = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                    formula::Measured<EdgeX> { formula::Rational { 139 } });
} // namespace

int main()
{
    return formula::evaluate_method<Cube>(m, inputs).has_value() ? 0 : 1;
}
