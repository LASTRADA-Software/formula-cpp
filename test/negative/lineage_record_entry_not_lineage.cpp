// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a record takes only lineage entries after its environment
//
// Two bare integers where lineage entries belong. The duplicate-attribute
// rule is gated on every entry being a lineage entry: without the gate the
// two integers would look like two keys for the same attribute, and this one
// mistake would draw that message too. The registration rejects it.
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

constexpr auto broken = formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)),
                                                   there, formula::lineage<MaterialBatch>(4411), 12, 13);
} // namespace

int main()
{
    return broken.is_bound() ? 0 : 1;
}
