// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this exact lookup table declares the same key twice
//
// The same defect as `exact_lookup_duplicate_key.cpp`, declared WITHOUT the
// factory -- and that single difference is the whole point of this file.
//
// `ExactLookupNode` is a public aggregate with public members, so a caller can
// declare one directly, as below. Only a `static_assert` in the CLASS BODY
// refuses that; with the validation moved into `exact_lookup` this
// translation unit compiles and links, carrying a silently unreachable row.
// Measured: moving both asserts into the factory leaves the entire positive
// suite green and leaves `exact_lookup_duplicate_key.cpp` failing to compile
// exactly as before, because `exact_lookup` returns the node by value and so
// completes the class either way. This file is the one that tells the two
// placements apart. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    enum class SpecimenShape
    {
        Cube100,
        Cube150,
        CylinderShort,
        Prism,
    };

    inline constexpr formula::KeyTable<SpecimenShape, 5> DuplicatedKeys {
        SpecimenShape::Cube100,
        SpecimenShape::Cube150,
        SpecimenShape::CylinderShort,
        SpecimenShape::Cube150, // already declared two rows above
        SpecimenShape::Prism,
    };

    // No `exact_lookup` call anywhere in this file.
    inline constexpr formula::ExactLookupNode<DuplicatedKeys, formula::unit::One> broken {
        {},
        { formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 },
          formula::Rational { 1 } },
        SpecimenShape::Cube100,
    };
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
