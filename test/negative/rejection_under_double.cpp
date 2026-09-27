// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a rejection of outliers can only be evaluated with Rep = Rational
//
// A rejection decides which determinations remain, so it evaluates in
// Rational only (S15). Asked in double, it is refused once.
// This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", formula::unit::Gram>
{
};
struct Width: formula::Quantity<Width, "w", "specimen width", formula::unit::Millimetre>
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
            formula::deviation_from_mean(formula::constant<formula::unit::Gram>(formula::Rational { 1 })),
            formula::Verdict { "repeat" });
    auto const outcome = formula::checked_evaluate_si<double>(formula::sample_mean(rejection), threeDeterminations);
    return outcome.has_value() ? 0 : 1;
}
