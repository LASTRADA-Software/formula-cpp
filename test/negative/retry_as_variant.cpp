// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a retry is evaluated at the top, by checked_evaluate_retry; it cannot stand in a formula
// REJECT: no matching
// REJECT: RequireVariantsAgree
// REJECT: RequireRoundingRuleMeasuresVariants
//
// A retry as a method's variant: refused once, and the method around it
// finds nothing further to refuse.
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
    constexpr auto method = formula::method(
        formula::variants(formula::variant<Fitted>(four)),
        formula::rounding_rule<unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    return std::tuple_size_v<decltype(method.variantSet.cases)> == 0 ? 1 : 0;
}
