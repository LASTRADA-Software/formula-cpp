// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this lineage requirement names no attribute
//
// same_lineage<>() names nothing, so it would check nothing while reading in
// the source as a check. The requirement stands alone here, so no record or
// scope rule can fire: the empty list is the only thing wrong.
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

constexpr auto broken = formula::same_lineage<>();
} // namespace

int main()
{
    (void) broken;
    return 0;
}
