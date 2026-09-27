// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this rejection's limit does not measure what its criterion compares
//
// 6/100 of a width's pass mean is a width: the limit measures the wrong
// thing, and the pass mean it reads is of the wrong quantity too. Refused
// once, for the limit: the pass mean's check is asked only once the limit
// measures what it must.
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
            formula::deviation_from_mean(formula::Rational { 6, 100 } * formula::pass_mean<Width>),
            formula::Verdict { "repeat" });
    auto const outcome = formula::checked_evaluate<Mass>(formula::sample_mean(rejection), threeDeterminations);
    return outcome.has_value() ? 0 : 1;
}
