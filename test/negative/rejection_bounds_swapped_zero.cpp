// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: without_outliers takes AtMost<k> and then KeepAtLeast<m>, in that order
//
// The two bounds swapped, each zero: the swap is refused once. Were the
// positive-count checks asked before the order is known, they would read
// KeepAtLeast<0> as AtMost<0> and AtMost<0> as KeepAtLeast<0> and add two
// messages about bounds nobody declared.
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
    constexpr auto swappedZero = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::KeepAtLeast<0>, formula::AtMost<0>>(
            formula::series<Mass, 4>, relative, formula::Verdict { "repeat" });
    auto const outcome = formula::checked_evaluate<Mass>(formula::sample_mean(swappedZero), fourDeterminations);
    return outcome.has_value() ? 0 : 1;
}
