// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: without_outliers takes AtMost<k> and then KeepAtLeast<m>, in that order
//
// The two bounds swapped: without the refusal this would reject up to four
// and keep two. Refused once, naming both; the positive-count and the
// limit's dimension checks are asked only once the bounds are in order.
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
    constexpr auto swapped = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::KeepAtLeast<4>, formula::AtMost<2>>(
            formula::series<Mass, 4>, relative, formula::Verdict { "repeat" });
    auto const outcome = formula::checked_evaluate<Mass>(formula::sample_mean(swapped), fourDeterminations);
    return outcome.has_value() ? 0 : 1;
}
