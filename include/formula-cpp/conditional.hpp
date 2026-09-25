// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A threshold that selects between two formulas: `when(predicate, then, else)`.
///
/// Two design decisions, both load-bearing.
///
/// **Only the selected branch is evaluated.** A formula guarded by
/// `when(v != 0, x / v, fallback)` exists precisely because the other branch
/// is invalid for these inputs; evaluating it anyway could raise an
/// arithmetic error that has nothing to do with the answer, and would put
/// work in the trace that did not determine the result -- which misleads
/// whoever reads it. `checked_evaluate_si` below reaches this by ordinary C++
/// selection: the unselected branch's `dispatch` call is never made, not
/// merely discarded after being made.
///
/// **Both branches must have the same dimension.** A conditional whose answer
/// is sometimes a length and sometimes an area has no dimension to report, so
/// `WhenNode` asserts the two branches agree the same way `RequireAddendsAgree`
/// in `expression.hpp` asserts two addends agree -- see that one for the
/// diagnostic reasoning `detail::RequireBranchesAgree` below follows.
///
/// `WhenNode` is a `Node`: it has one dimension, the (already agreeing)
/// dimension of its two branches, and a formula may nest one `when()` inside
/// another or use one as an operand of `+`, `*`, and so on like any other
/// node. Its own predicate is not a `Node` -- see `predicate.hpp` for why --
/// so only the two branches are dispatched as subtrees; the predicate is
/// evaluated through `checked_evaluate_predicate` directly, with the sink
/// threaded through so the predicate's own operands still trace.
///
/// **Which branch was taken** is reported through one optional hook,
/// `sink.branch_taken(node, thenTaken)`, called below only once the
/// predicate has actually resolved and only when the sink defines one --
/// `if constexpr (requires {...})`, the same pattern `sink.hpp`'s `dispatch`
/// uses to find a sink-aware `checked_evaluate_si` overload. A sink with no
/// use for it, `NullSink` included, defines nothing and pays nothing for the
/// check. `RecordingSink` (`trace.hpp`) is the one sink that defines it,
/// because it is the only place a step exists to record the branch onto --
/// `PredicateNode` is not a `Node` and so never gets a step of its own to
/// carry it instead.

#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/sink.hpp>

namespace formula
{

namespace detail
{
    /// Fails to compile when the two branches of a `when()` measure different
    /// dimensions.
    ///
    /// A named template, and templated on the *branches* rather than on the
    /// `when()` call, for the same diagnostic reason as `RequireAddendsAgree`
    /// in `expression.hpp` -- see that one for the measurement behind it.
    template <Node Then, Node Else>
    struct RequireBranchesAgree
    {
        static_assert(Then::dimension == Else::dimension,
                      "formula: the two branches of this when() measure different dimensions; the "
                      "offending branches appear in this diagnostic as the template arguments of "
                      "RequireBranchesAgree");

        static constexpr bool value = true;
    };
} // namespace detail

/// A threshold that selects between two expressions of the same dimension.
/// Not evaluated by comparing both and discarding one -- see the file comment.
template <Predicate P, Node Then, Node Else>
struct WhenNode: NodeBase
{
    static_assert(detail::RequireBranchesAgree<Then, Else>::value);

    /// The condition that selects a branch.
    P predicate {};
    /// Evaluated, and only evaluated, when `predicate` holds.
    Then thenBranch {};
    /// Evaluated, and only evaluated, when `predicate` does not hold.
    Else elseBranch {};

    /// The two branches already agree; this is that (shared) dimension.
    static constexpr Dimension dimension = Then::dimension;

    /// Which way `predicate` compares its two sides, re-exported from `P`.
    ///
    /// A sink is handed the node, not the node's type spelled out, and
    /// `RecordingSink::produced` (`trace.hpp`) reads a node's compile-time
    /// facts as `N::something` -- `N::dimension`, `N::unit`, `N::exponent`,
    /// `N::mode`. Without this member the comparison would be reachable only
    /// by naming `P` and reaching into it, which a generic sink cannot do,
    /// and a recorded conditional step could say that two things were
    /// compared but never which way. The alias-shaped alternative
    /// (`using predicate = P;`) is not available here: `predicate` is already
    /// the name of the data member above.
    static constexpr Comparison comparison = P::comparison;
};

/// `when(predicate, thenBranch, elseBranch)`: `thenBranch` where `predicate`
/// holds, `elseBranch` where it does not. Fails to compile if the two
/// branches measure different dimensions -- see `detail::RequireBranchesAgree`.
template <Predicate P, Node Then, Node Else>
[[nodiscard]] constexpr auto when(P predicate, Then thenBranch, Else elseBranch) noexcept
{
    return WhenNode<P, Then, Else> { {}, predicate, thenBranch, elseBranch };
}

/// Evaluates `predicate`; if it holds, evaluates and returns `thenBranch`, if
/// it does not, evaluates and returns `elseBranch`, and if it is absent,
/// returns absent without evaluating either. Exactly one branch is ever
/// dispatched -- the ternary below selects the call, not its result -- so an
/// arithmetic error confined to the branch not taken never surfaces.
template <typename Rep = Rational, Predicate P, Node Then, Node Else, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(WhenNode<P, Then, Else> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);

    std::expected<std::optional<bool>, ArithmeticError> const verdict =
        checked_evaluate_predicate<Rep>(node.predicate, environment, sink);
    if (!verdict.has_value())
    {
        Evaluated<Rep> const failed = std::unexpected { verdict.error() };
        sink.produced(node, failed);
        return failed;
    }
    if (!verdict->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    bool const thenTaken = **verdict;
    Evaluated<Rep> const result = thenTaken ? detail::dispatch<Rep>(node.thenBranch, environment, sink)
                                             : detail::dispatch<Rep>(node.elseBranch, environment, sink);
    if constexpr (requires { sink.branch_taken(node, thenTaken); })
        sink.branch_taken(node, thenTaken);
    sink.produced(node, result);
    return result;
}

} // namespace formula
