// SPDX-License-Identifier: Apache-2.0
// EXPECT: published_positions_must_increase_and_stay_below_the_published_count
//
// A variants pack whose published layout puts its second variant at position
// 2 of a count of 2 -- the 3rd of 2. The two positions increase, so only
// the range rule can refuse it, and the offending position is the LAST one,
// so a check stopping short of it would let the pack through.
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
                          formula::detail::PublishedLayoutAccess::checked<2>({ 0, 2 }, 2) };
    return pack.published.count() == 2 ? 0 : 1;
}
