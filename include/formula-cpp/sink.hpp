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
///
/// **A series (`series.hpp`) reaches a sink through two optional members of
/// its own**, never through `entered` and `produced`, which stay constrained
/// on `Node` because every `Node` still produces one value:
///
///     sink.series_entered(node);            // before the series is evaluated
///     sink.series_produced(node, result);   // after, with its EvaluatedSeries
///
/// A sink defines both or neither: the evaluator asks for the pair in one
/// `requires` (`detail::HearsSeries`), so a sink defining only one is told
/// nothing. This sink defines neither and pays nothing, and a sink written
/// before series existed keeps compiling and is told nothing about them.
/// `RecordingSink` (`trace.hpp`) defines both.
///
/// **Optional hooks.** Each is asked for through its own `requires`, so a
/// sink that does not define one is told nothing through it and still
/// compiles. `RecordingSink` defines them all:
///
///     sink.branch_taken(node, thenTaken);           // a `when` chose a branch
///     sink.input_source(varNode, source);           // a variable was measured or typed in
///     sink.series_input_source(seriesNode, source); // the same, of a whole series
///     sink.replaced_entry_source(node, source);     // an overlay's constant or derived
///                                                   // quantity replaced an entry of this source
///     sink.replaced_entry_empty(node);              // ... and that entry held no value
///     sink.record_entered(origin);                  // a read from another record began
///     sink.lineage_checked(check, outcome);         // one lineage attribute compared
///
/// Two of them are easy to misread:
/// - `input_source` is told about a **variable's own value** only. An
///   overlay's fixed constant and derived quantity are `VarNode<Q>`s too, and
///   bind to an `input_source(VarNode<Q> const&, ...)` overload; but the
///   source they have is the replaced entry's, never the value the step shows
///   -- the jurisdiction's -- so they report it through
///   `replaced_entry_source` instead, and a sink reading `input_source` as
///   "this value was typed in" is never misled by a constant.
/// - `record_entered` and `lineage_checked` come as a pair around a read from
///   another record (`record.hpp`); a sink defining only one is refused
///   (`detail::RequireScopeHooksTogether`), since the attribute steps it
///   records would otherwise belong to no record.
///
/// **A sink that records steps** must say, of every step recorded between a
/// `record_entered` and the scope's own `produced`, that it was read from that
/// record -- on every path that records one, a series', a curve's and a
/// conformity check's included. `RecordingSink` does it in one place
/// (`stamp_origin`), which every path that appends to `Trace::steps` calls;
/// a path added later that does not traces a value read from another record as
/// this record's.
///
/// **An opaque call (`opaque.hpp`) reaches a sink the same way**, through
/// `opaque_entered(info)` before its first input is evaluated and
/// `opaque_produced(info, result)` after `compute`, asked for together
/// (`detail::HearsOpaque`); `info` is an `OpaqueCallInfo`, plain data. Each
/// output used is a `Node`, and is told through `entered` and `produced`.
///
/// **A retry (`retry.hpp`) reaches a sink the same way**, through two optional
/// pairs, each asked for together: `retry_entered(info)` before its starting
/// value and `retry_produced(info, ended)` after its last attempt, with what
/// `checked_evaluate_retry` returns (`detail::HearsRetry`); and
/// `attempt_entered(info)` and `attempt_produced(info, produced, judgement,
/// failure)` around each attempt that runs (`detail::HearsAttempts`). `info`
/// is a `RetryInfo` or an `AttemptInfo`, plain data. The attempt's nodes are
/// told through `entered` and `produced`.
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

struct Citation;

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
    /// `Variants::published`.
    std::size_t index {};

    /// How many variants the method declares as published.
    std::size_t count {};

    /// How many variants overlays pruned from the method before this one
    /// was selected (`prune_variant`, `overlay.hpp`); zero when none did.
    /// Spec section 16.7 makes which variants apply a jurisdiction's
    /// decision, so a trace says when one made it.
    std::size_t prunedCount {};

    /// What the last overlay that pruned cited; null when none pruned.
    ///
    /// This and `pinnedBy` are held by the method being evaluated, and valid
    /// only for the call that tells a sink of this selection; a sink that
    /// keeps one copies it. Pointers because `citation.hpp`, which defines
    /// `Citation`, includes this header.
    Citation const* prunedBy = nullptr;

    /// What the overlay that pinned the method to this variant cited
    /// (`pin_variant`); null when none pinned it. Prunes before a pin are
    /// still reported: a jurisdiction may prune what another then pins.
    Citation const* pinnedBy = nullptr;
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
