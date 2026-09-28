// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: gap_to_range examines only the lowest and the highest value; declare PerPass::MostExtreme
//
// gap_to_range examines two determinations, so "every exceeding" is refused.
// The limit here is in grams as well, which gap_to_range also refuses: the
// policy is asked first, and the limit's dimension only once the policy is
// allowed, so this draws the one message.
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
    constexpr auto rejection = formula::without_outliers<formula::PerPass::EveryExceeding,
                                                         formula::OnLimit::Keep,
                                                         formula::AtMost<1>,
                                                         formula::KeepAtLeast<2>>(
        formula::series<Mass, 3>,
        formula::gap_to_range(formula::constant<formula::unit::Gram>(formula::Rational { 1 })),
        formula::Verdict { "repeat" });
    auto const outcome = formula::checked_evaluate<Mass>(formula::sample_mean(rejection), threeDeterminations);
    return outcome.has_value() ? 0 : 1;
}
