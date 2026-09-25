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
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/sink.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
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
    /// A `RoundNode`: rounded to a number of decimal places. Checked against
    /// `formula::round` (`rounding.hpp`) the same way `PiConstant` above was
    /// checked against `formula::Pi` -- different case, so it does not
    /// actually collide, but verified on GCC rather than assumed.
    Round,
    /// A `RoundSignificantNode`: rounded to a number of significant digits.
    RoundSignificant,
    /// A `WhenNode`.
    Conditional,
    /// A `NumericValueNode`: the traced escape hatch.
    NumericValue,
};

/// Which branch a `Conditional` step took, if any.
///
/// Not a `bool`: `WhenNode`'s own predicate can itself be absent, in which
/// case neither branch ever ran -- a state a `bool` has no room for. Zero-
/// initialises to `Neither`, which is also the correct default for every
/// step that is *not* a `Conditional`: "no branch taken" is exactly as true
/// there as it is for a conditional whose predicate never resolved, so no
/// separate sentinel is needed for "not applicable".
enum class Branch : std::uint8_t
{
    Neither,
    Then,
    Else,
};

/// `branch` in prose, for a trace render.
[[nodiscard]] constexpr std::string_view describe(Branch branch) noexcept
{
    switch (branch)
    {
        case Branch::Neither:
            return "no branch";
        case Branch::Then:
            return "then";
        case Branch::Else:
            return "else";
    }
    return "unknown branch";
}

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

    /// For `NumericValue`: why the dimension was dropped -- the compile-time
    /// justification `numeric_value_of` was written with. Points into the
    /// static storage of the node's own template-parameter object, the same
    /// guarantee `symbol` above relies on for `Describe`. Empty otherwise, and
    /// the renderer treats an empty one as absent -- see `step_line`.
    std::string_view justification {};

    /// For `Power`: the exponent. For `Root`: the degree. Zero otherwise.
    int exponent {};

    /// For `Round`: the decimal places kept. For `RoundSignificant`: the
    /// significant digits kept. Zero otherwise.
    ///
    /// A field of its own rather than a third and fourth meaning piled onto
    /// `exponent` above, which already carries two (`Power`'s exponent,
    /// `Root`'s degree). `exponent` reused would still work -- both are a
    /// single `int` a switch on `kind` disambiguates -- but a field named
    /// `exponent` holding a decimal-place count is a field whose name lies
    /// about what it holds, and that is how a later reader gets it wrong. A
    /// dedicated field costs four bytes and stays honest.
    int granularity {};

    /// Which branch a `Conditional` step took. See `Branch`'s own comment for
    /// why this is a three-state enum rather than a `bool`, and why its
    /// zero value is already the correct default for every other kind.
    Branch branch {};

    /// For `Conditional`: which way the predicate compared its two sides.
    ///
    /// Recorded because a derivation that says two values were compared but
    /// not *how* is not an audit trail: the trace is the artefact that
    /// survives on its own, away from the formula text, and `#1` and `#2`
    /// with no operator between them leaves a reader unable to check the
    /// step against the method it came from.
    ///
    /// Unlike `branch` above, this field's zero value (`Comparison::Less`) is
    /// **not** a neutral "not applicable": it is a real comparison. It is
    /// meaningful only when `kind` is `Conditional`, exactly as `exponent`
    /// and `granularity` above are meaningful only for the kinds that set
    /// them, and no renderer may read it without checking `kind` first.
    Comparison comparison {};

    /// For `Round` and `RoundSignificant`: the tie-breaking rule the node
    /// rounded under.
    ///
    /// Two rounding nodes differing only in their mode produce different
    /// numbers -- 13 mm and 12 mm from the same 12.5 mm -- so a derivation
    /// that omits the mode cannot explain its own result. The mode is
    /// deliberately absent from `render()` and hence from `document()` (a
    /// standard states a granularity, not a tie rule; see
    /// `render_node(RoundNode ...)`); the trace is where it belongs, because
    /// a trace exists to say why *this* number came out as it did.
    ///
    /// As with `comparison` above, the zero value is a real mode
    /// (`RoundingMode::HalfAwayFromZero`) and not a "not applicable"
    /// sentinel: meaningful only for the two rounding kinds.
    RoundingMode mode {};

    /// The dimension of what this step produced.
    Dimension dimension {};

    /// The unit this step's value was **declared** in -- `Describe<Q>::unit`
    /// for a variable, the constant's own unit for a constant, the node's own
    /// unit for a `Round` or `RoundSignificant` step, and the coherent SI unit
    /// of `dimension` for anything else computed, which has no declared unit
    /// of its own.
    ///
    /// `value` is always in the coherent SI unit, so that steps are
    /// comparable; this is what a renderer converts back to before showing a
    /// number to a person. Without it a derivation restates every input in a
    /// unit nobody typed: someone who entered 180 l reads `9/50`, which is
    /// the same volume and a worse record. The renderer cannot recover this
    /// on its own -- by the time a `Step` exists the quantity type is erased,
    /// so the recorder captures it here.
    ///
    /// **Deliberately not the unit `NumericValue` was read in.** That unit
    /// measures the *operand's* dimension (`Megapascal`, say), while a
    /// `NumericValueNode`'s own `dimension` is always `Scalar` -- assigning it
    /// here would make the renderer's `checked_convert(value, coherent(dimension),
    /// unit)` compare a `Scalar` `from` against a non-`Scalar` `to` and refuse
    /// every such step with a dimension-mismatch error, hiding the very number
    /// this node exists to produce. See `sourceUnit` below for that unit.
    Unit unit {};

    /// For `NumericValue`: the unit the escape hatch read its number in --
    /// `Megapascal` for `numeric_value_of<Megapascal, "...">(...)`. Kept
    /// separate from `unit` above for the reason documented there: this one
    /// deliberately does not share `dimension`, so it is never used to
    /// convert `value` -- it is read only by the renderer, to say what unit
    /// the bare number came from. A default-constructed `Unit` (dimension
    /// `Scalar`, empty symbol) otherwise.
    Unit sourceUnit {};

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

    /// Where each node in progress found the arena when it was entered, so
    /// that `produced` can tell which steps are its operands.
    ///
    /// Bookkeeping written by `RecordingSink` during a walk and meaningless
    /// once the walk is over. It lives here rather than in the sink because a
    /// sink is copied by value at every node and must stay cheap to copy.
    std::vector<std::size_t> marks {};

    /// Steps recorded but not yet claimed as some other step's operand. After
    /// a completed walk this holds exactly one entry: that walk's root.
    ///
    /// Bookkeeping, as `marks` is, and for the same reason.
    std::vector<std::size_t> unclaimed {};

    /// Which branch each still-open `Conditional` step is on its way to
    /// recording. `entered` pushes `Branch::Neither` for a `WhenNode` and
    /// nothing else; `RecordingSink::branch_taken` (called, optionally, from
    /// `conditional.hpp`'s `checked_evaluate_si(WhenNode ...)`) overwrites the
    /// top entry once a branch is actually selected; `produced` pops it onto
    /// the step. A stack, not a single slot, for the same reason `marks` is
    /// one: a `when()` nested inside another's branch must not clobber its
    /// still-open parent's pending entry.
    ///
    /// Bookkeeping, as `marks` and `unclaimed` are, and for the same reason.
    std::vector<Branch> branchStack {};

    /// The index of the outermost step -- the one nothing else consumed.
    ///
    /// A `Trace` may hold more than one walk's steps: constructing a
    /// `RecordingSink` over an existing `Trace` starts a new walk without
    /// discarding the steps an earlier walk already recorded. `root()` always
    /// names the **most recent** walk's root, since that is the one whose
    /// bookkeeping `RecordingSink` just cleared -- an earlier walk's root is
    /// simply some other step this one's arithmetic never reaches.
    ///
    /// A `Trace` is empty whenever the walk that would have filled it never
    /// ran at all -- `explain` leaves it empty for a result the environment
    /// overrides, since the expression is never dispatched. Check `empty()`
    /// before calling this; on an empty `Trace`, `steps.size() - 1` underflows
    /// and the returned index names no step.
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

    template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct StepKindOf<RoundNode<U, Places, Mode, Operand>>
    {
        static constexpr StepKind value = StepKind::Round;
    };

    template <Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
    struct StepKindOf<RoundSignificantNode<U, Digits, Mode, Operand>>
    {
        static constexpr StepKind value = StepKind::RoundSignificant;
    };

    template <Predicate P, Node Then, Node Else>
    struct StepKindOf<WhenNode<P, Then, Else>>
    {
        static constexpr StepKind value = StepKind::Conditional;
    };

    template <Unit U, FixedString Justification, Node Operand>
    struct StepKindOf<NumericValueNode<U, Justification, Operand>>
    {
        static constexpr StepKind value = StepKind::NumericValue;
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
/// @p trace's `marks`, `unclaimed`, and `branchStack`, which belong to
/// whichever walk is currently in flight and never to the ones before it.
/// Without this, a second walk into the same `Trace` would find the first
/// walk's root still sitting in `unclaimed` -- nothing left to claim it,
/// since that walk is already over -- and it would linger there, unclaimed,
/// for as long as the `Trace` lives. `steps` itself is left alone: several
/// walks may accumulate their steps into one `Trace` on purpose, which is
/// exactly why `root()` documents itself as naming the most recent walk's
/// root rather than "the" root.
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
    /// comment for why this clears `trace.marks`, `trace.unclaimed`, and
    /// `trace.branchStack`.
    ///
    /// @pre no other `RecordingSink` is part-way through a walk of @p trace.
    explicit constexpr RecordingSink(Trace<Rep>& trace) noexcept: _trace { &trace }
    {
        _trace->marks.clear();
        _trace->unclaimed.clear();
        _trace->branchStack.clear();
    }

    /// Remembers how much of the arena predates this node, so `produced` can
    /// tell which steps are its operands. For a `WhenNode` specifically, also
    /// pushes a pending `Branch::Neither` onto `branchStack` -- popped by
    /// `produced` below, and overwritten in between by `branch_taken` only if
    /// a branch actually runs. Pushed unconditionally, not only once a branch
    /// is known to run, because `produced` always pops exactly one entry for
    /// every `Conditional` step, including one whose predicate errored or was
    /// absent and so never called `branch_taken` at all.
    template <Node N>
    void entered(N const&)
    {
        _trace->marks.push_back(_trace->steps.size());
        if constexpr (detail::StepKindOf<N>::value == StepKind::Conditional)
            _trace->branchStack.push_back(Branch::Neither);
    }

    /// Told which branch a `WhenNode` selected, right before it dispatches
    /// that branch. Not part of `SinkFor` (`sink.hpp`): `conditional.hpp`'s
    /// `checked_evaluate_si(WhenNode ...)` calls it through `if constexpr
    /// (requires {...})`, the same pattern `sink.hpp`'s `dispatch` uses to
    /// find a sink-aware `checked_evaluate_si` overload, so a sink that has
    /// no use for it -- `NullSink` included -- simply does not define it and
    /// pays nothing.
    ///
    /// Updates the top of `branchStack`, which the matching `entered` above
    /// pushed and the matching `produced` below will pop -- the same
    /// push-in-`entered`, pop-in-`produced` discipline as `marks`, and for
    /// the same reason: a `when()` nested inside another's branch must not
    /// clobber its still-open parent's pending entry.
    template <Node N>
    void branch_taken(N const&, bool thenTaken) noexcept
    {
        _trace->branchStack.back() = thenTaken ? Branch::Then : Branch::Else;
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
        // the truthful answer; a variable overrides it with the unit its
        // quantity is declared in. `requires { N::unit; }` now also selects
        // `ConstantNode<U>`, `RoundNode`, and `RoundSignificantNode` -- every
        // one of them declares a unit that is the single most load-bearing
        // fact about the step: `rounded<Megapascal, 1>(...)` rounds *in
        // megapascals*, and a step recording "rounded to 1 dp" without saying
        // 1 dp of what is not a record of anything. `VarNode` still carries
        // its unit on `Describe<quantity>` instead of a member of its own,
        // which is why it needs the branch above rather than this one.
        //
        // `NumericValueNode` is excluded even though it also declares
        // `unit`: unlike the three kinds above, its declared unit measures
        // its *operand's* dimension, not its own -- a `NumericValueNode` is
        // always `Scalar` -- so assigning it here would make this step's
        // `unit` disagree with its `dimension`, and the renderer's
        // `checked_convert(value, coherent(dimension), unit)` would refuse
        // every such step as a dimension mismatch. See `Step::sourceUnit`,
        // which is where that unit goes instead.
        step.unit = coherent(N::dimension);
        if constexpr (detail::StepKindOf<N>::value == StepKind::Variable)
            step.unit = Describe<typename N::quantity>::unit;
        else if constexpr (detail::StepKindOf<N>::value != StepKind::NumericValue && requires { N::unit; })
            step.unit = N::unit;

        if constexpr (detail::StepKindOf<N>::value == StepKind::Variable)
            step.symbol = Describe<typename N::quantity>::symbol;
        if constexpr (detail::StepKindOf<N>::value == StepKind::Documented)
            step.citation = node.citation;
        if constexpr (detail::StepKindOf<N>::value == StepKind::NumericValue)
        {
            step.justification = N::justification;
            step.sourceUnit = N::unit;
        }
        if constexpr (requires { N::exponent; })
            step.exponent = N::exponent;
        else if constexpr (requires { N::degree; })
            step.exponent = N::degree;

        if constexpr (requires { N::places; })
            step.granularity = N::places.value;
        else if constexpr (requires { N::digits; })
            step.granularity = N::digits.value;

        // `RoundNode` and `RoundSignificantNode` are the only kinds that
        // declare one, so the `requires` alone selects them -- the same shape
        // `exponent` and `granularity` above use.
        if constexpr (requires { N::mode; })
            step.mode = N::mode;

        if constexpr (detail::StepKindOf<N>::value == StepKind::Conditional)
        {
            // `WhenNode` re-exports its predicate's comparison for exactly
            // this: a sink is handed a node, never the predicate's type.
            step.comparison = N::comparison;
            step.branch = _trace->branchStack.back();
            _trace->branchStack.pop_back();
        }

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
    /// How it was reached -- **empty** when `outcome` is a manual override.
    /// See `explain`'s own comment for why.
    Trace<Rep> trace {};
};

/// Evaluates @p expression for @p Result and records how.
///
/// The outcome is identical to `evaluate<Result>(expression, environment)` --
/// tracing observes, it does not participate. What `explain` adds is a
/// `Trace` of every step the evaluator took to reach it.
///
/// **`explained.trace` can come back empty.** When `environment` carries a
/// manual override for `Result`, `checked_evaluate` returns that value
/// without ever dispatching @p expression -- correctly: an overridden number
/// was not derived, so there is nothing to trace -- and nothing is recorded.
/// Check `explained.trace.empty()` before indexing into `steps` with `root()`;
/// on an empty `Trace`, `root()` names no step at all.
///
/// Only `Rep = Rational` is supported. `evaluate<Result>` computes in
/// `Rational` internally and hands the sink an `Evaluated<Rational>`
/// regardless of @p Rep, so a `RecordingSink<Rep>` built for any other @p Rep
/// cannot receive what the evaluator actually passes it -- the `static_assert`
/// below turns that mismatch into one sentence instead of a template-frame
/// dump. Call `checked_evaluate_si<Rep>` directly with your own
/// `RecordingSink<Rep>` to trace a `double` computation.
template <Described Result, typename Rep = Rational, Node Expression, typename Env>
[[nodiscard]] Explained<Result, Rep> explain(Expression const& expression, Env const& environment)
{
    static_assert(std::is_same_v<Rep, Rational>,
                  "formula: explain only supports Rep = Rational -- evaluate<Result> always computes "
                  "in Rational internally and hands its sink an Evaluated<Rational>, so a "
                  "RecordingSink<Rep> built for a different Rep cannot receive it. Call "
                  "checked_evaluate_si<Rep> directly with your own RecordingSink<Rep> to trace a "
                  "double computation.");

    Explained<Result, Rep> explained {};
    RecordingSink<Rep> sink { explained.trace };
    explained.outcome = evaluate<Result>(expression, environment, sink);
    return explained;
}

} // namespace formula
