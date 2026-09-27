// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: deviation_in_stddevs needs at least 3 determinations in every pass it decides
//
// Keeping at least two under a criterion in standard deviations: a pass that
// reaches two could not be decided, and the result would fail though
// neither bound was passed. Refused once, where it is written. This must
// not compile.
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
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<3>, formula::KeepAtLeast<2>>(
            formula::series<Mass, 4>,
            formula::deviation_in_stddevs(formula::number(formula::Rational { 1, 2 })),
            formula::Verdict { "repeat" });
    static_cast<void>(rejection);
    return 0;
}
