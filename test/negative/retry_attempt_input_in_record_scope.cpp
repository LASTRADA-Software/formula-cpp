// SPDX-License-Identifier: Apache-2.0
// EXPECT: inside from_record<Role>(...) the other record's data is read, where no attempt runs
// REJECT: no matching
//
// attempt_input inside from_record<Role>(...) in a retry's attempt: the
// scope reads the other record's data, where no attempt runs. Told so, and
// not only that the node is outside a retry, which it is written inside.
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

struct ReferenceRecord
{
};
} // namespace

#include <tuple>
#include <type_traits>

int main()
{
    auto const here = formula::environment(formula::measured_series<Tolerance>(
        formula::Measured<Tolerance> { formula::Rational { 1 } }, formula::Measured<Tolerance> { formula::Rational { 1 } }));
    auto const both = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::record<ReferenceRecord>(formula::record_key(formula::sample_id(23), formula::test_id(3)), here));
    constexpr auto retrying = formula::retry<Estimate, 2, formula::FirstJudged::AtFirstAttempt>(
        fromZero,
        formula::constant<unit::Gram>(formula::Rational { 1 })
            + formula::from_record<ReferenceRecord>(formula::attempt_input<Tolerance>),
        settled,
        repeat,
        cite);
    return formula::checked_evaluate_retry(retrying, both).has_value() ? 0 : 1;
}