// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay derives a quantity in an expression holding a node kind it cannot see inside
// REJECT: formula: this overlay overrides a constant
// REJECT: formula: this overlay derives a quantity that no variant
//
// Defining the shape factor in a method whose only use of it sits inside a
// consumer's own node kind. The rewrite cannot reach it, and refuses in the
// words of the operation it was doing -- a derivation, not a constant -- and
// the unused-quantity rule waits, since the use is there, unseen.
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
    static_cast<void>(m);
    constexpr auto opaque =
        formula::method(formula::variants(formula::variant<Cube>(OpaqueNode<formula::VarNode<ShapeFactor>> {} * var<Force>
                                                                 / (var<EdgeX> * var<EdgeY>) )),
                        formula::rounding_rule<formula::unit::Megapascal,
                                               formula::DecimalPlaces { 1 },
                                               formula::RoundingMode::HalfAwayFromZero>(),
                        formula::constraints());
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::add_derived<ShapeFactor>(
                           var<EdgeY> / var<EdgeX>, formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                       opaque);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 1 ? 0 : 1;
}
