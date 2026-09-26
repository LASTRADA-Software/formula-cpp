// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this rejection's limit does not measure what its criterion compares
//
// A deviation of masses compared with a width. Refused once where the
// rejection is declared. This must not compile.
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
    constexpr auto wrong = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
            formula::series<Mass, 4>,
            formula::deviation_from_mean(formula::constant<formula::unit::Millimetre>(formula::Rational { 2 })),
            formula::Verdict { "repeat" });
    auto const outcome = formula::checked_evaluate<Mass>(formula::sample_mean(wrong), fourDeterminations);
    return outcome.has_value() ? 0 : 1;
}
