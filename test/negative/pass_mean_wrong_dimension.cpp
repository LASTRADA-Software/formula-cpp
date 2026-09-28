// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this rejection's limit reads a pass_mean whose quantity does not measure the determinations' dimension
//
// (1 g/mm) * pass_mean<Width> is a mass, so the limit's own dimension check
// passes; the pass mean it reads is a width's, and is refused, once.
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
    constexpr auto perWidth = formula::constant<formula::unit::Gram>(formula::Rational { 1 })
                              / formula::constant<formula::unit::Millimetre>(formula::Rational { 1 })
                              * formula::pass_mean<Width>;
    constexpr auto rejection = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
            formula::series<Mass, 3>, formula::deviation_from_mean(perWidth), formula::Verdict { "repeat" });
    auto const outcome = formula::checked_evaluate<Mass>(formula::sample_mean(rejection), threeDeterminations);
    return outcome.has_value() ? 0 : 1;
}
