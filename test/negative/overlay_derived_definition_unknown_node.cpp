// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay derives a quantity in an expression holding a node kind it cannot see inside
// REJECT: formula: this overlay derives a quantity from an expression that reads the quantity itself
// REJECT: formula: this method reads a quantity both where an overlay fixed or derived it and, elsewhere, unsubstituted from
// the environment REJECT: formula: this overlay derives a quantity that no variant or constraint of the method uses
//
// A definition holding a consumer's own node kind, which the overlay cannot
// see inside -- so it cannot tell whether the definition reads the quantity
// it defines. The REJECT pins that the self-reference rule waits for every
// node to be known rather than answering a question it cannot.
//
// Applied as well as written, and the REJECTs pin that applying it adds
// nothing: an operation its own class body refuses is part of what `apply`
// asks before instantiating its body, so the definition is never also judged
// against the method it would have produced.
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
struct Prism
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
struct Ratio: formula::Quantity<Ratio, "r", "a dimensionless ratio", formula::unit::One>
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
inline constexpr auto m =
    formula::method(formula::variants(formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
                    formula::rounding_rule<formula::unit::Megapascal,
                                           formula::DecimalPlaces { 1 },
                                           formula::RoundingMode::HalfAwayFromZero>(),
                    formula::constraints());
} // namespace

int main()
{
    constexpr auto deriving = formula::add_derived<ShapeFactor>(OpaqueNode<formula::VarNode<ShapeFactor>> {});
    constexpr auto overlaid = formula::apply(formula::overlay(deriving), m);
    return deriving.source.title.empty() && std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 2 ? 0 : 1;
}
