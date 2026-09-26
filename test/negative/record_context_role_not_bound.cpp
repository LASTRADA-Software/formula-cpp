// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record_context binds no record to this role
//
// A context asked directly for the record of a role it does not bind. The
// refusal is a static_assert in record<Role>(), not a constraint, so it is
// this library's message; afterwards the call returns this record, so the
// key read below draws nothing more. The context is well formed, so its own
// rules have nothing to say.
//
// This must not compile.
#include <formula-cpp/record.hpp>

namespace
{
struct Reference
{
};
struct PriorTest
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
} // namespace

int main()
{
    return records.record<PriorTest>().key().has_value() ? 0 : 1;
}
