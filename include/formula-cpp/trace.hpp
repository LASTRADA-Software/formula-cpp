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
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/sink.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
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
    /// A `Constraint`'s verdict. Checked against `formula::Constraint` the
    /// same way `PiConstant` above was checked against `formula::Pi` -- same
    /// spelling this time, not merely the same risk -- and confirmed clean on
    /// GCC under `-Wshadow` rather than assumed clean because the Windows
    /// presets raised nothing.
    Constraint,
    /// A `BandedLookupNode`: a measured value fell in an interval, and that
    /// interval selected a correction (`lookup.hpp`).
    ///
    /// Spelled without the `Node` suffix the type carries, so that this
    /// enumerator and the two after it read as what a step *is* rather than as
    /// what class produced it -- the same relationship `Conditional` has to
    /// `WhenNode`.
    /// Checked on GCC under `-Wshadow` against every name in namespace
    /// `formula`, the way `PiConstant` above had to be: the factories are
    /// `banded_lookup`, `exact_lookup` and `interpolating_lookup`, which
    /// differ in case as well as in spelling, and the node types carry the
    /// suffix -- but that was verified on the compiler that objects rather
    /// than assumed from the four that do not.
    BandedLookup,
    /// An `ExactLookupNode`: a category key named a row directly.
    ExactLookup,
    /// An `InterpolatingLookupNode`: a measured value sat between two rows,
    /// and the answer is the value those rows imply at that point -- a number
    /// that appears in no row of the table.
    InterpolatingLookup,
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

/// Whose failure a lookup step is carrying, and of what kind.
///
/// **The one field this whole surface exists for.** All three lookup kinds
/// report every failure through `Evaluated<Rep>`'s single error channel, and
/// `checked_evaluate_si` (`lookup.hpp`) calls `sink.produced(node, failed)`
/// for an error it is merely *passing upward* in exactly the same way it
/// calls it for one of its **own**. So a lookup step carrying
/// `ArithmeticError::DomainError` is ambiguous on its face between "the value
/// fell in no band" and "the operand failed, two levels down, and I am
/// relaying it" -- and a renderer reading such a step alone would emit
/// something true-sounding and useless ("lookup failed: argument outside the
/// domain of the operation") for a case where nothing was outside any domain.
/// Phase 9 refused `bool satisfied()` for exactly this shape of defect: a
/// surface that must answer something for a case it cannot distinguish, where
/// the plausible answer is a lie.
///
/// `ArithmeticError::Overflow` is ambiguous the same way and **is not the
/// same case**: only the interpolating lookup computes anything, so only it
/// can overflow of its own accord, where the banded and the exact lookup
/// merely select and can do no arithmetic beyond converting units.
///
/// The recorder resolves both ambiguities at the moment the step is made, by
/// inspecting the operand step the node just claimed and by re-asking
/// `lookup.hpp`'s **own** `find_band`, `find_key` and `interpolate` -- the
/// very functions that made the decision -- rather than by re-deciding with
/// logic of its own that could drift from them. See `detail::record_lookup`.
enum class LookupFailure : std::uint8_t
{
    /// Nothing failed. The zero value, so every step that is not a lookup is
    /// already correct without the recorder saying anything -- exactly as
    /// `Branch::Neither` is the right default for every step that is not a
    /// `Conditional`. Also the right answer for a lookup that produced a
    /// value, and for one whose operand was **absent**: absence is not a
    /// failure and must never be rendered as one.
    None,
    /// This lookup missed: the value fell in no band, the key names no row of
    /// the table, or the value lies outside the closed breakpoint range.
    /// Always this node's own answer, never a relayed one.
    Missed,
    /// This lookup's **own interpolation** failed -- an intermediate outside
    /// `Rational`'s representable range. Reachable only for
    /// `StepKind::InterpolatingLookup`: the other two kinds select a number
    /// their author wrote down and compute nothing.
    Computation,
    /// This lookup's **own unit conversion** failed -- converting the
    /// operand's value into the table's key unit, or the selected row's value
    /// out of the table's result unit. Its own failure, and specifically not
    /// a miss: the table was asked nothing, or answered fine and the answer
    /// then would not fit. Reachable for all three kinds, since all three
    /// convert their answer out of `ResultUnit`.
    Conversion,
    /// An operand failed and this step is relaying its error unchanged.
    /// Nothing about the table went wrong. Unreachable for
    /// `StepKind::ExactLookup`, which has no operand at all.
    Propagated,
    /// This step failed, and which of the four above it was **cannot be
    /// told**: the operand contributed no step to inspect, because it is a
    /// consumer's own node kind evaluated through the two-parameter
    /// `checked_evaluate_si` extension point (`sink.hpp`) and so is untraced.
    ///
    /// Recorded rather than guessed. Choosing `Propagated` here would be a
    /// plausible answer to a question the recorder cannot actually answer,
    /// which is the whole defect this enum exists to close.
    Undetermined,
};

/// An interval a lookup step reports about, as its table declared it --
/// numerator over denominator at each end.
///
/// Pairs of `std::int64_t` rather than `Rational`, for `Band`'s own reason
/// (`band.hpp`): a table's bounds are declared as pairs and this repeats them
/// back rather than reducing them behind the author's back -- the rendering
/// layer reduces, in the one place that already does it for every other
/// declared bound.
///
/// **Deliberately not a `Band`.** A `Band` is half-open by definition, and
/// this type must also carry an interpolating curve's extent, which is closed
/// at **both** ends -- the one difference between the two table kinds that
/// `lookup.hpp` and `render.hpp` both go out of their way not to blur. Which
/// one a given value is, is `Step::kind`'s to say, and `trace_render.hpp`
/// spells the two differently on purpose: `2 to under 19 mm` against
/// `2 to 19 mm`.
struct LookupRange
{
    /// Numerator of the low end.
    std::int64_t lowNumerator = 0;
    /// Denominator of the low end.
    std::int64_t lowDenominator = 1;
    /// Numerator of the high end.
    std::int64_t highNumerator = 0;
    /// Denominator of the high end.
    std::int64_t highDenominator = 1;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(LookupRange const&) const noexcept = default;
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

    /// For `Conditional` and `Constraint`: which way the predicate compared
    /// its two sides. Both kinds wrap a `PredicateNode` and neither is one
    /// themselves, so this is where each puts the comparison its own
    /// predicate made.
    ///
    /// Recorded because a derivation that says two values were compared but
    /// not *how* is not an audit trail: the trace is the artefact that
    /// survives on its own, away from the formula text, and `#1` and `#2`
    /// with no operator between them leaves a reader unable to check the
    /// step against the method it came from.
    ///
    /// Unlike `branch` above, this field's zero value (`Comparison::Less`) is
    /// **not** a neutral "not applicable": it is a real comparison. It is
    /// meaningful only when `kind` is `Conditional` or `Constraint`, exactly
    /// as `exponent` and `granularity` above are meaningful only for the
    /// kinds that set them, and no renderer may read it without checking
    /// `kind` first.
    ///
    /// It is recorded even when the predicate never resolved, because what a
    /// step could not decide is still a comparison a reader needs named --
    /// with one exception in the rendering, not here: a predicate whose left
    /// side errored never dispatched its right one and so never compared
    /// anything at all. See `detail::conditional_expression` and
    /// `detail::constraint_expression` (`trace_render.hpp`), which share the
    /// exact same exception for the same reason.
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

    /// A second unit this step needs to name, which is **not** the unit its
    /// own value is in. Kept separate from `unit` above for the reason
    /// documented there: this one deliberately does not share `dimension`, so
    /// it is never used to convert `value` -- it is read only by the
    /// renderer. A default-constructed `Unit` (dimension `Scalar`, empty
    /// symbol) for every step that has no second unit to name.
    ///
    ///  - For `NumericValue`: the unit the escape hatch read its number in --
    ///    `Megapascal` for `numeric_value_of<Megapascal, "...">(...)`.
    ///  - For `BandedLookup` and `InterpolatingLookup`: the table's **key
    ///    unit** -- the unit its bands or breakpoints are declared in, and
    ///    the unit the operand's value is compared against them in. It is
    ///    independent of the node's own `unit` on purpose (a banded lookup
    ///    may select a pressure correction from a measured length), so a
    ///    derivation that named only `unit` would state a band's bounds in a
    ///    scale nobody declared them in.
    ///
    /// The lookup kinds reuse this field rather than adding one of their own,
    /// because "a second unit that is not the step's own" is exactly what it
    /// already means. `ExactLookup` has none: a category key is a
    /// discriminator, not a quantity, so there is no key unit to name.
    Unit sourceUnit {};

    /// What the step produced, in the coherent SI unit of `dimension`. Empty
    /// when the value was **absent** -- which is not an error and must not be
    /// rendered as one.
    std::optional<Rep> value {};

    /// Set when this step failed. A step has a `value` or an `error` or
    /// neither (absent); never both.
    std::optional<ArithmeticError> error {};

    /// For `Constraint`: the verdict reached checking it.
    ///
    /// The whole `ConstraintOutcome` rather than a kind plus a separate label
    /// and a separate error field of its own: `ConstraintOutcome` already
    /// carries exactly those three things behind one safe interface, and
    /// splitting it back out here would be the identical duplication phase 8
    /// undid when it removed the member it had added to `WhenNode` to expose
    /// a comparison already reachable another way. `check()` (`constraint.hpp`)
    /// hands this to `RecordingSink::constraint_produced` verbatim.
    ///
    /// Default-constructs to `ConstraintOutcomeKind::NotChecked` -- see
    /// `ConstraintOutcome`'s own comment for why that default, not
    /// `Satisfied`, is the safe one -- which doubles as the correct default
    /// for every step that is *not* a `Constraint`, exactly as `branch` and
    /// `comparison` above default to values that are harmless when `kind`
    /// says they do not apply.
    ConstraintOutcome outcome {};

    /// For the three lookup kinds: whose failure this step is carrying, and
    /// of what kind. See `LookupFailure`, which exists entirely for this
    /// field and gives the ambiguity it resolves in full.
    ///
    /// Zero-initialises to `LookupFailure::None`, which is also the correct
    /// default for every step that is *not* a lookup -- the same property
    /// `branch` has, and unlike `comparison` and `mode`, whose zero values
    /// are real settings rather than "not applicable".
    LookupFailure lookupFailure {};

    /// For `BandedLookup`: the band the value fell in, exactly as the table
    /// declared it -- the one fact a banded lookup's derivation is for. A
    /// step reading only `= 19/20` explains nothing; what a reader checking a
    /// number needs is that 19/20 came from the band containing the input.
    ///
    /// Empty when no band was selected -- a miss, a failure of any kind, an
    /// absent operand -- and also when the operand contributed no step for
    /// the recorder to read its value from (an untraced consumer node), in
    /// which case the band really is unknown and the renderer says nothing
    /// rather than guessing.
    ///
    /// A field of its own rather than a second meaning piled onto
    /// `coveredRange` below, which the two are otherwise mutually exclusive
    /// enough to share: a field that holds "the selected band" on a hit and
    /// "the whole table's extent" on a miss is a field whose name must lie
    /// about one of them, and that is how a later reader gets it wrong. The
    /// reasoning is `granularity`'s, above, unchanged.
    std::optional<Band> selectedBand {};

    /// For `InterpolatingLookup` when the curve answered: the two rows the
    /// answer came from, as the table declared them -- `low == high` when the
    /// value sat exactly **on** a row, and the surrounding pair when it sat
    /// between two.
    ///
    /// The interpolating counterpart of `selectedBand` above, and set under
    /// the same rule: only when the step produced a value. An interpolating
    /// table selects no single row -- between two rows its answer appears in
    /// neither of them -- so the honest equivalent of "which band" is "which
    /// two rows", which is what an auditor reconciles against the published
    /// curve.
    ///
    /// Empty for every other kind, for a miss, for any failure, for an absent
    /// operand, and when the operand contributed no step for the recorder to
    /// read its value from -- the same silences `selectedBand` keeps, for the
    /// same reason.
    ///
    /// The segment comes back from `detail::locate_and_interpolate`
    /// (`lookup.hpp`), the single scan that also produced the value, rather
    /// than from a second scan here: two scans of one table against one rule
    /// are two surfaces obliged to agree.
    std::optional<Segment> selectedSegment {};

    /// For `BandedLookup` and `InterpolatingLookup` when this lookup
    /// **missed**: the interval the table covers as a whole, stated in
    /// `sourceUnit`. What a reader needs in order to see *why* nothing
    /// matched, and the only part of the table a derivation records -- the
    /// rows themselves are `render()`'s to print from the formula, and
    /// copying them into every step would be a second surface obliged to
    /// agree with that one.
    ///
    /// **Half-open for a banded table and closed for an interpolating one.**
    /// A band table's bands are contiguous and ascending -- every
    /// `BandedLookupNode` instantiates `RequireValidBandTable`, which refuses
    /// a gap, an overlap or an inversion -- so their union is exactly one
    /// half-open interval, `[first low, last high)`. An interpolating curve's
    /// extent is `[first key, last key]`, closed at both ends, because a
    /// breakpoint is a row the table states a value at rather than a boundary
    /// between rows. `kind` says which, and `trace_render.hpp` spells them
    /// differently on purpose.
    ///
    /// Empty for a table with no rows at all, which covers nothing and always
    /// misses.
    std::optional<LookupRange> coveredRange {};

    /// For `ExactLookup`: the key this lookup selected with, as the
    /// underlying value of the author's enumerator.
    ///
    /// Recorded here because there is nowhere else it could survive. An
    /// exact lookup has **no operand**, so unlike a banded or an
    /// interpolating miss -- whose missed value is the operand's own result,
    /// sitting in the operand's own step -- a key that names no row would
    /// otherwise appear in no step of the derivation at all.
    ///
    /// The underlying **value**, not the enumerator's name: a C++ enumerator
    /// has no name at run time, so the value is the only part of it that
    /// survives to a trace. The cost is real and is stated in full by
    /// `detail::key_text` (`render.hpp`), which shows the same number for the
    /// same reason.
    ///
    /// Stored as the bit pattern with `lookupKeyIsSigned` beside it rather
    /// than as one signed integer, because an enumeration's underlying type
    /// may be `unsigned long long`, whose top half no signed type can hold --
    /// the same case `key_text` spells its two casts separately for.
    std::uint64_t lookupKey {};

    /// Whether `lookupKey` above is to be read as a signed value. Meaningful
    /// only when `kind` is `ExactLookup`, exactly as `lookupKey` itself is.
    bool lookupKeyIsSigned {};

    /// Indices of the steps this one consumed, in evaluation order.
    ///
    /// **Not necessarily as many as the node kind suggests.** When an operand
    /// fails, the evaluator returns without evaluating the remaining ones, so
    /// a `Divide` may hold one operand rather than two. What is recorded is
    /// what actually ran. A `Constraint` step is the same shape as
    /// `Conditional`'s own predicate: two operands whenever both sides of it
    /// were dispatched -- whether or not the predicate went on to resolve --
    /// and one when the left side raised an arithmetic error before the
    /// right was ever dispatched. Never three: unlike `Conditional`, nothing
    /// is dispatched after the predicate, because a constraint has no
    /// branch.
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

    template <Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
    struct StepKindOf<BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand>>
    {
        static constexpr StepKind value = StepKind::BandedLookup;
    };

    template <KeyTable Keys, Unit ResultUnit>
    struct StepKindOf<ExactLookupNode<Keys, ResultUnit>>
    {
        static constexpr StepKind value = StepKind::ExactLookup;
    };

    template <Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
    struct StepKindOf<InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand>>
    {
        static constexpr StepKind value = StepKind::InterpolatingLookup;
    };

    /// Whether @p kind is one of the three lookup kinds. Written once because
    /// two surfaces ask it -- `RecordingSink::produced`, which dispatches to
    /// `record_lookup` below, and `trace_render.hpp`'s `step_line`, which
    /// appends the clause that keeps a lookup line from lying -- and spelling
    /// the three-way `||` in each is how one of them ends up missing a kind
    /// once a fourth table kind is added.
    [[nodiscard]] constexpr bool is_lookup(StepKind kind) noexcept
    {
        return kind == StepKind::BandedLookup || kind == StepKind::ExactLookup
               || kind == StepKind::InterpolatingLookup;
    }

    /// Whether any step @p step claimed as an operand failed.
    ///
    /// This is the whole of "something below me failed": a lookup dispatches
    /// exactly one operand and returns its error untouched the moment it has
    /// one, so an operand step carrying an error and a lookup step carrying
    /// an error are the same error, and there is no third possibility in
    /// which the operand failed and the lookup did not.
    ///
    /// **It cannot false-positive, and that is provable rather than merely
    /// plausible.** The worry would be a claimed operand step that failed
    /// while the parent went on to succeed -- an evaluated-then-abandoned
    /// child. No node in this library produces one: the only kind that
    /// chooses between children is `WhenNode`, and `conditional.hpp`
    /// dispatches **only** the branch it took, so an abandoned branch is
    /// never evaluated and contributes no step to abandon. A failed claimed
    /// operand therefore always is the error this step is carrying.
    template <typename Rep>
    [[nodiscard]] bool an_operand_failed(std::vector<Step<Rep>> const& steps, Step<Rep> const& step)
    {
        for (std::size_t const operand: step.operands)
            if (steps[operand].error.has_value())
                return true;
        return false;
    }

    /// The value a lookup step's single operand recorded, or nothing when it
    /// recorded no value (the operand was absent) or when there is no operand
    /// step at all (the operand is an untraced consumer node, `sink.hpp`).
    ///
    /// The operand's recorded value is the **same** `Rational` the node was
    /// handed: `produced` stores `**result` verbatim, and both are in the
    /// coherent SI unit of the operand's dimension. So locating it against
    /// the table here reaches the same row the evaluation reached.
    template <typename Rep>
    [[nodiscard]] std::optional<Rep> sole_operand_value(std::vector<Step<Rep>> const& steps, Step<Rep> const& step)
    {
        if (step.operands.empty())
            return std::nullopt;
        return steps[step.operands.front()].value;
    }

    /// The half-open interval a whole band table covers, or nothing when it
    /// declares no bands.
    ///
    /// One interval and not a list, because `RequireValidBandTable` -- which
    /// every `BandedLookupNode` instantiates in its own body -- has already
    /// refused a gap, an overlap and an inverted band, so the bands are
    /// contiguous and ascending and their union is exactly
    /// `[first low, last high)`. This is a consequence of that validation
    /// rather than an assumption about how tables are usually written.
    template <BandTable Bands>
    [[nodiscard]] constexpr std::optional<LookupRange> bands_cover() noexcept
    {
        if constexpr (Bands.size() == 0)
            return std::nullopt;
        else
            return LookupRange { Bands.front().lowNumerator,
                                 Bands.front().lowDenominator,
                                 Bands.back().highNumerator,
                                 Bands.back().highDenominator };
    }

    /// The **closed** range an interpolating curve runs over, or nothing when
    /// it declares no rows. Closed at both ends, unlike `bands_cover` above,
    /// because a breakpoint is a row the table states a value at and not a
    /// boundary between rows -- `lookup.hpp` pins the two behaviours against
    /// each other rather than harmonising them.
    ///
    /// Strictly ascending by `RequireValidBreakpointTable`, so the first and
    /// last rows really are the extremes.
    template <BreakpointTable Points>
    [[nodiscard]] constexpr std::optional<LookupRange> points_cover() noexcept
    {
        if constexpr (Points.size() == 0)
            return std::nullopt;
        else
            return LookupRange { Points.front().numerator,
                                 Points.front().denominator,
                                 Points.back().numerator,
                                 Points.back().denominator };
    }

    /// Fills in a banded lookup step's `lookupFailure` and, on a hit, the
    /// band it selected.
    ///
    /// **Every decision below is `lookup.hpp`'s own, re-asked.** The band is
    /// found with `find_band` -- the very function that chose it during the
    /// evaluation -- rather than with a comparison written again here, so the
    /// derivation cannot come to disagree with the number it derives. The one
    /// line that is *repeated* rather than reused is the conversion of the
    /// operand's value into the key unit, which `checked_evaluate_si` does
    /// immediately before its own `find_band` call; a test whose key unit is
    /// not the coherent SI unit of its operand's dimension is what keeps the
    /// two honest.
    template <typename Rep, Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
    void record_lookup(BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand> const&,
                       Step<Rep>& step,
                       std::vector<Step<Rep>> const& steps)
    {
        if constexpr (std::is_same_v<Rep, Rational>)
        {
            constexpr Unit keyUnit = KeyUnit;

            if (an_operand_failed(steps, step))
            {
                step.lookupFailure = LookupFailure::Propagated;
                return;
            }

            std::optional<Rational> const value = sole_operand_value(steps, step);
            if (!value.has_value())
            {
                // No value to locate: either the operand was absent -- in
                // which case nothing was looked up and nothing failed -- or
                // it left no step, and then a failure here cannot be told
                // apart from one below it.
                if (step.error.has_value())
                    step.lookupFailure = LookupFailure::Undetermined;
                return;
            }

            std::expected<Rational, ArithmeticError> const valueInKey =
                checked_convert(*value, coherent(keyUnit.dimension), keyUnit);
            if (!valueInKey.has_value())
            {
                step.lookupFailure = LookupFailure::Conversion;
                return;
            }

            std::optional<std::size_t> const index = find_band<Bands>(*valueInKey);
            if (!index.has_value())
            {
                step.lookupFailure = LookupFailure::Missed;
                step.coveredRange = bands_cover<Bands>();
                return;
            }

            // A band was found, so anything still wrong happened after the
            // selection: converting the selected correction out of the
            // table's result unit is all that is left.
            if (step.error.has_value())
                step.lookupFailure = LookupFailure::Conversion;
            else
                step.selectedBand = Bands[*index];
        }
    }

    /// Fills in an exact lookup step's key and `lookupFailure`.
    ///
    /// The key is recorded unconditionally -- hit or miss -- because no other
    /// step in the derivation carries it: an exact lookup has no operand, so
    /// there is no step below to lean on the way the other two kinds lean on
    /// theirs.
    ///
    /// Neither `Propagated` nor `Undetermined` is reachable here, and that is
    /// a property of the node rather than an omission: with no operand there
    /// is nothing below this step that could have failed, so every failure it
    /// reports is its own.
    template <typename Rep, KeyTable Keys, Unit ResultUnit>
    void record_lookup(ExactLookupNode<Keys, ResultUnit> const& node,
                       Step<Rep>& step,
                       std::vector<Step<Rep>> const&)
    {
        using Underlying = std::underlying_type_t<KeyOf<Keys>>;
        step.lookupKeyIsSigned = std::is_signed_v<Underlying>;
        step.lookupKey = step.lookupKeyIsSigned ? static_cast<std::uint64_t>(static_cast<long long>(node.key))
                                                : static_cast<std::uint64_t>(static_cast<unsigned long long>(node.key));

        if constexpr (std::is_same_v<Rep, Rational>)
        {
            if (!find_key<Keys>(node.key).has_value())
                step.lookupFailure = LookupFailure::Missed;
            else if (step.error.has_value())
                step.lookupFailure = LookupFailure::Conversion;
        }
    }

    /// Fills in an interpolating lookup step's `lookupFailure`.
    ///
    /// The one kind that can fail **three** ways of its own, so the one that
    /// needs `locate_and_interpolate` re-asked rather than a rule of thumb: it
    /// is the function that decides whether a value is on the curve at all,
    /// and its `DomainError` is documented there as unambiguously a miss,
    /// every other error it returns coming from the arithmetic. Re-asking it
    /// is what separates "the interpolation overflowed" from "the table does
    /// not reach this specimen" without this file re-deciding either -- and
    /// the same call hands back the two rows the answer came from, so the
    /// derivation names them without a second scan.
    ///
    /// No band is recorded: an interpolating table selects no single row.
    /// Between two rows its answer appears in neither of them, and on a row
    /// the answer is that row's own value -- which the step's value already
    /// is. `selectedSegment` is the honest equivalent, and it names both rows.
    template <typename Rep, Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
    void record_lookup(InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand> const& node,
                       Step<Rep>& step,
                       std::vector<Step<Rep>> const& steps)
    {
        if constexpr (std::is_same_v<Rep, Rational>)
        {
            constexpr Unit keyUnit = KeyUnit;

            if (an_operand_failed(steps, step))
            {
                step.lookupFailure = LookupFailure::Propagated;
                return;
            }

            std::optional<Rational> const value = sole_operand_value(steps, step);
            if (!value.has_value())
            {
                if (step.error.has_value())
                    step.lookupFailure = LookupFailure::Undetermined;
                return;
            }

            std::expected<Rational, ArithmeticError> const valueInKey =
                checked_convert(*value, coherent(keyUnit.dimension), keyUnit);
            if (!valueInKey.has_value())
            {
                step.lookupFailure = LookupFailure::Conversion;
                return;
            }

            std::expected<std::pair<Rational, Segment>, ArithmeticError> const answered =
                locate_and_interpolate<Points>(*valueInKey, node.corrections);
            if (!answered.has_value())
            {
                if (answered.error() == ArithmeticError::DomainError)
                {
                    step.lookupFailure = LookupFailure::Missed;
                    step.coveredRange = points_cover<Points>();
                }
                else
                    step.lookupFailure = LookupFailure::Computation;
                return;
            }

            // The curve answered, so anything still wrong happened after it:
            // converting that answer out of the table's result unit.
            if (step.error.has_value())
                step.lookupFailure = LookupFailure::Conversion;
            else
                step.selectedSegment = answered->second;
        }
    }
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
        // The banded and the interpolating lookup declare a key unit that is
        // independent of their own: a band's bounds are stated in it, and the
        // operand's value is compared against them in it. `sourceUnit` is
        // where a second unit that is not the step's own already goes -- see
        // its comment -- so it goes there rather than into a parallel field.
        // The exact lookup has none: its key is a discriminator, not a
        // quantity.
        if constexpr (requires { N::keyUnit; })
            step.sourceUnit = N::keyUnit;
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
            // The predicate's comparison, taken off the node this sink was
            // handed. `PredicateNode::comparison` is a public
            // `static constexpr`, so naming it through the member is a
            // constant expression and `WhenNode` needs no re-export of its
            // own -- the same way `citation` above is read straight off a
            // `DocumentedNode`.
            step.comparison = std::remove_cvref_t<decltype(node.predicate)>::comparison;
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

        // After the operands are claimed, and not before: telling this
        // lookup's own failure apart from one it is merely relaying means
        // reading the operand step it just claimed, so the claim has to have
        // happened. See `LookupFailure` for the ambiguity this closes, and
        // `detail::record_lookup` for how each kind closes it.
        if constexpr (detail::is_lookup(detail::StepKindOf<N>::value))
            detail::record_lookup(node, step, _trace->steps);

        _trace->steps.push_back(std::move(step));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
    }

    /// Told that a `Constraint` is about to be checked. Remembers where the
    /// arena stood, exactly as `entered` above does for a `Node`, so
    /// `constraint_produced` below can tell which steps are the predicate's
    /// own operands.
    ///
    /// A constraint is not a `Node` -- it produces a verdict, not a value --
    /// so it cannot go through `entered` itself, which is constrained on
    /// `Node`. Called instead from `Constraint::check` (`constraint.hpp`)
    /// through `if constexpr (requires {...})`, the same optional-hook shape
    /// `branch_taken` above uses for the same reason: a sink with no use for
    /// it -- `NullSink` included -- simply does not define it and pays
    /// nothing.
    template <Predicate P>
    void constraint_entered(Constraint<P> const&)
    {
        _trace->marks.push_back(_trace->steps.size());
    }

    /// Told what checking a `Constraint` produced. Claims as its operands
    /// every step recorded at or after the matching `constraint_entered`
    /// that nothing else has claimed -- the predicate's own left and right
    /// sides -- exactly as `produced` above claims a node's.
    template <Predicate P>
    void constraint_produced(Constraint<P> const& constraint, ConstraintOutcome const& outcome)
    {
        std::size_t const mark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> step {};
        step.kind = StepKind::Constraint;
        // Read straight off the constraint's own predicate, the same way
        // `produced` above reads a `Conditional` step's comparison off
        // `node.predicate` -- `PredicateNode::comparison` is a public
        // `static constexpr`, so this needs no member added to `Constraint`
        // to expose it.
        step.comparison = std::remove_cvref_t<decltype(constraint.predicate)>::comparison;
        step.outcome = outcome;

        // Everything unclaimed from `mark` onwards belongs to this
        // constraint -- see `produced` above for why this is a `while`
        // rather than an index computed from `mark` directly.
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
