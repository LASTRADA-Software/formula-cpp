// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a record holds its values in an environment(...), and this is not one
//
// A context given as this record's values, and a formula then evaluated
// through the context built over that record. The inner context is well
// formed, and so is the outer one apart from its record's values, so the
// mistake is one: a record with records of its own. It must draw one
// message. After the refusal the record holds the inner context's own
// record's environment, so the evaluation reads the force and the edge from
// there and adds no RequireProvided for either.
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
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 85'902 } },
                                           formula::Measured<EdgeX> { formula::Rational { 139 } });
constexpr auto inner = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here));
constexpr auto outer = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(41), formula::test_id(5)), inner));
constexpr auto strength = formula::var<Force> / (formula::var<EdgeX> * formula::var<EdgeX>);
} // namespace

int main()
{
    return formula::checked_evaluate<Strength>(strength, outer).has_value() ? 0 : 1;
}
