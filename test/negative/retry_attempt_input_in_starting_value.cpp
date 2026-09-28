// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a retry's starting value is evaluated before its first attempt, so there is no recorded determination
// REJECT: only meaningful inside a retry
// REJECT: provides no value for this quantity
// REJECT: no matching
//
// attempt_input in the starting value: written inside the retry, but read
// before any attempt -- told so, and not that it is outside a retry.
#include <formula-cpp/method.hpp>
#include <formula-cpp/retry.hpp>

namespace
{
namespace unit = formula::unit;

struct Determination: formula::Quantity<Determination, "d", "an invented determination", unit::Gram>
{
};
struct Agreed: formula::Quantity<Agreed, "d_a", "an invented agreed determination", unit::Gram>
{
};

inline constexpr auto agree = formula::when(formula::this_attempt<Agreed> >= formula::previous_attempt<Agreed>,
                                            formula::this_attempt<Agreed> - formula::previous_attempt<Agreed>,
                                            formula::previous_attempt<Agreed> - formula::this_attempt<Agreed>)
                              <= formula::constant<unit::Gram>(formula::Rational { 127, 100 });
inline constexpr formula::Citation cite { .reference = "Example Standard 12", .section = "7" };
inline constexpr formula::Verdict repeat { "repeat the test" };
} // namespace
int main()
{
    constexpr auto fromRecorded = formula::retry<Agreed, 4, formula::FirstJudged::AtFirstAttempt>(
        formula::starting_from(formula::attempt_input<Determination>),
        formula::previous_attempt<Agreed> + formula::constant<unit::Gram>(formula::Rational { 0 }),
        agree,
        repeat,
        cite);
    auto const four = formula::environment(
        formula::measured_series<Determination>(formula::Measured<Determination> { formula::Rational { 413, 10 } },
                                                formula::Measured<Determination> { formula::Rational { 439, 10 } },
                                                formula::Measured<Determination> { formula::Rational { 427, 10 } },
                                                formula::Measured<Determination> { formula::Rational { 457, 10 } }));
    return formula::checked_evaluate_retry(fromRecorded, four).has_value() ? 0 : 1;
}
