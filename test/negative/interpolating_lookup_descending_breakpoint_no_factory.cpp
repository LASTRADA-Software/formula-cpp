// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this interpolating lookup table's breakpoints do not strictly ascend
//
// The same defect as `interpolating_lookup_descending_breakpoint.cpp`, declared
// WITHOUT the factory -- and that single difference is the whole point of this
// file, exactly as it is for `exact_lookup_duplicate_key_no_factory.cpp`.
//
// `InterpolatingLookupNode` is a public aggregate with public members, so a
// caller can declare one directly, as below. Only a `static_assert` in the
// CLASS BODY refuses that; with the validation moved into
// `interpolating_lookup` this translation unit compiles and links, carrying a
// curve whose rows are out of order. Discarding the factory's result is NOT
// what tells the two placements apart -- the factory returns the node by value,
// so calling it completes the class whichever placement is chosen. Declaring
// the node without ever calling the factory is. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BreakpointTable<4> DescendingPoints {
        formula::breakpoint(0),
        formula::breakpoint(20),
        formula::breakpoint(10), // below the row above it
        formula::breakpoint(30),
    };

    // No `interpolating_lookup` call anywhere in this file.
    inline constexpr formula::InterpolatingLookupNode<formula::unit::Millimetre, DescendingPoints,
                                                      formula::unit::One, decltype(formula::var<Diameter>)>
        broken {
            {},
            { formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 }, formula::Rational { 1 } },
            formula::var<Diameter>,
        };
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
