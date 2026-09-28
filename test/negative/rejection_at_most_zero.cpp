// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: without_outliers declares AtMost<0>
//
// A rejection that may reject nothing is no rejection. This must not compile.
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
    constexpr auto none = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<0>, formula::KeepAtLeast<2>>(
            formula::series<Mass, 4>, relative, formula::Verdict { "repeat" });
    auto const outcome = formula::checked_evaluate<Mass>(formula::sample_mean(none), fourDeterminations);
    return outcome.has_value() ? 0 : 1;
}
