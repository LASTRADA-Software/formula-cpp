// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a banded lookup node can only be evaluated with Rep = Rational
//
// `checked_evaluate_si<double>` on a banded lookup. The exact lookup's own
// `Rep` guard gained a negative case in the same round, and leaving this one
// untested would put the two halves of one rule in different states -- one
// pinned, one free to drift -- which is the failure this phase keeps finding.
// Unlike the exact lookup's, this message's reason is literally true: deciding
// which band a value falls in IS arithmetic, and a value a few ULPs off an
// intended boundary picks the wrong band silently. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
    struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
    {
    };

    inline constexpr formula::BandTable<2> Bands {
        formula::band(0, 1, 10, 1),
        formula::band(10, 1, 20, 1),
    };

    inline constexpr auto node = formula::banded_lookup<formula::unit::Millimetre, Bands, formula::unit::One>(
        formula::var<Diameter>, { formula::Rational { 1 }, formula::Rational { 1 } });
} // namespace

int main()
{
    auto const environment = formula::environment(formula::Measured<Diameter> { formula::Rational { 5 } });
    auto const computed = formula::checked_evaluate_si<double>(node, environment);
    return computed.has_value() ? 0 : 1;
}
