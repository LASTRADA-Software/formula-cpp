// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this_attempt is the value an attempt produced, known only when judging it
// REJECT: only meaningful inside a retry
// REJECT: names another
// REJECT: does not compute the dimension
//
// this_attempt<Estimate> in the attempt expression, and the retry only
// rendered -- never evaluated, as a method page would. Refused where the
// retry is built, so no page prints w(k) = 1 g + w(k) / 2.
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
    constexpr auto circular =
        formula::constant<unit::Gram>(formula::Rational { 1 }) + formula::this_attempt<Estimate> / formula::Rational { 2 };
    constexpr auto retrying =
        formula::retry<Estimate, 2, formula::FirstJudged::AtFirstAttempt>(fromZero, circular, settled, repeat, cite);
    return formula::render(retrying).empty() ? 1 : 0;
}