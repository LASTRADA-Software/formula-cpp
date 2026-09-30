// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this result quantity does not measure the dimension this expression computes
// REJECT: formula: this result quantity does not measure what the rejection's determinations measure
//
// A rejection of masses bound to a length: refused where it is bound, once.
// Evaluated and traced, it adds nothing: the rejection's own check of its
// result, which would say the same thing in other words, is not asked of a
// refused Yields.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

struct Mass: formula::Quantity<Mass, "m", "mass of a determination", formula::unit::Gram>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

// Invented determinations, in grams.
inline constexpr auto fixture = formula::environment(formula::measured_series<Mass>(formula::Measured<Mass> { 41 },
                                                                                    formula::Measured<Mass> { 43 },
                                                                                    formula::Measured<Mass> { 47 },
                                                                                    formula::Measured<Mass> { 53 },
                                                                                    formula::Measured<Mass> { 59 }));

int main()
{
    constexpr auto mostExtreme = formula::PerPass::MostExtreme;
    constexpr auto keep = formula::OnLimit::Keep;
    constexpr auto rejection = formula::without_outliers<mostExtreme, keep, formula::AtMost<1>, formula::KeepAtLeast<3>>(
        formula::series<Mass, 5>,
        formula::deviation_from_mean(formula::Rational { 6, 100 } * formula::pass_mean<Mass>),
        formula::Verdict { "repeat the determinations" },
        formula::Citation { .title = "Example Standard" });
    constexpr auto refused = formula::yields<Length>(rejection);
    auto const evaluated = formula::checked_evaluate_rejection(refused, fixture);
    auto const explained = formula::explain_rejection(refused, fixture);
    return evaluated.has_value() && explained.outcome.has_value() ? 0 : 1;
}
