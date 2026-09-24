// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A derivation, recorded: what the evaluator did, step by step.
///
/// **This header is deliberately absent from `formula.hpp`.** It pulls
/// `<vector>`, and a consumer who only evaluates numbers must not compile an
/// arena into every translation unit. It does **not** pull `<string>`: nothing
/// here formats anything, which is what keeps `trace_render.hpp` a separate,
/// separately-optional header. Include this one to record a derivation, and
/// that one as well to print it.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/sink.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace formula
{

/// What kind of node a step came from.
enum class StepKind : std::uint8_t
{
    Variable,
    Constant,
    /// Spelled `PiConstant` rather than `Pi` because GCC's `-Wshadow` reports
    /// an enumerator that shares a name with a global -- and `formula::Pi`,
    /// the rational approximation, is one. The four Windows presets do not
    /// flag it; the Linux CI leg does, with warnings as errors.
    PiConstant,
    Negate,
    Add,
    Subtract,
    Multiply,
    Divide,
    Power,
    Root,
    Documented,
};

/// One node's contribution to a derivation.
///
/// Operands are **indices into the owning `Trace`'s `steps`**, never pointers
/// and never owned children. That is what makes a trace destructible at any
/// depth without recursion, and serialisable without it either.
template <typename Rep = Rational>
struct Step
{
    /// Which kind of node produced this step.
    StepKind kind {};

    /// For `Variable`: how the quantity is written. Points into the static
    /// storage of the quantity's `Describe` specialisation, so it outlives any
    /// trace -- the same guarantee `document.hpp`'s `SymbolEntry` relies on.
    std::string_view symbol {};

    /// For `Documented`: what the wrapped formula cites.
    Citation citation {};

    /// For `Power`: the exponent. For `Root`: the degree. Zero otherwise.
    int exponent {};

    /// The dimension of what this step produced.
    Dimension dimension {};

    /// The unit this step's value was **declared** in -- `Describe<Q>::unit`
    /// for a variable, the constant's own unit for a constant, and the
    /// coherent SI unit of `dimension` for anything computed, which has no
    /// declared unit of its own.
    ///
    /// `value` is always in the coherent SI unit, so that steps are
    /// comparable; this is what a renderer converts back to before showing a
    /// number to a person. Without it a derivation restates every input in a
    /// unit nobody typed: someone who entered 180 l reads `9/50`, which is
    /// the same volume and a worse record. The renderer cannot recover this
    /// on its own -- by the time a `Step` exists the quantity type is erased,
    /// so the recorder captures it here.
    Unit unit {};

    /// What the step produced, in the coherent SI unit of `dimension`. Empty
    /// when the value was **absent** -- which is not an error and must not be
    /// rendered as one.
    std::optional<Rep> value {};

    /// Set when this step failed. A step has a `value` or an `error` or
    /// neither (absent); never both.
    std::optional<ArithmeticError> error {};

    /// Indices of the steps this one consumed, in evaluation order.
    ///
    /// **Not necessarily as many as the node kind suggests.** When an operand
    /// fails, the evaluator returns without evaluating the remaining ones, so
    /// a `Divide` may hold one operand rather than two. What is recorded is
    /// what actually ran.
    std::vector<std::size_t> operands {};
};

/// A recorded derivation: a flat arena of steps.
template <typename Rep = Rational>
struct Trace
{
    /// Every step, in the order they completed -- children before parents.
    std::vector<Step<Rep>> steps {};

    /// Bookkeeping written by `RecordingSink` during a walk. Meaningless once
    /// the walk is over; kept here rather than in the sink because a sink is
    /// copied by value at every node and must stay cheap.
    /// @{
    std::vector<std::size_t> marks {};
    std::vector<std::size_t> unclaimed {};
    /// @}

    /// The index of the outermost step -- the one nothing else consumed.
    ///
    /// A `Trace` may hold more than one walk's steps: constructing a
    /// `RecordingSink` over an existing `Trace` starts a new walk without
    /// discarding the steps an earlier walk already recorded. `root()` always
    /// names the **most recent** walk's root, since that is the one whose
    /// bookkeeping `RecordingSink` just cleared -- an earlier walk's root is
    /// simply some other step this one's arithmetic never reaches.
    ///
    /// @pre `steps` is not empty.
    [[nodiscard]] std::size_t root() const noexcept { return steps.size() - 1; }

    /// Whether anything was recorded.
    [[nodiscard]] bool empty() const noexcept { return steps.empty(); }
};

namespace detail
{
    /// The `StepKind` a node maps to, as a compile-time property of its type.
    template <typename N>
    struct StepKindOf;

    template <Described Q>
    struct StepKindOf<VarNode<Q>>
    {
        static constexpr StepKind value = StepKind::Variable;
    };

    template <Unit U>
    struct StepKindOf<ConstantNode<U>>
    {
        static constexpr StepKind value = StepKind::Constant;
    };

    template <>
    struct StepKindOf<PiNode>
    {
        static constexpr StepKind value = StepKind::PiConstant;
    };

    template <UnaryOperator Op, Node Operand>
    struct StepKindOf<UnaryNode<Op, Operand>>
    {
        static constexpr StepKind value = StepKind::Negate;
    };

    template <BinaryOperator Op, Node Left, Node Right>
    struct StepKindOf<BinaryNode<Op, Left, Right>>
    {
        static constexpr StepKind value = Op == BinaryOperator::Add        ? StepKind::Add
                                          : Op == BinaryOperator::Subtract ? StepKind::Subtract
                                          : Op == BinaryOperator::Multiply ? StepKind::Multiply
                                                                           : StepKind::Divide;
    };

    template <int Exponent, Node Operand>
    struct StepKindOf<PowerNode<Exponent, Operand>>
    {
        static constexpr StepKind value = StepKind::Power;
    };

    template <int Degree, Node Operand>
    struct StepKindOf<RootNode<Degree, Operand>>
    {
        static constexpr StepKind value = StepKind::Root;
    };

    template <Node Inner>
    struct StepKindOf<DocumentedNode<Inner>>
    {
        static constexpr StepKind value = StepKind::Documented;
    };
} // namespace detail

/// Records a derivation into a `Trace` the caller owns.
///
/// A **handle**, not an owner: the evaluator copies its sink by value at every
/// node, so a sink that owned a `std::vector` would copy the whole arena each
/// time. One pointer copies for free. See `sink.hpp` for the measurement that
/// forces this.
///
/// Constructing a `RecordingSink` **begins a walk**: the constructor clears
/// @p trace's `marks` and `unclaimed`, which belong to whichever walk is
/// currently in flight and never to the ones before it. Without this, a
/// second walk into the same `Trace` would find the first walk's root still
/// sitting in `unclaimed` -- nothing left to claim it, since that walk is
/// already over -- and it would linger there, unclaimed, for as long as the
/// `Trace` lives. `steps` itself is left alone: several walks may accumulate
/// their steps into one `Trace` on purpose, which is exactly why `root()`
/// documents itself as naming the most recent walk's root rather than "the"
/// root.
///
/// A `Trace` may therefore be walked repeatedly **in sequence, but never by
/// two sinks at once**: constructing a second `RecordingSink` on a `Trace`
/// whose walk is still in progress clears the bookkeeping that walk is using,
/// and the outer walk's next `produced` then reads `marks.back()` on an empty
/// vector -- undefined behaviour. Nothing in this library does that; only a
/// consumer sharing one `Trace` with an evaluation already under way can, and
/// no runtime guard is levied on every walk to prevent it.
template <typename Rep = Rational>
class RecordingSink
{
  public:
    /// @p trace must outlive the evaluation. Begins a new walk: see the class
    /// comment for why this clears `trace.marks` and `trace.unclaimed`.
    ///
    /// @pre no other `RecordingSink` is part-way through a walk of @p trace.
    explicit constexpr RecordingSink(Trace<Rep>& trace) noexcept: _trace { &trace }
    {
        _trace->marks.clear();
        _trace->unclaimed.clear();
    }

    /// Remembers how much of the arena predates this node, so `produced` can
    /// tell which steps are its operands.
    template <Node N>
    void entered(N const&)
    {
        _trace->marks.push_back(_trace->steps.size());
    }

    /// Records the step, claiming as its operands every step recorded at or
    /// after the matching `entered` that nothing else has claimed.
    template <Node N>
    void produced(N const& node, Evaluated<Rep> const& result)
    {
        std::size_t const mark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> step {};
        step.kind = detail::StepKindOf<N>::value;
        step.dimension = N::dimension;

        // Anything computed has no declared unit, so the coherent SI one is
        // the truthful answer; a variable and a constant each override it
        // with the unit they were written in. `requires { N::unit; }` selects
        // exactly `ConstantNode<U>`, the only node kind that declares such a
        // member -- `VarNode` carries its unit on `Describe<quantity>`
        // instead, and the rest carry none at all.
        step.unit = coherent(N::dimension);
        if constexpr (detail::StepKindOf<N>::value == StepKind::Variable)
            step.unit = Describe<typename N::quantity>::unit;
        else if constexpr (requires { N::unit; })
            step.unit = N::unit;

        if constexpr (detail::StepKindOf<N>::value == StepKind::Variable)
            step.symbol = Describe<typename N::quantity>::symbol;
        if constexpr (detail::StepKindOf<N>::value == StepKind::Documented)
            step.citation = node.citation;
        if constexpr (requires { N::exponent; })
            step.exponent = N::exponent;
        else if constexpr (requires { N::degree; })
            step.exponent = N::degree;

        if (!result.has_value())
            step.error = result.error();
        else if (result->has_value())
            step.value = **result;

        // Everything unclaimed from `mark` onwards belongs to this node.
        auto first = _trace->unclaimed.begin();
        while (first != _trace->unclaimed.end() && *first < mark)
            ++first;
        step.operands.assign(first, _trace->unclaimed.end());
        _trace->unclaimed.erase(first, _trace->unclaimed.end());

        _trace->steps.push_back(std::move(step));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
    }

  private:
    Trace<Rep>* _trace;
};

/// An outcome together with the derivation that produced it.
template <Described Result, typename Rep = Rational>
struct Explained
{
    /// Exactly what `evaluate<Result>` would have returned.
    Outcome<Result> outcome {};
    /// How it was reached.
    Trace<Rep> trace {};
};

/// Evaluates @p expression for @p Result and records how.
///
/// The outcome is identical to `evaluate<Result>(expression, environment)` --
/// tracing observes, it does not participate. What `explain` adds is a
/// `Trace` of every step the evaluator took to reach it.
template <Described Result, typename Rep = Rational, Node Expression, typename Env>
[[nodiscard]] Explained<Result, Rep> explain(Expression const& expression, Env const& environment)
{
    Explained<Result, Rep> explained {};
    RecordingSink<Rep> sink { explained.trace };
    explained.outcome = evaluate<Result>(expression, environment, sink);
    return explained;
}

} // namespace formula
