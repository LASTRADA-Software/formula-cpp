// SPDX-License-Identifier: Apache-2.0
// EXPECT: published_positions_must_increase_and_stay_below_the_published_count
//
// A variants pack whose published layout swaps its two variants: `{ 1, 0 }` of
// 2. Each position is below the count and the two are distinct, so a rule
// asking only for that lets it through -- and a trace then reports the Cube as
// the 2nd of 2 and the Cylinder as the 1st. No overlay can produce it: a pin
// or a prune keeps its variants in declaration order, so their published
// positions always increase.
//
// This must not compile.
#include <formula-cpp/method.hpp>

#include <tuple>

namespace
{
struct Cube
{
};
struct Cylinder
{
};

struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

using Pack = formula::Variants<formula::VariantCase<Cube, formula::VarNode<EdgeX>>,
                               formula::VariantCase<Cylinder, formula::VarNode<EdgeX>>>;
} // namespace

int main()
{
    constexpr Pack pack { std::tuple { formula::variant<Cube>(var<EdgeX>), formula::variant<Cylinder>(var<EdgeX>) },
                          formula::detail::PublishedLayout<2> { { 1, 0 }, 2 } };
    return pack.published.count() == 2 ? 0 : 1;
}
