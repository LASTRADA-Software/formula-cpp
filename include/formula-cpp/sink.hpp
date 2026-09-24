// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// The tracing seam: what the evaluator tells an observer, and the observer
/// that costs nothing.
///
/// A sink is passed to the evaluator **by value**, not by reference. That is
/// not a style choice: passing a stateless sink by reference forces the
/// compiler to materialise the address of an empty object that nothing reads,
/// which clang emits as a real instruction at every call site. By value it
/// disappears. Measured on cl 19.51, clang-cl 22.1.3, clang++ 22.1.3 and
/// g++ 13.3 -- see the phase-7 prep notes.
///
/// The consequence for anyone writing a stateful sink: keep it small and
/// cheap to copy. A sink that owns its storage would copy that storage at
/// every node. `RecordingSink` in `trace.hpp` is a pointer to a `Trace` the
/// caller owns, for exactly this reason.

#include <formula-cpp/expression.hpp>

namespace formula
{

/// What the evaluator tells an observer as it walks a tree.
///
/// `entered` comes before the node's operands are evaluated, `produced` after
/// the node has its answer -- so a sink sees the tree on the way down and the
/// values on the way up.
///
/// **`produced` is not guaranteed for every node that was entered.** When a
/// node fails, its parent returns without evaluating its remaining operands,
/// so those operands are neither entered nor produced. A sink that pairs the
/// two must tolerate an `entered` with no matching `produced` for the failing
/// node's siblings. `RecordingSink` handles this by recording what actually
/// arrived rather than what the node's arity predicts.
template <typename S, typename N, typename V>
concept SinkFor = requires(S sink, N const& node, V const& value) {
    sink.entered(node);
    sink.produced(node, value);
};

/// An observer that observes nothing: the default, and the one the untraced
/// path uses. Empty and stateless, so a by-value copy is free.
struct NullSink
{
    template <Node N>
    constexpr void entered(N const&) noexcept
    {
    }

    template <Node N, typename V>
    constexpr void produced(N const&, V const&) noexcept
    {
    }
};

namespace detail
{
    /// Evaluates @p node with @p sink, through whichever overload exists.
    ///
    /// Phase 5 published `checked_evaluate_si(node, environment)` as an
    /// extension point: a consumer with their own node kind writes an overload
    /// and the evaluator finds it by ADL. This phase adds a third parameter,
    /// which would leave every such overload unreachable. The `requires` below
    /// prefers a sink-aware overload where one exists and falls back to the
    /// two-parameter one where it does not, so a consumer's existing node keeps
    /// evaluating correctly. It simply contributes no trace steps -- the honest
    /// outcome, since the library was never told how to trace it. A consumer
    /// who wants their node traced adds the third parameter.
    ///
    /// `checked_evaluate_si` is not declared at this point in the include
    /// order, and does not need to be: the body is instantiated later, where
    /// ADL sees the whole overload set. Verified on all four compilers.
    template <typename Rep, typename N, typename Env, typename Sink>
    [[nodiscard]] constexpr auto dispatch(N const& node, Env const& environment, Sink sink) noexcept
    {
        if constexpr (requires { checked_evaluate_si<Rep>(node, environment, sink); })
            return checked_evaluate_si<Rep>(node, environment, sink);
        else
            return checked_evaluate_si<Rep>(node, environment);
    }
} // namespace detail

} // namespace formula
