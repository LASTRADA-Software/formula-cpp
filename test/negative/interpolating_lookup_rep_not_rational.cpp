// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an interpolating lookup node can only be evaluated with Rep = Rational
//
// `checked_evaluate_si<double>` on an interpolating lookup. The other two
// lookup kinds' `Rep` guards each have a negative case, and leaving this one
// untested would put the three halves of one rule in different states -- some
// pinned, one free to drift.
//
// This message's reason is its own, not either neighbour's: the banded node
// says selecting a band needs exact arithmetic, the exact node says explicitly
// that comparing enumerators does NOT, and this node says both that locating
// the two rows needs exact comparison and -- the part unique to it -- that the
// answer is computed rather than selected, so a representation that rounds
// answers with a number the table's own rows do not imply. That is the defect
// this whole table kind exists to avoid, so the text is tested API like every
// other static_assert here. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BreakpointTable<2> Points {
        formula::breakpoint(0),
        formula::breakpoint(10),
    };

    inline constexpr auto node =
        formula::interpolating_lookup<formula::unit::Millimetre, Points, formula::unit::One>(
            formula::var<Diameter>, { formula::Rational { 1 }, formula::Rational { 2 } });
} // namespace

int main()
{
    auto const environment = formula::environment(formula::Measured<Diameter> { formula::Rational { 5 } });
    auto const computed = formula::checked_evaluate_si<double>(node, environment);
    return computed.has_value() ? 0 : 1;
}
