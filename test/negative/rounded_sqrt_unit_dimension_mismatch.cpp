// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this rounded_sqrt names a unit whose square does not measure the dimension of its radicand
//
// The root of a spread, not of a variance: `Spread` is already a mass, so its
// square root is not one, and "to 0.01 g" names nothing it could be rounded
// to. The likeliest way to meet this refusal is to pass the standard
// deviation where the variance belongs.
//
// The node is evaluated as well as built, into a quantity of the unit's
// dimension, so that the evaluation path is shown to add no second message:
// the node's dimension is the unit's, whatever its radicand was.
//
// This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Spread: formula::Quantity<Spread, "s", "spread of the determinations", formula::unit::Gram>
{
};
} // namespace

int main()
{
    constexpr auto broken =
        formula::rounded_sqrt<formula::unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
            formula::var<Spread>);
    auto const outcome = formula::checked_evaluate<Spread>(
        broken, formula::environment(formula::Measured<Spread> { formula::Rational { 427, 125 } }));
    return outcome.has_value() ? 0 : 1;
}
