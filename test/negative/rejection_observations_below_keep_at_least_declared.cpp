// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: without_outliers keeps at least m determinations of observations that hold at most fewer
//
// Room for four observations, to keep at least nine, declared without the
// factory: the sample's type is decltype of a constexpr node, so it is
// const. Every check sits in the class body and sees through that. This
// must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", formula::unit::Gram>
{
};
} // namespace

int main()
{
    constexpr auto criterion = formula::deviation_from_mean(formula::constant<formula::unit::Gram>(formula::Rational { 1 }));
    constexpr auto four = formula::observations<Mass, 4>;
    constexpr formula::RejectionNode<formula::PerPass::MostExtreme,
                                     formula::OnLimit::Keep,
                                     formula::AtMost<1>,
                                     formula::KeepAtLeast<9>,
                                     decltype(four),
                                     decltype(criterion)>
        rejection { four, criterion, formula::Verdict { "repeat" }, {} };
    static_cast<void>(rejection);
    return 0;
}
