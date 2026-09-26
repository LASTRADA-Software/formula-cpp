// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a record holds its values in an environment(...), and this is not one
//
// A context given as a record's values: a record with records of its own.
// A formula reads another record through the context, never through a
// record nested in one. The context itself is well formed and the role is
// plain, so no other rule has anything to say. After the refusal the call
// returns a record over the context's own record's environment, so reading
// its key and evaluating through it, below, draw nothing more: no
// RequireProvided for the force.
//
// This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Reference
{
};
struct Nested
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 60'000 } });

constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));

constexpr auto broken = formula::record<Nested>(formula::record_key(formula::sample_id(41), formula::test_id(3)), records);
} // namespace

int main()
{
    auto const force = formula::checked_evaluate_si<formula::Rational>(formula::var<Force>, broken.environment());
    return broken.key().has_value() && force.has_value() ? 0 : 1;
}
