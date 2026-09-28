// SPDX-License-Identifier: Apache-2.0
// EXPECT: this environment holds a series of a different length for this quantity than the one read
// REJECT: provides no value for this quantity
// REJECT: no matching
//
// A retry of at most 4 attempts reads one recorded determination per attempt,
// a series of 4; the environment holds 3. The environment's own message,
// naming both lengths as RequireSeriesLength's Supplied = 3 and Read = 4,
// once -- and not also "provides no value", which would say nothing was
// recorded at all.
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
    constexpr auto successive = formula::retry<Agreed, 4, formula::FirstJudged::AtSecondAttempt>(
        formula::attempt_input<Determination>, agree, repeat, cite);
    auto const three = formula::environment(
        formula::measured_series<Determination>(formula::Measured<Determination> { formula::Rational { 413, 10 } },
                                                formula::Measured<Determination> { formula::Rational { 439, 10 } },
                                                formula::Measured<Determination> { formula::Rational { 427, 10 } }));
    return formula::checked_evaluate_retry(successive, three).has_value() ? 0 : 1;
}
