// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: from_record<ThisRecord> reads this record as though it were another
//
// A scope naming `ThisRecord const`. It can only mean this record, and is
// refused as ThisRecord is. No context binds a cv-qualified role, so the
// bound-role rule would report it too, for the same one mistake, unless the
// evaluator returns at once for a scope its node refused: the registration
// rejects that second message.
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

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 60'000 } });

constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
} // namespace

int main()
{
    auto const evaluated = formula::checked_evaluate_si<formula::Rational>(
        formula::from_record<formula::ThisRecord const>(formula::var<Force>), records);
    return evaluated.has_value() ? 0 : 1;
}
