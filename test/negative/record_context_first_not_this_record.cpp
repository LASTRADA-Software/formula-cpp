// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: the first record of a record_context must be record<ThisRecord>(...)
//
// Both records are here, and each role is bound once, but in the wrong
// order: the context would be the environment of the reference record while
// calling it this one, and every local input would be read from the
// reference. The roles are distinct and plain, so neither the duplicate-role
// rule nor the plain-role rule has anything to say: the order is the only
// thing wrong.
//
// This must not compile.
#include <formula-cpp/record.hpp>

namespace
{
struct Reference
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 60'000 } });

constexpr auto broken = formula::record_context(
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there),
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here));
} // namespace

int main()
{
    return static_cast<int>(broken.this_record().key().test().value());
}
