// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this formula reads from another record, but was evaluated against an environment that holds only one
//
// A formula that reads the reference's strength, evaluated against this
// record's plain environment: there is no other record to read from. The
// environment holds no strength either, and the evaluator must not dispatch
// the operand after refusing, or RequireProvided would follow for it -- the
// registration rejects that. The role is plain and not ThisRecord, so the
// scope's other rules have nothing to say.
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
} // namespace

int main()
{
    auto const evaluated =
        formula::checked_evaluate_si<formula::Rational>(formula::from_record<Reference>(formula::var<Strength>), here);
    return evaluated.has_value() ? 0 : 1;
}
