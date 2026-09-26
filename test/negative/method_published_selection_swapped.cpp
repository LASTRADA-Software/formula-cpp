// SPDX-License-Identifier: Apache-2.0
// EXPECT: published_positions_must_increase_and_stay_below_the_published_count
//
// A selection that keeps both variants, in the opposite order:
// `v.published = v.published.select<1, 0>()` would swap the positions a trace
// reports for them. The positions to keep are distinct and below the count, so
// a rule asking only for that lets it through; `select` holds them to the
// layout's own rule, which also asks that they increase, as a pin or a prune
// keeps them.
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
    constexpr auto swapped = pack.published.select<1, 0>();
    return swapped.count() == 2 ? 0 : 1;
}
