// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record role is not a plain class type
//
// `ThisRecord const` as the first record of a context. Removing the
// qualifier fixes everything, so this is one mistake and must draw one
// message: the plain-role rule's. The first-record rule deliberately ignores
// cv-qualifiers so that it does not report the same mistake a second time,
// and the registration rejects its text.
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
    formula::record<formula::ThisRecord const>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
} // namespace

int main()
{
    return static_cast<int>(broken.this_record().key().test().value());
}
