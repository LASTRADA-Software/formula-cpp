// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: attempt_input reads the determination recorded for the attempt that is running
// REJECT: provides no value for this quantity
// REJECT: no matching
//
// attempt_input in a formula documented on its own, outside any retry: refused
// where the page is made, so that no page lists a series of no determinations.
#include <formula-cpp/document.hpp>
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
    return formula::document(formula::attempt_input<Determination> * formula::Rational { 2 }).symbols.empty() ? 1 : 0;
}
