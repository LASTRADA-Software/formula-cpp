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
    [[nodiscard]] inline std::string operand_reference(std::size_t index)
    {
        return "#" + std::to_string(index + 1);
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

        std::string text { operatorText };
        for (std::size_t const operand: step.operands)
            text += " " + operand_reference(operand);
        return text;
    }

    /// The operand a one-operand step consumed, or an empty string when it
    /// recorded none -- see `binary_expression` for why that can happen.
    template <typename Rep>
    [[nodiscard]] std::string sole_operand(Step<Rep> const& step)
    {
        return step.operands.empty() ? std::string {} : operand_reference(step.operands[0]);
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

        std::string text = "if";
        if (predicateOperands >= 1)
            text += " " + operand_reference(step.operands[0]);
        if (predicateOperands >= 2)
            text += " " + std::string { comparison_symbol(step.comparison) } + " "
                    + operand_reference(step.operands[1]);
        if (branchRan)
            text += " " + std::string { describe(step.branch) } + " " + operand_reference(step.operands.back());
        return text;
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
        std::string text = "require";
        if (!step.operands.empty())
            text += " " + operand_reference(step.operands[0]);
        if (step.operands.size() >= 2)
            text += " " + std::string { comparison_symbol(step.comparison) } + " "
                    + operand_reference(step.operands[1]);
        return text;
    }

    /// An exact lookup's key, spelled the way `render()` spells it: `key 7`.
    ///
    /// The underlying value rather than the enumerator's name, for the reason
    /// `detail::key_text` (`render.hpp`) sets out in full: a C++ enumerator
    /// has no name at run time. This cannot call that function -- it is a
    /// template on the author's enumeration, and a `Step` has erased the type
    /// -- so the two spellings are independent and the cross-surface test in
    /// `trace_render_tests.cpp` is what ties them together, exactly as it
    /// does for the six comparison tokens.
    ///
    /// The two casts are spelled separately for `key_text`'s own reason: an
    /// enumeration's underlying type may be `unsigned long long`, whose top
    /// half no signed type can hold.
    [[nodiscard]] inline std::string lookup_key_text(Step<Rational> const& step)
    {
        return "key "
               + (step.lookupKeyIsSigned ? std::to_string(static_cast<long long>(step.lookupKey))
                                         : std::to_string(step.lookupKey));
    }

    /// A half-open interval a lookup step reports about -- a selected band,
    /// or the extent a whole band table covers: `2 to under 61/2 mm`.
    ///
    /// Delegates to `render.hpp`'s `band_text`, which is **the** spelling of a
    /// half-open interval in this library, so that a derivation and the
    /// formula it derives cannot name one band two ways. That ruling, and the
    /// published defect that bought it, are in `render.hpp`'s file comment.
    [[nodiscard]] inline std::string half_open_range_text(LookupRange const& range, std::string_view keySymbol)
    {
        return band_text(
            Band { range.lowNumerator, range.lowDenominator, range.highNumerator, range.highDenominator },
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
    [[nodiscard]] inline std::string closed_range_text(LookupRange const& range, std::string_view keySymbol)
    {
        return number_with_unit(declared_number_text(range.lowNumerator, range.lowDenominator) + " to "
                                    + declared_number_text(range.highNumerator, range.highDenominator),
                                keySymbol);
    }

    /// Why a lookup found nothing, in one clause -- the clause that stops
    /// `describe(ArithmeticError::DomainError)` from being read as a claim
    /// about something it does not know.
    ///
    /// Each kind says it in its own terms, because the three misses are
    /// genuinely different questions: a value in none of a table's bands, a
    /// key in none of its rows, a value off the ends of a curve.
    [[nodiscard]] inline std::string lookup_miss_text(Step<Rational> const& step, std::string_view keySymbol)
    {
        if (step.kind == StepKind::ExactLookup)
            return "no row has this key";

        if (!step.coveredRange.has_value())
            return step.kind == StepKind::BandedLookup ? "the table declares no bands" : "the curve declares no rows";

        if (step.kind == StepKind::BandedLookup)
            return "in no band; the bands cover " + half_open_range_text(*step.coveredRange, keySymbol);

        // A curve with exactly one row covers that one key and nothing else,
        // and "runs 15/2 to 15/2 mm" would describe it as a range it is not.
        // `at <key>` is the spelling `render()` gives a breakpoint, for the
        // same reason: a row is a point.
        std::string const low = declared_number_text(step.coveredRange->lowNumerator, step.coveredRange->lowDenominator);
        std::string const high =
            declared_number_text(step.coveredRange->highNumerator, step.coveredRange->highDenominator);
        if (low == high)
            return "outside the curve, whose only row is at " + number_with_unit(low, keySymbol);
        return "outside the curve, which runs " + closed_range_text(*step.coveredRange, keySymbol);
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
    [[nodiscard]] inline std::string lookup_suffix(Step<Rational> const& step)
    {
        std::string_view const keySymbol = view(step.sourceUnit.symbolText);
        switch (step.lookupFailure)
        {
            case LookupFailure::None:
                // Nothing failed. A banded lookup names the band its value
                // fell in, which is the one fact its derivation is for; the
                // other two kinds have nothing to add that the line does not
                // already carry -- an exact lookup's key is its subject, and
                // an interpolating lookup selects no row at all.
                return step.selectedBand.has_value()
                           ? " [" + band_text(*step.selectedBand, keySymbol) + "]"
                           : std::string {};
            case LookupFailure::Missed:
                return " [" + lookup_miss_text(step, keySymbol) + "]";
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
                return step.operands.empty() ? std::string { " [carried up from the operand]" }
                                             : " [carried up from " + sole_operand(step) + "]";
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
            case StepKind::Variable:
                return std::string { step.symbol };
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
            case StepKind::Round:
                return "round(" + sole_operand(step) + ", to " + std::to_string(step.granularity) + " dp of "
                       + std::string { view(step.unit.symbolText) } + ")";
            case StepKind::RoundSignificant:
                return "round(" + sole_operand(step) + ", to " + std::to_string(step.granularity) + " sf of "
                       + std::string { view(step.unit.symbolText) } + ")";
            case StepKind::NumericValue:
                return "numeric(" + sole_operand(step) + ", in " + std::string { view(step.sourceUnit.symbolText) }
                       + ")";
            case StepKind::Conditional:
                return conditional_expression(step);
            case StepKind::Constraint:
                return constraint_expression(step);
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
    [[nodiscard]] inline std::string citation_suffix(Citation const& citation)
    {
        std::string text;
        for (std::string_view const part: { citation.title, citation.reference, citation.section, citation.equation })
        {
            if (part.empty())
                continue;
            if (!text.empty())
                text += ", ";
            text += part;
        }
        return text.empty() ? text : " [" + text + "]";
    }

    /// What a step produced, as a person should read it.
    ///
    /// The value is stored in the coherent SI unit of the step's dimension;
    /// this converts it back into the unit the step was declared in and
    /// appends that unit's symbol, so an input entered as 180 l reads
    /// `180 l`. A step that failed shows why, and one with no value at all
    /// says so -- absence is not an error and must not be rendered as one.
    [[nodiscard]] inline std::string step_value_text(Step<Rational> const& step)
    {
        if (step.error.has_value())
            return std::string { describe(*step.error) };
        if (!step.value.has_value())
            return "(not measured)";

        std::expected<Rational, ArithmeticError> const shown =
            checked_convert(*step.value, coherent(step.dimension), step.unit);
        // Unreachable for a `Step` the recorder built -- it records a unit of
        // the step's own dimension -- but a `Step` is a public aggregate and a
        // caller may fill one in by hand. Refusing to print is the only
        // honest answer: the alternative is a number in a scale the line
        // claims it is not in.
        if (!shown)
            return "(not shown: " + std::string { describe(shown.error()) } + ")";

        std::string text = number_text(*shown);
        std::string_view const unitSymbol = view(step.unit.symbolText);
        if (!unitSymbol.empty())
            text += " " + std::string { unitSymbol };
        return text;
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
    [[nodiscard]] inline std::string rounding_mode_suffix(RoundingMode mode)
    {
        return " [" + std::string { describe(mode) } + "]";
    }

    /// A `Constraint` step's outcome, in one bracketed clause: `[satisfied]`,
    /// `[reject the specimen]`, `[not checked]`, or the arithmetic error that
    /// made it impossible to check at all.
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
    [[nodiscard]] inline std::string constraint_outcome_suffix(ConstraintOutcome const& outcome)
    {
        switch (outcome.kind())
        {
            case ConstraintOutcomeKind::Satisfied:
                return " [satisfied]";
            case ConstraintOutcomeKind::Violated:
                // `verdict()` is guaranteed present here -- `kind()` just
                // said `Violated`, the only state it is set for.
                return " [" + std::string { outcome.verdict()->label } + "]";
            case ConstraintOutcomeKind::NotChecked:
                return " [not checked]";
            case ConstraintOutcomeKind::Invalid:
                // Likewise guaranteed present for `Invalid`.
                return " [" + std::string { describe(*outcome.error()) } + "]";
        }
        return " [unknown outcome]";
    }

    /// One step's line, without its number: the expression, an `=`, the value,
    /// and a trailing clause for the kinds that need one -- a citation for
    /// `Documented`, a justification for `NumericValue`, the tie-breaking rule
    /// for the two rounding kinds, for a `Conditional` whose predicate never
    /// resolved `[no branch]`, and for the three lookup kinds the row selected
    /// or the failure's origin (see `lookup_suffix`).
    ///
    /// `Constraint` is handled separately, first: a constraint produces a
    /// verdict, not a quantity (`constraint.hpp`'s own file comment explains
    /// why), so there is no value at all for `step_value_text` to convert or
    /// print. The expression and the outcome suffix are the whole line.
    [[nodiscard]] inline std::string step_line(Step<Rational> const& step)
    {
        if (step.kind == StepKind::Constraint)
            return constraint_expression(step) + constraint_outcome_suffix(step.outcome);

        std::string const value = step_value_text(step);
        std::string suffix;
        if (step.kind == StepKind::Documented)
            suffix = citation_suffix(step.citation);
        else if (step.kind == StepKind::NumericValue)
            suffix = justification_suffix(step.justification);
        // Only `[no branch]`. A branch that ran is named by the keyword in
        // the body (`... then #3`, `... else #5`), and a suffix repeating it
        // would be noise; `[no branch]` is the one thing the body cannot
        // say, and it is what separates a predicate that never resolved from
        // one that resolved false.
        else if (step.kind == StepKind::Conditional && step.branch == Branch::Neither)
            suffix = " [" + std::string { describe(step.branch) } + "]";
        else if (step.kind == StepKind::Round || step.kind == StepKind::RoundSignificant)
            suffix = rounding_mode_suffix(step.mode);
        // Present for a lookup that succeeded as well as for one that failed,
        // unlike the three suffixes above: on a hit it names the band the
        // value fell in, and on a failure it is the only thing separating a
        // miss from a relayed error. See `lookup_suffix`.
        else if (is_lookup(step.kind))
            suffix = lookup_suffix(step);

        if (step.kind == StepKind::Constant)
            return value + suffix;
        return step_expression(step) + " = " + value + suffix;
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

    std::string text;
    for (std::size_t index = 0; index < shown; ++index)
    {
        text += std::to_string(index + 1);
        text += ". ";
        text += detail::step_line(trace.steps[index]);
        text += "\n";
    }

    if (trace.steps.size() > shown)
    {
        text += "... ";
        text += std::to_string(trace.steps.size() - shown);
        // Singular when there is one. A derivation is read by a person
        // checking a number they are about to sign off on; "1 further
        // steps" reads as carelessness, and carelessness is the last
        // impression an audit trail should give.
        text += (trace.steps.size() - shown) == 1 ? " further step not shown\n"
                                                 : " further steps not shown\n";
    }

    return text;
}

} // namespace formula
