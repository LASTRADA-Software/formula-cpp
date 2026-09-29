// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this library node kind has no detail::LevelChildren specialisation
// REJECT: formula: this definition holds a node kind the calculation cannot see inside
//
// A node kind declared in this library's namespace with no `LevelChildren`
// entry of its own -- what a library kind added without one would be. The
// entry's primary template refuses it in its own words, as an omission of the
// library's; the calculation's message for a consumer's kind must not follow
// it, which would report the one omission twice and name the wrong culprit.
//
// Declared here rather than borrowed from the library, so that this stays a
// kind without an entry whatever entries the library gains.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

namespace formula
{
/// A node kind of this namespace's that no `LevelChildren` entry lists.
struct UnlistedNodeKind: NodeBase
{
    static constexpr Dimension dimension = dim::Scalar;
};
} // namespace formula

namespace
{
struct Factor: formula::Quantity<Factor, "k", "an invented factor", formula::unit::One>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", formula::unit::One>
{
};
} // namespace

int main()
{
    [[maybe_unused]] constexpr auto share =
        formula::define<Share>(formula::UnlistedNodeKind {} * formula::var<Factor>);
    return 0;
}
