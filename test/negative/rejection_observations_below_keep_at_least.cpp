// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: without_outliers keeps at least m determinations of observations that hold at most fewer
//
// Room for eight observations, to keep at least nine: every evaluation
// would give the verdict before a single pass, as for a series that holds
// fewer. Refused once, where it is written. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", formula::unit::Gram>
{
};
} // namespace

int main()
{
    constexpr auto rejection = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<9>>(
            formula::observations<Mass, 8>,
            formula::deviation_from_mean(formula::constant<formula::unit::Gram>(formula::Rational { 1 })),
            formula::Verdict { "repeat" });
    static_cast<void>(rejection);
    return 0;
}
