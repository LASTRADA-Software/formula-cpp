// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a retry allows no attempts at all; MaxAttempts must be at least one
// REJECT: allows at most 64 attempts
// REJECT: second attempt allows only one attempt
//
// Zero attempts judged from the second: one mistake, one message -- "no attempts", and not also "second attempt of one".
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
    constexpr auto retrying =
        formula::retry<Estimate, 0, formula::FirstJudged::AtSecondAttempt>(fromZero, halving, settled, repeat, cite);
    return retrying.maxAttempts == 0 ? 1 : 0;
}
