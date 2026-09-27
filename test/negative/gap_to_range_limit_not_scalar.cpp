// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this rejection's limit does not measure what its criterion compares
//
// A gap ratio is a bare number, and this limit is in grams. Refused once.
// This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", formula::unit::Gram>
{
};
inline constexpr auto threeDeterminations =
    formula::environment(formula::measured_series<Mass>(formula::Measured<Mass> { formula::Rational { 40 } },
                                                        formula::Measured<Mass> { formula::Rational { 41 } },
                                                        formula::Measured<Mass> { formula::Rational { 44 } }));
} // namespace

int main()
{
    constexpr auto rejection = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
            formula::series<Mass, 3>,
            formula::gap_to_range(formula::constant<formula::unit::Gram>(formula::Rational { 1 })),
            formula::Verdict { "repeat" });
    auto const outcome = formula::checked_evaluate<Mass>(formula::sample_mean(rejection), threeDeterminations);
    return outcome.has_value() ? 0 : 1;
}
