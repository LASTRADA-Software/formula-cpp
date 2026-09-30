// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this is a rejection of outliers, not a single value
// REJECT: no matching
//
// A bound rejection of outliers handed to each verb that answers with one
// value: evaluate, checked_evaluate, explain, checked_explain, trace_of and
// define. Its result is more than a value, and it has verbs of its own, which
// the refusal names -- rather than an overload nobody matched. A rejection of
// its own bound for each verb, so that each refusal is its own message: six.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

#include <cstddef>

using Mass = formula::Quantity<struct MassTag, "m", "mass of a determination", formula::unit::Gram>;

// Invented determinations, in grams.
inline constexpr auto inputs = formula::environment(formula::measured_series<Mass>(41, 43, 47, 53, 59, 61));

// A 6 % rejection that rejects at most K of the six determinations.
template <std::size_t K>
[[nodiscard]] constexpr auto bound_rejection()
{
    constexpr auto mostExtreme = formula::PerPass::MostExtreme;
    constexpr auto keep = formula::OnLimit::Keep;
    return formula::yields<Mass>(formula::without_outliers<mostExtreme, keep, formula::AtMost<K>, formula::KeepAtLeast<1>>(
        formula::series<Mass, 6>,
        formula::deviation_from_mean(formula::Rational { 6, 100 } * formula::pass_mean<Mass>),
        formula::Verdict { "repeat the determinations" },
        formula::Citation { .title = "Example Standard" }));
}

int main()
{
    auto const evaluated = formula::evaluate(bound_rejection<1>(), inputs);
    auto const checked = formula::checked_evaluate(bound_rejection<2>(), inputs);
    auto const explained = formula::explain(bound_rejection<3>(), inputs);
    auto const checkedExplained = formula::checked_explain(bound_rejection<4>(), inputs);
    auto const traced = formula::trace_of(bound_rejection<5>(), inputs);
    auto const defined = formula::define(bound_rejection<6>());
    return evaluated.is_value() && checked.has_value() && explained.outcome.is_value() && checkedExplained.has_value()
                   && !traced.empty() && decltype(defined)::valid
               ? 0
               : 1;
}
