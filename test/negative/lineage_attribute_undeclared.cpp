// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this lineage requirement compares an attribute the record does not declare
//
// CuringRegime is compared, and NEITHER record declares it. That draws one
// message, not one per record: the compared record is asked only once the
// read record is known to declare the attribute. The message names the
// attribute and the read record's role; the other attributes are declared
// on both, and the context is well formed. The one message is counted by
// hand (see the phase 14 task 6 report), since REJECT cannot refuse a second
// copy of it.
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
struct TestMethod
{
};
struct CuringRegime
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 60'000 } });

constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                         formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12)),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there,
                               formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12)));
} // namespace

int main()
{
    auto const gated = formula::from_record<Reference>(
        formula::var<Force>, formula::same_lineage<MaterialBatch, TestMethod, CuringRegime>());
    auto const evaluated = formula::checked_evaluate_si<formula::Rational>(gated, records);
    return evaluated.has_value() ? 0 : 1;
}
