// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a retry allows at most 64 attempts; the methods this shape exists for repeat a step a few times
// REJECT: allows no attempts at all
// REJECT: second attempt allows only one attempt
//
// A hundred and three attempts: over the cap of 64, most likely a typo, refused where it is written.
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
        formula::retry<Estimate, 103, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, settled, repeat, cite);
    return retrying.maxAttempts == 0 ? 1 : 0;
}
