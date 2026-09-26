// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: pass_mean and pass_count are meaningful only inside the limit expression of without_outliers
//
// A pass placeholder evaluated where no rejection binds it. This must not
// compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", formula::unit::Gram>
{
};
} // namespace

int main()
{
    auto const outcome = formula::checked_evaluate<Mass>(
        formula::pass_mean<Mass>, formula::environment(formula::Measured<Mass> { formula::Rational { 40 } }));
    return outcome.has_value() ? 0 : 1;
}
