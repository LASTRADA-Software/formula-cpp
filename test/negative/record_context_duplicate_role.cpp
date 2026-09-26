// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record_context binds the same role to more than one record
//
// Four records, with `Reference` bound at positions 2 and 4: not the first
// pair, not adjacent, and not involving this record. The rule compares every
// pair of roles, and this placement is what tells that apart from its likely
// narrowings -- the first pair only, neighbours only, and "every role
// against ThisRecord" only -- each of which finds no repeat here and lets
// the context compile.
//
// The first record is this record's, and every role is a plain class type,
// so neither of the context's other rules has anything to say: the repeat is
// the only thing wrong.
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
constexpr auto before = formula::environment(formula::Measured<Force> { formula::Rational { 30'000 } });

// Two records for Reference: a formula reading from Reference would read one
// of them, and which one would be a guess.
constexpr auto broken = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there),
    formula::record<PriorTest>(formula::record_key(formula::sample_id(17), formula::test_id(3)), before),
    formula::record<Reference>(formula::record_key(formula::sample_id(41), formula::test_id(3)), there));
} // namespace

int main()
{
    return broken.this_record().is_bound() ? 0 : 1;
}
