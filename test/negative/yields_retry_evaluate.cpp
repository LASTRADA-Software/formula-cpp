// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a retry is evaluated at the top, by checked_evaluate_retry
// REJECT: no matching
//
// A bound retry handed to each verb that answers with one value: evaluate,
// checked_evaluate, explain, checked_explain, trace_of and define. Refused as
// checked_evaluate refuses a retry, pointing at checked_evaluate_retry --
// rather than an overload nobody matched. A retry of its own number of
// attempts for each verb, so that each refusal is its own message: six.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

#include <cstddef>

using Estimate = formula::Quantity<struct EstimateTag, "w", "an invented iterated estimate", formula::unit::Gram>;

inline constexpr auto inputs = formula::environment();

// w_k = 6.08 g + w_{k-1} / 2 from 0 g, accepted once it rises by at most
// 0.76 g, within Max attempts.
template <std::size_t Max>
[[nodiscard]] constexpr auto bound_retry()
{
    return formula::yields<Estimate>(formula::retry<Estimate, Max, formula::FirstJudged::AtFirstAttempt>(
        formula::starting_from(formula::constant<formula::unit::Gram>(formula::Rational { 0 })),
        formula::constant<formula::unit::Gram>(formula::Rational { 152, 25 })
            + formula::previous_attempt<Estimate> / formula::Rational { 2 },
        formula::previous_attempt<Estimate> - formula::this_attempt<Estimate>
            >= formula::constant<formula::unit::Gram>(formula::Rational { -19, 25 }),
        formula::Verdict { "repeat the determination" },
        formula::Citation { .reference = "Example Standard 12" }));
}

int main()
{
    auto const evaluated = formula::evaluate(bound_retry<1>(), inputs);
    auto const checked = formula::checked_evaluate(bound_retry<2>(), inputs);
    auto const explained = formula::explain(bound_retry<3>(), inputs);
    auto const checkedExplained = formula::checked_explain(bound_retry<4>(), inputs);
    auto const traced = formula::trace_of(bound_retry<5>(), inputs);
    auto const defined = formula::define(bound_retry<6>());
    return evaluated.is_value() && checked.has_value() && explained.outcome.is_value() && checkedExplained.has_value()
                   && !traced.empty() && decltype(defined)::valid
               ? 0
               : 1;
}
