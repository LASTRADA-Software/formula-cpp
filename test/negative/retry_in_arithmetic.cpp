// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a retry is evaluated at the top, by checked_evaluate_retry; it cannot stand in a formula
// REJECT: no matching
// REJECT: invalid operands
// REJECT: no match for
// REJECT: RequireResultDimension
//
// A retry in arithmetic, then evaluated, rendered and documented: refused
// once, in this library's words, and the refused value asks nothing more of
// any of the three.
#include <formula-cpp/document.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/render.hpp>
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
    constexpr auto misused = four + formula::constant<unit::Gram>(formula::Rational { 1 });
    auto const shown = formula::render(misused);
    auto const written = formula::document(misused);
    return formula::checked_evaluate<Estimate>(misused, formula::environment()).has_value() && !shown.empty()
                   && !written.formula.empty()
               ? 0
               : 1;
}
