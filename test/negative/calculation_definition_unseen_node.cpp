// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this definition holds a node kind the calculation cannot see inside
// REJECT: has no detail::LevelChildren specialisation
// REJECT: formula: this definition's expression measures a different dimension from the quantity it defines
// REJECT: no matching
//
// A consumer's own node kind, holding a read of the factor the calculation
// cannot see: the factor would be missing from the dependency graph, and a
// changed factor would leave the share out of date. Refused -- and in the
// calculation's words, not in the words a precision limit's checks use for a
// library kind without an entry.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

namespace
{
namespace unit = formula::unit;

struct Factor: formula::Quantity<Factor, "k", "an invented factor", unit::One>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};

// A consumer's node kind, which holds an operand the walk cannot see.
template <typename Operand>
struct Hidden: formula::NodeBase
{
    Operand operand;

    static constexpr formula::Dimension dimension = formula::dim::Scalar;
};
} // namespace

int main()
{
    constexpr auto share = formula::define<Share>(Hidden<formula::VarNode<Factor>> { {}, formula::var<Factor> }
                                                  * formula::var<Factor>);
    return decltype(share)::valid ? 0 : 1;
}
