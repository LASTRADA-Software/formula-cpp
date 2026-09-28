// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this_attempt is the value an attempt produced, known only when judging it
// REJECT: only meaningful inside a retry
// REJECT: no matching
//
// this_attempt in the attempt expression: the value it names is still being computed.
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
        fromZero, formula::this_attempt<Estimate> / formula::Rational { 2 }, settled, repeat, cite);
    return formula::checked_evaluate_retry(retrying, formula::environment()).has_value() ? 0 : 1;
}
