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

#include <formula-cpp/binning.hpp>
#include <formula-cpp/citation.hpp>
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/conformity.hpp>
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/critical_value.hpp>
#include <formula-cpp/curve.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/record.hpp>
#include <formula-cpp/rejection.hpp>
#include <formula-cpp/retry.hpp>
#include <formula-cpp/rounded_root.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/snap.hpp>
#include <formula-cpp/statistics.hpp>
#include <formula-cpp/vocabulary.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
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
    /// A method selecting one of its variants. Carries the tag's name and
    /// the discriminator it matched, because "a variant was selected" is
    /// true of every outcome and therefore answers nothing.
    ///
    /// The discriminator is the tag itself: `evaluate_method<Cylinder>`
    /// selects the variant declared `variant<Cylinder>(...)` and no other, so
    /// the step records the tag's name (`Step::variantTag`) together with the
    /// variant's position among its siblings (`Step::variantIndex`,
    /// `Step::variantCount`). Its one operand is the selected variant,
    /// rounded by the method's rule.
    ///
    /// Recorded by `RecordingSink::variant_produced`, not through
    /// `detail::StepKindOf`: a method is not a `Node`, so it has no entry in
    /// that registry and needs none. Checked on GCC under `-Wshadow`, the way
    /// `PiConstant` above had to be: nothing in namespace `formula` is spelt
    /// `VariantSelected` -- the plain data the step is built from is
    /// `VariantSelection` (`sink.hpp`) -- and that was compiled, not assumed.
    VariantSelected,
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
    /// A method's rounding rule, applied to the variant it selected
    /// (`RoundingRuleNode`, `method.hpp`). Rounds exactly as `Round` does,
    /// and carries what `Round` cannot: where the rule came from --
    /// `Step::roundingProvenance`, and for an overlay's rule what it cited,
    /// in `Step::citation`. A step naming only the granularity is true
    /// whether the method or a jurisdiction chose it, and so answers only
    /// half of what spec section 9.1 asks.
    ///
    /// Checked on GCC under `-Wshadow`, the way `PiConstant` above had to
    /// be: nothing in namespace `formula` is spelt `RoundingRuleApplied` --
    /// the rule is `RoundingRule` and its node `RoundingRuleNode`.
    RoundingRuleApplied,
    /// A quantity an overlay fixed (`OverriddenConstantNode`, `overlay.hpp`):
    /// the quantity's symbol and unit, the overlay's value, and what the
    /// overlay cited, in `Step::citation`. Not a `Variable` step, which
    /// reads as a number the specimen supplied.
    ///
    /// Checked on GCC under `-Wshadow`: the node type carries the `Node`
    /// suffix, so nothing in namespace `formula` is spelt
    /// `OverriddenConstant`.
    OverriddenConstant,
    /// A quantity a jurisdiction defined by an expression
    /// (`DerivedQuantityNode`, `overlay.hpp`): the quantity's symbol and unit,
    /// the value its definition produced, the definition's derivation as its
    /// operand, and what the overlay cited, in `Step::citation`.
    ///
    /// Checked on GCC under `-Wshadow`: the node type carries the `Node`
    /// suffix and the operation is `QuantityDerivation`, so nothing in
    /// namespace `formula` is spelt `DerivedQuantity`.
    DerivedQuantity,
    /// A variant's formula a jurisdiction replaced wholesale
    /// (`ReplacedVariantNode`, `overlay.hpp`): the replacement's derivation as
    /// its operand, and what the overlay cited, in `Step::citation`.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `ReplacedVariantNode` and
    /// the operation `VariantReplacement`, so nothing in namespace `formula`
    /// is spelt `ReplacedVariant`.
    ReplacedVariant,
    /// A method's constraints, checked by `check_method` (`method.hpp`): whose
    /// they are, in `Step::constraintProvenance` and, for an overlay's, what
    /// it cited, in `Step::citation`; its operands are the constraints'
    /// verdicts, in the order `check_method` returns them. Recorded even for
    /// a method with no constraints, which then has no operands -- so a trace
    /// of a method whose overlay removed every check still says so.
    ///
    /// Recorded by `RecordingSink::acceptance_produced`, not through
    /// `detail::StepKindOf`: a method is not a `Node`. Checked on GCC under
    /// `-Wshadow`: nothing in namespace `formula` is spelt
    /// `AcceptanceChecked`.
    AcceptanceChecked,
    /// A series variable (`SeriesVarNode`, `series.hpp`): the quantity's
    /// symbol and declared unit, and every element, in `Step::elements` --
    /// never in `Step::value`, which a series step leaves empty. A failure
    /// records its element in `Step::failedElement`.
    ///
    /// Recorded by `RecordingSink::series_produced`, not through
    /// `detail::StepKindOf`: a series is not a `Node`, and its registry is
    /// `detail::SeriesStepKindOf`. Checked on GCC under `-Wshadow`: the node
    /// is `SeriesVarNode` and its spelling in a formula `series`, so nothing
    /// in namespace `formula` is spelt `SeriesVariable`.
    SeriesVariable,
    /// A per-element constant (`SeriesConstantNode`): its values, in the unit
    /// it was written in, in `Step::elements`. Checked on GCC under
    /// `-Wshadow`: the node carries the `Node` suffix and the factory is
    /// `series_constant`, so nothing in namespace `formula` is spelt
    /// `SeriesConstant`.
    SeriesConstant,
    /// Elementwise negation (`ElementwiseUnaryNode`). This and the four
    /// below record one step for the whole series, with every element in
    /// `Step::elements` and the operands -- a broadcast scalar's step once --
    /// in `Step::operands`. Checked on GCC under `-Wshadow`: the nodes are
    /// `ElementwiseUnaryNode` and `ElementwiseBinaryNode`, so nothing in
    /// namespace `formula` is spelt like these five.
    ElementwiseNegate,
    /// Elementwise addition (`ElementwiseBinaryNode`).
    ElementwiseAdd,
    /// Elementwise subtraction.
    ElementwiseSubtract,
    /// Elementwise multiplication.
    ElementwiseMultiply,
    /// Elementwise division.
    ElementwiseDivide,
    /// A running total along a series (`CumulativeNode`): one step for the
    /// whole series, every total in `Step::elements`, the end it ran from in
    /// `Step::cumulativeDirection`, shown in its operand's unit. Checked on
    /// GCC under `-Wshadow`: the node is `CumulativeNode` and its factory
    /// `cumulative`, so nothing in namespace `formula` is spelt
    /// `CumulativeSum`.
    CumulativeSum,
    /// The total of a series (`SumNode`): a single-value step, its value in
    /// `Step::value`, whose operand is the series step; shown in its
    /// operand's unit. Recorded through `detail::StepKindOf`, since a sum is a
    /// `Node`. Checked on GCC under `-Wshadow`: the node is `SumNode` and its
    /// factory `sum`, so nothing in namespace `formula` is spelt `SeriesSum`.
    SeriesSum,
    /// A series rounded element by element (`ElementwiseRoundNode`): one step
    /// for the whole series, every rounded element in `Step::elements`, each
    /// element's granularity in `Step::elementGranularities`, the unit
    /// rounded in in `Step::unit` and the mode in `Step::mode`. Checked on
    /// GCC under `-Wshadow`: the node is `ElementwiseRoundNode` and its
    /// factory `rounded_elementwise`, so nothing in namespace `formula` is
    /// spelt `ElementwiseRound`.
    ElementwiseRound,
    /// A conformity check (`Conformity`, `conformity.hpp`): one step for the
    /// whole check, one outcome per element in `Step::elementOutcomes`, and
    /// the subject's step as its operand. Recorded by
    /// `RecordingSink::conformity_produced`, not through `detail::StepKindOf`:
    /// a conformity check is not a `Node`. Checked on GCC under `-Wshadow`:
    /// the type is `Conformity` and its factory `conformity`, so nothing in
    /// namespace `formula` is spelt `ConformityChecked`.
    ConformityChecked,
    /// A value snapped to the nearest permitted one (`SnapNode`, `snap.hpp`):
    /// the two permitted neighbours in `Step::selectedSegment`, the tie rule
    /// in `Step::snapTie`, whether it decided in `Step::tieBroken`, and on a
    /// miss the set's extent in `Step::coveredRange`. Checked on GCC under
    /// `-Wshadow`: the node is `SnapNode` and its factory `snapped`, so
    /// nothing in namespace `formula` is spelt `SnappedToPermitted`.
    SnappedToPermitted,
    /// A declared domain (`DomainNode`, `curve.hpp`): its points, in the unit
    /// they were declared in -- a series step, recorded as a per-element
    /// constant is. Checked on GCC under `-Wshadow`: the node is `DomainNode`
    /// and its factory `domain`, so nothing in namespace `formula` is spelt
    /// `SeriesDomain`.
    SeriesDomain,
    /// A domain paired with its values (`CurveNode`): the points in
    /// `Step::domainElements`, shown in `Step::sourceUnit`, and the values in
    /// `Step::elements`, shown in `Step::unit`; the two series' steps are its
    /// operands. Recorded by `RecordingSink::curve_produced`: a curve is
    /// neither a `Node` nor a series. Checked on GCC under `-Wshadow`: the
    /// node is `CurveNode` and its factory `curve`.
    CurvePairing,
    /// A curve read at a point (`InterpolateAlongNode`): a single-value step
    /// whose operands are the curve's step and the point's; the two points it
    /// lay between in `Step::selectedSegment`, or on a miss the curve's extent
    /// in `Step::coveredRange`, both in `Step::sourceUnit`. Checked on GCC
    /// under `-Wshadow`: the node is `InterpolateAlongNode` and its factory
    /// `interpolate_at`.
    CurveInterpolation,
    /// Two curves spliced into one (`SpliceNode`): recorded as a
    /// `CurvePairing` is, with the direction in `Step::monotone` and on a
    /// failure the element in `Step::failedElement` and the rule it broke in
    /// `Step::curveBreak`. Checked on GCC under
    /// `-Wshadow`: the node is `SpliceNode` and its factory `splice`.
    CurveSplice,
    /// Raw observations (`ObservationsVarNode`, `binning.hpp`): the
    /// quantity's symbol and declared unit, and every observation made, in
    /// `Step::elements`, as many as were made. A failure records the
    /// observation it arose at in `Step::failedElement`. Recorded by
    /// `RecordingSink::observations_produced`. Checked on GCC under
    /// `-Wshadow`: the node is `ObservationsVarNode` and its spelling in a
    /// formula `observations`, so nothing in namespace `formula` is spelt
    /// `ObservationsVariable`.
    ObservationsVariable,
    /// Raw observations counted into classes (`BinnedNode`): one count per
    /// class in `Step::elements`; the classes' unit in `Step::sourceUnit`,
    /// their extent in `Step::coveredRange`, and the observations binned, in
    /// the coherent SI unit, in `Step::domainElements`. A failure's
    /// `Step::failedElement` is the **observation** it arose at, not a
    /// count. Checked on GCC under `-Wshadow`: the node is `BinnedNode` and
    /// its factory `binned`.
    Binning,

    /// A `RoundedRootNode`: the square root of its one operand, rounded to a
    /// number of decimal places of the node's unit -- `Step::granularity`,
    /// `Step::unit` and `Step::mode`, as for `Round`. One step and not a
    /// `Root` beneath a `Round`: the root is irrational for almost every
    /// radicand, so a `Root` step would have to show a number the evaluator
    /// never had. The operand's value is exact, and so is this step's.
    ///
    /// Checked on GCC under `-Wshadow`, the way `PiConstant` above had to be:
    /// the node is `RoundedRootNode` and the factory `rounded_sqrt`, so
    /// nothing in namespace `formula` is spelt `RoundedRoot`.
    RoundedRoot,
    /// A `SampleSizeLookupNode`: a critical value read from an author's table
    /// by sample size (`critical_value.hpp`). A lookup like the three above:
    /// `Step::lookupFailure` says whose failure a failed step carries,
    /// `Step::lookupKey` holds the count it selected with, and
    /// `Trace::sampleSizeRecords` the sizes the table declares, keyed by the
    /// step's index, so that a miss can say which counts would have hit.
    ///
    /// Checked on GCC under `-Wshadow`, the way `PiConstant` above had to be:
    /// the node is `SampleSizeLookupNode` and the factory `critical_value`, so
    /// nothing in namespace `formula` is spelt `SampleSizeLookup`.
    SampleSizeLookup,
    /// An `AbsoluteValueNode`: the magnitude of its one operand.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `AbsoluteValueNode` and the
    /// factory `abs`, so nothing in namespace `formula` is spelt
    /// `AbsoluteValue`.
    AbsoluteValue,
    /// A precision limit's level, in one of two roles its side-table record
    /// (`Trace::precisionRecords`) names: **pass 1** of a `precision_limit`,
    /// the level expression's value, whose operand is that expression; or a
    /// `precision_level` placeholder read inside the limit expression, which
    /// names the limit that bound it.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `PrecisionLevelNode` and
    /// the variable template `precision_level`, so nothing in namespace
    /// `formula` is spelt `PrecisionLevel`.
    PrecisionLevel,
    /// A `PrecisionLimitNode`: **pass 2**, the limit evaluated at the level
    /// pass 1 produced. Its operands are the pass-1 step and then the limit
    /// expression's steps; its record names its kind and its level step.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `PrecisionLimitNode` and
    /// the factory `precision_limit`, so nothing in namespace `formula` is
    /// spelt `PrecisionLimit`.
    PrecisionLimit,
    /// A `SampleCountNode`: how many determinations a sample holds, a bare
    /// number. Its one operand is the sample's own step, with every element.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `SampleCountNode` and the
    /// factory `sample_count`, so nothing in namespace `formula` is spelt
    /// `SampleCount`.
    SampleCount,
    /// A `SampleMeanNode`: the mean of a sample, shown in its operand's unit,
    /// as a sum is. Its one operand is the sample's own step. A total that
    /// overflowed names, in `Step::failedElement`, the determination at which
    /// it did.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `SampleMeanNode` and the
    /// factory `sample_mean`, so nothing in namespace `formula` is spelt
    /// `SampleMean`.
    SampleMean,
    /// A `SampleVarianceNode`: the sample variance, over n - 1. Its one
    /// operand is the sample's own step. Shown in the coherent unit of its
    /// squared dimension, as every computed step is: no declared unit names
    /// a squared mass.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `SampleVarianceNode` and
    /// the factory `sample_variance`, so nothing in namespace `formula` is
    /// spelt `SampleVariance`.
    SampleVariance,
    /// A `SampleRangeNode`: the largest determination less the smallest,
    /// shown in its operand's unit, as a mean is.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `SampleRangeNode` and the
    /// factory `sample_range`, so nothing in namespace `formula` is spelt
    /// `SampleRange`.
    SampleRange,
    /// A `PassMeanNode`: the current pass's mean, read inside a rejection's
    /// limit expression, shown in the unit of the quantity it names.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `PassMeanNode` and the
    /// variable template `pass_mean`, so nothing in namespace `formula` is
    /// spelt `PassMean`.
    PassMean,
    /// A `PassCountNode`: the current pass's number of determinations.
    ///
    /// Checked on GCC under `-Wshadow`: the node is `PassCountNode` and the
    /// variable `pass_count`, so nothing in namespace `formula` is spelt
    /// `PassCount`.
    PassCount,
    /// One pass of a rejection (`without_outliers`, `rejection.hpp`): its
    /// number, the sample size and the mean it ran at, in the sample's unit.
    /// Its operands are the limit expression's steps; its record is in
    /// `Trace::rejectionRecords`.
    ///
    /// Recorded by `RecordingSink::rejection_pass_produced`, not through
    /// `detail::StepKindOf`: a rejection is not a `Node`. The eight rejection
    /// kinds are spelt as no name in namespace `formula` is (checked on GCC
    /// under `-Wshadow`).
    RejectionPass,
    /// One rejected determination: its position, its value, and the
    /// statistic and limit the decision used, exactly.
    OutlierRejected,
    /// A rejection that ended with a pass rejecting nothing: how many were
    /// rejected and how many remain. Its operands are every step of the
    /// rejection, the sample's own first.
    RejectionSettled,
    /// A rejection that ended at its bound: the determinations that would
    /// have been rejected, the bound, and the author's verdict and citation.
    RejectionAborted,
    /// A rejection whose pass failed: what failed -- the mean, the variance,
    /// the limit (or a negative limit), limit^2 * s^2, or a determination's
    /// deviation -- and the determination, when there is one. Its error is
    /// the failure's. Its operands are every step of the rejection.
    RejectionFailed,
    /// A rejection whose limit was absent in a pass: no decision, and an
    /// absent result. Its operands are every step of the rejection.
    RejectionUndecided,
    /// A read from another record (`RecordScopeNode`, `record.hpp`): its
    /// operand is the derivation over that record's values, and
    /// `Step::recordNumber` says which record (`origin_of`). With no record
    /// bound to the role it has no operand, since nothing was read.
    ///
    /// Nothing in namespace `formula` is spelt `RecordScope` -- the node is
    /// `RecordScopeNode` and the factory `from_record` -- so GCC's
    /// `-Wshadow` has nothing to report; the gcc-release preset builds with
    /// it.
    RecordScope,
    /// One attribute a lineage requirement compared before a scope read
    /// (`lineage.hpp`): which attribute, against which record, and both keys,
    /// in `Trace::lineageChecks` (`lineage_of`); the verdict in
    /// `Step::outcome` -- satisfied, violated, or not checked when a key is
    /// unknown. Recorded as the scope's first operands, in the order the
    /// requirement names them, and without operands of its own.
    ///
    /// Recorded by `RecordingSink::lineage_checked`, not through
    /// `detail::StepKindOf`: an attribute is not a `Node`. Nothing in
    /// namespace `formula` is spelt `LineageChecked` -- the value is a
    /// `LineageCheck` -- so GCC's `-Wshadow`, with which gcc-release builds,
    /// has nothing to report.
    LineageChecked,
    /// An opaque operation's call (`OpaqueCall`, `opaque.hpp`): its inputs'
    /// steps as its operands, the call's citation in `Step::citation`, and on
    /// a relayed failure the input's element in `Step::failedElement`. The
    /// operation's name, each output's name, dimension and value, and whose
    /// failure it carries are in `Trace::opaqueSteps`, not on the `Step`.
    /// Its line always ends by saying that the operation's inside is not
    /// shown -- on this kind alone, which no field can switch off.
    ///
    /// The step's `dimension`, `unit` and `value` mean nothing for a call --
    /// they are placeholders, `dim::Scalar`, `One` and empty -- since a call
    /// has several outputs: those are in `Trace::opaqueSteps`, and a reader
    /// such as an exporter takes them from there.
    ///
    /// Recorded by `RecordingSink::opaque_produced`: a call is not a `Node`.
    /// **Each output an expression uses records the whole call again**, its
    /// inputs included, and `compute` runs once per output used: a call is
    /// evaluated where its output is, as every other subexpression is. Nothing
    /// is wrong in the second copy, and `document()` lists the operation once.
    /// A later memoisation would rely on the side tables' step keys staying
    /// unique, which they do: steps are only ever appended.
    ///
    /// The concept `OpaqueOperation` is at namespace scope and this
    /// enumerator in `StepKind`'s, so the two do not clash and GCC's
    /// `-Wshadow` has nothing to report (checked with g++ 13.3).
    OpaqueOperation,
    /// One output of an opaque call (`OpaqueOutputNode`): a single-value step
    /// whose operand is the call's step; which output, in
    /// `Trace::opaqueOutputSteps`. The node is `OpaqueOutputNode` and its
    /// factory `opaque_output`, so no name here is spelt `OpaqueOutput` twice.
    OpaqueOutput,
    /// One attempt of a retry (`retry.hpp`): its value, in `Step::value`, and
    /// how it was judged. Its operands are the attempt's derivation and, when
    /// it was judged, the acceptance's two sides, whose comparison is in
    /// `Step::comparison`; its number and its judgement are in
    /// `Trace::attemptSteps`. Recorded for an attempt that ran, and for none
    /// that did not.
    RetryAttempt,
    /// How a retry ended: its value when an attempt was accepted, its error
    /// when one failed, and its citation. Its operands are the starting
    /// value's step, when there is one, and then every attempt's; how it
    /// ended, the attempt limit and the verdict it would end in are in
    /// `Trace::retrySteps`.
    RetryConcluded,
    /// The attempt's number, k (`attempt_number`).
    AttemptNumber,
    /// The previous attempt's value (`previous_attempt<R>`), or the starting
    /// value's; at the first attempt of a retry with no starting value, its
    /// own `DomainError`, which reads as none before attempt 1.
    PreviousAttempt,
    /// The value the attempt being judged produced (`this_attempt<R>`).
    ThisAttempt,
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
    /// This lookup's key was no count at all -- not a whole, non-negative
    /// number -- so no row could be asked. Reachable only for
    /// `StepKind::SampleSizeLookup`: 5.5 determinations names no row, and
    /// neither truncating nor rounding it is the table's rule. Its own
    /// failure, and not `Missed`, because the table was never consulted.
    NotACount,
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
/// spells the two differently on purpose: `209/10 to under 293/10 mm` against
/// `209/10 to 293/10 mm`.
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

namespace detail
{
    /// What a critical-value lookup step's table declared, in the trace's
    /// side table, keyed by the step's index.
    struct SampleSizeRecord
    {
        /// The index of the `SampleSizeLookup` step this record belongs to.
        std::size_t step {};

        /// The declared sizes, spelled `3, 4, 5, 6, 8`; empty for a table of
        /// no rows.
        ///
        /// **A view, safe to keep for the life of the trace and beyond**: it
        /// points into a `static constexpr` array built from the table's type
        /// (`detail::SampleSizeList`), static storage as `lookupKeyName`'s
        /// names are, with the same limit for a shared library that is
        /// unloaded.
        std::string_view declaredSizes {};
    };

    /// Which part of a two-pass precision limit a step is.
    enum class PrecisionStepRole : std::uint8_t
    {
        /// Pass 1: the level expression's value.
        LevelPass,
        /// A `precision_level` placeholder read inside the limit expression.
        Placeholder,
        /// Pass 2: the limit, evaluated at the level.
        LimitPass,
    };

    /// What a precision step is and where its level is, in the trace's side
    /// table, keyed by the step's index.
    struct PrecisionRecord
    {
        /// The index of the step this record belongs to.
        std::size_t step {};
        /// Which precision the limit states: `r` or `R`.
        PrecisionKind kind {};
        /// Which part of the limit the step is.
        PrecisionStepRole role {};
        /// The index of the pass-1 step: the step itself for `LevelPass`, and
        /// the level the placeholder or the limit read otherwise.
        std::size_t levelStep {};
        /// The index of the `PrecisionLimit` step the level belongs to -- the
        /// step itself for `LimitPass`, the limit that bound a placeholder, the
        /// limit a pass-1 step fed. Empty until that limit is recorded, and for
        /// good when it never is.
        std::optional<std::size_t> limitStep {};
    };

    /// What a rejection step is, and every number it names, in the trace's
    /// side table, keyed by the step's index -- the same shape as
    /// `PrecisionRecord`, for the same reason: `Step` gains no field (T10,
    /// and the lead's ruling on the task 3 review).
    template <typename Rep>
    struct RejectionRecord
    {
        /// The index of the step this record belongs to.
        std::size_t step {};
        /// The pass, from 1; for a terminal step, the last pass run.
        std::size_t pass {};
        /// The determinations in the pass.
        std::size_t sampleSize {};
        /// The determinations in the sample as entered.
        std::size_t originalSize {};
        /// For a rejected determination, or a failed pass that failed at one:
        /// its zero-based position as entered.
        std::optional<std::size_t> position {};
        /// For a rejected determination: its value, in the coherent SI unit.
        std::optional<Rep> rejectedValue {};
        /// For a rejected determination: its statistic -- abs(x - mean), or
        /// (x - mean)^2 when `squared`.
        std::optional<Rep> statistic {};
        /// For a rejected determination: the limit it was compared with --
        /// limit^2 * s^2 when `squared`.
        std::optional<Rep> limit {};
        /// Whether the statistic and limit are squares (`deviation_in_stddevs`).
        bool squared {};
        /// The criterion.
        CriterionKind criterion {};
        /// What becomes of a determination on the limit.
        OnLimit onLimit {};
        /// k and m.
        std::size_t atMost {};
        std::size_t keepAtLeast {};
        /// For a terminal step: how many were rejected, and how many remain.
        std::size_t rejectedCount {};
        std::size_t remaining {};
        /// For an abort: the positions that would have been rejected, and
        /// which bound -- `AtMost`, `KeepAtLeast`, or both -- would have been
        /// passed.
        std::vector<std::size_t> wouldReject {};
        bool pastAtMost {};
        bool belowKeepAtLeast {};
        /// For an abort before pass 1: the sample started with fewer than
        /// `keepAtLeast`. `pass` is then 0.
        bool startedShort {};
        /// Whether the sample is raw observations (`observations<Q,
        /// Capacity>`): its positions are then observations', as
        /// `FailureSite::InputObservation` counts them.
        bool ofObservations {};
        /// For a failed pass: what failed. The determination it failed at,
        /// when there is one, is `position`; the error is the step's own.
        std::optional<RejectionFailurePoint> failurePoint {};
        /// For an abort: the author's verdict and citation.
        Verdict verdict {};
        Citation citation {};
    };

    /// A rejection in progress: bookkeeping for `RecordingSink`. Where the
    /// arena stood when it began, and the sample's own step once known, whose
    /// unit its passes and rejections are shown in.
    struct RejectionInProgress
    {
        std::size_t mark {};
        std::optional<std::size_t> sampleStep {};
    };

    /// A precision limit whose level is bound, while its limit expression is
    /// being evaluated: bookkeeping for `RecordingSink`.
    struct PrecisionBinding
    {
        /// Which precision the limit states.
        PrecisionKind kind {};
        /// The index of the pass-1 step.
        std::size_t levelStep {};
        /// Where in `Trace::precisionRecords` the pass-1 step's record is, and
        /// the records of every placeholder read under this binding, so that
        /// each can be told which limit bound it once that limit is recorded.
        std::vector<std::size_t> pendingRecords {};
    };
} // namespace detail

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

    /// For `Variable`, `OverriddenConstant` and `DerivedQuantity`: how the
    /// quantity is written, under the vocabulary the `RecordingSink` was given
    /// (`vocabulary.hpp`).
    /// Points into static storage -- the quantity's `Describe` specialisation,
    /// or the string literal `renames` was given -- so it outlives any trace,
    /// the same guarantee `document.hpp`'s `SymbolEntry` relies on.
    ///
    /// Written here, while the formula is **evaluated**, and only read by
    /// `render_trace`: a trace recorded in one vocabulary cannot be rendered
    /// in another afterwards. These three kinds are the only steps that name
    /// a quantity; every other step refers to its operands by position, so a
    /// rounding, lookup, constraint, variant or replacement step reaches the
    /// vocabulary through the steps beneath it.
    std::string_view symbol {};

    /// For `Documented`: what the wrapped formula cites. For
    /// `OverriddenConstant`, `DerivedQuantity`, `ReplacedVariant` and `RoundingRuleApplied`:
    /// what the overlay that fixed the value, defined the quantity, replaced the formula or set
    /// the rule cited, empty when it cited nothing --
    /// and always empty for a method's own rule. For `VariantSelected`: what
    /// the overlay that pinned the method to that variant cited, when one did
    /// (`variantPinned`); a prune's citation is `variantPrunedBy`. For `AcceptanceChecked`, and
    /// for a `Constraint` it holds: what the overlay that replaced the
    /// method's constraints cited, empty for the method's own. A
    /// `Constraint`'s own citation is `render()`'s and `document()`'s to show,
    /// and is not copied here.
    Citation citation {};

    /// For `NumericValue`: why the dimension was dropped -- the compile-time
    /// justification `numeric_value_of` was written with. Points into the
    /// static storage of the node's own template-parameter object, the same
    /// guarantee `symbol` above relies on for `Describe`. Empty otherwise, and
    /// the renderer treats an empty one as absent -- see `step_line`.
    std::string_view justification {};

    /// For `Power`: the exponent. For `Root`: the degree. Zero otherwise.
    int exponent {};

    /// For `Round`, `RoundedRoot` and `RoundingRuleApplied`: the decimal
    /// places kept. For `RoundSignificant`: the significant digits kept. Zero
    /// otherwise.
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

    /// For `Round`, `RoundSignificant`, `RoundedRoot` and
    /// `RoundingRuleApplied`: the tie-breaking rule the node rounded under.
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
    /// sentinel: meaningful only for the four rounding kinds.
    RoundingMode mode {};

    /// For `RoundingRuleApplied`: where the rule came from -- the method's
    /// own, or a jurisdiction's overlay. The other half of what spec section
    /// 9.1 asks of a rounding step; `granularity`, `unit` and `mode` are the
    /// first.
    ///
    /// As with `mode` above, the zero value is a real provenance
    /// (`RoundingProvenance::MethodDefault`) and not a "not applicable"
    /// sentinel: meaningful only for `RoundingRuleApplied`.
    RoundingProvenance roundingProvenance {};

    /// For `AcceptanceChecked`, and for each `Constraint` step it holds as an
    /// operand: whose constraints they were -- the method's own, or a
    /// jurisdiction's overlay. Spec section 9.1 asks the trace for every
    /// constraint verdict, and section 16.7 lets a jurisdiction supply its
    /// own acceptance logic; a verdict that does not say which is true of
    /// both and answers neither.
    ///
    /// Empty for a constraint checked on its own, by `check` or `check_all`
    /// (`constraint.hpp`), where there is no method to be anyone's -- which
    /// is why this is optional where `roundingProvenance` is not: a
    /// `RoundingRuleApplied` step always has a method behind it.
    std::optional<ConstraintProvenance> constraintProvenance {};

    /// The dimension of what this step produced.
    Dimension dimension {};

    /// The unit this step's value was **declared** in -- `Describe<Q>::unit`
    /// for a variable or an overridden constant, the constant's own unit for
    /// a constant, the node's own unit for a `Round`, `RoundSignificant`,
    /// `RoundedRoot` or `RoundingRuleApplied` step, the unit of the step it
    /// wraps for a `Documented`, `ReplacedVariant` or `VariantSelected` step --
    /// each passes its operand's value through unchanged, so it states it as
    /// that operand's line does, whenever that line is the wrapped node's own
    /// and not the operands of a consumer's node -- and the coherent SI unit of
    /// `dimension` for anything else computed, which has no declared unit of
    /// its own.
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
    /// step reading only `= 863/1000` explains nothing; what a reader checking a
    /// number needs is that 863/1000 came from the band containing the input.
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
    ///
    /// **"As the table declared them" is a guarantee about this field and not
    /// about the rendered line.** `trace_render.hpp` reduces both keys through
    /// `declared_number_text` before printing, so a row typed `14/4` reads
    /// `7/2` in a derivation -- deliberately, because `render()` prints `7/2`
    /// for that same row and a trace disagreeing with the formula it derives
    /// is the defect this phase exists to refuse. An auditor reconciling
    /// *values* against a published curve therefore matches; one reconciling
    /// the *literal spelling* an author typed needs this field, which is where
    /// the unreduced pair survives for a programmatic consumer to read.
    /// `selectedBand` and `coveredRange` carry the same split, for the same
    /// reason.
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
    ///
    /// For `Binning`: its classes' extent, half-open as a band table's is,
    /// recorded whether or not an observation missed.
    std::optional<LookupRange> coveredRange {};

    /// For `ExactLookup`: the key this lookup selected with, as the
    /// underlying value of the author's enumerator. Its name, when it has
    /// one, is `lookupKeyName` below.
    ///
    /// Recorded here because there is nowhere else it could survive. An
    /// exact lookup has **no operand**, so unlike a banded or an
    /// interpolating miss -- whose missed value is the operand's own result,
    /// sitting in the operand's own step -- a key that names no row would
    /// otherwise appear in no step of the derivation at all.
    ///
    /// The value is recorded even when the key has a name, because it is the
    /// only thing a key that names no row still has: a miss is exactly a key
    /// the table does not declare, and `lookupKeyName` is empty for it.
    ///
    /// Stored as the bit pattern with `lookupKeyIsSigned` beside it rather
    /// than as one signed integer, because an enumeration's underlying type
    /// may be `unsigned long long`, whose top half no signed type can hold --
    /// the same case `key_text` spells its two casts separately for.
    ///
    /// For `SampleSizeLookup`: the count this lookup selected with, when it
    /// was a whole, non-negative number, read unsigned. A count that was not
    /// one (`LookupFailure::NotACount`) leaves this zero, and its value stays
    /// in the operand's own step, where the renderer points.
    std::uint64_t lookupKey {};

    /// Whether `lookupKey` above is to be read as a signed value. Meaningful
    /// only when `kind` is `ExactLookup`, exactly as `lookupKey` itself is.
    bool lookupKeyIsSigned {};

    /// For `ExactLookup`: the name of the key this lookup selected with --
    /// `Cylinder`, or the author's own spelling of it through
    /// `EnumeratorName` (`enumerator.hpp`) -- and empty when the key names no
    /// row of the table, or when the row it names has a key that is itself no
    /// enumerator.
    ///
    /// **Empty does not by itself mean a miss.** A table may declare a row
    /// under a value that names no enumerator -- `static_cast<Shape>(9)` is a
    /// perfectly good key -- and a hit on that row records no name, because
    /// there is none to record. `lookupFailure` is what says whether the
    /// lookup missed.
    ///
    /// **Carried here because nothing downstream can compute it.** A `Step`
    /// has erased the key's type, so `trace_render.hpp` cannot ask the
    /// author's enumeration for a name the way `render()` does; the name has
    /// to be captured while the type is still known, at record time. It is
    /// matched against the table's declared keys by `detail::key_name`
    /// (`lookup.hpp`), using the same predicate the evaluator selects with,
    /// so it is always the name of the row the lookup actually selected. A
    /// key outside the table has no name here even if it is a real
    /// enumerator of the author's type.
    ///
    /// **A view, and safe to keep for the life of the trace and beyond.**
    /// Every name `enumerator_name` produces has static storage duration: a
    /// reflected name points into the compiler's function-signature literal,
    /// and a customized one is refused at compile time unless it passes the
    /// three gates described on `EnumeratorName` (`enumerator.hpp`) -- the
    /// library's own check, in its own words, on cl, clang-cl and clang; the
    /// compiler's `consteval` result rule, in the compiler's words, which is
    /// where g++ 13 refuses a view of a dead local buffer. So a trace
    /// outliving the formula, the table and the environment that produced it
    /// still holds a valid name. The one limit is the code image itself: a
    /// name recorded by a shared library or plugin points into that image's
    /// read-only data, and a trace kept after it is unloaded holds a dangling
    /// view.
    std::string_view lookupKeyName {};

    /// For `VariantSelected`: the selected variant's tag, as `tag_name`
    /// (`tag.hpp`) spells it -- `Cylinder`, or the author's own spelling of
    /// it through `TagName`. Empty for every other kind, and for a selection
    /// whose tag the compiler's signature did not let the library read, in
    /// which case `variantIndex` is what still identifies the variant.
    ///
    /// **A view, and safe to keep for the life of the trace and beyond,** for
    /// the reason `lookupKeyName` above gives, with one difference in where
    /// the characters live. A reflected tag name is **not** a substring of
    /// the compiler's signature literal -- stripping a qualifier from inside
    /// a template argument list leaves nothing contiguous to point at -- so
    /// it is written into a `static constexpr` array of its own
    /// (`detail::TypeNameStorage`, `detail/type_name.hpp`), which has static
    /// storage duration exactly as the literal does. A customized one passes
    /// the gates described on `TagName`. The same limit applies: a name
    /// recorded by a shared library or plugin dangles once that image is
    /// unloaded.
    std::string_view variantTag {};

    /// For `VariantSelected`: the selected variant's ZERO-BASED position in
    /// the method's `variants(...)` **as published** -- the order `Variants`
    /// makes part of its contract so that a reader can count back to the
    /// declaration. An overlay that pinned or pruned does not move it: the
    /// published `variants(...)` is the only one in the source to count in
    /// (`Variants::published`).
    /// `trace_render.hpp` prints it one-based, as an ordinal. Zero otherwise,
    /// which is a real position, so -- as with `comparison` -- no reader may
    /// use it without checking `kind` first.
    std::size_t variantIndex {};

    /// For `VariantSelected`: how many variants the method declares as
    /// published, however many an overlay left. Zero
    /// otherwise -- never a real count, since a method with no variants is
    /// refused where it is declared.
    std::size_t variantCount {};

    /// For `VariantSelected`: how many variants overlays pruned before it
    /// was selected (`VariantSelection::prunedCount`, `sink.hpp`). Zero
    /// otherwise, and when none did.
    std::size_t variantPrunedCount {};

    /// For `VariantSelected`: what the last overlay that pruned cited. Empty
    /// otherwise.
    Citation variantPrunedBy {};

    /// For `VariantSelected`: whether an overlay pinned the method to the
    /// selected variant; what it cited is in `citation`. `false` otherwise.
    bool variantPinned {};

    /// For `Variable`: whether the value was measured or typed in by a
    /// person, as the environment's entry says -- `Measured<Q>` or
    /// `Entered<Q>` (`environment.hpp`). For `OverriddenConstant` and
    /// `DerivedQuantity`: the same, of the environment's entry the overlay's
    /// constant or definition replaced -- whether or not that entry held a
    /// value, which `replacedEntryEmpty` says -- and empty when the
    /// environment has no entry for the quantity at all. For
    /// `SeriesVariable`: the same, of the whole series -- a measured series
    /// or `entered(measured_series<Q>(...))`. Never
    /// `Derived`: an input is not computed. Empty for every other kind, and
    /// for an environment that cannot say (one without `is_entered`), which
    /// is recorded as not known rather than guessed.
    std::optional<ValueSource> inputSource {};

    /// For `OverriddenConstant` and `DerivedQuantity`: true when the
    /// environment's entry the overlay replaced held no value -- an entry
    /// left empty, by hand or not -- so a trace says no value was replaced.
    /// False for every other kind, and when the environment has no entry for
    /// the quantity.
    bool replacedEntryEmpty {};

    /// Which record this step's value was read from, counted from one in its
    /// trace's `Trace::origins`, and zero for a step of the record being
    /// evaluated: set on **every** step recorded inside a `from_record` scope
    /// -- a constant, a lookup or a conditional there as much as a variable
    /// -- and on the scope's own step. A constant's value is the formula's
    /// and not the record's, but it is part of the computation over that
    /// record, and a consumer grouping steps by record finds it there. Every
    /// step carries it, so that a consumer reading one step need not walk up
    /// the operands to learn whose number it is; `origin_of(trace, step)`
    /// answers the origin itself. `render_trace` prints it on the scope and
    /// on the steps that name a quantity.
    ///
    /// A number rather than the origin, so that a step read from no other
    /// record -- nearly every step of every trace -- pays four bytes, which
    /// fit in padding `Step` had already, rather than an origin's
    /// forty-eight; and a plain number with zero for none, rather than an
    /// optional one, for the same four bytes. It means nothing without its
    /// trace: a step copied out of one trace into another names whatever
    /// that trace's table holds at the number. Only the library builds a
    /// `RecordOrigin` -- see `record.hpp`.
    std::uint32_t recordNumber {};

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

    /// For a series step (`SeriesVariable`): every element it produced, in
    /// the series' own order and in the coherent SI unit of `dimension`, each
    /// empty when that element was not measured. `value` stays empty for a
    /// series step, so that no renderer can mistake a series for one absent
    /// number; `trace_render.hpp` reads these instead, and spends one unit of
    /// `maxSteps` on each element it shows.
    ///
    /// Empty when the series failed: there is no partial series, and the
    /// elements computed before the failure are not a result (`series.hpp`).
    /// Empty for every step that is not a series.
    ///
    /// For `ConformityChecked`: the subject's elements, the values judged,
    /// likewise in SI, so that each outcome can state the value it judged in
    /// the check's own unit. Empty when the subject failed.
    std::vector<std::optional<Rep>> elements {};

    /// For a series step that failed: the ZERO-BASED position of the element
    /// it failed at, or empty when the failure belongs to no single element.
    /// `trace_render.hpp` prints it one-based, as every text this library
    /// writes prints a position. Empty for every other step.
    std::optional<std::size_t> failedElement {};

    /// For `CumulativeSum`: the end the running total started from. Zero-
    /// initialises to `FromFirst`, a real direction, so -- as with
    /// `comparison` -- no reader may use it without checking `kind` first.
    CumulativeDirection cumulativeDirection {};

    /// For `ElementwiseRound`: the decimal places each element was rounded
    /// to, in the series' own order. Empty for every other step, which keeps
    /// its one granularity in `granularity`.
    std::vector<int> elementGranularities {};

    /// For `ConformityChecked`: the outcome for each element of the subject,
    /// in the series' own order. Empty for every other step.
    std::vector<ConstraintOutcome> elementOutcomes {};

    /// For `SnappedToPermitted`: the rule for a value exactly midway.
    /// Zero-initialises to `TowardLower`, a real rule, so -- as with
    /// `comparison` -- no reader may use it without checking `kind` first.
    SnapTie snapTie {};

    /// For `SnappedToPermitted`: true when the value sat exactly midway
    /// between two permitted values and `snapTie` chose between them. False
    /// for every other step.
    bool tieBroken {};

    /// For `CurvePairing` and `CurveSplice`: the curve's points, in the
    /// coherent SI unit of `sourceUnit`'s dimension, each at the position of
    /// its value in `elements`. Empty for every other kind, and for a curve
    /// that failed -- unless `curveBreak` names a rule, when they are the
    /// points that broke it. For `Binning`: the observations it binned, in
    /// the coherent SI unit of `sourceUnit`'s dimension, in the order made.
    std::vector<std::optional<Rep>> domainElements {};

    /// For `CurveSplice`: the direction its values had to run in.
    /// Zero-initialises to `NonDecreasing`, a real setting, so -- like
    /// `comparison` -- no reader may use it without checking `kind` first.
    Monotone monotone {};

    /// For a `CurvePairing` or `CurveSplice` that failed at an element: the
    /// rule it broke there, at the point `domainElements[*failedElement]`.
    /// A pairing's `domainElements` are then its points as its domain series
    /// gave them; a splice's `domainElements` and `elements` are the sorted
    /// union it judged. `None` for every other step, and for a failure that
    /// broke no rule of a curve's own -- an operand's, or an overflow.
    CurveBreak curveBreak {};

    /// For a series step that failed at a position: what `failedElement`
    /// counts, as the evaluation's `SeriesFailure::site` said -- an element of
    /// the step's own series, or an observation it read (raw observations and
    /// a binning). Zero-initialises to `ResultElement`, the site of every
    /// other failure.
    FailureSite failureSite {};
};

/// The rows one conformity step judged its elements against, in the unit
/// its `Step::unit` names, one per element in the subject's order.
///
/// An envelope is master data, read at run time and free to change (see
/// `conformity.hpp`): what a derivation says was judged must be the rows as
/// they were, so they are copied here when the step is recorded.
struct ConformityLimits
{
    /// The index, in `Trace::steps`, of the `ConformityChecked` step.
    std::size_t step;
    /// The rows it judged against.
    std::vector<LimitRow> rows;
};

/// The comparison one `LineageChecked` step recorded -- see
/// `Trace::lineageChecks`.
struct LineageRow
{
    /// The index, in `Trace::steps`, of the `LineageChecked` step.
    std::size_t step;
    /// The attribute compared, the record compared with, and both keys. Its
    /// verdict is the step's `outcome`. Only the library builds a
    /// `LineageCheck` -- see `record.hpp`.
    LineageCheck check;
};

/// One output of an opaque call, as its step recorded it.
template <typename Rep = Rational>
struct OpaqueOutputValue
{
    /// The output's name, as the operation declares it.
    std::string_view name {};
    /// The dimension the operation declares for it.
    Dimension dimension {};
    /// The unit it is shown in: the declared unit of the first input step of
    /// its dimension, as a sum is shown in its series' unit, and the coherent
    /// SI unit otherwise.
    Unit unit {};
    /// Its value, in the coherent SI unit of `dimension`; empty when the call
    /// was absent or failed.
    std::optional<Rep> value {};
};

/// What an `OpaqueOperation` step carries beyond its `Step`, keyed by its
/// index in `Trace::steps`. A side table rather than members of `Step`, so
/// that every other step pays nothing for them (`Trace::conformityLimits` is
/// the precedent).
template <typename Rep = Rational>
struct OpaqueStepData
{
    /// The index, in `Trace::steps`, of the `OpaqueOperation` step.
    std::size_t step {};
    /// The operation's name, `Op::name`: static storage, so the view outlives
    /// the trace.
    std::string_view operationName {};
    /// Every output, in the operation's declared order.
    std::vector<OpaqueOutputValue<Rep>> outputs {};
    /// Whose failure the step carries: `None`, the operation's `Own`, an
    /// input's relayed (`Propagated`), or `Undetermined` when the call relayed
    /// a failure no input step shows.
    OpaqueFailure failure {};
};

/// Which output an `OpaqueOutput` step selected, keyed by its index in
/// `Trace::steps`.
struct OpaqueOutputStepData
{
    /// The index, in `Trace::steps`, of the `OpaqueOutput` step.
    std::size_t step {};
    /// The output's ZERO-BASED position among the operation's outputs.
    std::size_t outputIndex {};
};

/// What a `RetryAttempt` step carries beyond its `Step`, keyed by its index
/// in `Trace::steps`: 24 bytes, so that no other step pays for them.
struct AttemptStepData
{
    /// The index, in `Trace::steps`, of the `RetryAttempt` step.
    std::size_t step {};
    /// The attempt's number, from 1.
    std::size_t attemptNumber {};
    /// How it was judged.
    AttemptJudgement judgement {};
};

/// What a `RetryConcluded` step carries beyond its `Step`, keyed by its index
/// in `Trace::steps`.
struct RetryStepData
{
    /// The index, in `Trace::steps`, of the `RetryConcluded` step.
    std::size_t step {};
    /// The most attempts the retry allowed.
    std::size_t attemptLimit {};
    /// How it ended.
    RetryEnd end {};
    /// The verdict it ends in when no attempt is accepted: author text,
    /// escaped where a line is rendered.
    std::string_view verdictLabel {};
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

    /// The limits each conformity step judged against, keyed by its index in
    /// `steps`. A side table rather than a member of `Step`, so that every
    /// other step pays nothing for them.
    std::vector<ConformityLimits> conformityLimits {};

    /// What each `SampleSizeLookup` step's table declared, one record per
    /// such step, **keyed by the step's index** and appended in step order,
    /// so that a renderer finds a step's record by searching for its index.
    ///
    /// A side table rather than a field on every step (T10), and keyed by the
    /// step rather than reached through an index on it: an index would cost
    /// every step what the view it replaces cost, and this costs a step
    /// nothing (the lead's ruling on the task 3 review). Written by
    /// `RecordingSink` alone. `Step` and `Trace` are public aggregates, so a
    /// renderer that finds no record for a step says the record is missing
    /// rather than guess.
    std::vector<detail::SampleSizeRecord> sampleSizeRecords {};

    /// What each `PrecisionLevel` and `PrecisionLimit` step is and where its
    /// level is, one record per such step, keyed by the step's index and
    /// appended in step order -- the same side-table shape as
    /// `sampleSizeRecords`, for the same reason. Written by `RecordingSink`
    /// alone; a renderer that finds no record for such a step says so.
    std::vector<detail::PrecisionRecord> precisionRecords {};

    /// The precision limits whose level is bound, innermost last, while their
    /// limit expressions are evaluated. Bookkeeping, as `marks` is: a
    /// placeholder read now belongs to the innermost one.
    std::vector<detail::PrecisionBinding> precisionBindings {};

    /// What each rejection step is and every number it names, one record per
    /// such step, keyed by the step's index and appended in step order --
    /// the shape of `precisionRecords`. Written by `RecordingSink` alone; a
    /// renderer that finds no record for such a step says so.
    std::vector<detail::RejectionRecord<Rep>> rejectionRecords {};

    /// The rejections in progress, innermost last. Bookkeeping, as `marks` is.
    std::vector<detail::RejectionInProgress> rejectionsInProgress {};

    /// Every record a `from_record` scope in this trace read from, one entry
    /// per scope entered, in the order they were entered. `Step::recordNumber`
    /// counts into it from one; `origin_of` reads it. Not bookkeeping: it is part of the
    /// derivation, and a new walk over the same trace keeps it, as it keeps
    /// the steps.
    std::vector<RecordOrigin> origins {};

    /// The comparison each `LineageChecked` step recorded -- the attribute,
    /// the record compared with, and both keys -- keyed by its index in
    /// `steps`, as `conformityLimits` is; `lineage_of` reads it. A side table
    /// rather than a member of `Step`, for the same reason.
    std::vector<LineageRow> lineageChecks {};

    /// Where the variable being recorded read its value from, between its
    /// `entered` and its `produced`: `RecordingSink::input_source` writes it
    /// and `produced` moves it onto the `Variable` step. A series variable's
    /// comes the same way, between `series_entered` and `series_produced`,
    /// through `RecordingSink::series_input_source`.
    ///
    /// A single slot, not a stack as `branchStack` is: a variable has no
    /// operands, so nothing can be entered between its own `entered` and
    /// `produced` to need a slot of its own. `entered` empties it for every
    /// node, and `produced` empties it for every kind, so it never carries
    /// one variable's source onto another step.
    ///
    /// Bookkeeping, as `marks` is, and for the same reason.
    std::optional<ValueSource> pendingInputSource {};

    /// Whether the overridden constant being recorded replaced an empty
    /// entry, between its `entered` and its `produced`:
    /// `RecordingSink::replaced_entry_empty` sets it and `produced` moves it
    /// onto the `OverriddenConstant` step. A single slot, emptied by
    /// `entered` and `produced` for every node, for the reasons
    /// `pendingInputSource` gives.
    ///
    /// Bookkeeping, as `marks` is, and for the same reason.
    bool pendingReplacedEntryEmpty {};

    /// The number, counted from one in `origins`, of each `from_record`
    /// scope still open: `record_entered`
    /// pushes one, and `produced` pops it with the scope's own step. A stack
    /// by the shape `branchStack` has, though a scope cannot be nested in a
    /// scope today (`record.hpp` refuses it).
    ///
    /// Bookkeeping, as `marks` is, and for the same reason.
    std::vector<std::uint32_t> recordStack {};

    /// What each opaque call's step carries beyond its `Step`, keyed by its
    /// index in `steps` -- see `OpaqueStepData`. Written by `RecordingSink`
    /// alone; a `Trace` is plain data, so a caller may edit one by hand, as it
    /// may edit any `Step`.
    std::vector<OpaqueStepData<Rep>> opaqueSteps {};

    /// Which output each opaque output's step selected, keyed as
    /// `opaqueSteps` is.
    std::vector<OpaqueOutputStepData> opaqueOutputSteps {};

    /// Each retry attempt's number and judgement, keyed as `opaqueSteps` is.
    std::vector<AttemptStepData> attemptSteps {};

    /// How each retry ended, keyed as `opaqueSteps` is.
    std::vector<RetryStepData> retrySteps {};

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

/// What @p trace recorded for the opaque call whose step is at @p stepIndex,
/// or null when that step is not one.
template <typename Rep>
[[nodiscard]] OpaqueStepData<Rep> const* opaque_data(Trace<Rep> const& trace, std::size_t stepIndex) noexcept
{
    for (OpaqueStepData<Rep> const& kept: trace.opaqueSteps)
        if (kept.step == stepIndex)
            return &kept;
    return nullptr;
}

/// Which output the opaque output step at @p stepIndex of @p trace selected,
/// or null when that step is not one.
template <typename Rep>
[[nodiscard]] OpaqueOutputStepData const* opaque_output_data(Trace<Rep> const& trace, std::size_t stepIndex) noexcept
{
    for (OpaqueOutputStepData const& kept: trace.opaqueOutputSteps)
        if (kept.step == stepIndex)
            return &kept;
    return nullptr;
}

/// What @p trace recorded for the retry attempt whose step is at
/// @p stepIndex, or null when that step is not one.
template <typename Rep>
[[nodiscard]] AttemptStepData const* attempt_data(Trace<Rep> const& trace, std::size_t stepIndex) noexcept
{
    for (AttemptStepData const& kept: trace.attemptSteps)
        if (kept.step == stepIndex)
            return &kept;
    return nullptr;
}

/// What @p trace recorded for the retry whose concluding step is at
/// @p stepIndex, or null when that step is not one.
template <typename Rep>
[[nodiscard]] RetryStepData const* retry_data(Trace<Rep> const& trace, std::size_t stepIndex) noexcept
{
    for (RetryStepData const& kept: trace.retrySteps)
        if (kept.step == stepIndex)
            return &kept;
    return nullptr;
}

namespace detail
{
    /// Whether the positions in @p trace's step @p stepIndex count raw
    /// observations, as `FailureSite::InputObservation` does: the step is
    /// the observations' own. False for any other step, and for an index
    /// past the steps. (A rejection's sample is a series or observations:
    /// `without_outliers` refuses another rejection as its sample.)
    template <typename Rep>
    [[nodiscard]] bool step_counts_observations(Trace<Rep> const& trace, std::size_t stepIndex)
    {
        return stepIndex < trace.steps.size() && trace.steps[stepIndex].kind == StepKind::ObservationsVariable;
    }

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

    template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Radicand>
    struct StepKindOf<RoundedRootNode<U, Places, Mode, Radicand>>
    {
        static constexpr StepKind value = StepKind::RoundedRoot;
    };

    template <SampleSizeTable Sizes, Unit ResultUnit, Node Count>
    struct StepKindOf<SampleSizeLookupNode<Sizes, ResultUnit, Count>>
    {
        static constexpr StepKind value = StepKind::SampleSizeLookup;
    };

    template <Node Operand>
    struct StepKindOf<AbsoluteValueNode<Operand>>
    {
        static constexpr StepKind value = StepKind::AbsoluteValue;
    };

    template <Described Q>
    struct StepKindOf<PrecisionLevelNode<Q>>
    {
        static constexpr StepKind value = StepKind::PrecisionLevel;
    };

    template <PrecisionKind K, Node Level, Node Limit>
    struct StepKindOf<PrecisionLimitNode<K, Level, Limit>>
    {
        static constexpr StepKind value = StepKind::PrecisionLimit;
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

    /// Required, not a refinement: a partial specialisation never matches a
    /// derived class, so without this a `RoundingRuleNode` -- which derives
    /// from `RoundNode` -- would have no entry at all, rather than the
    /// `Round` entry of its base.
    template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct StepKindOf<RoundingRuleNode<U, Places, Mode, Operand>>
    {
        static constexpr StepKind value = StepKind::RoundingRuleApplied;
    };

    /// Required for the same reason: `OverriddenConstantNode` derives from
    /// `VarNode`, whose entry does not reach it.
    template <Described Q>
    struct StepKindOf<OverriddenConstantNode<Q>>
    {
        static constexpr StepKind value = StepKind::OverriddenConstant;
    };

    /// Required for the same reason: `DerivedQuantityNode` derives from
    /// `VarNode`.
    template <Described Q, Node Expr>
    struct StepKindOf<DerivedQuantityNode<Q, Expr>>
    {
        static constexpr StepKind value = StepKind::DerivedQuantity;
    };

    template <Node Expr>
    struct StepKindOf<ReplacedVariantNode<Expr>>
    {
        static constexpr StepKind value = StepKind::ReplacedVariant;
    };

    /// Whether `RecordingSink` records a step of its own for a node of type
    /// @p N -- whether `StepKindOf` has an entry for it. False for a
    /// consumer's node, which the primary template, left undefined, does not
    /// describe: such a node records nothing itself, though a three-parameter
    /// overload that hands the sink on to its operands has theirs recorded.
    template <typename N>
    concept RecordsStep = requires { StepKindOf<N>::value; };

    /// The one node a pass-through kind -- `Documented`, `ReplacedVariant` --
    /// wraps and whose value it passes on unchanged. Undefined for every
    /// other kind.
    template <typename N>
    struct PassedThrough;

    template <Node Inner>
    struct PassedThrough<DocumentedNode<Inner>>
    {
        using type = Inner;
    };

    template <Node Expr>
    struct PassedThrough<ReplacedVariantNode<Expr>>
    {
        using type = Expr;
    };

    /// Whether @p N passes on the value of a node the recorder records a step
    /// for -- so that, after it has claimed its operands, a single claimed
    /// step of its own dimension is that node's and no other. A consumer's
    /// node that forwards the sink is not one: what the pass-through claims
    /// then are that node's operands, and none of them holds its value.
    template <typename N>
    concept PassesThroughRecordedStep =
        requires { typename PassedThrough<N>::type; } && RecordsStep<typename PassedThrough<N>::type>;

    template <SeriesNode S>
    struct StepKindOf<SumNode<S>>
    {
        static constexpr StepKind value = StepKind::SeriesSum;
    };

    template <Unit KeyUnit, BreakpointTable Permitted, SnapTie Tie, Node Operand>
    struct StepKindOf<SnapNode<KeyUnit, Permitted, Tie, Operand>>
    {
        static constexpr StepKind value = StepKind::SnappedToPermitted;
    };

    template <CurveExpression C, Node At>
    struct StepKindOf<InterpolateAlongNode<C, At>>
    {
        static constexpr StepKind value = StepKind::CurveInterpolation;
    };

    template <SampleSource S>
    struct StepKindOf<SampleCountNode<S>>
    {
        static constexpr StepKind value = StepKind::SampleCount;
    };

    template <SampleSource S>
    struct StepKindOf<SampleMeanNode<S>>
    {
        static constexpr StepKind value = StepKind::SampleMean;
    };

    template <SampleSource S>
    struct StepKindOf<SampleVarianceNode<S>>
    {
        static constexpr StepKind value = StepKind::SampleVariance;
    };

    template <SampleSource S>
    struct StepKindOf<SampleRangeNode<S>>
    {
        static constexpr StepKind value = StepKind::SampleRange;
    };

    template <Described Q>
    struct StepKindOf<PassMeanNode<Q>>
    {
        static constexpr StepKind value = StepKind::PassMean;
    };

    template <>
    struct StepKindOf<PassCountNode>
    {
        static constexpr StepKind value = StepKind::PassCount;
    };

    template <std::size_t I, typename Call, typename Origin>
    struct StepKindOf<OpaqueOutputNode<I, Call, Origin>>
    {
        static constexpr StepKind value = StepKind::OpaqueOutput;
    };

    template <>
    struct StepKindOf<AttemptNumberNode>
    {
        static constexpr StepKind value = StepKind::AttemptNumber;
    };

    template <Described R>
    struct StepKindOf<PreviousAttemptNode<R>>
    {
        static constexpr StepKind value = StepKind::PreviousAttempt;
    };

    template <Described R>
    struct StepKindOf<ThisAttemptNode<R>>
    {
        static constexpr StepKind value = StepKind::ThisAttempt;
    };

    /// The `StepKind` a series node maps to: `StepKindOf`'s counterpart for a
    /// `SeriesNode`, and closed the same way. The primary template is left
    /// undefined, so a series node kind added without an entry here fails to
    /// compile against `RecordingSink` rather than being recorded as some
    /// other kind.
    template <typename S>
    struct SeriesStepKindOf;

    template <Described Q, std::size_t N>
    struct SeriesStepKindOf<SeriesVarNode<Q, N>>
    {
        static constexpr StepKind value = StepKind::SeriesVariable;
    };

    template <Unit U, std::size_t N>
    struct SeriesStepKindOf<SeriesConstantNode<U, N>>
    {
        static constexpr StepKind value = StepKind::SeriesConstant;
    };

    template <UnaryOperator Op, SeriesNode Operand>
    struct SeriesStepKindOf<ElementwiseUnaryNode<Op, Operand>>
    {
        static constexpr StepKind value = StepKind::ElementwiseNegate;
    };

    template <BinaryOperator Op, typename Left, typename Right>
    struct SeriesStepKindOf<ElementwiseBinaryNode<Op, Left, Right>>
    {
        static constexpr StepKind value = Op == BinaryOperator::Add        ? StepKind::ElementwiseAdd
                                          : Op == BinaryOperator::Subtract ? StepKind::ElementwiseSubtract
                                          : Op == BinaryOperator::Multiply ? StepKind::ElementwiseMultiply
                                                                           : StepKind::ElementwiseDivide;
    };

    template <CumulativeDirection D, SeriesNode S>
    struct SeriesStepKindOf<CumulativeNode<D, S>>
    {
        static constexpr StepKind value = StepKind::CumulativeSum;
    };

    template <Unit U, auto Places, RoundingMode Mode, SeriesNode S>
    struct SeriesStepKindOf<ElementwiseRoundNode<U, Places, Mode, S>>
    {
        static constexpr StepKind value = StepKind::ElementwiseRound;
    };

    template <Unit U, BreakpointTable Points>
    struct SeriesStepKindOf<DomainNode<U, Points>>
    {
        static constexpr StepKind value = StepKind::SeriesDomain;
    };

    template <Unit KeyUnit, BandTable Classes, ObservationsNode Obs>
    struct SeriesStepKindOf<BinnedNode<KeyUnit, Classes, Obs>>
    {
        static constexpr StepKind value = StepKind::Binning;
    };

    /// The `StepKind` a curve node maps to, closed as `SeriesStepKindOf` is.
    template <typename C>
    struct CurveStepKindOf;

    template <SeriesNode D, SeriesNode V>
    struct CurveStepKindOf<CurveNode<D, V>>
    {
        static constexpr StepKind value = StepKind::CurvePairing;
    };

    template <Monotone M, CurveExpression A, CurveExpression B>
    struct CurveStepKindOf<SpliceNode<M, A, B>>
    {
        static constexpr StepKind value = StepKind::CurveSplice;
    };

    /// The unit a total is shown in: its operand step's, when it claimed one
    /// of its own dimension -- a total of grams reads in grams, as the masses
    /// summed do -- and @p fallback otherwise. Read off the operand's step,
    /// never off a type, so a computed operand's coherent unit carries over
    /// too.
    template <typename Rep>
    [[nodiscard]] constexpr Unit operand_unit_or(std::vector<Step<Rep>> const& steps,
                                                 std::vector<std::size_t> const& operands,
                                                 Dimension dimension,
                                                 Unit fallback) noexcept
    {
        if (operands.size() != 1)
            return fallback;
        Unit const operandUnit = steps[operands.front()].unit;
        return operandUnit.dimension == dimension ? operandUnit : fallback;
    }

    template <typename Role, typename Requirement, Node Operand>
    struct StepKindOf<RecordScopeNode<Role, Requirement, Operand>>
    {
        static constexpr StepKind value = StepKind::RecordScope;
    };

    /// Whether @p stepKind is one of the four lookup kinds. Written once because
    /// two surfaces ask it -- `RecordingSink::produced`, which dispatches to
    /// `record_lookup` below, and `trace_render.hpp`'s `step_line`, which
    /// appends the clause that keeps a lookup line from lying -- and spelling
    /// the three-way `||` in each is how one of them ends up missing a kind
    /// once a fourth table kind is added.
    [[nodiscard]] constexpr bool is_lookup(StepKind stepKind) noexcept
    {
        return stepKind == StepKind::BandedLookup || stepKind == StepKind::ExactLookup
               || stepKind == StepKind::InterpolatingLookup || stepKind == StepKind::SampleSizeLookup;
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
        for (std::size_t const operandIndex: step.operands)
            if (steps[operandIndex].error.has_value())
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

            std::optional<Rational> const operandValue = sole_operand_value(steps, step);
            if (!operandValue.has_value())
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
                checked_convert(*operandValue, coherent(keyUnit.dimension), keyUnit);
            if (!valueInKey.has_value())
            {
                step.lookupFailure = LookupFailure::Conversion;
                return;
            }

            std::optional<std::size_t> const matchedBand = find_band<Bands>(*valueInKey);
            if (!matchedBand.has_value())
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
                step.selectedBand = Bands[*matchedBand];
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
        step.lookupKeyName = key_name<Keys>(node.key);
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

            std::optional<Rational> const operandValue = sole_operand_value(steps, step);
            if (!operandValue.has_value())
            {
                if (step.error.has_value())
                    step.lookupFailure = LookupFailure::Undetermined;
                return;
            }

            std::expected<Rational, ArithmeticError> const valueInKey =
                checked_convert(*operandValue, coherent(keyUnit.dimension), keyUnit);
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
    /// Fills in a snap step's tie rule and, from `locate_and_snap` re-asked --
    /// the one scan the evaluation used -- the two neighbours and whether the
    /// tie rule decided, or on a miss the set's extent. Nothing more when the
    /// operand failed, was absent or left no step: then nothing was snapped.
    template <typename Rep, Unit KeyUnit, BreakpointTable Permitted, SnapTie Tie, Node Operand>
    void record_snap(SnapNode<KeyUnit, Permitted, Tie, Operand> const&, Step<Rep>& step, std::vector<Step<Rep>> const& steps)
    {
        step.snapTie = Tie;
        if constexpr (std::is_same_v<Rep, Rational>)
        {
            constexpr Unit keyUnit = KeyUnit;
            if (an_operand_failed(steps, step))
                return;
            std::optional<Rational> const operandValue = sole_operand_value(steps, step);
            if (!operandValue.has_value())
                return;
            std::expected<Rational, ArithmeticError> const valueInKey =
                checked_convert(*operandValue, coherent(keyUnit.dimension), keyUnit);
            if (!valueInKey.has_value())
                return;
            std::expected<SnapAnswer, ArithmeticError> const answered = locate_and_snap<Permitted, Tie>(*valueInKey);
            if (!answered.has_value())
            {
                if (answered.error() == ArithmeticError::DomainError)
                    step.coveredRange = points_cover<Permitted>();
                return;
            }
            step.selectedSegment = answered->neighbours;
            step.tieBroken = answered->tieBroken;
        }
    }

    /// @p point, a point of a curve in the coherent SI unit, as a declared
    /// `Breakpoint` in @p pointUnit, or nothing when it cannot be stated there.
    [[nodiscard]] inline std::optional<Breakpoint> point_in(Rational point, Unit pointUnit) noexcept
    {
        std::expected<Rational, ArithmeticError> const stated =
            checked_convert(point, coherent(pointUnit.dimension), pointUnit);
        if (!stated.has_value())
            return std::nullopt;
        return Breakpoint { stated->numerator(), stated->denominator() };
    }

    /// Fills in an interpolation step along a curve: its values' unit and its
    /// points' unit, taken off the curve's step, and -- from
    /// `interpolate_along` re-asked on that step's points and values, the one
    /// scan the evaluation used -- the two points the answer lay between, or
    /// on a miss the curve's extent. Nothing more when the curve or the point
    /// failed, was absent, or left no step.
    template <typename Rep, CurveExpression C, Node At>
    void record_curve_interpolation(InterpolateAlongNode<C, At> const&, Step<Rep>& step, std::vector<Step<Rep>> const& steps)
    {
        std::optional<std::size_t> curveStep;
        std::optional<std::size_t> pointStep;
        for (std::size_t const operandIndex: step.operands)
        {
            StepKind const operandKind = steps[operandIndex].kind;
            if (!curveStep.has_value() && (operandKind == StepKind::CurvePairing || operandKind == StepKind::CurveSplice))
                curveStep = operandIndex;
            else
                pointStep = operandIndex;
        }
        if (!curveStep.has_value())
            return;
        Step<Rep> const& curveRecorded = steps[*curveStep];
        if (curveRecorded.unit.dimension == step.dimension)
            step.unit = curveRecorded.unit;
        step.sourceUnit = curveRecorded.sourceUnit;

        if constexpr (std::is_same_v<Rep, Rational>)
        {
            if (curveRecorded.error.has_value() || !pointStep.has_value() || !steps[*pointStep].value.has_value())
                return;
            // An absent element anywhere makes the answer absent (S7): nothing
            // was located, so no clause -- not a segment, and never a miss's
            // range, which a curve absent inside its extent would otherwise
            // state falsely.
            if (curveRecorded.domainElements.size() != curveRecorded.elements.size())
                return;
            for (std::size_t at = 0; at < curveRecorded.elements.size(); ++at)
                if (!curveRecorded.domainElements[at].has_value() || !curveRecorded.elements[at].has_value())
                    return;
            std::expected<std::pair<Rational, KeyPosition>, ArithmeticError> const answered =
                interpolate_along(curveRecorded.domainElements, curveRecorded.elements, *steps[*pointStep].value);
            if (answered.has_value())
            {
                std::optional<Breakpoint> const lowPoint =
                    point_in(*curveRecorded.domainElements[answered->second.low], step.sourceUnit);
                std::optional<Breakpoint> const highPoint =
                    point_in(*curveRecorded.domainElements[answered->second.high], step.sourceUnit);
                if (lowPoint.has_value() && highPoint.has_value())
                    step.selectedSegment = Segment { *lowPoint, *highPoint };
                return;
            }
            // A miss on a curve with every point present: say what it covers.
            if (answered.error() != ArithmeticError::DomainError || curveRecorded.domainElements.empty())
                return;
            for (std::optional<Rational> const& curvePoint: curveRecorded.domainElements)
                if (!curvePoint.has_value())
                    return;
            std::optional<Breakpoint> const lowPoint = point_in(*curveRecorded.domainElements.front(), step.sourceUnit);
            std::optional<Breakpoint> const highPoint = point_in(*curveRecorded.domainElements.back(), step.sourceUnit);
            if (lowPoint.has_value() && highPoint.has_value())
                step.coveredRange = LookupRange { .lowNumerator = lowPoint->numerator,
                                                  .lowDenominator = lowPoint->denominator,
                                                  .highNumerator = highPoint->numerator,
                                                  .highDenominator = highPoint->denominator };
        }
    }

    /// Whether @p recorded, a curve's step, succeeded with @p pointCount
    /// points and values, every one present.
    [[nodiscard]] inline bool whole(Step<Rational> const& recorded, std::size_t pointCount) noexcept
    {
        if (recorded.error.has_value() || recorded.domainElements.size() != pointCount
            || recorded.elements.size() != pointCount)
            return false;
        for (std::size_t at = 0; at < pointCount; ++at)
            if (!recorded.domainElements[at].has_value() || !recorded.elements[at].has_value())
                return false;
        return true;
    }

    /// Names the rule a failed curve broke at its failed element, from the
    /// judgement the evaluation made -- `judge_domain` or `judge_splice`,
    /// re-asked on the operands' steps -- and keeps the points it judged, so
    /// that the trace can state the point. Nothing when an operand failed, or
    /// a splice's operand is not all there: the failure then was not a
    /// curve's own rule. Otherwise a rule is the only way the evaluation
    /// fails at an element, and the judgement lands on that element.
    template <CurveExpression C>
    void record_curve_break(Step<Rational>& failedStep, std::vector<Step<Rational>> const& steps)
    {
        if (failedStep.operands.size() != 2)
            return;
        Step<Rational> const& firstOperand = steps[failedStep.operands.front()];
        Step<Rational> const& secondOperand = steps[failedStep.operands.back()];
        std::vector<std::optional<Rational>> points;
        std::vector<std::optional<Rational>> pointValues;
        std::optional<CurveBreakAt> broken;
        if constexpr (CurveStepKindOf<C>::value == StepKind::CurvePairing)
        {
            // The values must have succeeded too: the evaluation judges the
            // domain only when both series did. An absent point is judged
            // past, as the evaluation judges it.
            if (firstOperand.error.has_value() || firstOperand.elements.size() != C::length
                || secondOperand.error.has_value())
                return;
            points = firstOperand.elements;
            broken = judge_domain(points);
        }
        else
        {
            std::size_t const firstCount = firstOperand.domainElements.size();
            if (!whole(firstOperand, firstCount) || !whole(secondOperand, C::length - firstCount))
                return;
            points = firstOperand.domainElements;
            points.insert(points.end(), secondOperand.domainElements.begin(), secondOperand.domainElements.end());
            pointValues = firstOperand.elements;
            pointValues.insert(pointValues.end(), secondOperand.elements.begin(), secondOperand.elements.end());
            sort_by_domain(points, pointValues);
            broken = judge_splice(points, pointValues, C::monotone);
        }
        if (!broken.has_value())
            return;
        failedStep.curveBreak = broken->rule;
        failedStep.domainElements = std::move(points);
        failedStep.elements = std::move(pointValues);
    }

    /// Whether @p unitSymbol is more than one unit word -- `mPa.s`, `N m`,
    /// `m^2` -- so that written after a slash it would read two ways:
    /// `mm/mPa.s` is (mm/mPa) s read left to right.
    [[nodiscard]] inline bool compound_unit_symbol(std::string_view unitSymbol) noexcept
    {
        return unitSymbol.find_first_of(".*^() ") != std::string_view::npos
               || unitSymbol.find("\xc2\xb7") != std::string_view::npos       // U+00B7 middle dot
               || unitSymbol.find("\xe2\x8b\x85") != std::string_view::npos; // U+22C5 dot operator
    }

    /// The quotient of two units, `N/mm` from `N` and `mm`: its magnitude the
    /// quotient of theirs and its symbol theirs joined by a slash, the
    /// denominator bracketed when it is more than one unit word
    /// (`mm/(mPa.s)`, `compound_unit_symbol`). Empty when either has an
    /// offset, has no symbol, is dimensionless -- a ratio is not a percentage
    /// because some input was one, as `opaque_output_unit` rules for a
    /// dimensionless output -- or already holds a slash (`m/s/s` reads two
    /// ways), or when the symbol or the magnitude would not fit.
    [[nodiscard]] inline std::optional<Unit> unit_quotient(Unit const& over, Unit const& under) noexcept
    {
        if (over.offsetNumerator != 0 || under.offsetNumerator != 0 || over.dimension == dim::Scalar
            || under.dimension == dim::Scalar)
            return std::nullopt;
        std::string_view const overSymbol = view(over.symbolText);
        std::string_view const underSymbol = view(under.symbolText);
        bool const bracketed = compound_unit_symbol(underSymbol);
        if (overSymbol.empty() || underSymbol.empty() || overSymbol.find('/') != std::string_view::npos
            || underSymbol.find('/') != std::string_view::npos
            || overSymbol.size() + 1 + underSymbol.size() + (bracketed ? 2 : 0) + 1 > SymbolCapacity)
            return std::nullopt;
        std::expected<Rational, ArithmeticError> const magnitude =
            RepTraits<Rational>::divide(Rational { over.magnitudeNumerator, over.magnitudeDenominator },
                                        Rational { under.magnitudeNumerator, under.magnitudeDenominator });
        if (!magnitude.has_value())
            return std::nullopt;
        Unit quotientUnit { .dimension = over.dimension / under.dimension,
                            .magnitudeNumerator = magnitude->numerator(),
                            .magnitudeDenominator = magnitude->denominator(),
                            .decimals = over.decimals < under.decimals ? under.decimals : over.decimals };
        std::size_t written = 0;
        for (char const spelt: overSymbol)
            quotientUnit.symbolText.characters[written++] = spelt;
        quotientUnit.symbolText.characters[written++] = '/';
        if (bracketed)
            quotientUnit.symbolText.characters[written++] = '(';
        for (char const spelt: underSymbol)
            quotientUnit.symbolText.characters[written++] = spelt;
        if (bracketed)
            quotientUnit.symbolText.characters[written++] = ')';
        return quotientUnit;
    }

    /// The unit an opaque output of @p dimension is shown in, from the units
    /// its input steps are shown in -- a curve's values, then its points:
    ///
    ///  1. the first of those units of that dimension, as a sum reads in its
    ///     series' unit;
    ///  2. else the first quotient of two of them, either way up, of that
    ///     dimension, so that a slope along a curve of millimetres over
    ///     seconds reads `mm/s` (`unit_quotient`) -- at most one way up can
    ///     match, since the output is not dimensionless;
    ///  3. else the coherent SI unit, which the trace spells out
    ///     (`coherent_unit_text`, `trace_render.hpp`).
    ///
    /// Two exceptions keep a borrowed unit honest. A unit with an offset is
    /// never borrowed: an output of an input's dimension is not in general a
    /// reading on its scale -- a span of Celsius readings is a difference, and
    /// shown in degrees Celsius it would be off by the offset -- so it reads
    /// in kelvin. And a dimensionless output borrows nothing: a ratio of two
    /// masses is not a percentage because some input was one, and an
    /// operation declares no unit for its outputs.
    template <typename Rep>
    [[nodiscard]] Unit opaque_output_unit(std::vector<Step<Rep>> const& steps,
                                          std::vector<std::size_t> const& operands,
                                          Dimension dimension)
    {
        if (dimension == dim::Scalar)
            return coherent(dimension);
        std::vector<Unit> shownIn;
        for (std::size_t const operandIndex: operands)
        {
            Step<Rep> const& inputStep = steps[operandIndex];
            if (inputStep.unit.offsetNumerator == 0)
                shownIn.push_back(inputStep.unit);
            if ((inputStep.kind == StepKind::CurvePairing || inputStep.kind == StepKind::CurveSplice)
                && inputStep.sourceUnit.offsetNumerator == 0)
                shownIn.push_back(inputStep.sourceUnit);
        }
        for (Unit const& candidate: shownIn)
            if (candidate.dimension == dimension)
                return candidate;
        for (std::size_t earlier = 0; earlier < shownIn.size(); ++earlier)
            for (std::size_t later = earlier + 1; later < shownIn.size(); ++later)
                for (auto const& [over, under]: { std::pair { earlier, later }, std::pair { later, earlier } })
                    if (std::optional<Unit> const quotientUnit = unit_quotient(shownIn[over], shownIn[under]);
                        quotientUnit.has_value() && quotientUnit->dimension == dimension)
                        return *quotientUnit;
        return coherent(dimension);
    }
    /// Fills in a binning step: the classes' unit and extent, from the node's
    /// type, and the observations it binned, off its operand's step -- in
    /// the coherent SI unit, as that step holds them. Nothing of the
    /// observations when their step failed or is not there.
    template <SeriesNode S, typename Rep>
    void record_binning(Step<Rep>& binningStep, std::vector<Step<Rep>> const& steps)
    {
        binningStep.sourceUnit = S::unit;
        constexpr auto binnedClasses = S::classes;
        if constexpr (binnedClasses.size() > 0)
            binningStep.coveredRange = LookupRange { .lowNumerator = binnedClasses.front().lowNumerator,
                                                     .lowDenominator = binnedClasses.front().lowDenominator,
                                                     .highNumerator = binnedClasses.back().highNumerator,
                                                     .highDenominator = binnedClasses.back().highDenominator };
        if (binningStep.operands.size() != 1)
            return;
        Step<Rep> const& observedStep = steps[binningStep.operands.front()];
        if (observedStep.kind != StepKind::ObservationsVariable || observedStep.error.has_value())
            return;
        binningStep.domainElements = observedStep.elements;
    }

    /// Fills in a critical-value lookup step's count and `lookupFailure`. The
    /// table's declared sizes go to the trace's side table, which
    /// `RecordingSink::produced` owns.
    ///
    /// The count is read from the operand's own step and turned into a sample
    /// size by `as_sample_size`, and the row found by `find_sample_size` --
    /// `critical_value.hpp`'s own functions, the ones that decided during the
    /// evaluation -- so the derivation cannot disagree with the number.
    template <typename Rep, SampleSizeTable Sizes, Unit ResultUnit, Node Count>
    void record_lookup(SampleSizeLookupNode<Sizes, ResultUnit, Count> const&,
                       Step<Rep>& step,
                       std::vector<Step<Rep>> const& steps)
    {
        if constexpr (std::is_same_v<Rep, Rational>)
        {
            if (an_operand_failed(steps, step))
            {
                step.lookupFailure = LookupFailure::Propagated;
                return;
            }

            std::optional<Rational> const operandValue = sole_operand_value(steps, step);
            if (!operandValue.has_value())
            {
                if (step.error.has_value())
                    step.lookupFailure = LookupFailure::Undetermined;
                return;
            }

            std::optional<std::uint64_t> const sampleSize = as_sample_size(*operandValue);
            if (!sampleSize.has_value())
            {
                step.lookupFailure = LookupFailure::NotACount;
                return;
            }
            step.lookupKey = *sampleSize;

            if (!find_sample_size<Sizes>(*sampleSize).has_value())
                step.lookupFailure = LookupFailure::Missed;
            else if (step.error.has_value())
                step.lookupFailure = LookupFailure::Conversion;
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
/// whose walk is still in progress clears the bookkeeping that walk is using.
/// Nothing in this library does that; only a consumer sharing one `Trace` with
/// an evaluation already under way can. The outer walk's next `produced` then
/// finds no mark to claim from, and drops its step rather than read an empty
/// stack -- as every `..._produced` does when told of a result without the
/// matching `..._entered`, which a consumer's own evaluator may forget, and as
/// `produced` of a `when()` and `branch_taken` do when no `when()` was
/// entered. The trace is then incomplete, and says less rather than something
/// false.
///
/// **A vocabulary, when one is given, is held by value**, and every step
/// naming a quantity writes its symbol through it (`Step::symbol`). By value
/// and not, like the `Trace`, by pointer: a vocabulary is plain data holding
/// views of static storage, so a copy has no lifetime to outlive, where a
/// pointer to one declared in a function that returned would dangle. The
/// default vocabulary is empty and takes no space (`FORMULA_NO_UNIQUE_ADDRESS`),
/// so a sink that names none is the one pointer it always was; a scoped one
/// adds one `std::string_view` per renamed quantity to every copy the evaluator
/// makes.
/// The symbol is written *here*, during evaluation: rendering the page in a
/// vocabulary does not make the trace agree with it, so give the sink the one
/// `render()` and `document()` are given.
///
/// **Its hooks are public**, as every sink hook is, and a consumer's own node
/// kind evaluated under it may call them. `Trace` is a public arena besides.
/// What the evaluator guarantees is what *it* records; a hook called by
/// consumer code during a recorded evaluation records what that code says --
/// within each hook's own checks (`sample_failed_at` amends only a failed
/// sample statistic, and the renderer checks the position it names).
///
///     formula::RecordingSink sink { trace, north };
template <typename Rep = Rational, Vocabulary V = DefaultVocabulary>
class RecordingSink
{
  public:
    /// @p trace must outlive the evaluation. Begins a new walk: see the class
    /// comment for why this clears `trace.marks`, `trace.unclaimed`, and
    /// `trace.branchStack`. Writes every symbol as @p vocabulary says, and
    /// keeps a copy of it; left out, it is the default vocabulary, which
    /// renames nothing.
    ///
    /// @pre no other `RecordingSink` is part-way through a walk of @p trace.
    explicit constexpr RecordingSink(Trace<Rep>& trace, V vocabulary = V {}) noexcept:
        _trace { &trace },
        _vocabulary { vocabulary }
    {
        _trace->marks.clear();
        _trace->unclaimed.clear();
        _trace->branchStack.clear();
        _trace->rejectionsInProgress.clear();
        _trace->pendingInputSource.reset();
        _trace->pendingReplacedEntryEmpty = false;
        _trace->recordStack.clear();
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
        _trace->pendingInputSource.reset();
        _trace->pendingReplacedEntryEmpty = false;
        if constexpr (detail::StepKindOf<N>::value == StepKind::Conditional)
            _trace->branchStack.push_back(Branch::Neither);
    }

    /// Told, by the variable evaluator (`evaluate.hpp`), whether the value it
    /// just read was measured or typed in; `produced` puts it on the step.
    /// Optional, as `branch_taken` is: a sink without it pays nothing.
    ///
    /// Public, because the evaluator is not this class's friend. A caller
    /// that calls it by hand, between a variable's own `entered` and
    /// `produced`, states a source the library did not read -- the same
    /// boundary `Trace::steps` has always had, since any code may edit a
    /// recorded step. Called at any other time it is discarded: `entered`
    /// empties the slot for every node.
    template <Described Q>
    void input_source(VarNode<Q> const&, ValueSource source) noexcept
    {
        _trace->pendingInputSource = source;
    }

    /// Told, by an overridden constant's or a derived quantity's evaluator
    /// (`overlay.hpp`), whether the environment's entry it replaced was
    /// measured or typed in; `produced` puts it on the step's `inputSource`.
    /// The replaced entry's source, never the step's value's -- which is why
    /// it is not `input_source`. Optional, and public, for the reasons
    /// `input_source` gives, with the same boundary.
    template <Described Q>
    void replaced_entry_source(OverriddenConstantNode<Q> const&, ValueSource source) noexcept
    {
        _trace->pendingInputSource = source;
    }

    /// `replaced_entry_source` for a derived quantity.
    template <Described Q, Node Expr>
    void replaced_entry_source(DerivedQuantityNode<Q, Expr> const&, ValueSource source) noexcept
    {
        _trace->pendingInputSource = source;
    }

    /// Told, by the same evaluators, that the environment's entry the node
    /// replaced held no value; `produced` puts it on the step. Optional, and
    /// public, for the reasons `input_source` gives, with the same boundary.
    template <Described Q>
    void replaced_entry_empty(OverriddenConstantNode<Q> const&) noexcept
    {
        _trace->pendingReplacedEntryEmpty = true;
    }

    /// `replaced_entry_empty` for a derived quantity.
    template <Described Q, Node Expr>
    void replaced_entry_empty(DerivedQuantityNode<Q, Expr> const&) noexcept
    {
        _trace->pendingReplacedEntryEmpty = true;
    }

    /// Told, by a `from_record` scope's evaluator (`record.hpp`), right after
    /// the scope was entered and before anything inside it, which record its
    /// values are read from. Every step `produced` records from here until
    /// the scope's own step is stamped with it.
    ///
    /// Public, for the reason `input_source` is. Handed a copy of an origin
    /// by hand, it stamps that origin: the boundary `record.hpp`'s file
    /// comment states.
    void record_entered(RecordOrigin const& openedFrom)
    {
        _trace->origins.push_back(openedFrom);
        _trace->recordStack.push_back(static_cast<std::uint32_t>(_trace->origins.size()));
    }

    /// Told, by a scope's evaluator, of one attribute its lineage requirement
    /// compared, and the verdict, in the order the requirement names them --
    /// after `record_entered` and before the operand is read. Records a step
    /// with no operands of its own, which the scope's step then claims as an
    /// operand, and stamps it with the scope's origin, as every step inside
    /// the scope is.
    ///
    /// Public, for the reason `input_source` is: a caller handing it a copy
    /// of a check by hand records that check, the boundary `record.hpp`'s file
    /// comment states.
    void lineage_checked(LineageCheck const& attributeCheck, ConstraintOutcome const& attributeOutcome)
    {
        Step<Rep> checkStep {};
        checkStep.kind = StepKind::LineageChecked;
        checkStep.outcome = attributeOutcome;
        stamp_origin(checkStep);
        _trace->lineageChecks.push_back(LineageRow { _trace->steps.size(), attributeCheck });
        _trace->steps.push_back(std::move(checkStep));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
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
    ///
    /// Told of a branch with no `when()` entered -- a consumer's own
    /// evaluator out of step, as `produced` below describes -- there is no
    /// entry to update, and nothing is recorded.
    template <Node N>
    void branch_taken(N const&, bool thenTaken) noexcept
    {
        if (_trace->branchStack.empty())
            return;
        _trace->branchStack.back() = thenTaken ? Branch::Then : Branch::Else;
    }

    /// Records the step, claiming as its operands every step recorded at or
    /// after the matching `entered` that nothing else has claimed.
    template <Node N>
    void produced(N const& node, Evaluated<Rep> const& result)
    {
        // Told what a walk produced without having been told it began -- a
        // consumer's own evaluator that skipped the matching `entered`, or
        // a second sink that cleared the bookkeeping mid-walk. There is no
        // mark to claim from, and reading one off an empty stack is undefined
        // behaviour (cl's debug library aborts), so the step is dropped.
        if (_trace->marks.empty())
            return;
        // The same, for a `when()` whose `entered` was never told: no pending
        // branch to pop. Dropped before the mark is taken, so that the mark,
        // which some other node's `entered` pushed, stays for that node.
        if constexpr (detail::StepKindOf<N>::value == StepKind::Conditional)
            if (_trace->branchStack.empty())
                return;
        std::size_t const nodeMark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> nodeStep {};
        nodeStep.kind = detail::StepKindOf<N>::value;
        nodeStep.dimension = N::dimension;

        // Anything computed has no declared unit, so the coherent SI one is
        // the truthful answer; a variable overrides it with the unit its
        // quantity is declared in. `requires { N::unit; }` now also selects
        // `ConstantNode<U>`, `RoundNode`, `RoundSignificantNode` and
        // `RoundedRootNode` -- every one of them declares a unit that is the
        // single most load-bearing fact about the step:
        // `rounded<Megapascal, 1>(...)` rounds *in megapascals*, and a step
        // recording "rounded to 1 dp" without saying 1 dp of what is not a
        // record of anything. `VarNode` still carries
        // its unit on `Describe<quantity>` instead of a member of its own,
        // which is why it needs the branch above rather than this one.
        //
        // `NumericValueNode` is excluded even though it also declares
        // `unit`: unlike the four kinds above, its declared unit measures
        // its *operand's* dimension, not its own -- a `NumericValueNode` is
        // always `Scalar` -- so assigning it here would make this step's
        // `unit` disagree with its `dimension`, and the renderer's
        // `checked_convert(value, coherent(dimension), unit)` would refuse
        // every such step as a dimension mismatch. See `Step::sourceUnit`,
        // which is where that unit goes instead.
        //
        // An overridden constant and a derived quantity are variables to this
        // branch: each is `Q`, at the overlay's value or its definition's, in
        // `Q`'s declared unit.
        constexpr bool namesQuantity = detail::StepKindOf<N>::value == StepKind::Variable
                                       || detail::StepKindOf<N>::value == StepKind::OverriddenConstant
                                       || detail::StepKindOf<N>::value == StepKind::DerivedQuantity
                                       || detail::StepKindOf<N>::value == StepKind::PreviousAttempt
                                       || detail::StepKindOf<N>::value == StepKind::ThisAttempt;
        nodeStep.unit = coherent(N::dimension);
        if constexpr (namesQuantity || detail::StepKindOf<N>::value == StepKind::PrecisionLevel)
            nodeStep.unit = Describe<typename N::quantity>::unit;
        else if constexpr (detail::StepKindOf<N>::value != StepKind::NumericValue && requires { N::unit; })
            nodeStep.unit = N::unit;
        // A pass's mean reads in its sample's unit, as the pass line beside it
        // does: grams for a series of masses, bare SI for a computed series.
        // The innermost rejection in progress is the one it is bound to.
        if constexpr (detail::StepKindOf<N>::value == StepKind::PassMean)
            if (!_trace->rejectionsInProgress.empty())
                if (std::optional<std::size_t> const sampleStep = _trace->rejectionsInProgress.back().sampleStep;
                    sampleStep.has_value() && *sampleStep < _trace->steps.size())
                    nodeStep.unit = _trace->steps[*sampleStep].unit;

        if constexpr (namesQuantity)
            nodeStep.symbol = symbol_of<typename N::quantity>(_vocabulary);
        // Only a variable reads an input, and only a fixed constant or a
        // derived quantity replaces an entry. For every other kind the slot
        // is already empty -- `entered` emptied it, and only those
        // evaluators write it -- and it is emptied again regardless, so that
        // nothing a caller wrote by hand outlives the step it was written
        // during.
        constexpr bool replacesEntry = detail::StepKindOf<N>::value == StepKind::OverriddenConstant
                                       || detail::StepKindOf<N>::value == StepKind::DerivedQuantity;
        if constexpr (detail::StepKindOf<N>::value == StepKind::Variable || replacesEntry)
            nodeStep.inputSource = _trace->pendingInputSource;
        _trace->pendingInputSource.reset();
        if constexpr (replacesEntry)
            nodeStep.replacedEntryEmpty = _trace->pendingReplacedEntryEmpty;
        _trace->pendingReplacedEntryEmpty = false;
        if constexpr (detail::StepKindOf<N>::value == StepKind::Documented)
            nodeStep.citation = node.citation;
        // What an overlay cited for the value it fixed, the quantity it
        // defined, the formula it replaced or the rule it set -- the
        // provenance each of these four steps exists to carry, read through
        // accessors of nodes only an overlay builds.
        if constexpr (detail::StepKindOf<N>::value == StepKind::OverriddenConstant
                      || detail::StepKindOf<N>::value == StepKind::DerivedQuantity
                      || detail::StepKindOf<N>::value == StepKind::ReplacedVariant)
            nodeStep.citation = node.source();
        if constexpr (detail::StepKindOf<N>::value == StepKind::RoundingRuleApplied)
        {
            nodeStep.roundingProvenance = node.rule().provenance();
            nodeStep.citation = node.rule().source();
        }
        if constexpr (detail::StepKindOf<N>::value == StepKind::NumericValue)
        {
            nodeStep.justification = N::justification;
            nodeStep.sourceUnit = N::unit;
        }
        // The banded and the interpolating lookup declare a key unit that is
        // independent of their own: a band's bounds are stated in it, and the
        // operand's value is compared against them in it. `sourceUnit` is
        // where a second unit that is not the step's own already goes -- see
        // its comment -- so it goes there rather than into a parallel field.
        // The exact lookup has none: its key is a discriminator, not a
        // quantity.
        if constexpr (requires { N::keyUnit; })
            nodeStep.sourceUnit = N::keyUnit;
        if constexpr (requires { N::exponent; })
            nodeStep.exponent = N::exponent;
        else if constexpr (requires { N::degree; })
            nodeStep.exponent = N::degree;

        if constexpr (requires { N::places; })
            nodeStep.granularity = N::places.value;
        else if constexpr (requires { N::digits; })
            nodeStep.granularity = N::digits.value;

        // `RoundNode`, `RoundSignificantNode` and `RoundedRootNode` are the
        // only kinds that declare one, so the `requires` alone selects them --
        // the same shape `exponent` and `granularity` above use.
        if constexpr (requires { N::mode; })
            nodeStep.mode = N::mode;

        if constexpr (detail::StepKindOf<N>::value == StepKind::Conditional)
        {
            // The predicate's comparison, taken off the node this sink was
            // handed. `PredicateNode::comparison` is a public
            // `static constexpr`, so naming it through the member is a
            // constant expression and `WhenNode` needs no re-export of its
            // own -- the same way `citation` above is read straight off a
            // `DocumentedNode`.
            nodeStep.comparison = std::remove_cvref_t<decltype(node.predicate)>::comparison;
            nodeStep.branch = _trace->branchStack.back();
            _trace->branchStack.pop_back();
        }

        if (!result.has_value())
            nodeStep.error = result.error();
        else if (result->has_value())
            nodeStep.value = **result;

        // Everything unclaimed from `nodeMark` onwards belongs to this node.
        auto firstClaimed = _trace->unclaimed.begin();
        while (firstClaimed != _trace->unclaimed.end() && *firstClaimed < nodeMark)
            ++firstClaimed;
        nodeStep.operands.assign(firstClaimed, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstClaimed, _trace->unclaimed.end());

        // A documented expression and a jurisdiction's replacement compute
        // nothing of their own: the value is their operand's, so it is stated
        // in the unit that operand's line states it in -- grams under a
        // citation over grams, not the same number rescaled into kilograms
        // and printed with no unit at all. Read off the operand's step rather
        // than off `N`, for the reason `variant_produced` gives: that step
        // already says what the number is, including for a variable, whose
        // unit is not a member of its node.
        //
        // Only when that step is provably the wrapped node's own. The wrapped
        // type must be a kind this sink records a step for: a consumer's node
        // that forwards the sink records none, and what is claimed here is
        // then its operands -- a Celsius reading under a temperature rise, or
        // a volume under a density -- whose unit would state the value as
        // something it is not. And, at run time, exactly one step was claimed
        // and it is of this step's dimension. Otherwise the coherent SI unit
        // set above stands, as for anything else computed.
        if constexpr (detail::PassesThroughRecordedStep<N>)
            if (nodeStep.operands.size() == 1 && _trace->steps[nodeStep.operands.front()].dimension == N::dimension)
                nodeStep.unit = _trace->steps[nodeStep.operands.front()].unit;

        // A sum, a mean and a range read in their series' unit, which only
        // the claimed operand step knows.
        if constexpr (detail::StepKindOf<N>::value == StepKind::SeriesSum
                      || detail::StepKindOf<N>::value == StepKind::SampleMean
                      || detail::StepKindOf<N>::value == StepKind::SampleRange)
            nodeStep.unit = detail::operand_unit_or(_trace->steps, nodeStep.operands, nodeStep.dimension, nodeStep.unit);

        // A read from another record is its operand's value, unchanged, so it
        // reads in the unit its operand's line does: `4 MPa` after a variable
        // or a rounding in MPa, and the coherent unit after a computation --
        // never the same value in two scales on consecutive lines. The operand
        // is the last step claimed that is not a lineage attribute; a scope
        // over an unbound record claims none, reads nothing, and keeps the
        // coherent unit.
        if constexpr (detail::StepKindOf<N>::value == StepKind::RecordScope)
            for (std::size_t const claimed: nodeStep.operands)
                if (_trace->steps[claimed].kind != StepKind::LineageChecked)
                    nodeStep.unit = _trace->steps[claimed].unit;

        // After the operands are claimed, and not before: telling this
        // lookup's own failure apart from one it is merely relaying means
        // reading the operand step it just claimed, so the claim has to have
        // happened. See `LookupFailure` for the ambiguity this closes, and
        // `detail::record_lookup` for how each kind closes it.
        if constexpr (detail::is_lookup(detail::StepKindOf<N>::value))
            detail::record_lookup(node, nodeStep, _trace->steps);
        if constexpr (detail::StepKindOf<N>::value == StepKind::SnappedToPermitted)
            detail::record_snap(node, nodeStep, _trace->steps);
        if constexpr (detail::StepKindOf<N>::value == StepKind::CurveInterpolation)
            detail::record_curve_interpolation(node, nodeStep, _trace->steps);
        // An opaque output is shown in the unit its call's step shows it in:
        // the call's step is the one this node claimed.
        if constexpr (detail::StepKindOf<N>::value == StepKind::OpaqueOutput)
            if (nodeStep.operands.size() == 1)
                if (OpaqueStepData<Rep> const* const callRow = opaque_data(*_trace, nodeStep.operands.front());
                    callRow != nullptr && N::index < callRow->outputs.size()
                    && callRow->outputs[N::index].unit.dimension == nodeStep.dimension)
                    nodeStep.unit = callRow->outputs[N::index].unit;

        if constexpr (detail::StepKindOf<N>::value == StepKind::SampleSizeLookup)
            _trace->sampleSizeRecords.push_back(detail::SampleSizeRecord {
                .step = _trace->steps.size(), .declaredSizes = detail::SampleSizeList<N::sizes>::view() });

        // A placeholder belongs to the innermost bound limit, which learns of
        // it here and names itself on it once it is recorded. A limit names
        // itself on its level step and on every placeholder read under it.
        if constexpr (detail::StepKindOf<N>::value == StepKind::PrecisionLevel)
        {
            if (!_trace->precisionBindings.empty())
            {
                detail::PrecisionBinding& binding = _trace->precisionBindings.back();
                binding.pendingRecords.push_back(_trace->precisionRecords.size());
                _trace->precisionRecords.push_back(detail::PrecisionRecord { .step = _trace->steps.size(),
                                                                             .kind = binding.kind,
                                                                             .role = detail::PrecisionStepRole::Placeholder,
                                                                             .levelStep = binding.levelStep });
            }
        }
        if constexpr (detail::StepKindOf<N>::value == StepKind::PrecisionLimit)
        {
            if (!_trace->precisionBindings.empty())
            {
                detail::PrecisionBinding const& binding = _trace->precisionBindings.back();
                std::size_t const limitIndex = _trace->steps.size();
                for (std::size_t const recordIndex: binding.pendingRecords)
                    _trace->precisionRecords[recordIndex].limitStep = limitIndex;
                _trace->precisionRecords.push_back(detail::PrecisionRecord { .step = limitIndex,
                                                                             .kind = N::kind,
                                                                             .role = detail::PrecisionStepRole::LimitPass,
                                                                             .levelStep = binding.levelStep,
                                                                             .limitStep = limitIndex });
            }
        }

        // Every step inside a scope, the scope's own included, says which
        // record it was read from; the scope's own step then closes it. The
        // pop is guarded: a consumer's own evaluator that reports a scope
        // without `record_entered` leaves nothing to pop, and popping an
        // empty vector would be undefined behaviour -- an abort under a
        // checked standard library.
        stamp_origin(nodeStep);
        if constexpr (detail::StepKindOf<N>::value == StepKind::RecordScope)
            if (!_trace->recordStack.empty())
                _trace->recordStack.pop_back();

        _trace->steps.push_back(std::move(nodeStep));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
        if constexpr (detail::StepKindOf<N>::value == StepKind::OpaqueOutput)
            _trace->opaqueOutputSteps.push_back(
                OpaqueOutputStepData { .step = _trace->steps.size() - 1, .outputIndex = N::index });
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
        // Told what a walk produced without having been told it began -- a
        // consumer's own evaluator that skipped the matching `constraint_entered`, or
        // a second sink that cleared the bookkeeping mid-walk. There is no
        // mark to claim from, and reading one off an empty stack is undefined
        // behaviour (cl's debug library aborts), so the step is dropped.
        if (_trace->marks.empty())
            return;
        std::size_t const constraintMark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> constraintStep {};
        constraintStep.kind = StepKind::Constraint;
        // Read straight off the constraint's own predicate, the same way
        // `produced` above reads a `Conditional` step's comparison off
        // `node.predicate` -- `PredicateNode::comparison` is a public
        // `static constexpr`, so this needs no member added to `Constraint`
        // to expose it.
        constraintStep.comparison = std::remove_cvref_t<decltype(constraint.predicate)>::comparison;
        constraintStep.outcome = outcome;

        // Everything unclaimed from `constraintMark` onwards belongs to this
        // constraint -- see `produced` above for why this is a `while`
        // rather than an index computed from `constraintMark` directly.
        auto firstClaimed = _trace->unclaimed.begin();
        while (firstClaimed != _trace->unclaimed.end() && *firstClaimed < constraintMark)
            ++firstClaimed;
        constraintStep.operands.assign(firstClaimed, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstClaimed, _trace->unclaimed.end());

        stamp_origin(constraintStep);
        _trace->steps.push_back(std::move(constraintStep));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
    }

    /// Told that a method is about to evaluate the variant it selected.
    /// Remembers where the arena stood, exactly as `entered` does for a
    /// `Node`, so that `variant_produced` below can claim the variant's own
    /// step as its operand.
    ///
    /// A method is not a `Node`, so it cannot come through `entered`; it
    /// comes through this pair instead, which `evaluate_method`
    /// (`method.hpp`) calls when a sink defines both -- see
    /// `VariantSelection` (`sink.hpp`).
    void variant_entered(VariantSelection const&)
    {
        _trace->marks.push_back(_trace->steps.size());
    }

    /// Records a `StepKind::VariantSelected` step for @p variantSelection, claiming
    /// as its operand the step the selected variant produced -- so the
    /// selection is the walk's root, and the formula that ran sits under it.
    ///
    /// The step's value is what the method returned, which is exactly what
    /// the variant produced; its dimension and unit are copied from the
    /// variant's step rather than passed in, since that step already says
    /// what the number is, and a second source for the same facts would be
    /// one obliged to agree with it. Absent an operand step -- which no method in this library leaves,
    /// since the rounding node it wraps is always traced -- the step records
    /// the selection and its error, if any, and no value it could not state
    /// the unit of.
    void variant_produced(VariantSelection const& variantSelection, Evaluated<Rep> const& produced)
    {
        // Told what a walk produced without having been told it began -- a
        // consumer's own evaluator that skipped the matching `variant_entered`, or
        // a second sink that cleared the bookkeeping mid-walk. There is no
        // mark to claim from, and reading one off an empty stack is undefined
        // behaviour (cl's debug library aborts), so the step is dropped.
        if (_trace->marks.empty())
            return;
        std::size_t const selectionMark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> selectionStep {};
        selectionStep.kind = StepKind::VariantSelected;
        selectionStep.variantTag = variantSelection.tag;
        selectionStep.variantIndex = variantSelection.index;
        selectionStep.variantCount = variantSelection.count;
        selectionStep.variantPrunedCount = variantSelection.prunedCount;
        if (variantSelection.prunedBy != nullptr)
            selectionStep.variantPrunedBy = *variantSelection.prunedBy;
        selectionStep.variantPinned = variantSelection.pinnedBy != nullptr;
        if (variantSelection.pinnedBy != nullptr)
            selectionStep.citation = *variantSelection.pinnedBy;

        // Everything unclaimed from `selectionMark` onwards belongs to this selection
        // -- see `produced` above for why this is a `while`.
        auto firstClaimed = _trace->unclaimed.begin();
        while (firstClaimed != _trace->unclaimed.end() && *firstClaimed < selectionMark)
            ++firstClaimed;
        selectionStep.operands.assign(firstClaimed, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstClaimed, _trace->unclaimed.end());

        if (!selectionStep.operands.empty())
        {
            Step<Rep> const& variant = _trace->steps[selectionStep.operands.back()];
            selectionStep.dimension = variant.dimension;
            selectionStep.unit = variant.unit;
        }
        if (!produced.has_value())
            selectionStep.error = produced.error();
        else if (produced->has_value() && !selectionStep.operands.empty())
            selectionStep.value = **produced;

        stamp_origin(selectionStep);
        _trace->steps.push_back(std::move(selectionStep));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
    }

    /// Told that a method is about to check its constraints. Remembers where
    /// the arena stood, exactly as `entered` does for a `Node`, so that
    /// `acceptance_produced` below can claim the verdicts as its operands.
    ///
    /// Called by `check_method` (`method.hpp`) when a sink defines both this
    /// and `acceptance_produced`, as `evaluate_method` calls
    /// `variant_entered` and `variant_produced`.
    void acceptance_entered(ConstraintOrigin const&)
    {
        _trace->marks.push_back(_trace->steps.size());
    }

    /// Told that a rejection is about to evaluate its sample: remembers where
    /// the arena stood, so that its terminal step can claim every step of it.
    void rejection_entered()
    {
        _trace->rejectionsInProgress.push_back(detail::RejectionInProgress { .mark = _trace->steps.size() });
    }

    /// Told that a pass is about to evaluate its limit: remembers where the
    /// arena stood, and, the first time, which step is the sample's own.
    void rejection_pass_entered()
    {
        if (!_trace->rejectionsInProgress.empty() && !_trace->rejectionsInProgress.back().sampleStep.has_value()
            && !_trace->steps.empty())
            _trace->rejectionsInProgress.back().sampleStep = _trace->steps.size() - 1;
        _trace->marks.push_back(_trace->steps.size());
    }

    /// Records a `RejectionPass` step, claiming the limit expression's steps.
    void rejection_pass_produced(detail::RejectionPassEvent const& event)
    {
        if (_trace->marks.empty())
            return;
        std::size_t const passMark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> passStep = rejection_step(StepKind::RejectionPass);
        auto firstClaimed = _trace->unclaimed.begin();
        while (firstClaimed != _trace->unclaimed.end() && *firstClaimed < passMark)
            ++firstClaimed;
        passStep.operands.assign(firstClaimed, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstClaimed, _trace->unclaimed.end());
        if (event.error.has_value())
            passStep.error = event.error;
        else if (event.passMean.has_value())
            passStep.value = *event.passMean;

        detail::RejectionRecord<Rep> rejectionRecord {};
        rejectionRecord.pass = event.pass;
        rejectionRecord.sampleSize = event.sampleSize;
        rejectionRecord.originalSize = event.originalSize;
        push_rejection_step(std::move(passStep), std::move(rejectionRecord));
    }

    /// Records an `OutlierRejected` step: every number the decision used.
    void outlier_rejected(detail::OutlierEvent const& event)
    {
        Step<Rep> rejectedStep = rejection_step(StepKind::OutlierRejected);
        detail::RejectionRecord<Rep> rejectionRecord {};
        rejectionRecord.pass = event.pass;
        rejectionRecord.originalSize = event.originalSize;
        rejectionRecord.position = event.position;
        rejectionRecord.rejectedValue = event.rejectedValue;
        rejectionRecord.statistic = event.statistic;
        rejectionRecord.limit = event.limit;
        rejectionRecord.squared = event.squared;
        rejectionRecord.criterion = event.criterion;
        rejectionRecord.onLimit = event.onLimit;
        push_rejection_step(std::move(rejectedStep), std::move(rejectionRecord));
    }

    /// Records the rejection's terminal step -- `RejectionSettled`,
    /// `RejectionAborted`, `RejectionFailed` or `RejectionUndecided` --
    /// claiming every step of the rejection, the sample's own first, so that
    /// whatever reads the rejection has one operand. A rejection that ended
    /// before its first pass -- its sample failed, or held an absent
    /// determination -- records none: the sample's own step, which says why,
    /// is then the reader's operand.
    void rejection_finished(detail::RejectionEndEvent const& event)
    {
        if (_trace->rejectionsInProgress.empty())
            return;
        detail::RejectionInProgress const inProgress = _trace->rejectionsInProgress.back();
        if (event.pass == 0 && !event.startedShort)
        {
            _trace->rejectionsInProgress.pop_back();
            return;
        }

        StepKind const endKind = event.end == detail::RejectionEnd::Settled   ? StepKind::RejectionSettled
                                 : event.end == detail::RejectionEnd::Aborted ? StepKind::RejectionAborted
                                 : event.end == detail::RejectionEnd::Failed  ? StepKind::RejectionFailed
                                                                              : StepKind::RejectionUndecided;
        Step<Rep> endStep = rejection_step(endKind);
        if (event.error.has_value())
            endStep.error = event.error;
        auto firstClaimed = _trace->unclaimed.begin();
        while (firstClaimed != _trace->unclaimed.end() && *firstClaimed < inProgress.mark)
            ++firstClaimed;
        endStep.operands.assign(firstClaimed, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstClaimed, _trace->unclaimed.end());

        detail::RejectionRecord<Rep> rejectionRecord {};
        rejectionRecord.pass = event.pass;
        rejectionRecord.originalSize = event.originalSize;
        rejectionRecord.rejectedCount = event.rejectedCount;
        rejectionRecord.remaining = event.remaining;
        rejectionRecord.wouldReject.assign(event.wouldReject.begin(), event.wouldReject.end());
        rejectionRecord.pastAtMost = event.pastAtMost;
        rejectionRecord.belowKeepAtLeast = event.belowKeepAtLeast;
        rejectionRecord.startedShort = event.startedShort;
        rejectionRecord.failurePoint = event.failurePoint;
        rejectionRecord.position = event.failedPosition;
        rejectionRecord.atMost = event.atMost;
        rejectionRecord.keepAtLeast = event.keepAtLeast;
        rejectionRecord.verdict = event.verdict;
        rejectionRecord.citation = event.citation;
        push_rejection_step(std::move(endStep), std::move(rejectionRecord));
        _trace->rejectionsInProgress.pop_back();
    }

    /// Told the zero-based position of the determination at which the
    /// statistic just produced failed -- a mean or a variance whose total
    /// overflowed there. Amends that step, the last recorded: the scalar
    /// channel carries only the error, and the position survives here, as a
    /// series step's does (phase 12's S8).
    ///
    /// **Amends nothing else.** The last step must be a failed `SampleMean`
    /// or `SampleVariance`; any other step, a present one included, is left
    /// alone. The hook is public, as every sink hook is, so code of a
    /// consumer's -- a node of its own, calling it during a recorded
    /// evaluation -- can reach it: what it can still do is name another
    /// position on a failed statistic, which the renderer checks against
    /// the statistic's sample (`(no such element)` beyond its end).
    void sample_failed_at(std::size_t at)
    {
        if (_trace->steps.empty())
            return;
        Step<Rep>& failed = _trace->steps.back();
        if ((failed.kind == StepKind::SampleMean || failed.kind == StepKind::SampleVariance) && failed.error.has_value())
            failed.failedElement = at;
    }

    /// Told that a precision limit is about to be evaluated. Remembers
    /// nothing: the limit's own `entered` marks the arena, and its level is
    /// not bound until pass 1 has produced it (`precision_level_produced`).
    void precision_limit_entered(PrecisionKind) noexcept {}

    /// Told that a precision limit has produced its value: its binding ends,
    /// and a placeholder read from now on belongs to an enclosing limit, if
    /// any.
    void precision_limit_produced(PrecisionKind, Evaluated<Rep> const&) noexcept
    {
        if (!_trace->precisionBindings.empty())
            _trace->precisionBindings.pop_back();
    }

    /// Told that pass 1 is about to evaluate a precision limit's level.
    /// Remembers where the arena stood, as `entered` does for a `Node`.
    void precision_level_entered(PrecisionKind)
    {
        _trace->marks.push_back(_trace->steps.size());
    }

    /// Records the pass-1 step, claiming the level expression's step as its
    /// operand, and binds the level for the limit expression that follows.
    ///
    /// Its value is in @p levelUnit, the unit of the quantity the limit's
    /// placeholders name, so that the level reads as the results do; its
    /// dimension is the level expression's.
    void precision_level_produced(PrecisionKind precisionKind, Unit levelUnit, Evaluated<Rep> const& produced)
    {
        // Told without `precision_level_entered`, or after a second sink
        // cleared the bookkeeping: as for `produced`, there is no mark to
        // claim from, and reading one off an empty stack is undefined
        // behaviour (cl's debug library aborts), so the step is dropped.
        if (_trace->marks.empty())
            return;
        std::size_t const levelMark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> levelStep {};
        levelStep.kind = StepKind::PrecisionLevel;
        levelStep.dimension = levelUnit.dimension;
        levelStep.unit = levelUnit;

        auto firstClaimed = _trace->unclaimed.begin();
        while (firstClaimed != _trace->unclaimed.end() && *firstClaimed < levelMark)
            ++firstClaimed;
        levelStep.operands.assign(firstClaimed, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstClaimed, _trace->unclaimed.end());

        if (!produced.has_value())
            levelStep.error = produced.error();
        else if (produced->has_value())
            levelStep.value = **produced;

        std::size_t const levelIndex = _trace->steps.size();
        std::size_t const recordIndex = _trace->precisionRecords.size();
        _trace->precisionRecords.push_back(detail::PrecisionRecord { .step = levelIndex,
                                                                     .kind = precisionKind,
                                                                     .role = detail::PrecisionStepRole::LevelPass,
                                                                     .levelStep = levelIndex });
        _trace->precisionBindings.push_back(
            detail::PrecisionBinding { .kind = precisionKind, .levelStep = levelIndex, .pendingRecords = { recordIndex } });

        stamp_origin(levelStep);
        _trace->steps.push_back(std::move(levelStep));
        _trace->unclaimed.push_back(levelIndex);
    }

    /// Records a `StepKind::AcceptanceChecked` step for constraints of
    /// @p constraintOrigin, claiming as its operands every verdict recorded since the
    /// matching `acceptance_entered`, and marks each of those verdicts with
    /// whose it was and what the overlay cited.
    ///
    /// Marked here, after the checks, rather than as each verdict is recorded:
    /// the verdicts claimed are exactly the constraints `check_method`
    /// checked, so no second record of which method is in progress is kept
    /// beside `marks`. A verdict recorded by `check` or `check_all` outside
    /// any method is claimed by nothing here, and keeps no provenance.
    void acceptance_produced(ConstraintOrigin const& constraintOrigin)
    {
        // Told what a walk produced without having been told it began -- a
        // consumer's own evaluator that skipped the matching `acceptance_entered`, or
        // a second sink that cleared the bookkeeping mid-walk. There is no
        // mark to claim from, and reading one off an empty stack is undefined
        // behaviour (cl's debug library aborts), so the step is dropped.
        if (_trace->marks.empty())
            return;
        std::size_t const acceptanceMark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> acceptanceStep {};
        acceptanceStep.kind = StepKind::AcceptanceChecked;
        acceptanceStep.constraintProvenance = constraintOrigin.provenance();
        acceptanceStep.citation = constraintOrigin.source();

        // Everything unclaimed from `acceptanceMark` onwards belongs to this method's
        // constraints -- see `produced` above for why this is a `while`.
        auto firstVerdict = _trace->unclaimed.begin();
        while (firstVerdict != _trace->unclaimed.end() && *firstVerdict < acceptanceMark)
            ++firstVerdict;
        acceptanceStep.operands.assign(firstVerdict, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstVerdict, _trace->unclaimed.end());

        for (std::size_t const verdictStep: acceptanceStep.operands)
        {
            _trace->steps[verdictStep].constraintProvenance = constraintOrigin.provenance();
            _trace->steps[verdictStep].citation = constraintOrigin.source();
        }

        stamp_origin(acceptanceStep);
        _trace->steps.push_back(std::move(acceptanceStep));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
    }

    /// Told that a series node is about to be evaluated. Remembers where the
    /// arena stood, exactly as `entered` does for a `Node`, so that
    /// `series_produced` below can claim the steps beneath it.
    ///
    /// A series is not a `Node`, so it comes through this pair rather than
    /// through `entered` and `produced` -- see `NullSink` (`sink.hpp`).
    template <SeriesNode S>
    void series_entered(S const&)
    {
        _trace->marks.push_back(_trace->steps.size());
        _trace->pendingInputSource.reset();
    }

    /// Told, by a series variable's evaluator, whether the series it read was
    /// measured or typed in; `series_produced` moves it onto the
    /// `SeriesVariable` step, in `Step::inputSource`, as `produced` does for a
    /// single value. The same single slot: nothing is entered between a series
    /// variable's own `series_entered` and `series_produced`.
    ///
    /// Optional, and public, for the reasons `input_source` gives, with the
    /// same boundary: `series_entered` empties the slot, and `series_produced`
    /// empties it for every kind.
    template <Described Q, std::size_t N>
    void series_input_source(SeriesVarNode<Q, N> const&, ValueSource source) noexcept
    {
        _trace->pendingInputSource = source;
    }

    /// Records one step for the whole series @p node -- however long it is --
    /// carrying every element in `Step::elements`, and claims as its operands
    /// every step recorded since the matching `series_entered`.
    ///
    /// A series variable names its quantity, so its symbol is written here
    /// through the vocabulary this sink was given, and its unit is the one
    /// the quantity is declared in, for the renderer to convert each element
    /// back to. A failure records its error and the element it arose at, and
    /// no elements.
    template <SeriesNode S>
    void series_produced(S const&, EvaluatedSeries<Rep, S::length> const& result)
    {
        std::size_t const seriesMark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> seriesStep {};
        seriesStep.kind = detail::SeriesStepKindOf<S>::value;
        seriesStep.dimension = S::dimension;
        seriesStep.unit = coherent(S::dimension);
        if constexpr (detail::SeriesStepKindOf<S>::value == StepKind::SeriesVariable)
        {
            seriesStep.unit = Describe<typename S::quantity>::unit;
            seriesStep.symbol = symbol_of<typename S::quantity>(_vocabulary);
            seriesStep.inputSource = _trace->pendingInputSource;
        }
        // A per-element constant is shown in the unit it was written in; a
        // computed series has no declared unit, as a computed scalar has
        // none, and keeps the coherent one.
        else if constexpr (detail::SeriesStepKindOf<S>::value == StepKind::SeriesConstant
                           || detail::SeriesStepKindOf<S>::value == StepKind::SeriesDomain)
            seriesStep.unit = S::unit;
        // A per-element rounding, like a scalar one, is shown in the unit it
        // rounded in -- the fact a reader checks each granularity against --
        // with every element's granularity and the one mode.
        else if constexpr (detail::SeriesStepKindOf<S>::value == StepKind::ElementwiseRound)
        {
            seriesStep.unit = S::unit;
            seriesStep.mode = S::mode;
            // Never instantiated for places already refused (`countMatches`):
            // the evaluator tells no sink then, so this loop only ever runs
            // over a table.
            for (DecimalPlaces const elementPlaces: S::places)
                seriesStep.elementGranularities.push_back(elementPlaces.value);
        }
        // Only a series variable reads an input; the slot is emptied for
        // every kind, as `produced` empties it.
        _trace->pendingInputSource.reset();

        // Everything unclaimed from `seriesMark` onwards belongs to this
        // series -- see `produced` above for why this is a `while`.
        auto firstClaimed = _trace->unclaimed.begin();
        while (firstClaimed != _trace->unclaimed.end() && *firstClaimed < seriesMark)
            ++firstClaimed;
        seriesStep.operands.assign(firstClaimed, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstClaimed, _trace->unclaimed.end());

        // A running total reads in its operand's unit, and says which end it
        // ran from.
        if constexpr (detail::SeriesStepKindOf<S>::value == StepKind::CumulativeSum)
        {
            seriesStep.unit =
                detail::operand_unit_or(_trace->steps, seriesStep.operands, seriesStep.dimension, seriesStep.unit);
            seriesStep.cumulativeDirection = S::direction;
        }
        // A binning names its classes' unit and extent, and keeps the
        // observations it binned, so that a miss can state the one that fit
        // no class.
        else if constexpr (detail::SeriesStepKindOf<S>::value == StepKind::Binning)
            detail::record_binning<S>(seriesStep, _trace->steps);

        if (!result.has_value())
        {
            seriesStep.error = result.error().error;
            seriesStep.failedElement = result.error().element;
            seriesStep.failureSite = result.error().site;
        }
        else
            seriesStep.elements.assign(result->elements.begin(), result->elements.end());

        stamp_origin(seriesStep);
        _trace->steps.push_back(std::move(seriesStep));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
    }

    /// Records one step for raw observations, carrying every observation made
    /// in `Step::elements`, in the coherent SI unit, shown in the unit the
    /// quantity is declared in under the symbol this sink's vocabulary gives
    /// it. A failure records its error and the observation it arose at, and
    /// no observations. Observations are a leaf: the step claims nothing.
    template <Described Q, std::size_t Capacity>
    void observations_produced(ObservationsVarNode<Q, Capacity> const&, EvaluatedObservations<Rep, Capacity> const& result)
    {
        Step<Rep> observationsStep {};
        observationsStep.kind = StepKind::ObservationsVariable;
        observationsStep.dimension = Describe<Q>::dimension;
        observationsStep.unit = Describe<Q>::unit;
        observationsStep.symbol = symbol_of<Q>(_vocabulary);
        if (!result.has_value())
        {
            observationsStep.error = result.error().error;
            observationsStep.failedElement = result.error().element;
            observationsStep.failureSite = result.error().site;
        }
        else
            for (std::size_t at = 0; at < result->count; ++at)
                observationsStep.elements.push_back(result->elements[at]);

        stamp_origin(observationsStep);
        _trace->steps.push_back(std::move(observationsStep));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
    }

    /// Told that a curve is about to be evaluated. Remembers where the arena
    /// stood, as `series_entered` does.
    template <CurveExpression C>
    void curve_entered(C const&)
    {
        _trace->marks.push_back(_trace->steps.size());
    }

    /// Records one step for the whole curve -- a pairing or a splice --
    /// carrying every point in `Step::domainElements` and every value in
    /// `Step::elements`, and claims as its operands every step recorded since
    /// the matching `curve_entered`. The points are shown in the unit of the
    /// step that supplied them and the values likewise -- a pairing's two
    /// series, a splice's first curve -- or in the coherent unit when that
    /// step's is of another dimension. A failure records its error and the
    /// element it arose at, and neither points nor values.
    template <CurveExpression C>
    void curve_produced(C const&, EvaluatedCurve<Rep, C::length> const& result)
    {
        std::size_t const curveMark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> curveStep {};
        curveStep.kind = detail::CurveStepKindOf<C>::value;
        curveStep.dimension = C::dimension;
        curveStep.unit = coherent(C::dimension);
        curveStep.sourceUnit = coherent(C::domainDimension);
        if constexpr (detail::CurveStepKindOf<C>::value == StepKind::CurveSplice)
            curveStep.monotone = C::monotone;

        // Everything unclaimed from `curveMark` onwards belongs to this curve
        // -- see `produced` above for why this is a `while`.
        auto firstClaimed = _trace->unclaimed.begin();
        while (firstClaimed != _trace->unclaimed.end() && *firstClaimed < curveMark)
            ++firstClaimed;
        curveStep.operands.assign(firstClaimed, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstClaimed, _trace->unclaimed.end());

        if (!curveStep.operands.empty())
        {
            Step<Rep> const& firstOperand = _trace->steps[curveStep.operands.front()];
            Step<Rep> const& lastOperand = _trace->steps[curveStep.operands.back()];
            if constexpr (detail::CurveStepKindOf<C>::value == StepKind::CurvePairing)
            {
                if (firstOperand.unit.dimension == C::domainDimension)
                    curveStep.sourceUnit = firstOperand.unit;
                if (lastOperand.unit.dimension == C::dimension)
                    curveStep.unit = lastOperand.unit;
            }
            else
            {
                if (firstOperand.sourceUnit.dimension == C::domainDimension)
                    curveStep.sourceUnit = firstOperand.sourceUnit;
                if (firstOperand.unit.dimension == C::dimension)
                    curveStep.unit = firstOperand.unit;
            }
        }

        if (!result.has_value())
        {
            curveStep.error = result.error().error;
            curveStep.failedElement = result.error().element;
            if constexpr (std::is_same_v<Rep, Rational>)
                if (curveStep.failedElement.has_value())
                    detail::record_curve_break<C>(curveStep, _trace->steps);
        }
        else
        {
            curveStep.domainElements.assign(result->domain.begin(), result->domain.end());
            curveStep.elements.assign(result->values.begin(), result->values.end());
        }

        stamp_origin(curveStep);
        _trace->steps.push_back(std::move(curveStep));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
    }

    /// Told that a conformity check is about to judge its subject. Remembers
    /// where the arena stood, as `series_entered` does, so that
    /// `conformity_produced` can claim the subject's step.
    template <Unit U, SeriesNode S>
    void conformity_entered(Conformity<U, S> const&)
    {
        _trace->marks.push_back(_trace->steps.size());
    }

    /// Records one step for the whole check, with every element's outcome in
    /// `Step::elementOutcomes`, claiming as its operand the subject's step,
    /// and the rows it judged against in `Trace::conformityLimits`.
    template <Unit U, SeriesNode S>
    void conformity_produced(Conformity<U, S> const& conformityCheck,
                             std::array<ConstraintOutcome, S::length> const& outcomes)
    {
        std::size_t const conformityMark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> conformityStep {};
        conformityStep.kind = StepKind::ConformityChecked;
        conformityStep.dimension = S::dimension;
        conformityStep.unit = U;
        conformityStep.elementOutcomes.assign(outcomes.begin(), outcomes.end());

        // Everything unclaimed from `conformityMark` onwards belongs to this
        // check -- see `produced` above for why this is a `while`.
        auto firstClaimed = _trace->unclaimed.begin();
        while (firstClaimed != _trace->unclaimed.end() && *firstClaimed < conformityMark)
            ++firstClaimed;
        conformityStep.operands.assign(firstClaimed, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstClaimed, _trace->unclaimed.end());
        // The subject's step is the last one claimed: its elements are the
        // values the check judged.
        if (!conformityStep.operands.empty())
            conformityStep.elements = _trace->steps[conformityStep.operands.back()].elements;

        stamp_origin(conformityStep);
        _trace->steps.push_back(std::move(conformityStep));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
        _trace->conformityLimits.push_back(ConformityLimits {
            .step = _trace->steps.size() - 1,
            .rows = std::vector<LimitRow>(conformityCheck.envelope.rows.begin(), conformityCheck.envelope.rows.end()) });
    }

    /// Told that an opaque call is about to evaluate its inputs. Remembers
    /// where the arena stood, as `series_entered` does, so that
    /// `opaque_produced` can claim the inputs' steps.
    void opaque_entered(OpaqueCallInfo const&)
    {
        _trace->marks.push_back(_trace->steps.size());
    }

    /// Records one `OpaqueOperation` step for the call @p callInfo describes,
    /// claiming as its operands every step recorded since the matching
    /// `opaque_entered` -- its inputs' -- and its operation's name, every
    /// output and whose failure it carries in `Trace::opaqueSteps`.
    ///
    /// Whose failure is the evaluation's own answer (`OpaqueCallFailure::origin`),
    /// never re-derived, with one exception: a relayed failure that no claimed
    /// input step shows -- an input evaluated through a consumer's untraced
    /// node -- is `Undetermined`, as `LookupFailure` records the same case.
    template <std::size_t M>
    void opaque_produced(OpaqueCallInfo const& callInfo, OpaqueEvaluated<Rep, M> const& result)
    {
        // Told what a walk produced without having been told it began: see
        // `produced`.
        if (_trace->marks.empty())
            return;
        std::size_t const callMark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> callStep {};
        callStep.kind = StepKind::OpaqueOperation;
        callStep.dimension = dim::Scalar;
        callStep.unit = coherent(dim::Scalar);
        callStep.citation = callInfo.citation;

        // Everything unclaimed from `callMark` onwards belongs to this call
        // -- see `produced` above for why this is a `while`.
        auto firstClaimed = _trace->unclaimed.begin();
        while (firstClaimed != _trace->unclaimed.end() && *firstClaimed < callMark)
            ++firstClaimed;
        callStep.operands.assign(firstClaimed, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstClaimed, _trace->unclaimed.end());

        OpaqueStepData<Rep> callRow {};
        callRow.operationName = callInfo.name;
        for (std::size_t outputAt = 0;
             outputAt < M && outputAt < callInfo.outputs.size() && outputAt < callInfo.dimensions.size();
             ++outputAt)
        {
            OpaqueOutputValue<Rep> recordedOutput {};
            recordedOutput.name = callInfo.outputs[outputAt];
            recordedOutput.dimension = callInfo.dimensions[outputAt];
            recordedOutput.unit = detail::opaque_output_unit(_trace->steps, callStep.operands, recordedOutput.dimension);
            if (result.has_value() && result->has_value())
                recordedOutput.value = (**result)[outputAt];
            callRow.outputs.push_back(recordedOutput);
        }

        if (!result.has_value())
        {
            callStep.error = result.error().error;
            callStep.failedElement = result.error().element;
            callRow.failure = result.error().origin;
            if (callRow.failure == OpaqueFailure::Propagated && !detail::an_operand_failed(_trace->steps, callStep))
                callRow.failure = OpaqueFailure::Undetermined;
        }

        _trace->steps.push_back(std::move(callStep));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
        callRow.step = _trace->steps.size() - 1;
        _trace->opaqueSteps.push_back(std::move(callRow));
    }

    /// Told that a retry is about to evaluate its starting value. Remembers
    /// where the arena stood, so that `retry_produced` claims the starting
    /// value's step and every attempt's.
    void retry_entered(RetryInfo const&)
    {
        _trace->marks.push_back(_trace->steps.size());
    }

    /// Told that an attempt is about to run. Remembers where the arena stood,
    /// so that `attempt_produced` claims the attempt's derivation and its
    /// judgement's.
    void attempt_entered(AttemptInfo const&)
    {
        _trace->marks.push_back(_trace->steps.size());
    }

    /// Records one `RetryAttempt` step for the attempt @p attemptInfo
    /// describes, claiming the steps recorded since the matching
    /// `attempt_entered`: its value or its error, and how it was judged, in
    /// `Trace::attemptSteps`. A judgement that failed is `JudgementFailed`
    /// there, and its error is on the failing side's step: the attempt step
    /// keeps its value alone, so a step never holds both. Its unit and symbol are
    /// the retry's result's, set by `retry_produced`, which knows it.
    void attempt_produced(AttemptInfo const& attemptInfo,
                          Evaluated<Rep> const& produced,
                          AttemptJudgement judgement)
    {
        // Told what a walk produced without having been told it began: see
        // `produced`.
        if (_trace->marks.empty())
            return;
        std::size_t const attemptMark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> attemptStep {};
        attemptStep.kind = StepKind::RetryAttempt;
        attemptStep.comparison = attemptInfo.comparison;
        if (!produced.has_value())
            attemptStep.error = produced.error();
        if (produced.has_value() && produced->has_value())
            attemptStep.value = **produced;

        // Everything unclaimed from `attemptMark` onwards belongs to this
        // attempt -- see `produced` above for why this is a `while`.
        auto firstClaimed = _trace->unclaimed.begin();
        while (firstClaimed != _trace->unclaimed.end() && *firstClaimed < attemptMark)
            ++firstClaimed;
        attemptStep.operands.assign(firstClaimed, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstClaimed, _trace->unclaimed.end());

        _trace->steps.push_back(std::move(attemptStep));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
        _trace->attemptSteps.push_back(AttemptStepData {
            .step = _trace->steps.size() - 1, .attemptNumber = attemptInfo.attemptNumber, .judgement = judgement });
    }

    /// Records one `RetryConcluded` step for the retry @p retryInfo
    /// describes, claiming the starting value's step and every attempt's, from
    /// what `checked_evaluate_retry` returned, @p ended, and nothing else:
    /// how it ended, its value or its error, and its citation. Gives every
    /// attempt step it claims the result's dimension, unit and symbol.
    template <Described R>
    void retry_produced(RetryInfo const& retryInfo, std::expected<RetryOutcome<R>, RetryFailure> const& ended)
    {
        // Told what a walk produced without having been told it began: see
        // `produced`.
        if (_trace->marks.empty())
            return;
        std::size_t const retryMark = _trace->marks.back();
        _trace->marks.pop_back();

        Step<Rep> retryStep {};
        retryStep.kind = StepKind::RetryConcluded;
        retryStep.dimension = Describe<R>::dimension;
        retryStep.unit = Describe<R>::unit;
        retryStep.symbol = symbol_of<R>(_vocabulary);
        retryStep.citation = retryInfo.citation;

        RetryStepData retryRow { .attemptLimit = retryInfo.attemptLimit, .verdictLabel = retryInfo.verdictLabel };
        if (!ended.has_value())
        {
            retryStep.error = ended.error().error;
            retryRow.end = RetryEnd::Failed;
        }
        else
        {
            retryRow.end = ended->end();
            Measured<R> const measurement = ended->outcome().measurement();
            // The accepted value, back in the coherent unit every step holds.
            // Reversing a conversion that just succeeded, it should not fail;
            // if it did, the step states the failure, never "not measured"
            // for an accepted retry.
            if (ended->end() == RetryEnd::Accepted && measurement.has_value())
            {
                std::expected<Rational, ArithmeticError> const inCoherentUnit =
                    checked_convert(measurement.value(), Describe<R>::unit, coherent(Describe<R>::dimension));
                if (inCoherentUnit.has_value())
                    retryStep.value = *inCoherentUnit;
                else
                    retryStep.error = inCoherentUnit.error();
            }
        }

        auto firstClaimed = _trace->unclaimed.begin();
        while (firstClaimed != _trace->unclaimed.end() && *firstClaimed < retryMark)
            ++firstClaimed;
        retryStep.operands.assign(firstClaimed, _trace->unclaimed.end());
        _trace->unclaimed.erase(firstClaimed, _trace->unclaimed.end());

        for (std::size_t const claimed: retryStep.operands)
            if (_trace->steps[claimed].kind == StepKind::RetryAttempt)
            {
                _trace->steps[claimed].dimension = Describe<R>::dimension;
                _trace->steps[claimed].unit = Describe<R>::unit;
                _trace->steps[claimed].symbol = retryStep.symbol;
            }

        _trace->steps.push_back(std::move(retryStep));
        _trace->unclaimed.push_back(_trace->steps.size() - 1);
        retryRow.step = _trace->steps.size() - 1;
        _trace->retrySteps.push_back(retryRow);
    }

  private:
    /// A rejection step of @p stepKind, in the dimension and unit of the
    /// rejection's sample -- its own step's, once known -- so that a mean or
    /// a rejected value reads as the determinations do.
    [[nodiscard]] Step<Rep> rejection_step(StepKind stepKind) const
    {
        Step<Rep> rejectionStep {};
        rejectionStep.kind = stepKind;
        rejectionStep.dimension = dim::Scalar;
        rejectionStep.unit = unit::One;
        if (!_trace->rejectionsInProgress.empty())
        {
            std::optional<std::size_t> const sampleStep = _trace->rejectionsInProgress.back().sampleStep;
            if (sampleStep.has_value() && *sampleStep < _trace->steps.size())
            {
                rejectionStep.dimension = _trace->steps[*sampleStep].dimension;
                rejectionStep.unit = _trace->steps[*sampleStep].unit;
            }
        }
        return rejectionStep;
    }

    /// Appends @p rejectionStep, unclaimed, with @p rejectionRecord keyed to it,
    /// marked as over observations when the rejection's sample is.
    void push_rejection_step(Step<Rep> rejectionStep, detail::RejectionRecord<Rep> rejectionRecord)
    {
        // Before pass 1 no sample step is noted; nor does a line that early
        // name a position.
        if (!_trace->rejectionsInProgress.empty())
            if (std::optional<std::size_t> const sampleStep = _trace->rejectionsInProgress.back().sampleStep;
                sampleStep.has_value())
                rejectionRecord.ofObservations = detail::step_counts_observations(*_trace, *sampleStep);
        std::size_t const stepIndex = _trace->steps.size();
        rejectionRecord.step = stepIndex;
        _trace->rejectionRecords.push_back(std::move(rejectionRecord));
        stamp_origin(rejectionStep);
        _trace->steps.push_back(std::move(rejectionStep));
        _trace->unclaimed.push_back(stepIndex);
    }

    /// Stamps @p recorded with the origin of the scope still open, if one is:
    /// every step recorded between a scope's `record_entered` and its own
    /// step says which record it was read from. Called by every path that
    /// records a step -- a node's, a lineage attribute's, a series', a
    /// curve's, raw observations', a constraint's, a conformity check's, a
    /// variant selection's, an acceptance check's, a precision level's first
    /// pass and a rejection's passes, rejections and verdict -- so that a
    /// recording path added later, as phase 12's series paths and phase 13's
    /// statistics paths were, has one rule to follow rather than one to
    /// forget. Outside every scope it sets nothing.
    ///
    /// **The rule for every recording path, present and future:** a path
    /// that appends to `Trace::steps` calls this on its step first. A path
    /// that does not traces a value read inside a scope as this record's.
    void stamp_origin(Step<Rep>& recorded) const noexcept
    {
        if (!_trace->recordStack.empty())
            recorded.recordNumber = _trace->recordStack.back();
    }

    Trace<Rep>* _trace;
    FORMULA_NO_UNIQUE_ADDRESS V _vocabulary;
};

/// `RecordingSink { trace, vocabulary }` records in @p vocabulary's terms.
template <typename Rep, Vocabulary V>
RecordingSink(Trace<Rep>&, V) -> RecordingSink<Rep, V>;

/// The record @p recorded, a step of @p trace, was read from; empty for a
/// step of the record being evaluated, and for a number @p trace does not
/// hold.
template <typename Rep>
[[nodiscard]] constexpr std::optional<RecordOrigin> origin_of(Trace<Rep> const& trace, Step<Rep> const& recorded) noexcept
{
    if (recorded.recordNumber == 0 || recorded.recordNumber > trace.origins.size())
        return std::nullopt;
    return trace.origins[recorded.recordNumber - 1];
}

/// The comparison the `LineageChecked` step at @p stepIndex of @p trace
/// recorded; empty for any other step.
template <typename Rep>
[[nodiscard]] constexpr std::optional<LineageCheck> lineage_of(Trace<Rep> const& trace, std::size_t stepIndex) noexcept
{
    for (LineageRow const& kept: trace.lineageChecks)
        if (kept.step == stepIndex)
            return kept.check;
    return std::nullopt;
}

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
///
/// Every step naming a quantity writes its symbol as @p vocabulary says
/// (`vocabulary.hpp`) -- pass the one the page is rendered in, so that the
/// trace and the page agree.
template <Described Result, typename Rep = Rational, Node Expression, typename Env, Vocabulary V = DefaultVocabulary>
[[nodiscard]] Explained<Result, Rep> explain(Expression const& expression,
                                             Env const& environment,
                                             V const& vocabulary = V {})
{
    static_assert(std::is_same_v<Rep, Rational>,
                  "formula: explain only supports Rep = Rational -- evaluate<Result> always computes "
                  "in Rational internally and hands its sink an Evaluated<Rational>, so a "
                  "RecordingSink<Rep> built for a different Rep cannot receive it. Call "
                  "checked_evaluate_si<Rep> directly with your own RecordingSink<Rep> to trace a "
                  "double computation.");

    Explained<Result, Rep> explained {};
    RecordingSink<Rep, V> recordingSink { explained.trace, vocabulary };
    explained.outcome = evaluate<Result>(expression, environment, recordingSink);
    return explained;
}

/// A series outcome together with the derivation that produced it -- the
/// series counterpart of `Explained`.
///
/// `outcome` is what `checked_evaluate_series` returned, **failure included**:
/// a series has no throwing spelling, since an exception would drop the
/// position `SeriesFailure` carries (`series.hpp`), so unlike `Explained` this
/// cannot hold a bare outcome and throw the failure away.
template <Described Result, std::size_t N>
struct ExplainedSeries
{
    /// Exactly what `checked_evaluate_series<Result>` returned.
    std::expected<SeriesOutcome<Result, N>, SeriesFailure> outcome;
    /// How it was reached -- **empty** when the outcome is a typed-in series,
    /// which was not derived. See `explain`'s comment on the same case.
    Trace<Rational> trace {};
};

/// Evaluates the series @p expression for @p Result and records how, writing
/// every symbol as @p vocabulary says -- the series counterpart of `explain`.
///
/// The outcome is identical to `checked_evaluate_series<Result>(expression,
/// environment)`: tracing observes, it does not participate.
template <Described Result, SeriesNode S, typename Env, Vocabulary V = DefaultVocabulary>
[[nodiscard]] ExplainedSeries<Result, S::length> explain_series(S const& expression,
                                                                Env const& environment,
                                                                V const& vocabulary = V {})
{
    Trace<Rational> recorded {};
    std::expected<SeriesOutcome<Result, S::length>, SeriesFailure> seriesOutcome =
        checked_evaluate_series<Result>(expression, environment, RecordingSink<Rational, V> { recorded, vocabulary });
    return ExplainedSeries<Result, S::length> { std::move(seriesOutcome), std::move(recorded) };
}

/// Why `checked_explain` has no outcome: the arithmetic error, and the
/// derivation recorded up to it. A refusal without the steps that led to it
/// -- which attribute of a lineage requirement disagreed, say -- would say
/// that the number was refused and not why.
///
/// Not default-constructible: a failure with no error given would have to
/// claim one -- the enumeration's first, `DivisionByZero` -- that nothing
/// raised.
template <typename Rep = Rational>
struct CheckedExplainFailure
{
    /// The failure @p raised, with the trace @p recorded up to it.
    CheckedExplainFailure(ArithmeticError raised, Trace<Rep> recorded) noexcept:
        error { raised },
        trace { std::move(recorded) }
    {
    }

    /// What `checked_evaluate` returned instead of an outcome.
    ArithmeticError error;
    /// Every step recorded before the error, the failing step included.
    Trace<Rep> trace;
};

/// Evaluates @p expression for @p Result and records how, without throwing:
/// `explain`'s counterpart through `checked_evaluate` rather than the
/// throwing `evaluate`.
///
/// On success, exactly what `explain` returns. On an arithmetic error --
/// including a read another record's lineage refused -- the error together
/// with the trace recorded up to it, where `explain` would throw and keep
/// nothing. The same `Rep = Rational` restriction and vocabulary as
/// `explain`, for the same reasons.
template <Described Result, typename Rep = Rational, Node Expression, typename Env, Vocabulary V = DefaultVocabulary>
[[nodiscard]] std::expected<Explained<Result, Rep>, CheckedExplainFailure<Rep>>
checked_explain(Expression const& expression, Env const& environment, V const& vocabulary = V {})
{
    static_assert(std::is_same_v<Rep, Rational>,
                  "formula: checked_explain only supports Rep = Rational, for the reason explain gives -- call "
                  "checked_evaluate_si<Rep> directly with your own RecordingSink<Rep> to trace a double "
                  "computation.");

    Trace<Rep> recorded {};
    RecordingSink<Rep, V> recordingSink { recorded, vocabulary };
    std::expected<Outcome<Result>, ArithmeticError> const checked =
        checked_evaluate<Result>(expression, environment, recordingSink);
    if (!checked.has_value())
        return std::unexpected { CheckedExplainFailure<Rep> { checked.error(), std::move(recorded) } };
    return Explained<Result, Rep> { *checked, std::move(recorded) };
}

/// A retry's result together with every attempt that produced it.
template <Described R>
struct ExplainedRetry
{
    /// Exactly what `checked_evaluate_retry` returned, failure included.
    std::expected<RetryOutcome<R>, RetryFailure> outcome;
    /// How it was reached: the starting value, every attempt that ran with
    /// its judgement, and how the retry ended -- **empty** when the result
    /// was entered by a person, since no attempt ran. See `explain`'s comment
    /// on the same case.
    Trace<Rational> trace {};
};

/// Evaluates @p retrying and records how, writing every symbol as
/// @p vocabulary says -- the retry counterpart of `explain`. The outcome is
/// identical to `checked_evaluate_retry(retrying, environment)`: tracing
/// observes, it does not participate.
template <typename Rep = Rational,
          Described R,
          std::size_t Max,
          FirstJudged J,
          typename Start,
          typename A,
          typename P,
          typename Env,
          Vocabulary V = DefaultVocabulary>
[[nodiscard]] ExplainedRetry<R> explain_retry(Retry<R, Max, J, Start, A, P> const& retrying,
                                              Env const& environment,
                                              V const& vocabulary = V {})
{
    static_assert(detail::RequireExactRetry<Rep>::value);
    Trace<Rational> recorded {};
    std::expected<RetryOutcome<R>, RetryFailure> retryOutcome =
        checked_evaluate_retry<Rational>(retrying, environment, RecordingSink<Rational, V> { recorded, vocabulary });
    return ExplainedRetry<R> { std::move(retryOutcome), std::move(recorded) };
}
} // namespace formula
