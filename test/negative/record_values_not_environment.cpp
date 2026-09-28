// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a record holds its values in an environment(...), and this is not one
//
// The likely slip: a bare `Measured<Force>` given as this record's values,
// where `environment(...)` was meant. A formula of three quantities is then
// evaluated through a context over that record. The mistake is one, and
// must draw one message however many quantities are read: after refusing,
// `record()` gives the record a detail::AbsentEnvironment, which answers
// every quantity as absent, so no RequireProvided follows for any of them.
// The registration rejects that, and the one message is counted by hand.
//
// This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", formula::unit::Millimetre>
{
};
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)),
                                         formula::Measured<Force> { formula::Rational { 90'000 } }));
constexpr auto strength = formula::var<Force> / (formula::var<EdgeX> * formula::var<EdgeY>);
} // namespace

int main()
{
    return formula::checked_evaluate<Strength>(strength, records).has_value() ? 0 : 1;
}
