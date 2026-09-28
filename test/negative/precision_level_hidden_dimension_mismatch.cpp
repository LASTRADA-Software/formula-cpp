// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this precision_level reads a level of another dimension
//
// The one route past the declaration-time dimension check: a placeholder
// inside a consumer's own node kind, which that check cannot see into. The
// consumer node hands its operand straight to the evaluator, so the
// placeholder is still evaluated in the limit's bound environment -- and
// refuses there, rather than read a mass as a width. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct ResultA: formula::Quantity<ResultA, "x_A", "first determination", formula::unit::Gram>
{
};
struct ResultB: formula::Quantity<ResultB, "x_B", "second determination", formula::unit::Gram>
{
};
struct Width: formula::Quantity<Width, "w", "specimen width", formula::unit::Millimetre>
{
};
} // namespace

namespace consumer
{
/// A consumer's node that evaluates to its operand, unseen by the library.
template <formula::Node Operand>
struct Passthrough: formula::NodeBase
{
    Operand operand;
    static constexpr formula::Dimension dimension = formula::dim::Mass;
};

template <typename Rep, formula::Node Operand, typename Env, typename Sink>
constexpr formula::Evaluated<Rep> checked_evaluate_si(Passthrough<Operand> const& node, Env const& environment, Sink sink)
{
    return formula::detail::dispatch<Rep>(node.operand, environment, sink);
}
} // namespace consumer

int main()
{
    constexpr auto hidden =
        consumer::Passthrough<formula::PrecisionLevelNode<Width>> { {}, formula::precision_level<Width> };
    constexpr auto broken = formula::precision_limit<formula::PrecisionKind::Repeatability>(formula::var<ResultA>, hidden);
    auto const outcome = formula::checked_evaluate<ResultA>(
        broken, formula::environment(formula::Measured<ResultA> { formula::Rational { 40 } }));
    return outcome.has_value() ? 0 : 1;
}
