// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a snap can only be evaluated with Rep = Rational
//
// `checked_evaluate_si<double>` on a snap. Each lookup kind pins its own `Rep`
// guard; leaving the snap's unpinned would leave one half of the rule free to
// drift. Deciding which permitted value is nearest, and whether two are
// exactly as near, needs exact comparison. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
    struct Opening: formula::Quantity<Opening, "d", "screen opening", formula::unit::Metre>
    {
    };

    inline constexpr formula::BreakpointTable<2> Permitted {
        formula::breakpoint(103),
        formula::breakpoint(127),
    };

    inline constexpr auto node =
        formula::snapped<formula::unit::Metre, Permitted, formula::SnapTie::TowardLower>(formula::var<Opening>);
} // namespace

int main()
{
    auto const environment = formula::environment(formula::Measured<Opening> { formula::Rational { 113 } });
    auto const computed = formula::checked_evaluate_si<double>(node, environment);
    return computed.has_value() ? 0 : 1;
}
