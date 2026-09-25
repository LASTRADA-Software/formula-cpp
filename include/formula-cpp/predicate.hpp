// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Predicates: comparing two expressions, without becoming one.
///
/// A `Node` has a dimension and evaluates to a number. Comparing two
/// expressions produces neither -- a truth value has no unit -- so a
/// predicate is deliberately not a `Node`. Giving it one would mean either
/// inventing a dimension to lie about, or weakening what `Node` promises
/// every other part of the library: that anything satisfying the concept can
/// be asked for `dimension` and get back a real answer. A predicate gets its
/// own type, its own concept, and its own evaluation entry point instead.
///
/// Absence propagates through a predicate exactly as it propagates through a
/// `BinaryNode`: if either side was never measured, the predicate has no
/// verdict. "The diameter nobody measured is not greater than 50" is not a
/// claim anyone is entitled to make, and answering `false` would silently
/// select a branch on the strength of a measurement that does not exist.

#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/sink.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <type_traits>

namespace formula
{

/// Which way two expressions are compared.
enum class Comparison : std::uint8_t
{
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual,
    Equal,
    NotEqual,
};

namespace detail
{
    /// Fails to compile when the two sides of a comparison measure different
    /// dimensions.
    ///
    /// A named template, and templated on the *operands* rather than on the
    /// comparison, for the same diagnostic reason as `RequireAddendsAgree` in
    /// `expression.hpp` -- see that one for the measurement behind it. Every
    /// `Comparison` enumerator imposes the same requirement, so unlike
    /// `AdditiveDimensionsAgree` this needs no per-operator routing: it is
    /// asserted once, unconditionally, from `PredicateNode` itself.
    template <Node Left, Node Right>
    struct RequireComparandsAgree
    {
        static_assert(Left::dimension == Right::dimension,
                      "formula: the two sides of this comparison measure different dimensions; the "
                      "offending operands appear in this diagnostic as the template arguments of "
                      "RequireComparandsAgree");

        static constexpr bool value = true;
    };
} // namespace detail

/// A comparison of two expressions. Not a `Node`: it has no dimension and
/// evaluates to a truth value, and pretending otherwise would either lie
/// about its dimension or weaken what `Node` promises.
template <Comparison Op, Node Left, Node Right>
struct PredicateNode
{
    static_assert(detail::RequireComparandsAgree<Left, Right>::value);

    /// The left-hand child expression.
    Left lhs {};
    /// The right-hand child expression.
    Right rhs {};

    /// Which comparison this is.
    static constexpr Comparison comparison = Op;
};

namespace detail
{
    /// The primary template is deliberately empty: a type that is not a
    /// `PredicateNode` specialisation simply does not satisfy `Predicate`,
    /// rather than triggering a hard error.
    template <typename T>
    struct IsPredicateNode: std::false_type
    {
    };

    template <Comparison Op, Node Left, Node Right>
    struct IsPredicateNode<PredicateNode<Op, Left, Right>>: std::true_type
    {
    };
} // namespace detail

/// Anything that compares two expressions.
template <typename P>
concept Predicate = detail::IsPredicateNode<std::remove_cvref_t<P>>::value;

// ---------------------------------------------------------------- operators
//
// Taken and returned by value, for the same reason as the arithmetic
// operators in `expression.hpp`: a node is empty or holds one `Rational`, so
// there is nothing to save by reference, and a reference into a temporary
// subtree is a dangling read waiting to happen.

/// `lhs < rhs`. Fails to compile if `lhs` and `rhs` measure different
/// dimensions -- see `detail::RequireComparandsAgree`.
template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator<(Left lhs, Right rhs) noexcept
{
    return PredicateNode<Comparison::Less, Left, Right> { lhs, rhs };
}

/// `lhs <= rhs`. Fails to compile if `lhs` and `rhs` measure different
/// dimensions -- see `detail::RequireComparandsAgree`.
template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator<=(Left lhs, Right rhs) noexcept
{
    return PredicateNode<Comparison::LessOrEqual, Left, Right> { lhs, rhs };
}

/// `lhs > rhs`. Fails to compile if `lhs` and `rhs` measure different
/// dimensions -- see `detail::RequireComparandsAgree`.
template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator>(Left lhs, Right rhs) noexcept
{
    return PredicateNode<Comparison::Greater, Left, Right> { lhs, rhs };
}

/// `lhs >= rhs`. Fails to compile if `lhs` and `rhs` measure different
/// dimensions -- see `detail::RequireComparandsAgree`.
template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator>=(Left lhs, Right rhs) noexcept
{
    return PredicateNode<Comparison::GreaterOrEqual, Left, Right> { lhs, rhs };
}

/// `lhs == rhs`. Fails to compile if `lhs` and `rhs` measure different
/// dimensions -- see `detail::RequireComparandsAgree`.
template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator==(Left lhs, Right rhs) noexcept
{
    return PredicateNode<Comparison::Equal, Left, Right> { lhs, rhs };
}

/// `lhs != rhs`. Fails to compile if `lhs` and `rhs` measure different
/// dimensions -- see `detail::RequireComparandsAgree`.
template <Node Left, Node Right>
[[nodiscard]] constexpr auto operator!=(Left lhs, Right rhs) noexcept
{
    return PredicateNode<Comparison::NotEqual, Left, Right> { lhs, rhs };
}

/// The truth value of @p predicate, or absence when either side is absent.
///
/// Evaluates both sides to the coherent SI unit of their (shared) dimension
/// -- the same scale `checked_evaluate_si` already puts every operand on --
/// so the comparison is exact and needs no rounding policy: comparing two
/// `Rational`s is exact by construction, and inventing a tolerance here would
/// silently answer a question the caller never asked.
///
/// Mirrors `checked_evaluate_si(BinaryNode ...)`'s rule for combining two
/// operands: an arithmetic error on either side is returned at once, and
/// absence is considered only once both sides have actually been evaluated,
/// so a real error on the present side is never hidden behind the other
/// side's absence.
template <typename Rep = Rational, Comparison Op, Node Left, Node Right, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<std::optional<bool>, ArithmeticError>
checked_evaluate_predicate(PredicateNode<Op, Left, Right> const& predicate, Env const& environment,
                           Sink sink = {}) noexcept
{
    Evaluated<Rep> const lhs = detail::dispatch<Rep>(predicate.lhs, environment, sink);
    if (!lhs.has_value())
        return std::unexpected { lhs.error() };

    Evaluated<Rep> const rhs = detail::dispatch<Rep>(predicate.rhs, environment, sink);
    if (!rhs.has_value())
        return std::unexpected { rhs.error() };

    if (!lhs->has_value() || !rhs->has_value())
        return std::optional<bool> {};

    bool const verdict = [&] {
        if constexpr (Op == Comparison::Less)
            return **lhs < **rhs;
        else if constexpr (Op == Comparison::LessOrEqual)
            return **lhs <= **rhs;
        else if constexpr (Op == Comparison::Greater)
            return **lhs > **rhs;
        else if constexpr (Op == Comparison::GreaterOrEqual)
            return **lhs >= **rhs;
        else if constexpr (Op == Comparison::Equal)
            return **lhs == **rhs;
        else
            return **lhs != **rhs;
    }();
    return std::optional<bool> { verdict };
}

} // namespace formula
