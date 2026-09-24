// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Function nodes: integer powers, roots, and pi.
///
/// Powers and roots act on the dimension as well as the number -- the square of
/// a length is an area, the cube root of a volume is a length -- which is why
/// they are nodes rather than free functions a caller applies to an evaluated
/// number. A root may also produce a fractional exponent, which is exactly why
/// `Dimension` carries rational exponents.

#include <formula-cpp/dimension.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/rational.hpp>

#include <cmath>

namespace formula
{

/// @p Operand raised to an integer power. Negative exponents are allowed and
/// invert the dimension with the value.
template <int Exponent, Node Operand>
struct PowerNode: NodeBase
{
    Operand operand {};

    static constexpr int exponent = Exponent;
    static constexpr Dimension dimension = power(Operand::dimension, Exponent);
};

namespace detail
{
    /// Fails to compile for a root of degree zero, which names no operation.
    template <int Degree>
    struct RequirePositiveRootDegree
    {
        static_assert(Degree > 0,
                      "formula: a root of degree zero describes no operation; the degree appears in "
                      "this diagnostic as the template argument of RequirePositiveRootDegree");

        static constexpr bool value = true;
    };
} // namespace detail

/// The @p Degree-th root of @p Operand.
template <int Degree, Node Operand>
struct RootNode: NodeBase
{
    static_assert(detail::RequirePositiveRootDegree<Degree>::value);

    Operand operand {};

    static constexpr int degree = Degree;
    static constexpr Dimension dimension = nth_root(Operand::dimension, Degree);
};

/// Pi, as a node, so that a formula containing it stays a formula.
struct PiNode: NodeBase
{
    static constexpr Dimension dimension = dim::Scalar;
};

template <int Exponent, Node Operand>
[[nodiscard]] constexpr auto pow(Operand operand) noexcept
{
    return PowerNode<Exponent, Operand> { {}, operand };
}

template <Node Operand>
[[nodiscard]] constexpr auto sqrt(Operand operand) noexcept
{
    return RootNode<2, Operand> { {}, operand };
}

template <Node Operand>
[[nodiscard]] constexpr auto cbrt(Operand operand) noexcept
{
    return RootNode<3, Operand> { {}, operand };
}

template <int Degree, Node Operand>
[[nodiscard]] constexpr auto root(Operand operand) noexcept
{
    return RootNode<Degree, Operand> { {}, operand };
}

/// The spelling of pi in a formula.
inline constexpr PiNode pi {};

// ---------------------------------------------------------------- evaluation

/// Powers and roots per representation. Kept here rather than in `RepTraits`
/// itself so that `evaluate.hpp` stays free of `<cmath>`: a consumer who never
/// writes a root never compiles it.
template <typename Rep>
struct RepFunctions;

template <>
struct RepFunctions<Rational>
{
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> raise(Rational base, int exponent) noexcept
    {
        return checked_pow(base, exponent);
    }

    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> root(Rational value, int degree) noexcept
    {
        return checked_exact_nth_root(value, degree);
    }

    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> pi_value() noexcept
    {
        return Pi;
    }
};

template <>
struct RepFunctions<double>
{
    [[nodiscard]] static std::expected<double, ArithmeticError> raise(double base, int exponent) noexcept
    {
        return std::pow(base, static_cast<double>(exponent));
    }

    [[nodiscard]] static std::expected<double, ArithmeticError> root(double value, int degree) noexcept
    {
        if (value < 0.0 && degree % 2 == 0)
            return std::unexpected { ArithmeticError::DomainError };
        if (value < 0.0)
            return -std::pow(-value, 1.0 / static_cast<double>(degree));
        return std::pow(value, 1.0 / static_cast<double>(degree));
    }

    [[nodiscard]] static std::expected<double, ArithmeticError> pi_value() noexcept
    {
        return 3.141592653589793238462643383279502884;
    }
};

template <typename Rep = Rational, int Exponent, Node Operand, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(PowerNode<Exponent, Operand> const& node,
                                                           Env const& environment) noexcept
{
    Evaluated<Rep> const operand = checked_evaluate_si<Rep>(node.operand, environment);
    if (!operand.has_value())
        return std::unexpected { operand.error() };
    if (!operand->has_value())
        return detail::nothing<Rep>();

    std::expected<Rep, ArithmeticError> const raised = RepFunctions<Rep>::raise(**operand, Exponent);
    if (!raised.has_value())
        return std::unexpected { raised.error() };
    return detail::present<Rep>(*raised);
}

template <typename Rep = Rational, int Degree, Node Operand, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(RootNode<Degree, Operand> const& node,
                                                           Env const& environment) noexcept
{
    Evaluated<Rep> const operand = checked_evaluate_si<Rep>(node.operand, environment);
    if (!operand.has_value())
        return std::unexpected { operand.error() };
    if (!operand->has_value())
        return detail::nothing<Rep>();

    std::expected<Rep, ArithmeticError> const rooted = RepFunctions<Rep>::root(**operand, Degree);
    if (!rooted.has_value())
        return std::unexpected { rooted.error() };
    return detail::present<Rep>(*rooted);
}

template <typename Rep = Rational, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(PiNode const&, Env const&) noexcept
{
    std::expected<Rep, ArithmeticError> const value = RepFunctions<Rep>::pi_value();
    if (!value.has_value())
        return std::unexpected { value.error() };
    return detail::present<Rep>(*value);
}

} // namespace formula
