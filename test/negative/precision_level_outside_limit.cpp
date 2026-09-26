// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: precision_level is meaningful only inside the limit expression of precision_limit
//
// The placeholder in an ordinary formula, with no precision limit to bind a
// level. It asks no environment for anything, so the environment's own "provides
// no value" never adds a second message. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct ResultA: formula::Quantity<ResultA, "x_A", "first determination", formula::unit::Gram>
{
};
struct ResultB: formula::Quantity<ResultB, "x_B", "second determination", formula::unit::Gram>
{
};
struct Width: formula::Quantity<Width, "w", "specimen width", formula::unit::Millimetre>
{
};
} // namespace

int main()
{
    auto const outcome = formula::checked_evaluate<ResultA>(
        formula::var<ResultA> + formula::precision_level<ResultA>,
        formula::environment(formula::Measured<ResultA> { formula::Rational { 40 } }));
    return outcome.has_value() ? 0 : 1;
}
