// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this library node kind has no detail::LevelChildren specialisation
//
// A node kind declared in namespace formula -- standing in for one the
// library adds later -- with no LevelChildren specialisation, inside a
// precision limit's level. Refused, rather than walked as a leaf: a
// placeholder inside it would read a level without a word. A consumer's own
// node kind in the same place is walked as unseen, and not refused
// (precision_level_hidden_dimension_mismatch). This must not compile.
#include <formula-cpp/formula.hpp>

namespace formula
{
/// A node kind of "the library" that the level checks do not list.
template <Node Operand>
struct UnlistedNode: NodeBase
{
    Operand operand;
    static constexpr Dimension dimension = Operand::dimension;
};
} // namespace formula

namespace
{
struct ResultA: formula::Quantity<ResultA, "x_A", "first determination", formula::unit::Gram>
{
};
} // namespace

int main()
{
    constexpr auto unlisted = formula::UnlistedNode<formula::VarNode<ResultA>> { {}, formula::var<ResultA> };
    constexpr auto broken = formula::precision_limit<formula::PrecisionKind::Repeatability>(
        unlisted, formula::Rational { 1, 50 } * formula::precision_level<ResultA>);
    return decltype(broken)::dimension == formula::dim::Mass ? 0 : 1;
}
