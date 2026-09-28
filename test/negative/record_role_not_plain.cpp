// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record role is not a plain class type
//
// `Reference const` is a class type, but a different one from `Reference`:
// a context binding both would escape the duplicate-role check, which
// compares roles with `is_same_v`. The record stands alone here, outside any
// context, so no context rule can fire: the qualifier is the only thing
// wrong.
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

constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 60'000 } });

constexpr auto broken =
    formula::record<Reference const>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there);
} // namespace

int main()
{
    return broken.is_bound() ? 0 : 1;
}
