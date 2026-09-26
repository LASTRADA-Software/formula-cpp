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

#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
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
    /// Author text, made safe to stand in a trace line: `\` becomes `\\`,
    /// `[` becomes `\[`, `]` becomes `\]`, `;` becomes `\;`, a newline,
    /// carriage return or tab becomes `\n`, `\r` or `\t`, and any other
    /// control character -- below 0x20, or 0x7f -- becomes `\x` and two hex
    /// digits.
    ///
    /// **Why.** A trace line is a numbered line whose provenance is a
    /// bracketed clause at its end -- `[fixed by jurisdiction overlay: ...]`,
    /// `(method default)`, `; the method's own constraint` -- and only the
    /// library may state provenance. A quantity's declared symbol, a
    /// citation, a verdict's label, a justification, a tag's or an
    /// enumerator's spelling and a unit's symbol are the author's, and
    /// printed as written, one holding `] [derived by jurisdiction overlay:
    /// ...` wrote a clause no overlay made, and one holding a newline wrote a
    /// line that is no step. Escaped, an author's bracket can never close or
    /// open a clause, nor an author's newline end a line. The semicolon,
    /// because a verdict's clause is `[<label>; <whose constraint>]`: a label
    /// holding `; jurisdiction overlay: ...` would otherwise name a second,
    /// false owner beside the true one. The backslash, so that an author's `\[` cannot pass for an
    /// escaped bracket.
    ///
    /// **What it does not do.** It stops author text from breaking a line's
    /// structure, not from holding a clause's words: a `documented()` citation
    /// titled `replaced by jurisdiction overlay: ...` renders as a genuine
    /// replacement's clause does. The structured fields of a `Step` -- `kind`,
    /// the provenance enums, `variantPinned` and so on -- are what is
    /// authoritative, and the method's author is trusted. Nor does it touch
    /// anything but ASCII: a Unicode look-alike of a bracket (U+FF3B, U+FF3D)
    /// or the line separator U+2028 is written as it is, since it cannot
    /// break the ASCII structure the library writes.
    ///
    /// Applied by `step_line`, once, to every piece of author text a step
    /// holds -- see `EscapedStep` and `unit_symbol_text` -- and nowhere else:
    /// the words this file writes itself go into the line as they are.
    [[nodiscard]] inline std::string escaped_author_text(std::string_view authored)
    {
        constexpr std::string_view hexDigits = "0123456789abcdef";
        std::string escaped;
        escaped.reserve(authored.size());
        for (char const glyph: authored)
        {
            auto const byte = static_cast<unsigned char>(glyph);
            if (glyph == '\\' || glyph == '[' || glyph == ']' || glyph == ';')
            {
                escaped += '\\';
                escaped += glyph;
            }
            else if (glyph == '\n')
                escaped += "\\n";
            else if (glyph == '\r')
                escaped += "\\r";
            else if (glyph == '\t')
                escaped += "\\t";
            else if (byte < 0x20 || byte == 0x7f)
            {
                escaped += "\\x";
                escaped += hexDigits[byte / 16];
                escaped += hexDigits[byte % 16];
            }
            else
                escaped += glyph;
        }
        return escaped;
    }

    /// A unit's symbol as a trace line shows it, escaped as author text: a
    /// unit may be the author's own, and `Symbol` is a public aggregate that
    /// `symbol()` does not have to have built. Read here rather than through
    /// `EscapedStep`, because a `Step` holds its units as fixed-capacity
    /// `Symbol` values, which an escaped symbol may not fit.
    [[nodiscard]] inline std::string unit_symbol_text(Unit const& shownUnit)
    {
        return escaped_author_text(view(shownUnit.symbolText));
    }

    /// A citation's five fields, escaped by `escaped_author_text` and held
    /// here, for a copy of the citation to point at -- see `EscapedStep`.
    struct EscapedCitation
    {
        explicit EscapedCitation(Citation const& cited):
            title { escaped_author_text(cited.title) },
            reference { escaped_author_text(cited.reference) },
            section { escaped_author_text(cited.section) },
            equation { escaped_author_text(cited.equation) },
            text { escaped_author_text(cited.text) }
        {
        }

        EscapedCitation(EscapedCitation const&) = delete;
        EscapedCitation& operator=(EscapedCitation const&) = delete;

        /// The escaped citation, pointing into this object.
        [[nodiscard]] Citation cited() const noexcept
        {
            return Citation {
                .title = title, .reference = reference, .section = section, .equation = equation, .text = text
            };
        }

        std::string title;
        std::string reference;
        std::string section;
        std::string equation;
        std::string text;
    };

    /// A copy of a step whose every piece of author text -- the symbol, both
    /// citations (the step's own, and a selection's prune's), the
    /// justification, the variant tag, the lookup key's name and a violated
    /// constraint's verdict label -- is escaped by `escaped_author_text`,
    /// held here, and pointed at by the copy's views.
    ///
    /// `step_line` renders from this copy and never from the step it was
    /// given, so no helper below can print author text unescaped by reading
    /// the wrong field. Neither copyable nor movable: the copy's views point
    /// into this object's own strings.
    struct EscapedStep
    {
        explicit EscapedStep(Step<Rational> const& recorded):
            symbol { escaped_author_text(recorded.symbol) },
            justification { escaped_author_text(recorded.justification) },
            variantTag { escaped_author_text(recorded.variantTag) },
            lookupKeyName { escaped_author_text(recorded.lookupKeyName) },
            citation { recorded.citation },
            variantPrunedBy { recorded.variantPrunedBy },
            verdictLabel { recorded.outcome.verdict().has_value() ? escaped_author_text(recorded.outcome.verdict()->label)
                                                                  : std::string {} },
            step { recorded }
        {
            step.symbol = symbol;
            step.justification = justification;
            step.variantTag = variantTag;
            step.lookupKeyName = lookupKeyName;
            step.citation = citation.cited();
            step.variantPrunedBy = variantPrunedBy.cited();
            if (recorded.outcome.kind() == ConstraintOutcomeKind::Violated)
                step.outcome = ConstraintOutcome::violated(Verdict { verdictLabel });
        }

        EscapedStep(EscapedStep const&) = delete;
        EscapedStep& operator=(EscapedStep const&) = delete;

        std::string symbol;
        std::string justification;
        std::string variantTag;
        std::string lookupKeyName;
        EscapedCitation citation;
        EscapedCitation variantPrunedBy;
        std::string verdictLabel;
        /// The step to render.
        Step<Rational> step;
    };

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
    /// The one operand named is then the **left** one, the operand that
    /// failed: `+ #5 = division by zero` means "#5 failed, and the right side
    /// was never evaluated", though it can read as "something plus #5". The
    /// elementwise steps (`ElementwiseAdd` and the rest) share this spelling,
    /// and `trace_render_tests.cpp` pins it for one.
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

    /// Every operand, in order, separated by `, `: `#1, #2`.
    template <typename Rep>
    [[nodiscard]] std::string operands_text(Step<Rep> const& step)
    {
        std::string listed;
        for (std::size_t const operandIndex: step.operands)
        {
            if (!listed.empty())
                listed += ", ";
            listed += operand_reference(operandIndex);
        }
        return listed;
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
    /// or the extent a whole band table covers: `211/100 to under 307/10 mm`.
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

    /// A **closed** range an interpolating curve runs over: `209/10 to 293/10 mm`.
    ///
    /// One word shorter than `half_open_range_text` above, and that word is
    /// the whole point. A band's top is excluded and `to under` says so; a
    /// breakpoint is a row the table states a value *at*, the last one
    /// included, so the curve's top end is reached and nothing may say
    /// otherwise. `lookup_tests.cpp` pins the two behaviours against each other
    /// and `render_tests.cpp` the two spellings; the trace is the third surface,
    /// pinned in `trace_render_tests.cpp`. Each pins them on one shared number
    /// of its own, so that "harmonising" the two in either direction fails here
    /// as well as there.
    ///
    /// The bounds are reduced through `declared_number_text`, the same helper
    /// every other declared bound in this library is printed with, so a curve
    /// whose first row was typed `1474/200` reads `737/100` here exactly as it
    /// does in `render()`.
    [[nodiscard]] inline std::string closed_range_text(LookupRange const& lookupRange, std::string_view keySymbol)
    {
        return number_with_unit(declared_number_text(lookupRange.lowNumerator, lookupRange.lowDenominator) + " to "
                                    + declared_number_text(lookupRange.highNumerator, lookupRange.highDenominator),
                                keySymbol);
    }

    /// The two rows an interpolating answer came from: `between 331/100 and
    /// 793/100 cm`, or `on the row at 293/10 mm` when the value sat exactly on
    /// one.
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

    /// The side-table record of the step at @p stepIndex in @p records, or
    /// nothing. The records are keyed by step index and appended in step
    /// order, so a binary search finds one; a hand-built trace that breaks the
    /// order finds nothing, and the caller says so rather than guess.
    template <typename Record>
    [[nodiscard]] Record const* record_for_step(std::vector<Record> const& records, std::size_t stepIndex)
    {
        auto const match = std::lower_bound(records.begin(), records.end(), stepIndex,
                                            [](Record const& each, std::size_t wanted) { return each.step < wanted; });
        if (match == records.end() || match->step != stepIndex)
            return nullptr;
        return &*match;
    }

    /// The declared sizes of the critical-value lookup step at @p stepIndex,
    /// as the clause ` (declared: 3, 4, 5, 6, 8)`, read from the trace's side
    /// table. A step with no record says so rather than print sizes it does
    /// not have.
    [[nodiscard]] inline std::string declared_sizes_text(Trace<Rational> const& trace, std::size_t stepIndex)
    {
        detail::SampleSizeRecord const* const sizes = record_for_step(trace.sampleSizeRecords, stepIndex);
        if (sizes == nullptr)
            return " (the table's sizes were not recorded)";
        if (sizes->declaredSizes.empty())
            return "; the table declares no sizes";
        return " (declared: " + std::string { sizes->declaredSizes } + ")";
    }

    /// Why a lookup found nothing, in one clause -- the clause that stops
    /// `describe(ArithmeticError::DomainError)` from being read as a claim
    /// about something it does not know.
    ///
    /// Each kind says it in its own terms, because the three misses are
    /// genuinely different questions: a value in none of a table's bands, a
    /// key in none of its rows, a value off the ends of a curve.
    [[nodiscard]] inline std::string lookup_miss_text(Trace<Rational> const& trace,
                                                      std::size_t stepIndex,
                                                      Step<Rational> const& recorded,
                                                      std::string_view keySymbol)
    {
        if (recorded.kind == StepKind::ExactLookup)
            return "no row has this key";

        // The count and every size the table does declare, so that a reader
        // sees the hole the count fell in: `no row for n = 7 (declared: 3, 4,
        // 5, 6, 8)`.
        if (recorded.kind == StepKind::SampleSizeLookup)
            return "no row for n = " + std::to_string(recorded.lookupKey) + declared_sizes_text(trace, stepIndex);

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
    ///
    /// @p recorded is the step's escaped copy (`EscapedStep`); @p trace and
    /// @p stepIndex are read only for the step's side-table record.
    [[nodiscard]] inline std::string lookup_suffix(Trace<Rational> const& trace,
                                                   std::size_t stepIndex,
                                                   Step<Rational> const& recorded)
    {
        std::string const keySymbol = unit_symbol_text(recorded.sourceUnit);
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
                // A critical value's row is its size, and the count is that
                // size -- the one thing the line's `critical(#1)` does not say.
                if (recorded.kind == StepKind::SampleSizeLookup && recorded.value.has_value() && !recorded.operands.empty())
                    return " [critical value at n = " + std::to_string(recorded.lookupKey) + "]";
                if (recorded.selectedBand.has_value())
                    return " [" + band_text(*recorded.selectedBand, keySymbol) + "]";
                if (recorded.selectedSegment.has_value())
                    return " [" + segment_text(*recorded.selectedSegment, keySymbol) + "]";
                return {};
            case LookupFailure::Missed:
                return " [" + lookup_miss_text(trace, stepIndex, recorded, keySymbol) + "]";
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
            // The count is not a number of determinations, so no row was
            // asked. Its value is the operand's, which the reference names.
            case LookupFailure::NotACount:
                return " [no row for n = " + (recorded.operands.empty() ? std::string { "the count" } : sole_operand(recorded))
                       + ", which is not a whole, non-negative number" + declared_sizes_text(trace, stepIndex) + "]";
        }
        return " [unknown lookup failure]";
    }

    /// A per-element rounding's granularities, `0/0/1`, in the series' order
    /// -- `render()`'s spelling (`detail::granularities_text` there is the
    /// same text from a `PlacesTable`).
    [[nodiscard]] inline std::string granularities_text(std::vector<int> const& granularities)
    {
        std::string listed;
        for (int const elementPlaces: granularities)
        {
            if (!listed.empty())
                listed += "/";
            listed += std::to_string(elementPlaces);
        }
        return listed;
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
            // `k_s = #3`, so that the line reads `k_s = #3 = 863/1000`. That it
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
                return "round(" + sole_operand(step) + ", to " + std::to_string(step.granularity) + " dp"
                       + unit_clause(" of ", unit_symbol_text(step.unit)) + ")";
            case StepKind::RoundSignificant:
                return "round(" + sole_operand(step) + ", to " + std::to_string(step.granularity) + " sf"
                       + unit_clause(" of ", unit_symbol_text(step.unit)) + ")";
            // The unit only: the granularity belongs with whose rule it is,
            // in the suffix -- see `rounding_rule_suffix`.
            case StepKind::RoundingRuleApplied:
                return "round(" + sole_operand(step) + unit_clause(", in ", unit_symbol_text(step.unit)) + ")";
            case StepKind::NumericValue:
                return "numeric(" + sole_operand(step) + unit_clause(", in ", unit_symbol_text(step.sourceUnit)) + ")";
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
            // The quantity, unmarked: the marker belongs to the formula
            // (`render()`), and a derivation line names what it read. That it
            // is a series shows in the list of elements after the `=`.
            case StepKind::SeriesVariable:
                return std::string { step.symbol };
            // A per-element constant's expression is its values, as a scalar
            // constant's is -- see `series_step_line`.
            case StepKind::SeriesConstant:
                return {};
            case StepKind::ElementwiseNegate:
                return "-" + sole_operand(step);
            case StepKind::ElementwiseAdd:
                return binary_expression(step, "+");
            case StepKind::ElementwiseSubtract:
                return binary_expression(step, "-");
            case StepKind::ElementwiseMultiply:
                return binary_expression(step, "*");
            case StepKind::ElementwiseDivide:
                return binary_expression(step, "/");
            // The end is written, as `render()` writes it: a running total
            // without it is half a derivation.
            case StepKind::CumulativeSum:
                return "cumulative(" + sole_operand(step) + ", " + std::string { describe(step.cumulativeDirection) } + ")";
            case StepKind::SeriesSum:
                return "sum(" + sole_operand(step) + ")";
            // The subject; each element's outcome follows, in the bracket --
            // see `conformity_line`.
            case StepKind::ConformityChecked:
                return "conform(" + sole_operand(step) + ")";
            // The set is `render()`'s to print; the step names where the
            // value landed in its suffix -- see `snap_suffix`.
            case StepKind::SnappedToPermitted:
                return "snap(" + sole_operand(step) + ")";
            // Every granularity, in the series' order, in the unit rounded
            // in, as `render()` writes it; the mode goes in the suffix, as
            // for `Round`.
            case StepKind::ElementwiseRound:
                return "round(" + sole_operand(step) + ", to " + granularities_text(step.elementGranularities) + " dp"
                       + unit_clause(" of ", unit_symbol_text(step.unit)) + ")";
            // A declared domain's line is its points, as a per-element
            // constant's is its values -- see `series_step_line`.
            case StepKind::SeriesDomain:
                return {};
            // The two series paired, in order; the pairs follow the `=` --
            // see `curve_step_line`.
            case StepKind::CurvePairing:
                return "curve(" + operands_text(step) + ")";
            // The curve, and where it was read; the segment goes in the
            // suffix -- see `curve_interpolation_suffix`.
            case StepKind::CurveInterpolation:
                return step.operands.size() >= 2 ? "interpolate(" + operand_reference(step.operands[0]) + ", at "
                                                       + operand_reference(step.operands[1]) + ")"
                                                 : "interpolate(" + sole_operand(step) + ")";
            // The direction is always written, as `render()` writes it.
            case StepKind::CurveSplice:
                return "splice(" + operands_text(step) + ", " + std::string { describe(step.monotone) } + ")";
            // The quantity, unmarked, as a series variable's is; the
            // observations follow the `=`.
            case StepKind::ObservationsVariable:
                return std::string { step.symbol };
            // The classes are `render()`'s to print, as a lookup's bands are;
            // the counts follow the `=`.
            case StepKind::Binning:
                return "bin(" + sole_operand(step) + ")";
            // `render()`'s spelling, `round(sqrt(...), to ...)`: one step, and
            // the root inside it, because the root itself was never a value.
            case StepKind::RoundedRoot:
                return "round(sqrt(" + sole_operand(step) + "), to " + std::to_string(step.granularity) + " dp"
                       + unit_clause(" of ", unit_symbol_text(step.unit)) + ")";
            // `render()`'s head name. The count is the subject, as a banded
            // lookup's operand is; which row it selected goes in the suffix.
            case StepKind::SampleSizeLookup:
                return "critical(" + sole_operand(step) + ")";
            // `render()`'s spelling: `abs(...)`, never bars, which a Markdown
            // table cell would read as its own delimiter.
            case StepKind::AbsoluteValue:
                return "abs(" + sole_operand(step) + ")";
            // Both read their side-table record, which only `step_line` can
            // reach; see `precision_expression`. Spelled here without it, for
            // a caller that has the step alone.
            case StepKind::PrecisionLevel:
                return "level";
            case StepKind::PrecisionLimit:
                return "precision limit";
        }
        return "unknown step kind";
    }

    /// What a clause says in place of a citation that names nothing. Every
    /// operation of an overlay takes a citation argument, but an empty one
    /// compiles -- `pin_variant<Cube>({})`, or an operation's aggregate built
    /// directly -- and a clause that then read `fixed by jurisdiction overlay`
    /// would look cited to a reader who does not know it could have said more.
    inline constexpr std::string_view noCitationGiven = "(no citation given)";

    /// A precision step's expression, read from its side-table record:
    ///
    ///  - pass 1: `level (pass 1 of 2) = #4`, the level expression's step;
    ///  - a placeholder: `level`, and its suffix names the limit that bound it;
    ///  - pass 2: `r at level #5 (pass 2 of 2) = #8`, the level step and the
    ///    limit expression's step -- or `r at level #5` alone when pass 1
    ///    produced no level and pass 2 never ran.
    ///
    /// A step with no record says so rather than guess which of the three it
    /// is: `Step` and `Trace` are public aggregates.
    [[nodiscard]] inline std::string precision_expression(Trace<Rational> const& trace,
                                                          std::size_t stepIndex,
                                                          Step<Rational> const& recorded)
    {
        detail::PrecisionRecord const* const precisionRecord = record_for_step(trace.precisionRecords, stepIndex);
        if (precisionRecord == nullptr)
            return recorded.kind == StepKind::PrecisionLimit ? "precision limit (its record is missing)"
                                                              : "level (its record is missing)";
        switch (precisionRecord->role)
        {
            case detail::PrecisionStepRole::LevelPass:
                return recorded.operands.empty() ? std::string { "level (pass 1 of 2)" }
                                                 : "level (pass 1 of 2) = " + operand_reference(recorded.operands.back());
            case detail::PrecisionStepRole::Placeholder:
                return "level";
            case detail::PrecisionStepRole::LimitPass:
            {
                std::string const heading =
                    std::string { precision_render_symbol(precisionRecord->kind) } + " at level " + operand_reference(precisionRecord->levelStep);
                return recorded.operands.size() < 2
                           ? heading
                           : heading + " (pass 2 of 2) = " + operand_reference(recorded.operands.back());
            }
        }
        return "precision step of unknown role";
    }

    /// A placeholder's clause: `[bound by #9]`, the limit whose level it read.
    /// Nothing for the two passes, whose expressions already say it all.
    [[nodiscard]] inline std::string precision_suffix(Trace<Rational> const& trace, std::size_t stepIndex)
    {
        detail::PrecisionRecord const* const precisionRecord = record_for_step(trace.precisionRecords, stepIndex);
        if (precisionRecord == nullptr || precisionRecord->role != detail::PrecisionStepRole::Placeholder)
            return {};
        if (!precisionRecord->limitStep.has_value())
            return " [bound by a limit that was not recorded]";
        return " [bound by " + operand_reference(*precisionRecord->limitStep) + "]";
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
    /// `jurisdiction overlay: `, followed by what the overlay cited --
    /// `jurisdiction overlay: Example Standard 12:2021 NA` -- or
    /// `jurisdiction overlay (no citation given)` when the citation names
    /// nothing. Every overlay clause is built on this, so none of them reads
    /// as cited when it was not.
    [[nodiscard]] inline std::string overlay_source_text(Citation const& overlayCitation)
    {
        std::string const cited = citation_text(overlayCitation);
        return cited.empty() ? "jurisdiction overlay " + std::string { noCitationGiven } : "jurisdiction overlay: " + cited;
    }

    /// An overridden constant's clause: `[fixed by jurisdiction overlay:
    /// Example Standard 12:2021 NA, NA.2.3]`.
    ///
    /// Present whether or not the overlay cited anything. The body of the line
    /// -- `k_s = 863/1000` -- reads exactly as a variable the specimen supplied,
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

    /// What jurisdictions' overlays did to the variants before one was
    /// selected, as further clauses of the variant's bracket: the prunes,
    /// `; 1 of 3 pruned by jurisdiction overlay: ...` -- or, when overlays
    /// pruned more than one, `; 2 of 3 pruned, the last by jurisdiction
    /// overlay: ...`, naming what the last one cited -- and then a pin, `;
    /// pinned by jurisdiction overlay: ...`. Both when one jurisdiction
    /// pruned and a later one pinned; one overlay cannot do both. Empty when
    /// none pinned or pruned.
    [[nodiscard]] inline std::string variant_narrowing_clause(Step<Rational> const& recorded)
    {
        std::string narrowingText;
        if (recorded.variantPrunedCount > 0)
            narrowingText += "; " + std::to_string(recorded.variantPrunedCount) + " of "
                             + std::to_string(recorded.variantCount)
                             + (recorded.variantPrunedCount == 1 ? " pruned by " : " pruned, the last by ")
                             + overlay_source_text(recorded.variantPrunedBy);
        if (recorded.variantPinned)
            narrowingText += "; pinned by " + overlay_source_text(recorded.citation);
        return narrowingText;
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
            return " [the " + ordinalPosition + " variants, selected by a tag whose name could not be read"
                   + variant_narrowing_clause(recorded) + "]";
        return " [variant " + std::string { recorded.variantTag } + " (" + ordinalPosition + "), selected by tag"
               + variant_narrowing_clause(recorded) + "]";
    }

    /// @p storedValue -- a step's own, or one element of a series step's -- converted
    /// from the coherent SI unit of @p recorded's dimension into the unit the
    /// step was declared in, with that unit's symbol, or `(not measured)` when
    /// it is empty. Shared by `step_value_text` and `series_step_line`, so that
    /// an element of a series reads exactly as a single value of the same
    /// quantity does.
    [[nodiscard]] inline std::string value_in_declared_unit(Step<Rational> const& recorded,
                                                            std::optional<Rational> const& storedValue)
    {
        if (!storedValue.has_value())
            return "(not measured)";

        std::expected<Rational, ArithmeticError> const shown =
            checked_convert(*storedValue, coherent(recorded.dimension), recorded.unit);
        // Unreachable for a `Step` the recorder built -- it records a unit of
        // the step's own dimension -- but a `Step` is a public aggregate and a
        // caller may fill one in by hand. Refusing to print is the only
        // honest answer: the alternative is a number in a scale the line
        // claims it is not in.
        if (!shown)
            return "(not shown: " + std::string { describe(shown.error()) } + ")";

        std::string valueText = number_text(*shown);
        std::string const unitSymbol = unit_symbol_text(recorded.unit);
        if (!unitSymbol.empty())
            valueText += " " + unitSymbol;
        return valueText;
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
        return value_in_declared_unit(recorded, recorded.value);
    }

    /// Whether @p kind is a series step, whose values are `Step::elements` and
    /// never `Step::value`. One place, for the reason `is_lookup` gives.
    [[nodiscard]] constexpr bool is_series(StepKind stepKind) noexcept
    {
        return stepKind == StepKind::SeriesVariable || stepKind == StepKind::SeriesConstant
               || stepKind == StepKind::ElementwiseNegate || stepKind == StepKind::ElementwiseAdd
               || stepKind == StepKind::ElementwiseSubtract || stepKind == StepKind::ElementwiseMultiply
               || stepKind == StepKind::ElementwiseDivide || stepKind == StepKind::CumulativeSum
               || stepKind == StepKind::ElementwiseRound || stepKind == StepKind::SeriesDomain
               || stepKind == StepKind::ObservationsVariable || stepKind == StepKind::Binning;
    }

    /// Where a series step's failure arose, counted from one: `at element 3`,
    /// or `at observation 3` when `Step::failureSite` says the position is an
    /// observation -- raw observations and a binning. A binning
    /// that found no class for it says which, and what the classes cover:
    /// `[331 m in no class; the classes cover 0 to under 331 m]`.
    [[nodiscard]] inline std::string failed_position_text(Step<Rational> const& recorded)
    {
        if (!recorded.failedElement.has_value())
            return {};
        std::size_t const failedAt = *recorded.failedElement;
        bool const namesObservation = recorded.failureSite == FailureSite::InputObservation;
        std::string positionText = (namesObservation ? " at observation " : " at element ") + std::to_string(failedAt + 1);
        if (recorded.kind != StepKind::Binning || recorded.error != ArithmeticError::DomainError
            || failedAt >= recorded.domainElements.size() || !recorded.domainElements[failedAt].has_value()
            || !recorded.coveredRange.has_value())
            return positionText;
        Step<Rational> observationShape {};
        observationShape.dimension = recorded.sourceUnit.dimension;
        observationShape.unit = recorded.sourceUnit;
        std::string const keySymbol = unit_symbol_text(recorded.sourceUnit);
        return positionText + " [" + value_in_declared_unit(observationShape, recorded.domainElements[failedAt])
               + " in no class; the classes cover " + half_open_range_text(*recorded.coveredRange, keySymbol) + "]";
    }

    /// A series step's line, without its number: the expression, an `=`, and
    /// the elements in order, separated by `; ` -- as many as @p budget
    /// allows. Each element shown spends one unit of @p budget, and a list cut
    /// short ends `... k more`, where `k` is exactly the number left out, so a
    /// truncated series never reads as a complete one.
    ///
    /// A failed series shows its error and, when the failure belongs to one
    /// element, that element counted from one: `overflow in exact arithmetic
    /// at element 3` for the element at zero-based position 2.
    ///
    /// Reads `Step::elements` and never `Step::value`, which a series step
    /// leaves empty: consulting it would print `(not measured)` for a series
    /// every element of which was measured.
    [[nodiscard]] inline std::string series_step_line(Step<Rational> const& recorded, std::size_t& budget)
    {
        // A per-element constant's line is its values alone, as a scalar
        // constant's is its value alone: `1 kg; 2 kg`, not the tautology
        // `values = 1 kg; 2 kg`.
        bool const listsItself = recorded.kind == StepKind::SeriesConstant || recorded.kind == StepKind::SeriesDomain;
        std::string lineText = listsItself ? std::string {} : step_expression(recorded) + " = ";
        if (recorded.error.has_value())
            return lineText + std::string { describe(*recorded.error) } + failed_position_text(recorded);
        std::size_t const elementCount = recorded.elements.size();
        if (elementCount == 0)
            return lineText + "(no elements)";

        std::size_t const listed = budget < elementCount ? budget : elementCount;
        budget -= listed;
        for (std::size_t at = 0; at < listed; ++at)
        {
            if (at > 0)
                lineText += "; ";
            lineText += value_in_declared_unit(recorded, recorded.elements[at]);
        }
        if (listed < elementCount)
            lineText += std::string { listed > 0 ? "; " : "" } + "... " + std::to_string(elementCount - listed) + " more";
        return lineText;
    }

    /// A curve step's line, without its number: the expression, an `=`, and
    /// each point with its value, `7/10 m: 894/25 %`, separated by `; ` --
    /// as many pairs as @p budget allows, one unit each, as a series step's
    /// elements are (`series_step_line`), and `... k more` where `k` is
    /// exactly the number left out. The points are shown in `sourceUnit`, the
    /// values in `unit`.
    ///
    /// A curve's point @p point, in the step's `sourceUnit`.
    [[nodiscard]] inline std::string curve_point_text(Step<Rational> const& recorded, std::optional<Rational> const& point)
    {
        // A point is shown as a value of the point's own dimension and unit.
        Step<Rational> pointShape {};
        pointShape.dimension = recorded.sourceUnit.dimension;
        pointShape.unit = recorded.sourceUnit;
        return value_in_declared_unit(pointShape, point);
    }

    /// The rule a failed curve broke and the point it broke it at:
    /// `[duplicate domain point 163 m]`, `[domain does not ascend at 113 m]`
    /// or `[breaks non-decreasing at 103 m]`. Nothing when the step names no rule
    /// or holds no point at its failed element.
    [[nodiscard]] inline std::string curve_break_suffix(Step<Rational> const& recorded)
    {
        if (recorded.curveBreak == CurveBreak::None || !recorded.failedElement.has_value()
            || *recorded.failedElement >= recorded.domainElements.size()
            || !recorded.domainElements[*recorded.failedElement].has_value())
            return {};
        std::string const pointText = curve_point_text(recorded, recorded.domainElements[*recorded.failedElement]);
        switch (recorded.curveBreak)
        {
            case CurveBreak::DuplicatePoint:
                return " [duplicate domain point " + pointText + "]";
            case CurveBreak::NotAscending:
                return " [domain does not ascend at " + pointText + "]";
            case CurveBreak::AgainstDirection:
                return " [breaks " + std::string { describe(recorded.monotone) } + " at " + pointText + "]";
            case CurveBreak::None:
                break;
        }
        return {};
    }

    /// A failed curve shows its error and, when it belongs to one element,
    /// that element counted from one, then the rule it broke there and the
    /// point (`curve_break_suffix`).
    [[nodiscard]] inline std::string curve_step_line(Step<Rational> const& recorded, std::size_t& budget)
    {
        std::string lineText = step_expression(recorded) + " = ";
        if (recorded.error.has_value())
        {
            lineText += describe(*recorded.error);
            if (recorded.failedElement.has_value())
                lineText += " at element " + std::to_string(*recorded.failedElement + 1) + curve_break_suffix(recorded);
            return lineText;
        }
        std::size_t const pairCount = recorded.elements.size();
        if (pairCount == 0 || recorded.domainElements.size() != pairCount)
            return lineText + "(no points)";

        std::size_t const listed = budget < pairCount ? budget : pairCount;
        budget -= listed;
        for (std::size_t at = 0; at < listed; ++at)
        {
            if (at > 0)
                lineText += "; ";
            lineText += curve_point_text(recorded, recorded.domainElements[at]) + ": "
                        + value_in_declared_unit(recorded, recorded.elements[at]);
        }
        if (listed < pairCount)
            lineText += std::string { listed > 0 ? "; " : "" } + "... " + std::to_string(pairCount - listed) + " more";
        return lineText;
    }

    /// An interpolation along a curve's clause: the two points the answer
    /// lay between, `[between 163 and 197 m]`, or `[on the row at
    /// 163 m]` -- `segment_text`, an interpolating lookup's words -- and on a miss
    /// `[outside the curve, which runs 103 to 241 m]`. Nothing when
    /// nothing was located: a failed or absent curve or point.
    [[nodiscard]] inline std::string curve_interpolation_suffix(Step<Rational> const& recorded)
    {
        std::string const pointSymbol = unit_symbol_text(recorded.sourceUnit);
        if (recorded.selectedSegment.has_value())
            return " [" + segment_text(*recorded.selectedSegment, pointSymbol) + "]";
        if (recorded.coveredRange.has_value())
            return " [outside the curve, which runs " + closed_range_text(*recorded.coveredRange, pointSymbol) + "]";
        return {};
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

    /// A snap step's clause: the two neighbours, `[127 m to 163 m;
    /// nearer 127 m]`, or with the tie rule when it decided, `[127 m to 163 m;
    /// tie, toward higher]`; `[on 127 m]` for an exact hit; and on a miss
    /// `[outside the permitted set, 103 m to 241 m]`. Nothing
    /// when nothing was snapped -- a failed or absent operand.
    [[nodiscard]] inline std::string snap_suffix(Step<Rational> const& recorded)
    {
        std::string const keySymbol = unit_symbol_text(recorded.unit);
        if (recorded.selectedSegment.has_value())
        {
            Segment const& neighbours = *recorded.selectedSegment;
            std::string const lowText =
                number_with_unit(declared_number_text(neighbours.low.numerator, neighbours.low.denominator), keySymbol);
            std::string const highText =
                number_with_unit(declared_number_text(neighbours.high.numerator, neighbours.high.denominator), keySymbol);
            if (neighbours.low == neighbours.high)
                return " [on " + lowText + "]";
            if (recorded.tieBroken)
                return " [" + lowText + " to " + highText + "; tie, " + std::string { describe(recorded.snapTie) } + "]";
            std::string const nearer =
                recorded.value.has_value() ? value_in_declared_unit(recorded, recorded.value) : std::string { "neither" };
            return " [" + lowText + " to " + highText + "; nearer " + nearer + "]";
        }
        if (recorded.coveredRange.has_value())
        {
            LookupRange const& covered = *recorded.coveredRange;
            return " [outside the permitted set, "
                   + number_with_unit(declared_number_text(covered.lowNumerator, covered.lowDenominator), keySymbol) + " to "
                   + number_with_unit(declared_number_text(covered.highNumerator, covered.highDenominator), keySymbol) + "]";
        }
        return {};
    }

    /// The rows @p trace kept for the step at @p stepIndex, or none.
    [[nodiscard]] inline std::span<LimitRow const> conformity_limits_of(Trace<Rational> const& trace, std::size_t stepIndex)
    {
        for (ConformityLimits const& kept: trace.conformityLimits)
            if (kept.step == stepIndex)
                return kept.rows;
        return {};
    }

    /// One element's outcome in a conformity step, counted from one, with
    /// the value judged in the check's unit and the row it was judged against
    /// when the trace kept them: `2 satisfied, 36 % (from 30 to 40 %)`, `2
    /// violated, 71 % (at least 60 %): reject the specimen`, `2 not checked
    /// (...)`, or `2 invalid, 71 % (from 80 to 70 %): <the arithmetic
    /// error>`. An element not measured states no value, and neither does
    /// one whose subject failed.
    [[nodiscard]] inline std::string element_outcome_text(std::size_t at,
                                                          ConstraintOutcome const& checkedOutcome,
                                                          std::string const& valueClause,
                                                          std::string const& rowClause)
    {
        std::string const ordinal = std::to_string(at + 1);
        switch (checkedOutcome.kind())
        {
            case ConstraintOutcomeKind::Satisfied:
                return ordinal + " satisfied" + valueClause + rowClause;
            case ConstraintOutcomeKind::Violated:
                return ordinal + " violated" + valueClause + rowClause + ": "
                       + std::string { checkedOutcome.verdict()->label };
            case ConstraintOutcomeKind::NotChecked:
                return ordinal + " not checked" + valueClause + rowClause;
            case ConstraintOutcomeKind::Invalid:
                return ordinal + " invalid" + valueClause + rowClause + ": "
                       + std::string { describe(*checkedOutcome.error()) };
        }
        return ordinal + " unknown outcome" + rowClause;
    }

    /// A conformity step's line, without its number: `conform(#1)` and every
    /// element's outcome in one bracket, each with the value judged, in the
    /// check's unit, and the row it was judged against -- `[1 satisfied, 36 %
    /// (from 30 to 40 %); 2 violated, 55 % (from 50 to 60 %): reject the
    /// specimen; ...]` -- as many as @p budget allows, one
    /// unit each, as a series step's elements are (`series_step_line`), and
    /// `... k more` where `k` is exactly the number left out.
    ///
    /// @p limits are the rows `Trace::conformityLimits` kept for this step.
    /// The rows are master data read at run time, so a derivation that
    /// omitted them would not say what was judged; a hand-built trace with
    /// no row for an element prints the outcome alone.
    [[nodiscard]] inline std::string conformity_line(Step<Rational> const& recorded,
                                                     std::span<LimitRow const> limits,
                                                     std::size_t& budget)
    {
        std::size_t const outcomeCount = recorded.elementOutcomes.size();
        std::size_t const listed = budget < outcomeCount ? budget : outcomeCount;
        budget -= listed;
        std::string lineText = step_expression(recorded) + " [";
        for (std::size_t at = 0; at < listed; ++at)
        {
            if (at > 0)
                lineText += "; ";
            std::string const rowClause = at < limits.size()
                                              ? " (" + limit_row_text(limits[at], unit_symbol_text(recorded.unit)) + ")"
                                              : std::string {};
            std::string const valueClause = at < recorded.elements.size() && recorded.elements[at].has_value()
                                                ? ", " + value_in_declared_unit(recorded, recorded.elements[at])
                                                : std::string {};
            lineText += element_outcome_text(at, recorded.elementOutcomes[at], valueClause, rowClause);
        }
        if (listed < outcomeCount)
            lineText += std::string { listed > 0 ? "; " : "" } + "... " + std::to_string(outcomeCount - listed) + " more";
        return lineText + "]";
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
    /// for the three rounding kinds, the granularity, provenance and tie rule
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
    ///
    /// **The one entry point for every step's line, a series included**, is
    /// `step_line` below, which escapes the step and renders the copy here.
    /// @p budget is what is left of `render_trace`'s `maxSteps` after this
    /// line's own unit: a series spends one more unit on each element it shows
    /// (`series_step_line`), and every other kind spends nothing here. Keeping
    /// every kind on this path is what lets one change reach every line --
    /// escaping author text, above all: the vocabulary symbol and the unit
    /// symbol a series line prints are escaped wherever a scalar line's are.
    ///
    /// @p limits are the rows a `ConformityChecked` step judged against
    /// (`Trace::conformityLimits`), and empty for every other kind.
    ///
    /// Renders an `EscapedStep`'s copy, never the step itself -- see
    /// `step_line`, which makes it.
    [[nodiscard]] inline std::string escaped_step_line(Trace<Rational> const& trace,
                                                       std::size_t stepIndex,
                                                       Step<Rational> const& recorded,
                                                       std::size_t& budget,
                                                       std::span<LimitRow const> limits)
    {
        // A series first, before anything reads `value`: its values are its
        // elements.
        // A per-element rounding ends with its mode, as a scalar rounding
        // does -- after the elements, however many were shown.
        if (recorded.kind == StepKind::ElementwiseRound)
            return series_step_line(recorded, budget) + rounding_mode_suffix(recorded.mode);
        // A conformity check has outcomes, not a value, and spends the
        // element budget on them as a series does on its elements.
        if (recorded.kind == StepKind::ConformityChecked)
            return conformity_line(recorded, limits, budget);
        if (is_series(recorded.kind))
            return series_step_line(recorded, budget);
        // A curve's values are its pairs, which spend the element budget as
        // a series' elements do.
        if (recorded.kind == StepKind::CurvePairing || recorded.kind == StepKind::CurveSplice)
            return curve_step_line(recorded, budget);
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
        else if (recorded.kind == StepKind::Round || recorded.kind == StepKind::RoundSignificant
                 || recorded.kind == StepKind::RoundedRoot)
            annotation = rounding_mode_suffix(recorded.mode);
        else if (recorded.kind == StepKind::RoundingRuleApplied)
            annotation = rounding_rule_suffix(recorded);
        else if (recorded.kind == StepKind::OverriddenConstant)
            annotation = overridden_constant_suffix(recorded.citation);
        else if (recorded.kind == StepKind::SnappedToPermitted)
            annotation = snap_suffix(recorded);
        else if (recorded.kind == StepKind::CurveInterpolation)
            annotation = curve_interpolation_suffix(recorded);
        else if (recorded.kind == StepKind::DerivedQuantity)
            annotation = derived_quantity_suffix(recorded.citation);
        else if (recorded.kind == StepKind::ReplacedVariant)
            annotation = replaced_variant_suffix(recorded.citation);
        // Present for a lookup that succeeded as well as for one that failed,
        // unlike the three suffixes above: on a hit it names the band the
        // value fell in, and on a failure it is the only thing separating a
        // miss from a relayed error. See `lookup_suffix`.
        else if (is_lookup(recorded.kind))
            annotation = lookup_suffix(trace, stepIndex, recorded);
        else if (recorded.kind == StepKind::PrecisionLevel)
            annotation = precision_suffix(trace, stepIndex);

        if (recorded.kind == StepKind::Constant)
            return valueText + annotation;
        if (recorded.kind == StepKind::PrecisionLevel || recorded.kind == StepKind::PrecisionLimit)
            return precision_expression(trace, stepIndex, recorded) + " = " + valueText + annotation;
        return step_expression(recorded) + " = " + valueText + annotation;
    }

    /// One step's line, without its number -- see `escaped_step_line` for
    /// what it holds. The one place author text is escaped: every piece of it
    /// in @p recorded is escaped into an `EscapedStep` here, before anything
    /// reads it, and the line is rendered from that copy.
    ///
    /// @p limits are the rows a `ConformityChecked` step judged against
    /// (`Trace::conformityLimits`), and empty for every other kind.
    [[nodiscard]] inline std::string step_line(Trace<Rational> const& trace,
                                               std::size_t stepIndex,
                                               std::size_t& budget,
                                               std::span<LimitRow const> limits = {})
    {
        EscapedStep const escaped { trace.steps[stepIndex] };
        return escaped_step_line(trace, stepIndex, escaped.step, budget, limits);
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
/// **A series step spends the same budget.** Its line costs one unit, as
/// every line does, and each element it shows one more; a series cut short
/// ends `... k more`, with `k` the exact number of elements left out, and the
/// footer then counts the steps not shown at all. One number, chosen by the
/// caller, bounds everything printed: a 256-element series printed in full on
/// one line is the unusable line the limit exists to prevent.
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

    std::size_t budget = options.maxSteps.value;
    std::size_t shown = 0;

    std::string renderedTrace;
    while (shown < trace.steps.size() && budget > 0)
    {
        --budget;
        renderedTrace += std::to_string(shown + 1);
        renderedTrace += ". ";
        renderedTrace += detail::step_line(trace, shown, budget, detail::conformity_limits_of(trace, shown));
        renderedTrace += "\n";
        ++shown;
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
