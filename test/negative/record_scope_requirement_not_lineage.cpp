// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a record scope's requirement is not a lineage requirement
//
// A scope spelt by hand with an int where its requirement belongs. The
// factories only ever give a lineage requirement or none; the class body
// refuses anything else, so that an aggregate spelling cannot smuggle a
// requirement nobody checks. The role is plain and not ThisRecord, so the
// scope's other rules have nothing to say.
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

constexpr auto broken = formula::RecordScopeNode<Reference, int, formula::VarNode<Force>> { {}, formula::var<Force> };
} // namespace

int main()
{
    (void) broken;
    return 0;
}
