// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: from_record was given a series, raw observations or a curve
//
// `record_scope_of_series` with a lineage requirement: refused in the same
// words, with one message.
//
// This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Reference
{
};
struct MaterialBatch
{
};

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

constexpr auto screens = formula::environment(formula::measured_series<Retained>(
    formula::Measured<Retained> { formula::Rational { 130 } }, formula::Measured<Retained> { formula::Rational { 210 } }));
constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), screens,
                                         formula::lineage<MaterialBatch>(4411)),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), screens,
                               formula::lineage<MaterialBatch>(4411)));
} // namespace

int main()
{
    constexpr auto read = formula::sum(formula::series<Retained, 2>)
                          / formula::from_record<Reference>(formula::series<Retained, 2>,
                                                            formula::same_lineage<MaterialBatch>());
    return formula::checked_evaluate_si<formula::Rational>(read, records).has_value() ? 0 : 1;
}
