// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// The expression layer: formulas written with ordinary operators.
///
/// A formula is a *type*. `var<WaterVolume> / var<CementVolume>` builds a
/// `BinaryNode<Divide, VarNode<WaterVolume>, VarNode<CementVolume>>`, and every
/// node publishes `static constexpr Dimension dimension` computed at class
/// scope. Because the operator the user wrote is what instantiates the node,
/// a dimensional mistake is a compile error on the line the formula is written
/// on -- not at evaluation, which may happen in another file compiled an hour
/// later.
///
/// A structure node declares no members of its own -- its whole shape is in the
/// type -- so a tree is a compile-time entity and traversing it inlines away.
/// It is not literally an empty class: a node holds its children by value, and
/// an empty child still occupies a byte, so a tree costs roughly one byte per
/// leaf and nothing per level. `ConstantNode` is the one node with real state,
/// because a coefficient may arrive from a table at runtime.

#include <formula-cpp/dimension.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/unit.hpp>

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace formula
{

/// The base every expression node derives from, so one concept recognises them
/// all. Empty, and inherited rather than held, so an empty node stays empty.
struct NodeBase
{
};

/// Anything that can appear in a formula.
template <typename T>
concept Node = std::derived_from<std::remove_cvref_t<T>, NodeBase>;

/// A named input: the quantity is the key the environment is asked with, and
/// the source of the node's dimension.
template <Described Q>
struct VarNode: NodeBase
{
    static_assert(DescribesConsistentDimension<Q>,
                  "formula: this quantity describes a dimension its own unit does not measure, so "
                  "no formula containing it can be trusted; the quantity appears in this "
                  "diagnostic as the template argument of VarNode");

    /// The quantity this node names -- the key an `Environment` is asked with.
    using quantity = Q;

    /// The dimension `Q` describes.
    static constexpr Dimension dimension = Describe<Q>::dimension;
};

/// The spelling of a variable in a formula: `var<WaterVolume>`.
///
/// `inline` matters and is not decoration: it is what guarantees the whole
/// program shares one object per `Q` rather than each translation unit
/// getting its own. cl and clang-cl happen to give the specialisations
/// external linkage even without it, so deleting `inline` breaks nothing on
/// a Windows-only build; g++ does not, so the guarantee is enforced by the
/// Linux-gcc CI leg and by `expression_cross_tu`.
template <Described Q>
inline constexpr VarNode<Q> var {};

/// A literal coefficient, stated in a unit so it can be converted like any
/// other value. The unit is part of the type; the number is not, because a
/// coefficient may arrive from a table at runtime.
template <Unit U>
struct ConstantNode: NodeBase
{
    /// The coefficient, in terms of `unit`.
    Rational number {};

    /// The unit the coefficient is stated in -- part of the type, not runtime state.
    static constexpr Unit unit = U;
    /// The dimension of `unit`.
    static constexpr Dimension dimension = U.dimension;
};

/// A coefficient with a unit: `constant<unit::Millimetre>(rat(150))`.
template <Unit U>
[[nodiscard]] constexpr ConstantNode<U> constant(Rational value) noexcept
{
    return ConstantNode<U> { {}, value };
}

/// A dimensionless coefficient: `number(rat(1, 4))`.
///
/// There is deliberately no overload that guesses a unit for a bare number.
/// Guessing wrong is exactly the failure the dimension layer exists to prevent.
[[nodiscard]] constexpr ConstantNode<unit::One> number(Rational value) noexcept
{
    return constant<unit::One>(value);
}

/// Which unary operation a `UnaryNode` performs.
enum class UnaryOperator : std::uint8_t
{
    Negate,
};

/// Which binary operation a `BinaryNode` performs.
enum class BinaryOperator : std::uint8_t
{
    Add,
    Subtract,
    Multiply,
    Divide,
};

namespace detail
{
    /// Fails to compile when the two sides of an addition or subtraction measure
    /// different dimensions.
    ///
    /// A named template, and templated on the *operands* rather than on the
    /// operator, entirely for the diagnostic's sake. Measured on cl 19.51,
    /// clang-cl 22 and g++ 13.3: an assertion whose condition mentions the
    /// operator makes clang print `(formula::BinaryOperator)0` in its "due to
    /// requirement" clause, because an enumerator used as a value in a
    /// dependent expression is rendered as a cast. Written this way, clang
    /// prints `formula::VarNode<WaterVolume>::dimension ==
    /// formula::VarNode<BeamLength>::dimension` instead, and all three
    /// compilers name the two operand types and the formula's own source line.
    template <Node Left, Node Right>
    struct RequireAddendsAgree
    {
        static_assert(Left::dimension == Right::dimension,
                      "formula: the two sides of this addition or subtraction measure different "
                      "dimensions; the offending operands appear in this diagnostic as the "
                      "template arguments of RequireAddendsAgree");

        static constexpr bool value = true;
    };

    /// True for multiplication and division, which impose nothing; the two
    /// specialisations below route addition and subtraction through the check.
    template <BinaryOperator Op, Node Left, Node Right>
    struct AdditiveDimensionsAgree: std::true_type
    {
    };

    template <Node Left, Node Right>
    struct AdditiveDimensionsAgree<BinaryOperator::Add, Left, Right>:
        std::bool_constant<RequireAddendsAgree<Left, Right>::value>
    {
    };

    template <Node Left, Node Right>
    struct AdditiveDimensionsAgree<BinaryOperator::Subtract, Left, Right>:
        std::bool_constant<RequireAddendsAgree<Left, Right>::value>
    {
    };

    template <BinaryOperator Op, Dimension Left, Dimension Right>
    [[nodiscard]] constexpr Dimension combined_dimension() noexcept
    {
        if constexpr (Op == BinaryOperator::Multiply)
            return Left * Right;
        else if constexpr (Op == BinaryOperator::Divide)
            return Left / Right;
        else
            return Left;
    }
} // namespace detail

/// A node built by applying one `UnaryOperator` to a single child.
template <UnaryOperator Op, Node Operand>
struct UnaryNode: NodeBase
{
    /// The child expression the operator is applied to.
    Operand operand {};

    /// Which operator this is.
    static constexpr UnaryOperator op = Op;
    /// A unary operator never changes the dimension of its operand.
    static constexpr Dimension dimension = Operand::dimension;
};

/// A node built by applying one `BinaryOperator` to two children.
template <BinaryOperator Op, Node Left, Node Right>
struct BinaryNode: NodeBase
{
    static_assert(detail::AdditiveDimensionsAgree<Op, Left, Right>::value);

    /// The left-hand child expression.
    Left lhs {};
    /// The right-hand child expression.
    Right rhs {};

    /// Which operator this is.
    static constexpr BinaryOperator op = Op;
    /// Add and subtract keep the (already agreeing) dimension; multiply and
    /// divide combine the two operands' dimensions.
    static constexpr Dimension dimension = detail::combined_dimension<Op, Left::dimension, Right::dimension>();
};

// ---------------------------------------------------------------- operators
//
// Taken and returned by value: nodes are empty or hold one `Rational`, so there
// is nothing to save by reference, and a reference into a temporary subtree is
// a dangling read waiting to happen.

/// Formula addition. Fails to compile if `lhs` and `rhs` measure different
/// dimensions -- see `detail::RequireAddendsAgree`.
template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator+(Left lhs, Right rhs) noexcept
{
    return BinaryNode<BinaryOperator::Add, Left, Right> { {}, lhs, rhs };
}

/// Formula subtraction. Fails to compile if `lhs` and `rhs` measure different
/// dimensions -- see `detail::RequireAddendsAgree`.
template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator-(Left lhs, Right rhs) noexcept
{
    return BinaryNode<BinaryOperator::Subtract, Left, Right> { {}, lhs, rhs };
}

/// Formula multiplication. The two operands need not share a dimension: the
/// result's dimension is their product.
template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator*(Left lhs, Right rhs) noexcept
{
    return BinaryNode<BinaryOperator::Multiply, Left, Right> { {}, lhs, rhs };
}

/// Formula division. The two operands need not share a dimension: the
/// result's dimension is their quotient.
template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator/(Left lhs, Right rhs) noexcept
{
    return BinaryNode<BinaryOperator::Divide, Left, Right> { {}, lhs, rhs };
}

/// Formula negation.
template <Node Operand>
[[nodiscard]] constexpr auto operator-(Operand operand) noexcept
{
    return UnaryNode<UnaryOperator::Negate, Operand> { {}, operand };
}

// A bare `Rational` in a formula is a dimensionless coefficient. Spelled out
// per operator rather than through a converting constructor, so that a
// `Rational` never silently becomes a node anywhere else.

/// `lhs + rhs`, with `rhs` taken as a dimensionless coefficient.
template <Node Left>
[[nodiscard]] constexpr auto operator+(Left lhs, Rational rhs) noexcept
{
    return lhs + number(rhs);
}
/// `lhs + rhs`, with `lhs` taken as a dimensionless coefficient.
template <Node Right>
[[nodiscard]] constexpr auto operator+(Rational lhs, Right rhs) noexcept
{
    return number(lhs) + rhs;
}
/// `lhs - rhs`, with `rhs` taken as a dimensionless coefficient.
template <Node Left>
[[nodiscard]] constexpr auto operator-(Left lhs, Rational rhs) noexcept
{
    return lhs - number(rhs);
}
/// `lhs - rhs`, with `lhs` taken as a dimensionless coefficient.
template <Node Right>
[[nodiscard]] constexpr auto operator-(Rational lhs, Right rhs) noexcept
{
    return number(lhs) - rhs;
}
/// `lhs * rhs`, with `rhs` taken as a dimensionless coefficient.
template <Node Left>
[[nodiscard]] constexpr auto operator*(Left lhs, Rational rhs) noexcept
{
    return lhs * number(rhs);
}
/// `lhs * rhs`, with `lhs` taken as a dimensionless coefficient.
template <Node Right>
[[nodiscard]] constexpr auto operator*(Rational lhs, Right rhs) noexcept
{
    return number(lhs) * rhs;
}
/// `lhs / rhs`, with `rhs` taken as a dimensionless coefficient.
template <Node Left>
[[nodiscard]] constexpr auto operator/(Left lhs, Rational rhs) noexcept
{
    return lhs / number(rhs);
}
/// `lhs / rhs`, with `lhs` taken as a dimensionless coefficient.
template <Node Right>
[[nodiscard]] constexpr auto operator/(Rational lhs, Right rhs) noexcept
{
    return number(lhs) / rhs;
}

} // namespace formula
