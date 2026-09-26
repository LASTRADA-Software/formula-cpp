// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay overrides a constant in an expression holding a node kind it cannot see inside
// REJECT: formula: this overlay overrides a quantity that no variant or constraint of the method uses
//
// `with_constant<ShapeFactor>` over a formula whose only use of the shape
// factor sits inside a consumer's own node kind, then `prune_variant<Cube>(formula::Citation { .reference = "Example Standard 12:2021 NA" })`,
// which removes the variant holding that node.
//
// The rewrite refuses the unknown node. The method the overlay produces no
// longer holds it, so it looks entirely known -- and asking the unused-quantity
// rule of it would add "no variant or constraint uses" to a refusal that
// already said the overlay cannot see where the quantity is used. The REJECT
// pins that the result check is gated on the method the overlay was APPLIED
// TO being all known, not only the method it produced.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>

#include <optional>
#include <tuple>

namespace
{
struct Cube
{
};
struct Cylinder
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", formula::unit::Millimetre>
{
};
struct ShapeFactor: formula::Quantity<ShapeFactor, "k_s", "shape factor", formula::unit::One>
{
};

// A consumer's own node kind, which evaluates to 1 whatever its operand
// holds. The overlay has no way to see the operand inside it.
template <typename Operand>
struct OpaqueNode: formula::NodeBase
{
    Operand operand {};

    static constexpr formula::Dimension dimension = formula::dim::Scalar;
};

template <typename Rep, typename Operand, typename Env>
[[nodiscard]] constexpr formula::Evaluated<Rep> checked_evaluate_si(OpaqueNode<Operand> const&, Env const&) noexcept
{
    return formula::Evaluated<Rep> { std::optional<Rep> { Rep { 1 } } };
}

using formula::var;

// Only the Cube variant reads the shape factor.
inline constexpr auto m = formula::method(
    formula::variants(
        formula::variant<Cube>(OpaqueNode<formula::VarNode<ShapeFactor>> {} * var<Force> / (var<EdgeX> * var<EdgeY>) ),
        formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
    formula::
        rounding_rule<formula::unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
} // namespace

int main()
{
    constexpr auto overlaid = formula::apply(
        formula::overlay(
            formula::with_constant<ShapeFactor>(formula::Rational { 97, 100 },
                                                formula::Citation { .reference = "Example Standard 12:2021 NA" }),
            formula::prune_variant<Cube>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
        m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 0 ? 1 : 0;
}
