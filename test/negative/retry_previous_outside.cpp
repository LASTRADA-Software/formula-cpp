// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: previous_attempt, this_attempt and attempt_number are only meaningful inside a retry
// REJECT: RequireProvided
// REJECT: no matching
//
// previous_attempt evaluated by checked_evaluate, outside any retry: its own
// message, once, and not the environment's "not provided".
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
    return formula::checked_evaluate<Estimate>(formula::previous_attempt<Estimate>
                                                   + formula::constant<unit::Gram>(formula::Rational { 1 }),
                                               formula::environment())
                   .has_value()
               ? 0
               : 1;
}