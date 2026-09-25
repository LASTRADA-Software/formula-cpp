// SPDX-License-Identifier: Apache-2.0
// EXPECT: this lookup table was given a different number of corrections than it has rows
//
// The interpolating lookup's own factory-free route -- see
// `lookup_short_corrections_no_factory.cpp` for why there is a case per node
// kind. This kind had the worst version of the defect: a row whose value was
// never typed does not merely answer `0` at its own key, it drags the whole
// segment either side of it towards zero, so every input between two real rows
// came back wrong rather than only the forgotten one. This must not compile.
#include <formula-cpp/lookup.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BreakpointTable<3> Curve {
        formula::breakpoint(0),
        formula::breakpoint(10),
        formula::breakpoint(20),
    };

    // Only ONE value for THREE breakpoints, handed straight to the aggregate.
    inline constexpr formula::InterpolatingLookupNode<formula::unit::Millimetre,
                                                      Curve,
                                                      formula::unit::One,
                                                      formula::VarNode<Diameter>>
        broken { {}, { formula::Rational { 95, 100 } }, formula::var<Diameter> };
} // namespace

int main()
{
    return static_cast<int>(decltype(broken)::dimension.length.numerator);
}
