// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: without_outliers keeps at least m determinations of a series that holds fewer
//
// Three determinations, to keep at least four: every evaluation would give
// the verdict before a single pass. Refused once, where it is written.
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
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<4>>(
            formula::series<Mass, 3>,
            formula::deviation_from_mean(formula::constant<formula::unit::Gram>(formula::Rational { 1 })),
            formula::Verdict { "repeat" });
    auto const outcome = formula::checked_evaluate<Mass>(formula::sample_mean(rejection), threeDeterminations);
    return outcome.has_value() ? 0 : 1;
}
