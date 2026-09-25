// SPDX-License-Identifier: Apache-2.0
// EXPECT: this lookup table was given a different number of corrections than it has rows
//
// The exact lookup's own factory-free route -- see
// `lookup_short_corrections_no_factory.cpp` for why a case per node kind
// exists rather than one standing for all three: each node declares its own
// `corrections` member, so a member that reverted to a bare
// `std::array<Rational, N>` on one kind alone would leave that kind silently
// zero-filling while the other two stayed refused. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    // Named for this file, not `Shape`: two translation units whose
    // internal-linkage key enumerations share a name and whose tables share
    // their element values mislink on clang with no diagnostic. See
    // `lookup.hpp`'s file comment.
    enum class ShortCorrectionsShape
    {
        Cube,
        Cylinder,
        Prism,
    };

    inline constexpr formula::KeyTable<ShortCorrectionsShape, 3> ShapeKeys {
        ShortCorrectionsShape::Cube,
        ShortCorrectionsShape::Cylinder,
        ShortCorrectionsShape::Prism,
    };

    // Only ONE correction for THREE keys, handed straight to the aggregate.
    inline constexpr formula::ExactLookupNode<ShapeKeys, formula::unit::One> broken {
        {}, { formula::Rational { 1 } }, ShortCorrectionsShape::Prism
    };
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
