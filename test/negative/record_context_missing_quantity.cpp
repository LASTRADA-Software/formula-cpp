// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this environment provides no value for this quantity
//
// A context whose own record holds no `Ratio`, asked for one. The reference
// record does hold one, and must not be read instead: a context is its own
// record's environment, so it is refused exactly as that environment is, by
// `RequireProvided`, once. Its message names the environment the context
// inherits, `Environment<Measured<Force>>`, since that is the one asked.
//
// The registration rejects the context's own rules, which have nothing to
// say about a context that is well formed.
//
// This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Reference
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct Ratio: formula::Quantity<Ratio, "r", "a ratio", formula::unit::One>
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 60'000 } },
                                            formula::Measured<Ratio> { formula::Rational { 1, 4 } });

constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
} // namespace

int main()
{
    auto const evaluated = formula::checked_evaluate<Ratio>(formula::var<Ratio>, records);
    return evaluated.has_value() ? 0 : 1;
}
