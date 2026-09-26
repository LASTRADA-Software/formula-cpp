// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this sink defines only one of record_entered and lineage_checked
//
// A sink of a consumer's own that is told the record a scope reads, but not
// the lineage attributes it compares. Told of neither, its trace of a refused
// read would show no origin and no attribute that refused it; told of one, it
// would show half. So it is refused, once, and the evaluator then reports to
// neither hook, so nothing else follows. The context and the requirement are
// well formed: the sink is the only thing wrong.
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

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 60'000 } });

constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                         formula::lineage<MaterialBatch>(4411)),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there,
                               formula::lineage<MaterialBatch>(4412)));

/// Knows the origin, not the attributes.
struct HalfSink
{
    int* origins;

    template <formula::Node N>
    void entered(N const&) const
    {
    }
    template <formula::Node N>
    void produced(N const&, formula::Evaluated<formula::Rational> const&) const
    {
    }
    void record_entered(formula::RecordOrigin const&) const
    {
        ++*origins;
    }
};
} // namespace

int main()
{
    int origins = 0;
    auto const gated = formula::from_record<Reference>(formula::var<Force>, formula::same_lineage<MaterialBatch>());
    auto const evaluated = formula::checked_evaluate_si<formula::Rational>(gated, records, HalfSink { &origins });
    return evaluated.has_value() ? origins : -1;
}
