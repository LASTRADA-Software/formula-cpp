// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: without_outliers takes AtMost<k> and then KeepAtLeast<m>, in that order
//
// A bound that is neither AtMost<k> nor KeepAtLeast<m>: refused by the same
// check, once. Every count is read through detail::bound_value, so nothing
// else asks `int` for a `value` it does not have.
// This must not compile.
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
    constexpr auto notABound =
        formula::without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, int, formula::KeepAtLeast<2>>(
            formula::series<Mass, 4>, relative, formula::Verdict { "repeat" });
    auto const outcome = formula::checked_evaluate<Mass>(formula::sample_mean(notABound), fourDeterminations);
    return outcome.has_value() ? 0 : 1;
}
