// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: without_outliers takes a series or observations as its sample, not another without_outliers
//
// A rejection of the survivors of another: the outer one would count and
// place its determinations among the inner one's survivors, and its trace
// could not say which it rejected. Refused once, where it is written.
// This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", formula::unit::Gram>
{
};
} // namespace

int main()
{
    constexpr auto gross = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<3>>(
            formula::series<Mass, 5>,
            formula::deviation_from_mean(formula::constant<formula::unit::Gram>(formula::Rational { 5 })),
            formula::Verdict { "repeat" });
    constexpr auto fine = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<3>>(
            gross,
            formula::deviation_from_mean(formula::constant<formula::unit::Gram>(formula::Rational { 2 })),
            formula::Verdict { "repeat" });
    static_cast<void>(fine);
    return 0;
}
