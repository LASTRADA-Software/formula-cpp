// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a retry's starting value is evaluated before its first attempt, so no attempt exists yet
// REJECT: names another
// REJECT: only meaningful inside a retry
// REJECT: does not compute the dimension
//
// previous_attempt<Tolerance> as a retry of Estimate's starting value, and the
// retry only rendered -- never evaluated, as a method page would. Refused
// where the retry is built, as the read before any attempt that it is,
// whatever quantity it names.
#include <formula-cpp/render.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/retry.hpp>

namespace
{
namespace unit = formula::unit;

struct Estimate: formula::Quantity<Estimate, "w", "an invented iterated estimate", unit::Gram>
{
};
struct Tolerance: formula::Quantity<Tolerance, "t_w", "an invented tolerance", unit::Gram>
{
};
struct Span: formula::Quantity<Span, "L_s", "an invented span", unit::Metre>
{
};

inline constexpr auto halving = formula::constant<unit::Gram>(formula::Rational { 152, 25 })
                                + formula::previous_attempt<Estimate> / formula::Rational { 2 };
inline constexpr auto settled = formula::previous_attempt<Estimate> - formula::this_attempt<Estimate>
                                >= formula::constant<unit::Gram>(formula::Rational { -19, 25 });
inline constexpr auto fromZero = formula::starting_from(formula::constant<unit::Gram>(formula::Rational { 0 }));
inline constexpr formula::Citation cite { .reference = "Example Standard 12", .section = "6" };
inline constexpr formula::Verdict repeat { "repeat the determination" };
inline constexpr auto four =
    formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, settled, repeat, cite);

struct Fitted
{
};
} // namespace

#include <tuple>
#include <type_traits>

int main()
{
    constexpr auto retrying = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(
        formula::starting_from(formula::previous_attempt<Tolerance>), halving, settled, repeat, cite);
    return formula::render(retrying).empty() ? 1 : 0;
}
