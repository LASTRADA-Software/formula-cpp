// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: two variants of this method measure different dimensions
// REJECT: RequireRoundingRuleMeasuresVariants
//
// A method whose variants disagree -- a pressure first, a length second --
// with a rounding rule in millimetres.
//
// The rounding rule is compared against the dimension the variants AGREE on,
// and these have none, so it must stay quiet and leave the refusal to the
// agreement rule, which names the real mistake. Asked anyway, it would compare
// against the first variant and pile a second error on top.
//
// The rule's unit is deliberately a length, which the SECOND variant measures
// and the first does not. A rounding rule that matched the first variant would
// be quiet even with the gate removed, and this file could not see the gate.
// EXPECT alone cannot see it either, since the agreement rule fires whether or
// not this one is also asked; only the absence of its template name can.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/method.hpp>

namespace
{
struct Cube
{
};
struct Cylinder
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

inline constexpr auto m = formula::method(formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                                            formula::variant<Cylinder>(var<EdgeX>)),
                                          formula::rounding_rule<formula::unit::Millimetre,
                                                                 formula::DecimalPlaces { 1 },
                                                                 formula::RoundingMode::HalfAwayFromZero>(),
                                          formula::constraints());
} // namespace

int main()
{
    return static_cast<int>(std::tuple_size_v<decltype(m.variantSet.cases)>);
}
