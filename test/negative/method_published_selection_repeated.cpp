// SPDX-License-Identifier: Apache-2.0
// EXPECT: published_positions_must_increase_and_stay_below_the_published_count
//
// A selection that keeps the same variant twice. `select` is how an overlay
// carries a published layout through a pin or a prune, copying the published
// positions of the variants it keeps out of a layout already known to be
// valid; a selection naming one variant twice would copy one published
// position into two places. The positions to keep are template arguments, and
// `select` holds them to the layout's own rule at compile time.
//
// This must not compile.
#include <formula-cpp/method.hpp>

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

inline constexpr auto pack = formula::variants(formula::variant<Cube>(var<EdgeX>), formula::variant<Cylinder>(var<EdgeX>));
} // namespace

int main()
{
    constexpr auto twice = pack.published.select<1, 1>();
    return twice.count() == 2 ? 0 : 1;
}
