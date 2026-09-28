// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: attempt_input reads the determination recorded for the attempt that is running
// REJECT: provides no value for this quantity
// REJECT: no matching
//
// attempt_input evaluated by checked_evaluate, outside any retry: its own
// message, once, and not the environment's "provides no value".
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
    return formula::checked_evaluate<Determination>(formula::attempt_input<Determination>, formula::environment())
                   .has_value()
               ? 0
               : 1;
}
