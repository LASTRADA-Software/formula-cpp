// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this formula reads from another record, but was evaluated against an environment that holds only one
//
// A scope inside a scope: the reference's record read "from the reference".
// A record has no records of its own, and there is no guard for this: the
// inner scope is evaluated against the reference record's plain
// environment, and the plain-environment refusal fires, once. The inner
// scope's role is bound in the outer context, so a bound-role message here
// would mean the inner scope looked at the wrong context; and the operand is
// not dispatched after the refusal, so no RequireProvided follows. Both are
// rejected. The one expected message is counted by hand, since REJECT cannot refuse a second copy of it.
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
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
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
    auto const nested = formula::from_record<Reference>(formula::from_record<Reference>(formula::var<Strength>));
    auto const evaluated = formula::checked_evaluate_si<formula::Rational>(nested, records);
    return evaluated.has_value() ? 0 : 1;
}
