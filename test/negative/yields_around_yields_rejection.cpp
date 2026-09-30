// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this formula is bound to its result quantity already
// REJECT: no viable conversion
// REJECT: no matching
//
// A rejection of outliers bound to its result quantity, bound again.
// Refused where it is written, once. Evaluated and traced as a rejection, it
// adds nothing: the rejection's verbs take a bound formula around a bound
// formula only to stay silent about it, where they would otherwise find no
// overload to take it.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

using Mass = formula::Quantity<struct MassTag, "m", "mass of a determination", formula::unit::Gram>;

// Invented determinations, in grams.
inline constexpr auto inputs = formula::environment(formula::measured_series<Mass>(41, 43, 47, 53, 59));

int main()
{
    constexpr auto mostExtreme = formula::PerPass::MostExtreme;
    constexpr auto keep = formula::OnLimit::Keep;
    constexpr auto settled =
        formula::yields<Mass>(formula::without_outliers<mostExtreme, keep, formula::AtMost<1>, formula::KeepAtLeast<3>>(
            formula::series<Mass, 5>,
            formula::deviation_from_mean(formula::Rational { 6, 100 } * formula::pass_mean<Mass>),
            formula::Verdict { "repeat the determinations" },
            formula::Citation { .title = "Example Standard" }));
    constexpr auto rebound = formula::yields<Mass>(settled);
    auto const evaluated = formula::checked_evaluate_rejection(rebound, inputs);
    auto const explained = formula::explain_rejection(rebound, inputs);
    return evaluated.has_value() && explained.outcome.has_value() ? 0 : 1;
}
