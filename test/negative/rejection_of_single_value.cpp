// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: without_outliers rejects determinations from a sample
//
// One value read once is no sample. Refused once; the refusing overload
// returns a rejection over a series already refused, so the mean over it adds
// nothing. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", formula::unit::Gram>
{
};
inline constexpr auto fourDeterminations =
    formula::environment(formula::measured_series<Mass>(formula::Measured<Mass> { formula::Rational { 40 } },
                                                        formula::Measured<Mass> { formula::Rational { 41 } },
                                                        formula::Measured<Mass> { formula::Rational { 44 } },
                                                        formula::Measured<Mass> { formula::Rational { 39 } }));
inline constexpr auto relative = formula::deviation_from_mean(formula::Rational { 6, 100 } * formula::pass_mean<Mass>);
} // namespace

int main()
{
    constexpr auto single = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
            formula::var<Mass>, relative, formula::Verdict { "repeat" });
    auto const outcome = formula::checked_evaluate<Mass>(formula::sample_mean(single), fourDeterminations);
    return outcome.has_value() ? 0 : 1;
}
