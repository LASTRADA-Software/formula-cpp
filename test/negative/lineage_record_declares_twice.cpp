// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record declares the same lineage attribute more than once
//
// A record with two keys for MaterialBatch, at positions 1 and 3 of 3: a
// requirement on it would compare one of them, and which one would be a
// guess. Every entry is a lineage entry, so the entry rule has nothing to
// say, and the role is plain: the repeat is the only thing wrong.
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

constexpr auto broken =
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there,
                               formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12),
                               formula::lineage<MaterialBatch>(4412));
} // namespace

int main()
{
    return broken.is_bound() ? 0 : 1;
}
