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

    using quantity = Q;

    static constexpr Dimension dimension = Describe<Q>::dimension;
};

/// The spelling of a variable in a formula: `var<WaterVolume>`.
///
/// `inline` matters and is not decoration. Without it each translation unit
/// would get its own object, two of which are not the same variable; with it
/// the whole program shares one, which `expression_cross_tu` pins.
template <Described Q>
inline constexpr VarNode<Q> var {};

/// A literal coefficient, stated in a unit so it can be converted like any
/// other value. The unit is part of the type; the number is not, because a
/// coefficient may arrive from a table at runtime.
template <Unit U>
struct ConstantNode: NodeBase
{
    Rational number {};

    static constexpr Unit unit = U;
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

enum class UnaryOperator : std::uint8_t
{
    Negate,
};

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

template <UnaryOperator Op, Node Operand>
struct UnaryNode: NodeBase
{
    Operand operand {};

    static constexpr UnaryOperator op = Op;
    static constexpr Dimension dimension = Operand::dimension;
};

template <BinaryOperator Op, Node Left, Node Right>
struct BinaryNode: NodeBase
{
    static_assert(detail::AdditiveDimensionsAgree<Op, Left, Right>::value);

    Left lhs {};
    Right rhs {};

    static constexpr BinaryOperator op = Op;
    static constexpr Dimension dimension = detail::combined_dimension<Op, Left::dimension, Right::dimension>();
};

// ---------------------------------------------------------------- operators
//
// Taken and returned by value: nodes are empty or hold one `Rational`, so there
// is nothing to save by reference, and a reference into a temporary subtree is
// a dangling read waiting to happen.

template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator+(Left lhs, Right rhs) noexcept
{
    return BinaryNode<BinaryOperator::Add, Left, Right> { {}, lhs, rhs };
}

template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator-(Left lhs, Right rhs) noexcept
{
    return BinaryNode<BinaryOperator::Subtract, Left, Right> { {}, lhs, rhs };
}

template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator*(Left lhs, Right rhs) noexcept
{
    return BinaryNode<BinaryOperator::Multiply, Left, Right> { {}, lhs, rhs };
}

template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator/(Left lhs, Right rhs) noexcept
{
    return BinaryNode<BinaryOperator::Divide, Left, Right> { {}, lhs, rhs };
}

template <Node Operand>
[[nodiscard]] constexpr auto operator-(Operand operand) noexcept
{
    return UnaryNode<UnaryOperator::Negate, Operand> { {}, operand };
}

// A bare `Rational` in a formula is a dimensionless coefficient. Spelled out
// per operator rather than through a converting constructor, so that a
// `Rational` never silently becomes a node anywhere else.

template <Node Left>
[[nodiscard]] constexpr auto operator+(Left lhs, Rational rhs) noexcept
{
    return lhs + number(rhs);
}
template <Node Right>
[[nodiscard]] constexpr auto operator+(Rational lhs, Right rhs) noexcept
{
    return number(lhs) + rhs;
}
template <Node Left>
[[nodiscard]] constexpr auto operator-(Left lhs, Rational rhs) noexcept
{
    return lhs - number(rhs);
}
template <Node Right>
[[nodiscard]] constexpr auto operator-(Rational lhs, Right rhs) noexcept
{
    return number(lhs) - rhs;
}
template <Node Left>
[[nodiscard]] constexpr auto operator*(Left lhs, Rational rhs) noexcept
{
    return lhs * number(rhs);
}
template <Node Right>
[[nodiscard]] constexpr auto operator*(Rational lhs, Right rhs) noexcept
{
    return number(lhs) * rhs;
}
template <Node Left>
[[nodiscard]] constexpr auto operator/(Left lhs, Rational rhs) noexcept
{
    return lhs / number(rhs);
}
template <Node Right>
[[nodiscard]] constexpr auto operator/(Rational lhs, Right rhs) noexcept
{
    return number(lhs) / rhs;
}

} // namespace formula
