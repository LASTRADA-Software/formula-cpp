// SPDX-License-Identifier: Apache-2.0
// EXPECT: published_positions_must_increase_and_stay_below_the_published_count
//
// A variants pack whose published layout names position 1 for both of its
// variants, each below the count of 3 -- so only the ordering rule, which a
// repeat breaks as surely as a swap, can refuse it. A trace would report both
// as the 2nd of 3, and a reader counting back could not tell which one ran.
//
// The positions are values, not types, so the refusal is a call no constant
// expression can make; its name is the message -- see
// `detail::published_positions_must_increase_and_stay_below_the_published_count`.
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
                          formula::detail::PublishedLayoutAccess::checked<2>({ 1, 1 }, 3) };
    return pack.published.count() == 3 ? 0 : 1;
}
