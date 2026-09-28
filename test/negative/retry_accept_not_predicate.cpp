// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a retry's acceptance is a comparison that holds or does not
// REJECT: no matching
// REJECT: checked_evaluate_predicate
//
// An acceptance that computes a value instead of comparing: refused in this library's words, and the retry is then never
// evaluated.
#include <formula-cpp/retry.hpp>

namespace
{
namespace unit = formula::unit;

struct Estimate: formula::Quantity<Estimate, "w", "an invented iterated estimate", unit::Gram>
{
};

inline constexpr auto halving = formula::constant<unit::Gram>(formula::Rational { 152, 25 })
                                + formula::previous_attempt<Estimate> / formula::Rational { 2 };
inline constexpr auto settled = formula::previous_attempt<Estimate> - formula::this_attempt<Estimate>
                                >= formula::constant<unit::Gram>(formula::Rational { -19, 25 });
inline constexpr auto fromZero = formula::starting_from(formula::constant<unit::Gram>(formula::Rational { 0 }));
inline constexpr formula::Citation cite { .reference = "Example Standard 12", .section = "6" };
inline constexpr formula::Verdict repeat { "repeat the determination" };
} // namespace
int main()
{
    constexpr auto retrying = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(
        fromZero, halving, formula::this_attempt<Estimate> - formula::previous_attempt<Estimate>, repeat, cite);
    return formula::checked_evaluate_retry(retrying, formula::environment()).has_value() ? 0 : 1;
}
