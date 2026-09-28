// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record_context binds no record to this role
//
// A formula that reads from a prior test, evaluated against a context that
// binds only this record and the reference. The context is well formed and
// the role is plain and not ThisRecord, so the unbound role is the only
// thing wrong. The evaluator must not dispatch the operand against any
// record after refusing: this record holds no strength, so a fallback to it
// would add RequireProvided, which the registration rejects.
//
// This must not compile.
#include <formula-cpp/formula.hpp>

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
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } });
constexpr auto there = formula::environment(formula::Measured<Strength> { formula::Rational { 4 } });

constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
} // namespace

int main()
{
    auto const evaluated =
        formula::checked_evaluate_si<formula::Rational>(formula::from_record<PriorTest>(formula::var<Strength>), records);
    return evaluated.has_value() ? 0 : 1;
}
