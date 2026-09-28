// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a sample statistic needs a sample (a series or observations), not a single value
//
// The count of a mean: a mean is one value, not a sample. Refused in this
// library's words, once. The refusing overload returns a bare number, so
// evaluating the result as a count adds no dimension message. This must not
// compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", formula::unit::Gram>
{
};
struct Determinations: formula::Quantity<Determinations, "n", "number of determinations", formula::unit::One>
{
};
} // namespace

int main()
{
    auto const outcome = formula::checked_evaluate<Determinations>(
        formula::sample_count(formula::sample_mean(formula::series<Mass, 3>)),
        formula::environment(formula::measured_series<Mass>(formula::Measured<Mass> { formula::Rational { 40 } },
                                                            formula::Measured<Mass> { formula::Rational { 41 } },
                                                            formula::Measured<Mass> { formula::Rational { 43 } })));
    return outcome.has_value() ? 0 : 1;
}
