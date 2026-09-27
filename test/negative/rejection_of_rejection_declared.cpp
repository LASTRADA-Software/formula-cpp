// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: without_outliers takes a series or observations as its sample, not another without_outliers
//
// A rejection of another's survivors, declared without the factory: the
// sample's type is decltype of a constexpr node, so it is const. Every
// check sits in the class body and sees through that. This must not
// compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", formula::unit::Gram>
{
};
} // namespace

int main()
{
    constexpr auto criterion = formula::deviation_from_mean(formula::constant<formula::unit::Gram>(formula::Rational { 5 }));
    constexpr auto inner = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<3>>(
            formula::observations<Mass, 8>, criterion, formula::Verdict { "repeat" });
    constexpr formula::RejectionNode<formula::PerPass::MostExtreme,
                                     formula::OnLimit::Keep,
                                     formula::AtMost<1>,
                                     formula::KeepAtLeast<3>,
                                     decltype(inner),
                                     decltype(criterion)>
        outer { inner, criterion, formula::Verdict { "repeat" }, {} };
    static_cast<void>(outer);
    return 0;
}
