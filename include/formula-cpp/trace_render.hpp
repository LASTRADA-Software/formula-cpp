// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A recorded derivation, as text a person reads.
///
/// **This header is deliberately absent from `formula.hpp` and from
/// `trace.hpp`.** It pulls `<string>`, which is exactly what `trace.hpp` goes
/// out of its way not to: recording a derivation and printing one are separate
/// costs, and a consumer who only records pays for neither. Include
/// `trace.hpp` to record, and this one as well to print.
///
/// Two things this renderer refuses to do, both of them deliberate:
///
///  - It has **no default limit**. `TraceRenderOptions::maxSteps` is a
///    `StepLimit`, a type with no default constructor, so a caller who
///    writes `render_trace(trace, {})` does not compile. That is stronger
///    than "no default member initialiser": `TraceRenderOptions` is an
///    aggregate, and a plain `std::size_t` member with no initialiser still
///    lets `{}` value-initialise it to zero and render nothing at all,
///    silently -- `StepLimit` exists to make that ill-formed instead. An
///    unbounded render of a derivation with a hundred thousand steps is one
///    unusable wall of text; a default limit is a limit someone forgets, and
///    a required one is a limit someone chooses.
///  - It never shows a value in a unit nobody entered. Every `Step` holds its
///    value in the coherent SI unit of its dimension so that steps are
///    comparable, and remembers the unit it was *declared* in; this converts
///    back before showing a number, so a volume entered as 180 l reads
///    `180 l` and not `9/50`.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/unit.hpp>

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <type_traits>

namespace formula
{

/// A step count that must be stated.
///
/// Not default-constructible, and that is the whole point: it is what makes
/// `render_trace(trace, {})` ill-formed instead of a silent zero. A plain
/// `std::size_t` member with no default initialiser does **not** achieve this
/// -- `TraceRenderOptions` is an aggregate, so `{}` value-initialises it to 0
/// and renders nothing at all.
struct StepLimit
{
    /// Deleted, and that is the entire point of this type: it is what makes
    /// `render_trace(trace, {})` ill-formed rather than a silent zero.
    StepLimit() = delete;

    /// Converts from a plain count, so `{ .maxSteps = 10 }` and `{ 25 }` both
    /// read naturally at a call site.
    ///
    /// The parameter is named differently from the member it initialises
    /// because GCC's `-Wshadow`, which this project's Linux CI leg runs with
    /// warnings as errors, flags a constructor parameter sharing a member's
    /// name even when it is used only in its own member-initialiser list.
    constexpr StepLimit(std::size_t steps) noexcept: value { steps } {}

    /// The count itself.
    std::size_t value {};
};

/// How much of a derivation to show.
struct TraceRenderOptions
{
    /// The most steps to render; the rest are replaced by one line saying how
    /// many were left out.
    ///
    /// A `StepLimit`, not a plain `std::size_t`, on purpose: see `StepLimit`'s
    /// own comment. `TraceRenderOptions` is an aggregate, and a `std::size_t`
    /// member here -- default member initialiser or not -- would let
    /// `render_trace(trace, {})` value-initialise it to zero and render
    /// nothing at all, silently, rather than fail to compile.
    StepLimit maxSteps;
};

namespace detail
{
    /// `#3` -- how a step refers to one of its operands. Steps are numbered
    /// from one in the rendered text, so this is the stored index plus one.
    [[nodiscard]] inline std::string operand_reference(std::size_t stepIndex)
    {
        return "#" + std::to_string(stepIndex + 1);
    }

    /// The infix spelling of a binary step.
    ///
    /// Falls back to prefix form when fewer than two operands were recorded.
    /// A genuine short circuit -- the left operand failed, so the evaluator
    /// never dispatched the right one -- leaves exactly **one** recorded
    /// operand, not zero: dividing by zero itself only happens after both
    /// sides have run, so a real `Divide` that fails this way always has two.
    /// Zero operands is rarer still: it takes both children being untraced
    /// extension-point nodes (`sink.hpp`) that produced no step of their own
    /// to consume. A `Divide` with nothing recorded therefore renders as a
    /// bare `/`.
    template <typename Rep>
    [[nodiscard]] std::string binary_expression(Step<Rep> const& step, std::string_view operatorText)
    {
        if (step.operands.size() >= 2)
            return operand_reference(step.operands[0]) + " " + std::string { operatorText } + " "
                   + operand_reference(step.operands[1]);

        std::string operatorExpression { operatorText };
        for (std::size_t const operandIndex: step.operands)
            operatorExpression += " " + operand_reference(operandIndex);
        return operatorExpression;
    }

    /// The operand a one-operand step consumed, or an empty string when it
    /// recorded none -- see `binary_expression` for why that can happen.
    template <typename Rep>
    [[nodiscard]] std::string sole_operand(Step<Rep> const& step)
    {
        return step.operands.empty() ? std::string {} : operand_reference(step.operands[0]);
    }

    /// A method's constraints, as the verdicts they reached:
    /// `acceptance(#3, #6, #9)`, in the order `check_method` returned them,
    /// and `acceptance(none)` for a method with no constraints -- which is
    /// the line that says an overlay removed every check.
    template <typename Rep>
    [[nodiscard]] std::string acceptance_expression(Step<Rep> const& step)
    {
        if (step.operands.empty())
            return "acceptance(none)";
        std::string acceptanceText = "acceptance(";
        for (std::size_t operandPosition = 0; operandPosition < step.operands.size(); ++operandPosition)
        {
            if (operandPosition != 0)
                acceptanceText += ", ";
            acceptanceText += operand_reference(step.operands[operandPosition]);
        }
        return acceptanceText + ")";
    }

    /// The token a comparison is written with in a derivation: `>`, `<=`.
    ///
    /// A table here rather than a call into `render.hpp`, whose own spelling
    /// lives inside `render_node(PredicateNode ...)` and is parameterised on
    /// `Dialect`. This renderer has no `Dialect` parameter by contract -- a
    /// derivation is plain text, and `render_trace` refuses even to render a
    /// non-`Rational` trace rather than acquire a policy -- so reaching into
    /// a dialect-aware spelling to take its plain arm would make the trace's
    /// contract depend on how many dialects `render()` grows.
    ///
    /// The six tokens must nonetheless match what `render()` writes in its
    /// plain dialects, or a reader checking a derivation against the formula
    /// it derives meets two notations for one comparison. That agreement is
    /// what `test/trace_render_tests.cpp`'s "a derivation spells a comparison
    /// the way render() does" pins, for all six, on both surfaces at once.
    [[nodiscard]] inline std::string_view comparison_symbol(Comparison comparison)
    {
        switch (comparison)
        {
            case Comparison::Less:
                return "<";
            case Comparison::LessOrEqual:
                return "<=";
            case Comparison::Greater:
                return ">";
            case Comparison::GreaterOrEqual:
                return ">=";
            case Comparison::Equal:
                return "==";
            case Comparison::NotEqual:
                return "!=";
        }
        return "unknown comparison";
    }

    /// A `Conditional` step, in the same infix shape `render()` gives the
    /// `WhenNode` it came from: `if #1 > #2 then #3`.
    ///
    /// It used to read `when(#1, #2, #3)`, which is positionally identical to
    /// the public `when(predicate, thenBranch, elseBranch)` and means
    /// something else entirely -- `(predicate lhs, predicate rhs, the branch
    /// that ran)`. A reader who had just met the API mapped the three slots
    /// onto it and concluded the *then* value was the second one, then read a
    /// `[then]` suffix next to a number that came from the third. A notation
    /// that needs prose to decode is not a smaller version of the problem; it
    /// is the problem. This shape needs none: it is the one `render()`
    /// already writes, minus the branch that did not run.
    ///
    /// **Three arities, not two.** `branch != Branch::Neither` exactly when a
    /// branch ran, and a branch that ran contributes exactly one step -- the
    /// last operand, since operands are recorded in evaluation order and the
    /// branch is dispatched after the predicate:
    ///
    ///  - three operands, a branch ran: `if #1 > #2 then #3`
    ///  - two, no branch: `if #1 > #2` -- both sides were evaluated and one
    ///    was absent, so the comparison was never decided
    ///  - one, no branch: `if #3` -- the predicate's **left** side raised an
    ///    arithmetic error, so the right side was never dispatched and no
    ///    comparison was ever made. The operator is left out for that reason
    ///    rather than for brevity: writing `#3 >` beside a side that does not
    ///    exist would claim a comparison that never happened.
    ///
    /// Which branch ran is named by the keyword in the body, so `step_line`
    /// below appends no `[then]`/`[else]` suffix that would only repeat it.
    /// It still appends `[no branch]`, which the body cannot say: that clause
    /// is the one that distinguishes a predicate which never resolved from
    /// one that resolved false, and nothing else carries that distinction.
    ///
    /// The bounds are checked rather than assumed. `Step` is a public
    /// aggregate and a caller may fill one in by hand, the same reason
    /// `step_value_text` below refuses to print a value whose unit disagrees
    /// with its dimension.
    template <typename Rep>
    [[nodiscard]] std::string conditional_expression(Step<Rep> const& step)
    {
        bool const branchRan = step.branch != Branch::Neither && !step.operands.empty();
        std::size_t const predicateOperands = step.operands.size() - (branchRan ? 1u : 0u);

        std::string conditionalText = "if";
        if (predicateOperands >= 1)
            conditionalText += " " + operand_reference(step.operands[0]);
        if (predicateOperands >= 2)
            conditionalText +=
                " " + std::string { comparison_symbol(step.comparison) } + " " + operand_reference(step.operands[1]);
        if (branchRan)
            conditionalText += " " + std::string { describe(step.branch) } + " " + operand_reference(step.operands.back());
        return conditionalText;
    }

    /// A `Constraint` step's expression, in a shape that borrows no name from
    /// the public API: `require #1 >= #2`.
    ///
    /// The public factory is `constraint(predicate, verdict)`, two positions,
    /// and a reader who knows it would map a two-slot call onto exactly
    /// those two things. This step means something else -- (predicate lhs,
    /// predicate rhs) -- so a call-shaped spelling here would repeat the
    /// mistake `Conditional`'s withdrawn `when(#1, #2, #3)` made, decoded
    /// only by a reader who happened to already distrust it. `require` names
    /// no public function, so there is nothing to misread it as, and the
    /// comparison operator between its two operand references is the one
    /// `render()` already writes for the predicate this step came from.
    ///
    /// **Two arities, not three.** A constraint has no branch, so nothing is
    /// dispatched once the predicate resolves -- unlike `Conditional`, whose
    /// own predicate is exactly this same shape plus a branch appended.
    /// `checked_evaluate_predicate` (`predicate.hpp`) dispatches the left
    /// side, and only if that succeeds does it dispatch the right, so:
    ///
    ///  - two operands: both sides were dispatched, whether or not the
    ///    predicate went on to resolve -- `require #1 >= #2`
    ///  - one operand: the left side raised an arithmetic error, so the
    ///    right was never dispatched and nothing was ever compared --
    ///    `require #1`, with no comparison token, for the same reason
    ///    `conditional_expression` above leaves one out in its own
    ///    one-operand case.
    ///
    /// A predicate side that is itself a custom untraced `Node` (`sink.hpp`)
    /// contributes no step of its own to consume, so **zero** operands is
    /// also constructible in principle -- the same pre-existing escape hatch
    /// `binary_expression` above already names for `Divide`, not a gap this
    /// step introduces. This function degrades the same way: plain
    /// `require`, no comparison token, nothing to claim was wrong to omit.
    ///
    /// The verdict reached checking it -- satisfied, violated, not checked,
    /// invalid -- is deliberately not part of this expression; `step_line`
    /// appends it as a suffix instead, because it is what checking the step
    /// *concluded*, not part of what the step computed.
    template <typename Rep>
    [[nodiscard]] std::string constraint_expression(Step<Rep> const& step)
    {
        std::string constraintText = "require";
        if (!step.operands.empty())
            constraintText += " " + operand_reference(step.operands[0]);
        if (step.operands.size() >= 2)
            constraintText +=
                " " + std::string { comparison_symbol(step.comparison) } + " " + operand_reference(step.operands[1]);
        return constraintText;
    }

    /// An exact lookup's key, spelled the way `render()` spells it in its
    /// plain dialect: `key Cylinder`, or `key 9` for a key that names no row
    /// (or a row whose key is itself no enumerator).
    ///
    /// The name is the one the step recorded (`Step::lookupKeyName`), and the
    /// underlying value is the fallback for an empty one, for the reasons
    /// `detail::key_text` (`render.hpp`) gives. This cannot call that
    /// function -- it is a template on the author's enumeration, and a `Step`
    /// has erased the type -- so the two spellings are independent and the
    /// cross-surface test in `trace_render_tests.cpp` is what ties them
    /// together, exactly as it does for the six comparison tokens.
    ///
    /// The two casts are spelled separately for `key_text`'s own reason: an
    /// enumeration's underlying type may be `unsigned long long`, whose top
    /// half no signed type can hold.
    [[nodiscard]] inline std::string lookup_key_text(Step<Rational> const& recorded)
    {
        if (!recorded.lookupKeyName.empty())
            return "key " + std::string { recorded.lookupKeyName };
        return "key "
               + (recorded.lookupKeyIsSigned ? std::to_string(static_cast<long long>(recorded.lookupKey))
                                             : std::to_string(recorded.lookupKey));
    }

    /// A half-open interval a lookup step reports about -- a selected band,
    /// or the extent a whole band table covers: `2 to under 61/2 mm`.
    ///
    /// Delegates to `render.hpp`'s `band_text`, which is **the** spelling of a
    /// half-open interval in this library, so that a derivation and the
    /// formula it derives cannot name one band two ways. That ruling, and the
    /// published defect that bought it, are in `render.hpp`'s file comment.
    [[nodiscard]] inline std::string half_open_range_text(LookupRange const& lookupRange, std::string_view keySymbol)
    {
        return band_text(Band { lookupRange.lowNumerator,
                                lookupRange.lowDenominator,
                                lookupRange.highNumerator,
                                lookupRange.highDenominator },
                         keySymbol);
    }

    /// A **closed** range an interpolating curve runs over: `2 to 19 mm`.
    ///
    /// One word shorter than `half_open_range_text` above, and that word is
    /// the whole point. A band's top is excluded and `to under` says so; a
    /// breakpoint is a row the table states a value *at*, the last one
    /// included, so the curve's top end is reached and nothing may say
    /// otherwise. `lookup.hpp` pins the two behaviours against each other at
    /// 30 mm and `render_tests.cpp` pins the two spellings; the trace is the
    /// third surface, and it is pinned in `trace_render_tests.cpp` on the same
    /// number, so that "harmonising" the two in either direction fails here as
    /// well as there.
    ///
    /// The bounds are reduced through `declared_number_text`, the same helper
    /// every other declared bound in this library is printed with, so a curve
    /// whose first row was typed `30/4` reads `15/2` here exactly as it does
    /// in `render()`.
    [[nodiscard]] inline std::string closed_range_text(LookupRange const& lookupRange, std::string_view keySymbol)
    {
        return number_with_unit(declared_number_text(lookupRange.lowNumerator, lookupRange.lowDenominator) + " to "
                                    + declared_number_text(lookupRange.highNumerator, lookupRange.highDenominator),
                                keySymbol);
    }

    /// The two rows an interpolating answer came from: `between 7/2 and 8 cm`,
    /// or `on the row at 30 mm` when the value sat exactly on one.
    ///
    /// Two clauses rather than one with a degenerate arm, because the two say
    /// genuinely different things. Between two rows the answer appears in
    /// neither of them and the reader has to check an interpolation; on a row
    /// the table stated the number directly and there is nothing to check. At
    /// the curve's **last** row that distinction is the whole behaviour
    /// `lookup.hpp` pins -- a breakpoint is a row, not a boundary, so the last
    /// one is reached -- and a derivation that spelled it as a segment would
    /// describe the table as doing something it does not.
    ///
    /// Neither spelling is an interval: `between 7/2 and 8 cm` names two rows
    /// and claims nothing about either end being included or excluded, which
    /// is why it needs neither `band_text`'s `to under` nor
    /// `closed_range_text`'s `to`. The numbers go through
    /// `declared_number_text` like every other declared bound, so a row typed
    /// `14/4` reads `7/2` here exactly as it does in `render()`.
    [[nodiscard]] inline std::string segment_text(Segment const& lookupSegment, std::string_view keySymbol)
    {
        std::string const lowText = declared_number_text(lookupSegment.low.numerator, lookupSegment.low.denominator);
        std::string const highText = declared_number_text(lookupSegment.high.numerator, lookupSegment.high.denominator);
        if (lowText == highText)
            return "on the row at " + number_with_unit(lowText, keySymbol);
        return "between " + number_with_unit(lowText + " and " + highText, keySymbol);
    }

    /// Why a lookup found nothing, in one clause -- the clause that stops
    /// `describe(ArithmeticError::DomainError)` from being read as a claim
    /// about something it does not know.
    ///
    /// Each kind says it in its own terms, because the three misses are
    /// genuinely different questions: a value in none of a table's bands, a
    /// key in none of its rows, a value off the ends of a curve.
    [[nodiscard]] inline std::string lookup_miss_text(Step<Rational> const& recorded, std::string_view keySymbol)
    {
        if (recorded.kind == StepKind::ExactLookup)
            return "no row has this key";

        if (!recorded.coveredRange.has_value())
            return recorded.kind == StepKind::BandedLookup ? "the table declares no bands" : "the curve declares no rows";

        if (recorded.kind == StepKind::BandedLookup)
            return "in no band; the bands cover " + half_open_range_text(*recorded.coveredRange, keySymbol);

        // A curve with exactly one row covers that one key and nothing else,
        // and "runs 15/2 to 15/2 mm" would describe it as a range it is not.
        // `at <key>` is the spelling `render()` gives a breakpoint, for the
        // same reason: a row is a point.
        std::string const lowText =
            declared_number_text(recorded.coveredRange->lowNumerator, recorded.coveredRange->lowDenominator);
        std::string const highText =
            declared_number_text(recorded.coveredRange->highNumerator, recorded.coveredRange->highDenominator);
        if (lowText == highText)
            return "outside the curve, whose only row is at " + number_with_unit(lowText, keySymbol);
        return "outside the curve, which runs " + closed_range_text(*recorded.coveredRange, keySymbol);
    }

    /// A lookup step's trailing clause: which row it selected, or -- when it
    /// produced no value -- whose failure it is carrying and of what kind.
    ///
    /// **This clause is not decoration; it is what keeps the line from
    /// lying.** All three kinds report every failure through one error
    /// channel, so a lookup step carrying `DomainError` is ambiguous on its
    /// face between "the value fell in no band" and "the operand failed and I
    /// am relaying it", and one carrying `Overflow` is ambiguous between "my
    /// own interpolation overflowed" and the same relaying. Without this
    /// clause the line would read `lookup(#1) = argument outside the domain of
    /// the operation` for a case where nothing was outside any domain and the
    /// real failure happened two levels down -- a plausible answer to a
    /// question the line cannot otherwise answer, which is the defect phase 9
    /// refused `bool satisfied()` over. `Step::lookupFailure` is what resolves
    /// it, and `LookupFailure` (`trace.hpp`) records how.
    ///
    /// The same bracket `citation_suffix`, `rounding_mode_suffix` and
    /// `constraint_outcome_suffix` use, for the reason the last of those gives
    /// at length: it is where a reader is already looking for a step's
    /// trailing qualifications, and the plain `--` this project's prose uses
    /// for a secondary aside would train them to skim past exactly the fact
    /// that must not be skimmed.
    [[nodiscard]] inline std::string lookup_suffix(Step<Rational> const& recorded)
    {
        std::string_view const keySymbol = view(recorded.sourceUnit.symbolText);
        switch (recorded.lookupFailure)
        {
            case LookupFailure::None:
                // Nothing failed, so the clause names where the answer came
                // from: the band a banded lookup's value fell in, or the two
                // rows an interpolating one drew on. An exact lookup adds
                // nothing here -- its key is already the subject of the line,
                // and the key is the row.
                //
                // Either may be absent even on a hit, and then the line says
                // nothing rather than guessing: the recorder locates a value
                // in the operand's own step, and an operand evaluated through
                // the two-parameter extension point (`sink.hpp`) contributes
                // none.
                if (recorded.selectedBand.has_value())
                    return " [" + band_text(*recorded.selectedBand, keySymbol) + "]";
                if (recorded.selectedSegment.has_value())
                    return " [" + segment_text(*recorded.selectedSegment, keySymbol) + "]";
                return {};
            case LookupFailure::Missed:
                return " [" + lookup_miss_text(recorded, keySymbol) + "]";
            case LookupFailure::Computation:
                return " [the interpolation itself overflowed, not anything below it]";
            case LookupFailure::Conversion:
                return " [this lookup's own unit conversion failed, not anything below it]";
            case LookupFailure::Propagated:
                // Never a claim about the table: nothing about it went wrong.
                // The operand is named so a reader is sent to the line that
                // does carry the failure. It contributed a step by
                // construction -- that is how the recorder knew -- but
                // `Step` is a public aggregate and a caller may fill one in
                // by hand, so the reference is not assumed into existence.
                return recorded.operands.empty() ? std::string { " [carried up from the operand]" }
                                                 : " [carried up from " + sole_operand(recorded) + "]";
            case LookupFailure::Undetermined:
                return " [this lookup or something below it: the operand recorded no step]";
        }
        return " [unknown lookup failure]";
    }

    /// What a step computed, written in terms of the steps it consumed.
    ///
    /// A `Constant` is absent from this deliberately: a constant's expression
    /// *is* its value, so `render_trace` writes the value alone rather than
    /// the tautology `0 = 0`.
    template <typename Rep>
    [[nodiscard]] std::string step_expression(Step<Rep> const& step)
    {
        switch (step.kind)
        {
            // An overridden constant reads as its quantity, as it does in
            // `render()`; that the overlay fixed it goes in the suffix -- see
            // `overridden_constant_suffix`.
            case StepKind::Variable:
            case StepKind::OverriddenConstant:
                return std::string { step.symbol };
            // The quantity, equal to the step its definition produced:
            // `k_s = #3`, so that the line reads `k_s = #3 = 97/100`. That it
            // is a jurisdiction's definition goes in the suffix -- see
            // `derived_quantity_suffix`. With no step to name -- an untraced
            // consumer node as the whole definition -- the quantity alone.
            case StepKind::DerivedQuantity:
                return step.operands.empty() ? std::string { step.symbol }
                                             : std::string { step.symbol } + " = " + sole_operand(step);
            // Its operand, as `Documented`'s is: the replacement computed the
            // value; the step says only whose formula it was.
            case StepKind::ReplacedVariant:
                return sole_operand(step);
            case StepKind::Constant:
                return {};
            case StepKind::PiConstant:
                return "pi";
            case StepKind::Negate:
                return "-" + sole_operand(step);
            case StepKind::Add:
                return binary_expression(step, "+");
            case StepKind::Subtract:
                return binary_expression(step, "-");
            case StepKind::Multiply:
                return binary_expression(step, "*");
            case StepKind::Divide:
                return binary_expression(step, "/");
            case StepKind::Power:
                return sole_operand(step) + "^" + std::to_string(step.exponent);
            case StepKind::Root:
                return step.exponent == 2 ? "sqrt(" + sole_operand(step) + ")"
                                          : "root" + std::to_string(step.exponent) + "(" + sole_operand(step) + ")";
            case StepKind::Documented:
                return sole_operand(step);
            // Its operand, exactly as `Documented`'s is: the selection chose
            // which formula ran, and computed nothing of its own. What it
            // chose goes in the suffix -- see `variant_suffix`.
            case StepKind::VariantSelected:
                return sole_operand(step);
            case StepKind::Round:
                return "round(" + sole_operand(step) + ", to " + std::to_string(step.granularity) + " dp of "
                       + std::string { view(step.unit.symbolText) } + ")";
            case StepKind::RoundSignificant:
                return "round(" + sole_operand(step) + ", to " + std::to_string(step.granularity) + " sf of "
                       + std::string { view(step.unit.symbolText) } + ")";
            // The unit only: the granularity belongs with whose rule it is,
            // in the suffix -- see `rounding_rule_suffix`.
            case StepKind::RoundingRuleApplied:
                return "round(" + sole_operand(step) + ", in " + std::string { view(step.unit.symbolText) } + ")";
            case StepKind::NumericValue:
                return "numeric(" + sole_operand(step) + ", in " + std::string { view(step.sourceUnit.symbolText) }
                       + ")";
            case StepKind::Conditional:
                return conditional_expression(step);
            case StepKind::Constraint:
                return constraint_expression(step);
            case StepKind::AcceptanceChecked:
                return acceptance_expression(step);
            // The head names are `render()`'s own, and the split between them
            // is the one `render.hpp` makes deliberately: the two *selecting*
            // kinds share `lookup`, and the one that *computes* a number
            // appearing in no row of its table is `interpolate`. A reader
            // checking a derivation against the formula it derives must meet
            // one name per kind, not two.
            case StepKind::BandedLookup:
                return "lookup(" + sole_operand(step) + ")";
            // The key sits where the other two kinds' operand sits, because
            // it plays that part: it is what is being looked up. It is data
            // and not a sub-expression -- an exact lookup has no operand at
            // all -- which is exactly why this step has to carry it.
            case StepKind::ExactLookup:
                return "lookup(" + lookup_key_text(step) + ")";
            case StepKind::InterpolatingLookup:
                return "interpolate(" + sole_operand(step) + ")";
        }
        return "unknown step kind";
    }

    /// What a citation identifies itself by, unbracketed: its title,
    /// reference, section and equation, each that is not empty, joined by
    /// commas. Empty when all four are.
    [[nodiscard]] inline std::string citation_text(Citation const& citation)
    {
        std::string citationWords;
        for (std::string_view const citationPart:
             { citation.title, citation.reference, citation.section, citation.equation })
        {
            if (citationPart.empty())
                continue;
            if (!citationWords.empty())
                citationWords += ", ";
            citationWords += citationPart;
        }
        return citationWords;
    }

    /// What a citation says, in one bracketed clause:
    /// `[Water/cement ratio, Example Standard 1:2020, 5.4.2, (3)]`.
    ///
    /// Empty when the citation names nothing, so a `Documented` step with a
    /// blank citation adds no trailing noise. "Names nothing" means all four
    /// identifying fields are empty -- a citation carrying only an equation
    /// number still identifies itself and must not render as though it had no
    /// citation at all.
    ///
    /// `Citation::text` is deliberately **not** included. It is the definition
    /// in full, written for a reader who does not have the document; a
    /// derivation is a line-per-step record, and a paragraph inside one line
    /// would defeat the bound the caller chose. A page that wants the full
    /// text has the `Citation` itself, through `document()`.
    ///
    /// The bracket goes around `citation_text`, which the overlay clauses
    /// below share, so that a source reads the same wherever it is cited.
    [[nodiscard]] inline std::string citation_suffix(Citation const& citation)
    {
        std::string const citationWords = citation_text(citation);
        return citationWords.empty() ? citationWords : " [" + citationWords + "]";
    }

    /// Where a value or a rule came from, when an overlay supplied it:
    /// `jurisdiction overlay`, followed by what the overlay cited, when it
    /// cited anything -- `jurisdiction overlay: Example Standard 12:2021 NA`.
    [[nodiscard]] inline std::string overlay_source_text(Citation const& overlayCitation)
    {
        std::string const cited = citation_text(overlayCitation);
        return cited.empty() ? std::string { "jurisdiction overlay" } : "jurisdiction overlay: " + cited;
    }

    /// An overridden constant's clause: `[fixed by jurisdiction overlay:
    /// Example Standard 12:2021 NA, NA.2.3]`.
    ///
    /// Present whether or not the overlay cited anything. The body of the line
    /// -- `k_s = 97/100` -- reads exactly as a variable the specimen supplied,
    /// and this clause is the only thing on it that says otherwise.
    [[nodiscard]] inline std::string overridden_constant_suffix(Citation const& cited)
    {
        return " [fixed by " + overlay_source_text(cited) + "]";
    }

    /// A derived quantity's clause: `[derived by jurisdiction overlay: ...]`.
    /// Present whether or not the overlay cited anything, for the reason
    /// `overridden_constant_suffix` gives.
    [[nodiscard]] inline std::string derived_quantity_suffix(Citation const& cited)
    {
        return " [derived by " + overlay_source_text(cited) + "]";
    }

    /// A replaced variant's clause: `[replaced by jurisdiction overlay: ...]`.
    /// Present whether or not the overlay cited anything: the body of the line
    /// -- `#5 = ...` -- says nothing of whose formula ran.
    [[nodiscard]] inline std::string replaced_variant_suffix(Citation const& cited)
    {
        return " [replaced by " + overlay_source_text(cited) + "]";
    }

    /// Whose a `RoundingRuleApplied` step's rule was: `method default`, or
    /// the overlay and what it cited.
    [[nodiscard]] inline std::string rounding_provenance_text(Step<Rational> const& recorded)
    {
        switch (recorded.roundingProvenance)
        {
            case RoundingProvenance::MethodDefault:
                return "method default";
            case RoundingProvenance::JurisdictionOverlay:
                return overlay_source_text(recorded.citation);
        }
        // A hand-built `Step` may hold any value of the underlying type, and
        // naming either provenance for it would be a guess.
        return "unknown provenance";
    }

    /// A method's rounding rule, and whose it was, in one bracketed clause:
    /// `[rounded to 1 dp (method default); nearest, ties away from zero]`, or
    /// `[rounded to 2 dp (jurisdiction overlay: ...); ...]`.
    ///
    /// **The provenance is the point.** Spec section 9.1 asks the trace to
    /// say which rule applied and where it came from; `rounded to 1 dp` alone
    /// is true of both, so the parenthesis is what makes the line answer the
    /// second half. It sits beside the granularity, not after the mode, because
    /// what it qualifies is the rule, and the granularity is the rule.
    ///
    /// A semicolon before the tie-breaking rule, not the comma
    /// `rounding_mode_suffix` could otherwise have been joined with: a cited
    /// source has commas of its own, and so does every half mode.
    [[nodiscard]] inline std::string rounding_rule_suffix(Step<Rational> const& recorded)
    {
        return " [rounded to " + std::to_string(recorded.granularity) + " dp (" + rounding_provenance_text(recorded) + "); "
               + std::string { describe(recorded.mode) } + "]";
    }

    /// @p ordinal as an English ordinal: `1st`, `2nd`, `3rd`, `4th`, and
    /// `11th`, `12th`, `13th` rather than `11st`, `12nd`, `13rd`.
    [[nodiscard]] inline std::string ordinal_text(std::size_t ordinal)
    {
        std::string_view ordinalSuffix = "th";
        if (ordinal % 100 < 11 || ordinal % 100 > 13)
        {
            switch (ordinal % 10)
            {
                case 1:
                    ordinalSuffix = "st";
                    break;
                case 2:
                    ordinalSuffix = "nd";
                    break;
                case 3:
                    ordinalSuffix = "rd";
                    break;
                default:
                    break;
            }
        }
        return std::to_string(ordinal) + std::string { ordinalSuffix };
    }

    /// Which variant a method selected, and on what, in one bracketed clause:
    /// `[variant Cylinder (2nd of 3), selected by tag]`.
    ///
    /// **Both the name and the position, because each answers what the other
    /// cannot.** The name is the discriminator the caller selected with, and
    /// what an inspector asking "why the cylinder formula?" reads; the
    /// position is what they count back to in the method's `variants(...)`,
    /// and it survives even where the name does not -- a tag the compiler's
    /// signature did not let the library read, recorded with an empty name,
    /// still says which variant ran: `[the 2nd of 3 variants, selected by a
    /// tag whose name could not be read]`. Printed one-based, as an ordinal,
    /// because a person counts from one; `Step::variantIndex` stays
    /// zero-based, as every position this library reports in a diagnostic
    /// is.
    ///
    /// `selected by tag` names **how** the choice was made, not only that it
    /// was: a tag is the only discriminator a method has in this phase, and
    /// saying so now is what will keep this line true once there is a second.
    ///
    /// The same bracket `citation_suffix` and `lookup_suffix` use, for the
    /// reason `lookup_suffix` gives: it is where a reader already looks for
    /// the fact about a step that must not be skimmed.
    [[nodiscard]] inline std::string variant_suffix(Step<Rational> const& recorded)
    {
        std::string const ordinalPosition =
            ordinal_text(recorded.variantIndex + 1) + " of " + std::to_string(recorded.variantCount);
        if (recorded.variantTag.empty())
            return " [the " + ordinalPosition + " variants, selected by a tag whose name could not be read]";
        return " [variant " + std::string { recorded.variantTag } + " (" + ordinalPosition + "), selected by tag]";
    }

    /// What a step produced, as a person should read it.
    ///
    /// The value is stored in the coherent SI unit of the step's dimension;
    /// this converts it back into the unit the step was declared in and
    /// appends that unit's symbol, so an input entered as 180 l reads
    /// `180 l`. A step that failed shows why, and one with no value at all
    /// says so -- absence is not an error and must not be rendered as one.
    [[nodiscard]] inline std::string step_value_text(Step<Rational> const& recorded)
    {
        if (recorded.error.has_value())
            return std::string { describe(*recorded.error) };
        if (!recorded.value.has_value())
            return "(not measured)";

        std::expected<Rational, ArithmeticError> const shown =
            checked_convert(*recorded.value, coherent(recorded.dimension), recorded.unit);
        // Unreachable for a `Step` the recorder built -- it records a unit of
        // the step's own dimension -- but a `Step` is a public aggregate and a
        // caller may fill one in by hand. Refusing to print is the only
        // honest answer: the alternative is a number in a scale the line
        // claims it is not in.
        if (!shown)
            return "(not shown: " + std::string { describe(shown.error()) } + ")";

        std::string valueText = number_text(*shown);
        std::string_view const unitSymbol = view(recorded.unit.symbolText);
        if (!unitSymbol.empty())
            valueText += " " + std::string { unitSymbol };
        return valueText;
    }

    /// A `NumericValue` step's justification, in one bracketed clause. Empty
    /// when the step carries none -- reachable only for a hand-built `Step`,
    /// since `NumericValueNode` itself refuses an empty one at compile time
    /// -- so a blank justification adds no trailing noise, the same guard
    /// `citation_suffix` above applies for the same reason.
    ///
    /// Rendered, not merely carried: the whole point of `numeric_value_of` is
    /// that a number left a named unit for a stated reason, and a step that
    /// recorded the reason without ever showing it would be exactly the
    /// silent failure mode the node exists to prevent.
    [[nodiscard]] inline std::string justification_suffix(std::string_view justification)
    {
        return justification.empty() ? std::string {} : " (" + std::string { justification } + ")";
    }

    /// A rounding step's tie-breaking rule, in one bracketed clause:
    /// `[nearest, ties away from zero]`.
    ///
    /// The mode is deliberately absent from `render()` -- a standard states a
    /// granularity, not a tie rule -- but a trace has the opposite job, and
    /// two rounding nodes differing only in their mode produce 13 mm and
    /// 12 mm from the same input. A derivation that showed identical text for
    /// both would be unable to explain either number.
    ///
    /// A bracketed suffix rather than a third argument inside the
    /// parentheses, which is where the granularity already sits. Every
    /// `describe(RoundingMode)` spelling for a half mode contains a comma of
    /// its own -- "nearest, ties away from zero" -- so `round(#1, to 0 dp of
    /// mm, nearest, ties away from zero)` would read as a four-argument call
    /// whose last two arguments are fragments. The suffix machinery this
    /// function already uses for `Documented` and `NumericValue` has no such
    /// collision, and puts the mode where a reader is already looking for a
    /// step's trailing qualifications.
    [[nodiscard]] inline std::string rounding_mode_suffix(RoundingMode roundingMode)
    {
        return " [" + std::string { describe(roundingMode) } + "]";
    }

    /// A constraint's outcome in words, unbracketed: `satisfied`, the verdict's
    /// label, `not checked`, or the arithmetic error that made it impossible
    /// to check at all.
    [[nodiscard]] inline std::string constraint_outcome_text(ConstraintOutcome const& checkedOutcome)
    {
        switch (checkedOutcome.kind())
        {
            case ConstraintOutcomeKind::Satisfied:
                return "satisfied";
            case ConstraintOutcomeKind::Violated:
                // `verdict()` is guaranteed present here -- `kind()` just
                // said `Violated`, the only state it is set for.
                return std::string { checkedOutcome.verdict()->label };
            case ConstraintOutcomeKind::NotChecked:
                return "not checked";
            case ConstraintOutcomeKind::Invalid:
                // Likewise guaranteed present for `Invalid`.
                return std::string { describe(*checkedOutcome.error()) };
        }
        return "unknown outcome";
    }

    /// Whose a method's constraints were: `the method's own`, or the overlay
    /// and what it cited, as every overlay clause in this file spells it.
    [[nodiscard]] inline std::string constraint_provenance_text(ConstraintProvenance provenance, Citation const& cited)
    {
        switch (provenance)
        {
            case ConstraintProvenance::MethodOwn:
                return "the method's own";
            case ConstraintProvenance::JurisdictionOverlay:
                return overlay_source_text(cited);
        }
        // A hand-built `Step` may hold any value of the underlying type, and
        // naming either provenance for it would be a guess.
        return "unknown provenance";
    }

    /// A verdict's second clause, after a semicolon: `; the method's own
    /// constraint`, or `; jurisdiction overlay: ...`. Empty for a constraint
    /// checked outside any method, which is no one's -- so a trace of
    /// `check` or `check_all` reads exactly as it did before methods had
    /// constraints.
    ///
    /// A semicolon, not a comma, for `rounding_rule_suffix`'s reason: a cited
    /// source has commas of its own.
    [[nodiscard]] inline std::string constraint_provenance_clause(Step<Rational> const& recorded)
    {
        if (!recorded.constraintProvenance.has_value())
            return {};
        if (*recorded.constraintProvenance == ConstraintProvenance::MethodOwn)
            return "; " + constraint_provenance_text(*recorded.constraintProvenance, recorded.citation) + " constraint";
        return "; " + constraint_provenance_text(*recorded.constraintProvenance, recorded.citation);
    }

    /// A method's constraints step's clause: `[the method's own constraints]`,
    /// or `[jurisdiction overlay: ...]`. Present whatever the provenance, for
    /// the reason `overridden_constant_suffix` gives: the body of the line
    /// says nothing of whose checks they were.
    [[nodiscard]] inline std::string acceptance_suffix(Step<Rational> const& recorded)
    {
        if (!recorded.constraintProvenance.has_value())
            return {};
        if (*recorded.constraintProvenance == ConstraintProvenance::MethodOwn)
            return " [" + constraint_provenance_text(*recorded.constraintProvenance, recorded.citation) + " constraints]";
        return " [" + constraint_provenance_text(*recorded.constraintProvenance, recorded.citation) + "]";
    }

    /// A `Constraint` step's outcome, in one bracketed clause: `[satisfied]`,
    /// `[reject the specimen]`, `[not checked]`, or the arithmetic error that
    /// made it impossible to check at all -- followed, for a method's
    /// constraint, by whose it was: `[satisfied; the method's own
    /// constraint]`, `[reject the specimen; jurisdiction overlay: ...]` (see
    /// `constraint_provenance_clause`).
    ///
    /// Present for **every** outcome, unlike the other bracketed suffixes in
    /// this file. `Conditional` needs `[no branch]` only for the one case its
    /// body cannot already say, because the other three name the branch in
    /// the keyword itself (`then #3`, `else #5`). A constraint's body never
    /// names its outcome: `require #1 >= #2` reads identically whether the
    /// requirement held, failed, was never checked, or could not be checked
    /// -- so unlike `Conditional`, nothing elsewhere in the line carries that
    /// distinction for any of the four states, and this suffix is the only
    /// place it is ever said.
    ///
    /// The same bracket `citation_suffix`, `justification_suffix` and
    /// `rounding_mode_suffix` use, not the plain `--` this project's own
    /// prose already uses throughout its comments and guides for a
    /// secondary aside. Reusing that glyph here would train a reader to
    /// skim past it as an aside, which is exactly wrong for the one fact a
    /// constraint step exists to make prominent: whether it passed. A
    /// constraint line is already the only kind with no `=` in it, so the
    /// structural difference alone already marks "this line reads
    /// differently" without a second, competing signal doing the same job.
    /// The citation suffix already proves a bracket can hold a full clause
    /// rather than a single word (`[Bulk density of a compacted specimen,
    /// Example Standard 1:2020, 4.2, (3)]`), so there is no shape a verdict
    /// label needs that the existing convention cannot give it.
    [[nodiscard]] inline std::string constraint_outcome_suffix(Step<Rational> const& recorded)
    {
        return " [" + constraint_outcome_text(recorded.outcome) + constraint_provenance_clause(recorded) + "]";
    }

    /// One step's line, without its number: the expression, an `=`, the value,
    /// and a trailing clause for the kinds that need one -- a citation for
    /// `Documented`, the variant and its discriminator for `VariantSelected`,
    /// a justification for `NumericValue`, the tie-breaking rule
    /// for the two rounding kinds, the granularity, provenance and tie rule
    /// for a method's rounding rule, the overlay that fixed an overridden
    /// constant, for a `Conditional` whose predicate never
    /// resolved `[no branch]`, and for the three lookup kinds the row selected
    /// or the failure's origin (see `lookup_suffix`).
    ///
    /// `Constraint` is handled separately, first: a constraint produces a
    /// verdict, not a quantity (`constraint.hpp`'s own file comment explains
    /// why), so there is no value at all for `step_value_text` to convert or
    /// print. The expression and the outcome suffix are the whole line. So is
    /// `AcceptanceChecked`, for the same reason: it gathers verdicts, and has
    /// no value of its own.
    [[nodiscard]] inline std::string step_line(Step<Rational> const& recorded)
    {
        if (recorded.kind == StepKind::Constraint)
            return constraint_expression(recorded) + constraint_outcome_suffix(recorded);
        if (recorded.kind == StepKind::AcceptanceChecked)
            return acceptance_expression(recorded) + acceptance_suffix(recorded);

        std::string const valueText = step_value_text(recorded);
        std::string annotation;
        if (recorded.kind == StepKind::Documented)
            annotation = citation_suffix(recorded.citation);
        else if (recorded.kind == StepKind::VariantSelected)
            annotation = variant_suffix(recorded);
        else if (recorded.kind == StepKind::NumericValue)
            annotation = justification_suffix(recorded.justification);
        // Only `[no branch]`. A branch that ran is named by the keyword in
        // the body (`... then #3`, `... else #5`), and a suffix repeating it
        // would be noise; `[no branch]` is the one thing the body cannot
        // say, and it is what separates a predicate that never resolved from
        // one that resolved false.
        else if (recorded.kind == StepKind::Conditional && recorded.branch == Branch::Neither)
            annotation = " [" + std::string { describe(recorded.branch) } + "]";
        else if (recorded.kind == StepKind::Round || recorded.kind == StepKind::RoundSignificant)
            annotation = rounding_mode_suffix(recorded.mode);
        else if (recorded.kind == StepKind::RoundingRuleApplied)
            annotation = rounding_rule_suffix(recorded);
        else if (recorded.kind == StepKind::OverriddenConstant)
            annotation = overridden_constant_suffix(recorded.citation);
        else if (recorded.kind == StepKind::DerivedQuantity)
            annotation = derived_quantity_suffix(recorded.citation);
        else if (recorded.kind == StepKind::ReplacedVariant)
            annotation = replaced_variant_suffix(recorded.citation);
        // Present for a lookup that succeeded as well as for one that failed,
        // unlike the three suffixes above: on a hit it names the band the
        // value fell in, and on a failure it is the only thing separating a
        // miss from a relayed error. See `lookup_suffix`.
        else if (is_lookup(recorded.kind))
            annotation = lookup_suffix(recorded);

        if (recorded.kind == StepKind::Constant)
            return valueText + annotation;
        return step_expression(recorded) + " = " + valueText + annotation;
    }
} // namespace detail

/// Renders @p trace as one numbered line per step, bounded by
/// @p options.maxSteps.
///
/// Steps are numbered from one, in the order they completed -- children before
/// parents -- and each names the steps it consumed as `#1`, `#2` and so on. An
/// empty trace renders an empty string rather than a header with nothing under
/// it. When the trace is longer than the limit, the first `maxSteps` are shown
/// followed by exactly one line stating how many were left out: a reader is
/// told what they are not seeing rather than silently handed a prefix.
///
/// Only an exact (`Rational`) trace can be rendered. Converting a value back
/// into the unit it was declared in is the unit layer's exact
/// multiply-then-divide, and there is no such operation for binary floating
/// point: a `double` trace would need a rounding policy, and choosing one on a
/// caller's behalf is how an audit trail acquires a number nobody can
/// reproduce. Evaluate in `double` by all means; print the exact trace.
template <typename Rep = Rational>
[[nodiscard]] std::string render_trace(Trace<Rep> const& trace, TraceRenderOptions options)
{
    static_assert(std::is_same_v<Rep, Rational>,
                  "formula: only an exact Rational trace can be rendered -- see render_trace's "
                  "documentation for why a floating-point derivation has no printable form here");

    std::size_t const shown =
        trace.steps.size() < options.maxSteps.value ? trace.steps.size() : options.maxSteps.value;

    std::string renderedTrace;
    for (std::size_t stepIndex = 0; stepIndex < shown; ++stepIndex)
    {
        renderedTrace += std::to_string(stepIndex + 1);
        renderedTrace += ". ";
        renderedTrace += detail::step_line(trace.steps[stepIndex]);
        renderedTrace += "\n";
    }

    if (trace.steps.size() > shown)
    {
        renderedTrace += "... ";
        renderedTrace += std::to_string(trace.steps.size() - shown);
        // Singular when there is one. A derivation is read by a person
        // checking a number they are about to sign off on; "1 further
        // steps" reads as carelessness, and carelessness is the last
        // impression an audit trail should give.
        renderedTrace += (trace.steps.size() - shown) == 1 ? " further step not shown\n" : " further steps not shown\n";
    }

    return renderedTrace;
}

} // namespace formula
