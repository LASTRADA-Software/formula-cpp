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

#include <cstddef>
#include <string_view>

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
///
/// **A sink must not throw.** Every `checked_evaluate_si` overload that calls
/// a sink is `noexcept`, so an exception escaping `entered` or `produced`
/// does not propagate to the caller -- it calls `std::terminate`. This is not
/// a hypothetical: `RecordingSink::produced` allocates on every call, and a
/// sink is free to allocate too -- what it must do instead of throwing is
/// catch, or otherwise avoid throwing, whatever an allocation failure or
/// other error inside it would raise.
template <typename S, typename N, typename V>
concept SinkFor = requires(S sink, N const& node, V const& value) {
    sink.entered(node);
    sink.produced(node, value);
};

/// An observer that observes nothing: the default, and the one the untraced
/// path uses. Empty and stateless, so a by-value copy is free.
struct NullSink
{
    /// Told that a node is about to be evaluated, and does nothing with it.
    template <Node N>
    constexpr void entered(N const&) noexcept
    {
    }

    /// Told what a node produced, and does nothing with it. Empty on purpose:
    /// the measurements behind the by-value sink design hold only because
    /// there is nothing here for a compiler to keep.
    template <Node N, typename V>
    constexpr void produced(N const&, V const&) noexcept
    {
    }
};

/// Which variant a method selected, as `evaluate_method` (`method.hpp`) tells
/// a sink that asks.
///
/// A method is not a `Node` -- it chooses between expressions rather than
/// standing where a number stands -- so its choice cannot reach a sink
/// through `entered` and `produced`. It is told instead through two optional
/// members, found the way `RecordingSink::branch_taken` is, through
/// `if constexpr (requires {...})`:
///
///     sink.variant_entered(selection);          // before the variant is evaluated
///     sink.variant_produced(selection, result); // after, with what it produced
///
/// A sink defines **both or neither**: `evaluate_method` asks for the pair in
/// one `requires`, so a sink defining only one is told nothing, rather than
/// told half and left with an `entered` it will never see matched. `NullSink`
/// defines neither and pays nothing.
///
/// Plain data, so that a sink needs neither the method's type nor the tag's
/// to record the decision: the tag has already been named here, while its
/// type was still known.
struct VariantSelection
{
    /// The selected variant's tag, as `tag_name` (`tag.hpp`) spells it --
    /// `Cylinder`, or the author's `TagName` spelling. Static storage, so a
    /// sink may keep the view; see `tag_name`. Empty only if the compiler's
    /// spelling could not be read, and then `index` is what identifies it.
    std::string_view tag {};

    /// The selected variant's ZERO-BASED position in the method's `variants(...)`
    /// as published, before any overlay pinned or pruned -- see
    /// `Variants::publishedPositions`.
    std::size_t index {};

    /// How many variants the method declares as published.
    std::size_t count {};
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
    /// outcome, since the library was never told how to trace it.
    ///
    /// **That graceful degradation is the two-parameter overload's alone.** A
    /// consumer's three-parameter overload that reports its own node --
    /// `sink.entered(node)`, `sink.produced(node, result)` -- compiles against
    /// `NullSink` and against a sink of the consumer's own, but **fails to
    /// compile** against `RecordingSink` (`trace.hpp`): that sink looks every
    /// node's kind up in `detail::StepKindOf`, a closed registry whose primary
    /// template is left undefined, and the consumer's node has no entry in it.
    /// Measured on g++ 13.3 ("incomplete type
    /// `formula::detail::StepKindOf<...>` used in nested name specifier") and
    /// on cl 19.51 (C2027, use of undefined type). So a consumer's node cannot
    /// yet appear in a recorded trace at all. A three-parameter overload that
    /// only hands the sink on to its operands' `dispatch`, and reports nothing
    /// of its own, does compile against `RecordingSink`: its operands are
    /// traced and it is not (measured on the same two). Opening the registry
    /// to consumers is a separate change.
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
