// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this lineage requirement names the same attribute more than once
//
// MaterialBatch named at positions 1 and 3 of 3: not adjacent, so a check of
// neighbours only would miss it, and not the last pair, so a check of the
// last pair only would too. Nothing else is wrong.
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

constexpr auto broken = formula::same_lineage<MaterialBatch, TestMethod, MaterialBatch>();
} // namespace

int main()
{
    (void) broken;
    return 0;
}
