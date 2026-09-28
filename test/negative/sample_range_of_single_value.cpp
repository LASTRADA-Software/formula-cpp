// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a sample statistic needs a sample (a series or observations), not a single value
//
// The range of one quantity read once: a range needs repeated determinations.
// Refused in this library's words, once. The refusing overload returns the
// value itself, so evaluating the result as a mass adds nothing, and the
// concept failure of the sample overload is never reported. This must not
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
    auto const outcome =
        formula::checked_evaluate<Mass>(formula::sample_range(formula::var<Mass>),
                                        formula::environment(formula::Measured<Mass> { formula::Rational { 40 } }));
    return outcome.has_value() ? 0 : 1;
}
