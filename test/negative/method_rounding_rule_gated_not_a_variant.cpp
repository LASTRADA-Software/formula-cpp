// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this argument of variants(...) is not a variant of a method
// REJECT: RequireRoundingRuleMeasuresVariants
// REJECT: RequireDistinctVariantTags
//
// A method whose variants pack holds a bare `int` beside a real variant.
//
// The other route into the rounding rule's gate. `method_rounding_rule_gated`
// reaches it with variants that disagree; this one with a pack that is a
// `Variants`, so it passes the shape rule, but holds something that is not a
// variant and so has no dimension at all. The rounding rule must stay quiet
// and leave the refusal to the pack's own rule. Asked anyway, it would look
// for the pack's agreed dimension, find none, and add the compiler's own
// "no such member" errors under its name -- which is what the REJECT refuses.
//
// The distinct-tag rule is refused the same way, and for the same reason: it
// reads each variant's `tag`, which a bare `int` has not got. The pack's own
// case, `method_variants_agreement_gated`, pins that gate for
// `variants(...)`; this one pins it for a pack reached through `method(...)`.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/method.hpp>

namespace
{
struct Cube
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

inline constexpr auto m =
    formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) ), 42),
                    formula::rounding_rule<formula::unit::Megapascal,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero>(),
                    formula::constraints());
} // namespace

int main()
{
    return static_cast<int>(sizeof(m));
}
