// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this lineage requirement compares the record it reads with itself
//
// A read from Reference that requires Reference to agree with Reference:
// always satisfied, so it checks nothing while reading as a check. Refused
// in the scope's class body, where the role and the comparand meet; the
// requirement itself is well formed. Nothing else is wrong.
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

constexpr auto broken = formula::from_record<Reference>(
    formula::var<Force>, formula::same_lineage<MaterialBatch>(formula::against<Reference>));
} // namespace

int main()
{
    (void) broken;
    return 0;
}
