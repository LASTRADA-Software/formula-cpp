// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a rounding node cannot be evaluated with Rep = double
// A correct rounded exponential evaluated in double: RepRounding<double> refuses, as for every rounding node.
// The argument is dimensionless, so the logarithm's own message must not fire.
// This must not compile.

#include <formula-cpp/formula.hpp>

namespace
{
struct Ratio: formula::Quantity<Ratio, "r", "an invented ratio", formula::unit::One>
{
};
} // namespace

     int main()
     {
         constexpr auto logarithm =
             formula::rounded_exp<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(formula::var<Ratio>);
         auto const evaluated = formula::checked_evaluate_si<double>(
             logarithm, formula::environment(formula::Measured<Ratio> { formula::Rational { 2 } }));
         return evaluated.has_value() ? 0 : 1;
     }
