// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Turning a formula and a set of inputs into an outcome.
///
/// Every leaf is converted to the **coherent SI unit** of its dimension on the
/// way in, the tree is evaluated there, and the result is converted once at the
/// end into the declared unit of the quantity it was asked to produce. Both
/// conversions are the exact multiply-then-divide of the unit layer, so 180 l
/// plus 300 l is exactly 480 l and not 479,999999.
///
/// Two entry points, deliberately different:
///
///  - `checked_evaluate_si<Rep>` is the representation-agnostic core. It answers
///    in the coherent SI unit and in whatever `Rep` the caller asked for --
///    exact `Rational` by default, `double` when a formula needs values exact
///    rationals cannot hold.
///  - `checked_evaluate<Result>` is the auditable one. It is always exact,
///    because a result that goes into an audit trail as binary floating point
///    would have to explain itself, and it returns an `Outcome<Result>` in
///    `Result`'s own unit.

#include <formula-cpp/environment.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/unit.hpp>

#include <expected>
#include <optional>

namespace formula
{

/// The coherent SI unit of a dimension: magnitude one, offset zero, no symbol.
///
/// Every `Unit` already states its own exact conversion to this one, so it is
/// the single scale on which values from different units can meet.
[[nodiscard]] constexpr Unit coherent(Dimension value) noexcept
{
    return Unit { .dimension = value };
}

/// How arithmetic is done for one representation.
///
/// The primary template is deliberately undefined: a representation that has
/// not been taught to the library fails at the point of use, naming itself,
/// rather than silently selecting something plausible.
template <typename Rep>
struct RepTraits;

/// Exact rational arithmetic -- the default, and the only one an `Outcome` is
/// built from.
template <>
struct RepTraits<Rational>
{
    /// A value already in `Rational`, unchanged -- present so `detail::in_si`
    /// can call `RepTraits<Rep>::from` uniformly for every representation.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> from(Rational value) noexcept
    {
        return value;
    }
    /// Exact addition; an overflowing sum is reported, never wrapped.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> add(Rational lhs, Rational rhs) noexcept
    {
        return checked_add(lhs, rhs);
    }
    /// Exact subtraction; an overflowing difference is reported, never wrapped.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> subtract(Rational lhs, Rational rhs) noexcept
    {
        return checked_sub(lhs, rhs);
    }
    /// Exact multiplication; an overflowing product is reported, never wrapped.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> multiply(Rational lhs, Rational rhs) noexcept
    {
        return checked_mul(lhs, rhs);
    }
    /// Exact division; division by zero is reported, never a trap or an infinity.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> divide(Rational lhs, Rational rhs) noexcept
    {
        return checked_div(lhs, rhs);
    }
    /// Exact negation.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> negate(Rational value) noexcept
    {
        return checked_negate(value);
    }
};

/// Binary floating point, for formulas whose values exact rationals cannot
/// hold. This arithmetic itself never reports `Overflow`: a `double` says
/// `inf` and the caller asked for `double`. That is not the last word on
/// `Overflow` for this representation, though -- `detail::in_si` converts
/// every leaf in exact `Rational` before handing it to `RepTraits<Rep>::from`,
/// so a leaf whose conversion to the coherent SI unit overflows still fails
/// the whole evaluation, `double` included.
template <>
struct RepTraits<double>
{
    /// Converts an exact `Rational` (already in the coherent SI unit) to `double`.
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> from(Rational value) noexcept
    {
        return value.to_double();
    }
    /// Ordinary floating-point addition.
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> add(double lhs, double rhs) noexcept
    {
        return lhs + rhs;
    }
    /// Ordinary floating-point subtraction.
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> subtract(double lhs, double rhs) noexcept
    {
        return lhs - rhs;
    }
    /// Ordinary floating-point multiplication.
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> multiply(double lhs, double rhs) noexcept
    {
        return lhs * rhs;
    }
    /// Floating-point division, except that division by zero is reported as
    /// `ArithmeticError::DivisionByZero` rather than becoming `inf` or `nan`.
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> divide(double lhs, double rhs) noexcept
    {
        if (rhs == 0.0)
            return std::unexpected { ArithmeticError::DivisionByZero };
        return lhs / rhs;
    }
    /// Ordinary floating-point negation.
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> negate(double value) noexcept
    {
        return -value;
    }
};

/// The result of evaluating a subtree: a number, nothing (an input was never
/// measured), or an arithmetic error. The two layers are different questions --
/// "there is no value" is a fact about the specimen, "the arithmetic failed" is
/// a fact about the library -- and collapsing them would lose one of them.
template <typename Rep>
using Evaluated = std::expected<std::optional<Rep>, ArithmeticError>;

namespace detail
{
    /// Fails to compile when the result quantity does not measure what the
    /// expression computes.
    template <typename Result, typename Expression>
    struct RequireResultDimension
    {
        static_assert(Describe<Result>::dimension == Expression::dimension,
                      "formula: this result quantity does not measure the dimension this expression "
                      "computes; the quantity and the expression appear in this diagnostic as the "
                      "template arguments of RequireResultDimension");

        static constexpr bool value = true;
    };

    /// Wraps a bare `Rep` as a present value.
    template <typename Rep>
    [[nodiscard]] constexpr Evaluated<Rep> present(Rep value) noexcept
    {
        return Evaluated<Rep> { std::optional<Rep> { value } };
    }

    /// The absent result, which every operator propagates.
    template <typename Rep>
    [[nodiscard]] constexpr Evaluated<Rep> nothing() noexcept
    {
        return Evaluated<Rep> { std::optional<Rep> {} };
    }

    /// A `Rational` stated in @p from, converted to the coherent SI unit and
    /// then into @p Rep.
    template <typename Rep>
    [[nodiscard]] constexpr Evaluated<Rep> in_si(Rational value, Unit from) noexcept
    {
        std::expected<Rational, ArithmeticError> const converted = checked_convert(value, from, coherent(from.dimension));
        if (!converted.has_value())
            return std::unexpected { converted.error() };

        std::expected<Rep, ArithmeticError> const represented = RepTraits<Rep>::from(*converted);
        if (!represented.has_value())
            return std::unexpected { represented.error() };
        return present<Rep>(*represented);
    }
} // namespace detail

/// Looks `Q` up in `environment` and, if present, converts it to the coherent
/// SI unit of its dimension.
template <typename Rep = Rational, Described Q, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(VarNode<Q> const&, Env const& environment) noexcept
{
    Measured<Q> const measured = environment.template get<Q>();
    if (measured.is_absent())
        return detail::nothing<Rep>();
    return detail::in_si<Rep>(*measured.stored(), Describe<Q>::unit);
}

/// A literal coefficient is always present; converts it to the coherent SI unit.
template <typename Rep = Rational, Unit U, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(ConstantNode<U> const& node, Env const&) noexcept
{
    return detail::in_si<Rep>(node.number, U);
}

/// Evaluates the operand, then applies `Op` -- absence and arithmetic errors
/// both propagate without applying the operator.
template <typename Rep = Rational, UnaryOperator Op, Node Operand, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(UnaryNode<Op, Operand> const& node,
                                                           Env const& environment) noexcept
{
    Evaluated<Rep> const operand = checked_evaluate_si<Rep>(node.operand, environment);
    if (!operand.has_value())
        return std::unexpected { operand.error() };
    if (!operand->has_value())
        return detail::nothing<Rep>();

    static_assert(Op == UnaryOperator::Negate, "formula: unknown unary operator");
    std::expected<Rep, ArithmeticError> const negated = RepTraits<Rep>::negate(**operand);
    if (!negated.has_value())
        return std::unexpected { negated.error() };
    return detail::present<Rep>(*negated);
}

/// Evaluates both operands -- always both, so an arithmetic error on one side
/// is never hidden behind the other side's absence -- then applies `Op`.
template <typename Rep = Rational, BinaryOperator Op, Node Left, Node Right, typename Env>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(BinaryNode<Op, Left, Right> const& node,
                                                           Env const& environment) noexcept
{
    Evaluated<Rep> const lhs = checked_evaluate_si<Rep>(node.lhs, environment);
    if (!lhs.has_value())
        return std::unexpected { lhs.error() };
    Evaluated<Rep> const rhs = checked_evaluate_si<Rep>(node.rhs, environment);
    if (!rhs.has_value())
        return std::unexpected { rhs.error() };

    // Absence wins over arithmetic, but only after both sides have been asked:
    // an arithmetic error in the side that *is* present is still an error, and
    // hiding it behind the other side's absence would lose it.
    if (!lhs->has_value() || !rhs->has_value())
        return detail::nothing<Rep>();

    std::expected<Rep, ArithmeticError> const combined = [&] {
        if constexpr (Op == BinaryOperator::Add)
            return RepTraits<Rep>::add(**lhs, **rhs);
        else if constexpr (Op == BinaryOperator::Subtract)
            return RepTraits<Rep>::subtract(**lhs, **rhs);
        else if constexpr (Op == BinaryOperator::Multiply)
            return RepTraits<Rep>::multiply(**lhs, **rhs);
        else
            return RepTraits<Rep>::divide(**lhs, **rhs);
    }();
    if (!combined.has_value())
        return std::unexpected { combined.error() };
    return detail::present<Rep>(*combined);
}

/// Evaluates @p expression for quantity @p Result.
///
/// `Result` is never deduced. An expression's dimension does not name a
/// quantity -- a mass over a volume is *a* density, but which one is the
/// author's decision -- and a library that picked one would eventually label a
/// number with someone else's symbol and description.
///
/// When the environment holds an `entered` value for `Result`, that value is
/// returned with `ValueSource::ManuallyEntered` and the expression is not
/// evaluated at all. That is what an override is; a number a person typed in
/// must never be reported as though the library derived it.
template <Described Result, Node Expression, typename Env>
[[nodiscard]] constexpr std::expected<Outcome<Result>, ArithmeticError> checked_evaluate(Expression const& expression,
                                                                                         Env const& environment) noexcept
{
    static_assert(detail::RequireResultDimension<Result, Expression>::value);

    if constexpr (Env::template is_entered<Result>)
    {
        return Outcome<Result>::value(environment.template get<Result>(), ValueSource::ManuallyEntered);
    }
    else
    {
        Evaluated<Rational> const computed = checked_evaluate_si<Rational>(expression, environment);
        if (!computed.has_value())
            return std::unexpected { computed.error() };
        if (!computed->has_value())
            return Outcome<Result>::empty();

        std::expected<Rational, ArithmeticError> const inDeclaredUnit =
            checked_convert(**computed, coherent(Expression::dimension), Describe<Result>::unit);
        if (!inDeclaredUnit.has_value())
            return std::unexpected { inDeclaredUnit.error() };

        return Outcome<Result>::value(Measured<Result> { *inDeclaredUnit }, ValueSource::Derived);
    }
}

/// Throwing spelling of `checked_evaluate`, for callers who would only rethrow.
template <Described Result, Node Expression, typename Env>
[[nodiscard]] constexpr Outcome<Result> evaluate(Expression const& expression, Env const& environment)
{
    return detail::or_throw(checked_evaluate<Result>(expression, environment));
}

} // namespace formula
