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
///    value in the coherent unit of its dimension so that steps are
///    comparable, and remembers the unit it was *declared* in; this converts
///    back before showing a number, so a volume entered as 180 l reads
///    `180 l` and not `9/50`.
///  - It never shows an approximation as exact. Numbers are fractions unless
///    `TraceRenderOptions::numbers` asks for decimals, and a decimal is shown
///    only where it is the exact value -- `3/5` reads `0.6`, `1/3` stays
///    `1/3` -- unless the caller asks for an approximation by naming its
///    rounding mode, and then the rounded value carries `≈`. Some numbers
///    are never rounded even then: a number typed rather than computed -- a
///    constant, a table's row or bound, a permitted value, a limit, and a
///    step that only passes one on -- and either side of a comparison a line
///    states beside its verdict. A value in a unit nobody declared, a
///    computed product or ratio, is never padded with zeros.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/number_text.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/unit.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

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

    /// How every number a line states is spelled. Fractions unless a caller asks.
    ///
    /// `NumberStyle::exact_decimal()` writes a decimal wherever that is the
    /// exact value and the fraction elsewhere; `approximate_decimal(mode)`
    /// rounds the rest in `mode` at the unit's declared decimals and marks
    /// each with `ApproximationMarker`. Whatever the style, a number typed
    /// rather than computed, and either side of a comparison a line states,
    /// are shown exact (`NumberStyle::exact_only`), and a value in a unit
    /// nobody declared is never padded; where its default 3 places would
    /// round a value other than zero to `≈0`, they are extended to its first
    /// significant digit, up to 18 (`checked_shown_text`). A value a formula
    /// rounded itself -- `rounded`, `rounded_sqrt`, `rounded_ln`,
    /// `rounded_log10`, `rounded_exp`, `rounded_output` -- is the step's exact
    /// value, a decimal, so it reads without `≈` in every style, its mode in
    /// brackets after it. A value the style cannot spell in its
    /// unit -- one padded or rounded in a unit whose declared decimals lie
    /// outside -18 to 18, say -- reads `(not shown: ...)`, as a value its unit
    /// cannot show does; a bound or a limit the author typed falls back to its
    /// exact fraction instead.
    ///
    /// `render_derivation` spells a worksheet's derivation in it too: its
    /// headers, its steps and its inputs.
    ///
    /// Defaulted, unlike `maxSteps`: fractions are how a trace has always
    /// read, and a caller who names no style gets exactly that. The default
    /// does not reopen `{}`: `maxSteps` still has to be stated.
    NumberStyle numbers = NumberStyle::fraction();
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
    /// false owner beside the true one. The backslash, so that an author's
    /// `\[` cannot pass for an escaped bracket.
    ///
    /// **What it does not do.** It stops author text from breaking a line's
    /// structure, not from holding a clause's words: a `documented()` citation
    /// titled `replaced by jurisdiction overlay: ...` renders as a genuine
    /// replacement's clause does, and one titled `inside not shown` writes the
    /// opaque marker onto a step that is not opaque. The structured fields of a
    /// `Step` -- `kind`, the provenance enums, `variantPinned` and so on -- are
    /// what is authoritative, and the method's author is trusted. Nor does it touch
    /// anything but ASCII: a Unicode look-alike of a bracket (U+FF3B, U+FF3D)
    /// or the line separator U+2028 is written as it is, since it cannot
    /// break the ASCII structure the library writes.
    ///
    /// Applied to every piece of author text a trace line states, on the way
    /// from `step_line`: once to each such field of the step and of a citation,
    /// in `EscapedStep` and `EscapedCitation`, with `unit_symbol_text` and, for
    /// a record's role and a lineage attribute's name, `tag_words`; and
    /// directly by the lines that state author text the step does not hold --
    /// a rejection's verdict label (`rejection_line`), an opaque operation's
    /// and its outputs' names (`opaque_call_line`, `opaque_output_line`), a
    /// retry's verdict label (`retry_concluded_line`), a named base
    /// dimension's name (`coherent_unit_text`) and the symbol a derivation
    /// block's header names (`render_derivation`). The words this file writes
    /// itself go into the line as they are. A rendered formula in a
    /// derivation block's header is made safe by `line_safe_text` instead.
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
    /// held here, and pointed at by the copy's views. A record's role and a
    /// lineage attribute's name, held in library-built values the copy cannot
    /// rebuild, are escaped where they are written, by `tag_words`.
    ///
    /// `step_line` renders from this copy and never from the step it was
    /// given, so no helper below can print author text unescaped by reading
    /// the wrong field. Neither copyable nor movable: the copy's views point
    /// into this object's own strings.
    /// A step as its line shows it: the step, with what its trace's side
    /// tables hold for it -- the record it was read from
    /// (`Trace::origins`), and for a lineage attribute the comparison
    /// (`Trace::lineageChecks`). Every helper below that prints either reads
    /// it here, so none can look in the wrong trace.
    struct ShownStep: Step<Rational>
    {
        /// The record the step was read from; empty for this record's.
        std::optional<RecordOrigin> readFrom;
        /// For a `LineageChecked` step, its comparison; empty otherwise.
        std::optional<LineageCheck> comparison;
    };

    struct EscapedStep
    {
        EscapedStep(Step<Rational> const& recorded, std::optional<RecordOrigin> const& originRead,
                    std::optional<LineageCheck> const& lineageCompared):
            symbol { escaped_author_text(recorded.symbol) },
            justification { escaped_author_text(recorded.justification) },
            variantTag { escaped_author_text(recorded.variantTag) },
            lookupKeyName { escaped_author_text(recorded.lookupKeyName) },
            citation { recorded.citation },
            variantPrunedBy { recorded.variantPrunedBy },
            verdictLabel { recorded.outcome.verdict().has_value() ? escaped_author_text(recorded.outcome.verdict()->label)
                                                                  : std::string {} },
            step { recorded, originRead, lineageCompared }
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
        ShownStep step;
    };

    /// `#3` -- how a step refers to one of its operands. Steps are numbered
    /// from one in the rendered text, so this is the stored index plus one.
    [[nodiscard]] inline std::string operand_reference(std::size_t stepIndex)
    {
        return "#" + std::to_string(stepIndex + 1);
    }

    /// The infix spelling of a binary step, `#1 / #2`, with each side where
    /// it stood even when it has no step to name (`Step::leftOperand`,
    /// `Step::rightOperand`):
    ///
    ///  - `#5 / (not evaluated)`: a genuine short circuit -- the left operand
    ///    failed, so the evaluator never dispatched the right one. The one
    ///    operand is the left, the one that failed; dividing by zero itself
    ///    happens only after both sides have run, so a `Divide` that fails
    ///    that way names two.
    ///  - `#1 + (untraced)`, `(untraced) + (not evaluated)`: a side evaluated
    ///    by a consumer's node that records no step of its own (`sink.hpp`).
    ///
    /// The elementwise steps (`ElementwiseAdd` and the rest) share this
    /// spelling. When the sides do not account for the operands claimed --
    /// a step built by hand, or an untraced left whose node forwarded the
    /// sink -- or a side is `NotEvaluated` on a step that did not fail, it
    /// names the operands as claimed: in infix form when there
    /// are two, and otherwise in prefix form, `/ #5`, or a bare `/` when
    /// there are none.
    template <typename Rep>
    [[nodiscard]] std::string binary_expression(Step<Rep> const& step, std::string_view operatorText)
    {
        std::size_t const sidesRecorded = static_cast<std::size_t>(step.leftOperand == OperandSide::Recorded)
                                          + static_cast<std::size_t>(step.rightOperand == OperandSide::Recorded);
        // A side never evaluated means the step failed; a step that holds a
        // value says otherwise, and its sides are not taken at their word.
        bool const claimsShortCircuit =
            step.leftOperand == OperandSide::NotEvaluated || step.rightOperand == OperandSide::NotEvaluated;
        if (sidesRecorded < 2 && sidesRecorded == step.operands.size()
            && (!claimsShortCircuit || step.error.has_value()))
        {
            std::size_t named = 0;
            auto const sideText = [&](OperandSide side) -> std::string {
                switch (side)
                {
                    case OperandSide::Recorded:
                        return operand_reference(step.operands[named++]);
                    case OperandSide::Untraced:
                        return "(untraced)";
                    case OperandSide::NotEvaluated:
                        return "(not evaluated)";
                }
                return "(unknown)";
            };
            std::string const leftText = sideText(step.leftOperand);
            return leftText + " " + std::string { operatorText } + " " + sideText(step.rightOperand);
        }

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
    [[nodiscard]] inline std::string half_open_range_text(LookupRange const& lookupRange,
                                                          std::string_view keySymbol,
                                                          Unit const& keyUnit,
                                                          NumberStyle numberStyle)
    {
        return band_text(Band { lookupRange.lowNumerator,
                                lookupRange.lowDenominator,
                                lookupRange.highNumerator,
                                lookupRange.highDenominator },
                         keySymbol,
                         keyUnit,
                         numberStyle);
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
    [[nodiscard]] inline std::string closed_range_text(LookupRange const& lookupRange,
                                                       std::string_view keySymbol,
                                                       Unit const& keyUnit,
                                                       NumberStyle numberStyle)
    {
        return number_with_unit(
            declared_number_text(lookupRange.lowNumerator, lookupRange.lowDenominator, keyUnit, numberStyle) + " to "
                + declared_number_text(lookupRange.highNumerator, lookupRange.highDenominator, keyUnit, numberStyle),
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
    [[nodiscard]] inline std::string segment_text(Segment const& lookupSegment,
                                                  std::string_view keySymbol,
                                                  Unit const& keyUnit,
                                                  NumberStyle numberStyle)
    {
        std::string const lowText =
            declared_number_text(lookupSegment.low.numerator, lookupSegment.low.denominator, keyUnit, numberStyle);
        std::string const highText =
            declared_number_text(lookupSegment.high.numerator, lookupSegment.high.denominator, keyUnit, numberStyle);
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
        auto const match =
            std::lower_bound(records.begin(), records.end(), stepIndex, [](Record const& each, std::size_t wanted) {
                return each.step < wanted;
            });
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
    ///
    /// @p keySymbol is `recorded.sourceUnit`'s symbol, escaped; the table's
    /// bounds are numbers in that unit.
    [[nodiscard]] inline std::string lookup_miss_text(Trace<Rational> const& trace,
                                                      std::size_t stepIndex,
                                                      Step<Rational> const& recorded,
                                                      std::string_view keySymbol,
                                                      NumberStyle numberStyle)
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

        Unit const& keyUnit = recorded.sourceUnit;
        if (recorded.kind == StepKind::BandedLookup)
            return "in no band; the bands cover "
                   + half_open_range_text(*recorded.coveredRange, keySymbol, keyUnit, numberStyle);

        // A curve with exactly one row covers that one key and nothing else,
        // and "runs 15/2 to 15/2 mm" would describe it as a range it is not.
        // `at <key>` is the spelling `render()` gives a breakpoint, for the
        // same reason: a row is a point.
        std::string const lowText = declared_number_text(
            recorded.coveredRange->lowNumerator, recorded.coveredRange->lowDenominator, keyUnit, numberStyle);
        std::string const highText = declared_number_text(
            recorded.coveredRange->highNumerator, recorded.coveredRange->highDenominator, keyUnit, numberStyle);
        if (lowText == highText)
            return "outside the curve, whose only row is at " + number_with_unit(lowText, keySymbol);
        return "outside the curve, which runs " + closed_range_text(*recorded.coveredRange, keySymbol, keyUnit, numberStyle);
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
                                                   Step<Rational> const& recorded,
                                                   NumberStyle numberStyle)
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
                    return " [" + band_text(*recorded.selectedBand, keySymbol, recorded.sourceUnit, numberStyle) + "]";
                if (recorded.selectedSegment.has_value())
                    return " [" + segment_text(*recorded.selectedSegment, keySymbol, recorded.sourceUnit, numberStyle)
                           + "]";
                return {};
            case LookupFailure::Missed:
                return " [" + lookup_miss_text(trace, stepIndex, recorded, keySymbol, numberStyle) + "]";
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
                return " [no row for n = "
                       + (recorded.operands.empty() ? std::string { "the count" } : sole_operand(recorded))
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

    /// A role's or a lineage attribute's name as a trace line writes it,
    /// escaped by `escaped_author_text` as every other piece of author text
    /// in a line is. The **one** place such a name -- author text, from
    /// `tag_name` -- enters a trace line: the scope's own line and the origin
    /// clause on a quantity's line come through `record_origin_text`, a
    /// lineage step through `lineage_expression`, and both through here, and
    /// all three are reached only from `step_line`.
    ///
    /// Escaped here rather than in `EscapedStep`, because a `RecordOrigin` and
    /// a `LineageCheck` are built only by the library (`record.hpp`), so the
    /// copy cannot hold escaped ones. A role's name is identifier-like
    /// (`RequireIdentifierLikeRoleName`), so only an attribute's name -- a
    /// tag with a `TagName` of the author's -- can hold what this escapes.
    [[nodiscard]] inline std::string tag_words(std::string_view tagName)
    {
        return escaped_author_text(tagName);
    }

    /// A role and which record played it: `Reference (sample 23, test 3)`,
    /// or `Reference (no record bound)`.
    [[nodiscard]] inline std::string role_and_record_text(RecordOrigin const& played)
    {
        std::string roleText = tag_words(played.role());
        std::optional<RecordKey> const recordKey = played.key();
        if (!recordKey.has_value())
            return roleText + " (no record bound)";
        return roleText + " (sample " + std::to_string(recordKey->sample().value()) + ", test "
               + std::to_string(recordKey->test().value()) + ")";
    }

    /// Which record a value was read from, in words: `from record Reference
    /// (sample 23, test 3)`, or `from record Reference (no record bound)`.
    /// Both keys, always: two tests of one sample share the sample key, so a
    /// sample alone could name either. No bracket, so that a trace pasted
    /// into Markdown stays plain text.
    [[nodiscard]] inline std::string record_origin_text(RecordOrigin const& readFrom)
    {
        return "from record " + role_and_record_text(readFrom);
    }

    /// One lineage attribute's comparison, each key named by whose it is:
    /// `same TestMethod as this record: 12 for this record, 13 for
    /// Reference`, and against another role, which record played it --
    /// `same MaterialBatch as PriorTest (sample 17, test 3): 4412 for
    /// PriorTest, 4411 for Reference`, or `as PriorTest (no record bound)`,
    /// so that an unbound comparand's `unknown` is not taken for a record
    /// that states no key. The verdict follows in the line.
    [[nodiscard]] inline std::string lineage_expression(ShownStep const& recorded)
    {
        if (!recorded.comparison.has_value())
            return "same lineage";
        LineageCheck const& compared = *recorded.comparison;
        auto const keyText = [](std::optional<std::uint64_t> lineageKey) {
            return lineageKey.has_value() ? std::to_string(*lineageKey) : std::string { "unknown" };
        };
        bool const againstThisRecord = compared.is_against_this_record();
        std::string const comparandName =
            againstThisRecord ? std::string { "this record" } : tag_words(compared.comparand());
        std::string const comparandText =
            againstThisRecord ? comparandName : role_and_record_text(compared.comparand_record());
        return "same " + tag_words(compared.attribute()) + " as " + comparandText + ": "
               + keyText(compared.comparand_key()) + " for " + comparandName + ", " + keyText(compared.subject_key())
               + " for " + tag_words(compared.subject());
    }

    /// `round(#1, to 2 dp of mm)`: @p inner rounded to @p granularity decimal places of the unit whose
    /// symbol is @p unitSymbolText, in `render()`'s words (`rounding_call`), for every step that rounds to
    /// decimal places. No unit clause for a unit with no symbol.
    [[nodiscard]] inline std::string rounding_call_text(std::string const& inner,
                                                        int granularity,
                                                        std::string const& unitSymbolText)
    {
        return rounding_call<Dialect::Plain>(inner, DecimalPlaces { granularity }, unitSymbolText);
    }

    /// `round(ln(#1), to 4 dp)`: `render()`'s spelling, one step with the function inside it, because the
    /// unrounded value was never a value. The function is named in `render()`'s words
    /// (`transcendental_text`), so that a derivation names the function its formula names. No unit
    /// clause: the node rounds a pure number.
    [[nodiscard]] inline std::string rounded_transcendental_expression(Transcendental function, ShownStep const& shownStep)
    {
        return rounding_call_text(
            transcendental_text<Dialect::Plain>(function, sole_operand(shownStep)), shownStep.granularity, {});
    }

    /// What a step computed, written in terms of the steps it consumed.
    ///
    /// A `Constant` is absent from this deliberately: a constant's expression
    /// *is* its value, so `render_trace` writes the value alone rather than
    /// the tautology `0 = 0`.
    [[nodiscard]] inline std::string step_expression(ShownStep const& shownStep)
    {
        switch (shownStep.kind)
        {
            // An overridden constant reads as its quantity, as it does in
            // `render()`; that the overlay fixed it goes in the suffix -- see
            // `overridden_constant_suffix`.
            case StepKind::Variable:
            case StepKind::OverriddenConstant:
                return std::string { shownStep.symbol };
            // The quantity, equal to the step its definition produced:
            // `k_s = #3`, so that the line reads `k_s = #3 = 863/1000`. That it
            // is a jurisdiction's definition goes in the suffix -- see
            // `derived_quantity_suffix`. With no step to name -- an untraced
            // consumer node as the whole definition -- the quantity alone.
            case StepKind::DerivedQuantity:
                return shownStep.operands.empty() ? std::string { shownStep.symbol }
                                                  : std::string { shownStep.symbol } + " = " + sole_operand(shownStep);
            // Its operand, as `Documented`'s is: the replacement computed the
            // value; the step says only whose formula it was.
            case StepKind::ReplacedVariant:
                return sole_operand(shownStep);
            case StepKind::Constant:
                return {};
            case StepKind::PiConstant:
                return "pi";
            case StepKind::Negate:
                return "-" + sole_operand(shownStep);
            case StepKind::Add:
                return binary_expression(shownStep, "+");
            case StepKind::Subtract:
                return binary_expression(shownStep, "-");
            case StepKind::Multiply:
                return binary_expression(shownStep, "*");
            case StepKind::Divide:
                return binary_expression(shownStep, "/");
            case StepKind::Power:
                return sole_operand(shownStep) + "^" + std::to_string(shownStep.exponent);
            case StepKind::Root:
                return shownStep.exponent == 2
                           ? "sqrt(" + sole_operand(shownStep) + ")"
                           : "root" + std::to_string(shownStep.exponent) + "(" + sole_operand(shownStep) + ")";
            // `render()`'s words, so that a derivation names the function its
            // formula names.
            case StepKind::NaturalLogarithm:
                return transcendental_text<Dialect::Plain>(Transcendental::NaturalLogarithm, sole_operand(shownStep));
            case StepKind::DecimalLogarithm:
                return transcendental_text<Dialect::Plain>(Transcendental::DecimalLogarithm, sole_operand(shownStep));
            case StepKind::Exponential:
                return transcendental_text<Dialect::Plain>(Transcendental::Exponential, sole_operand(shownStep));
            case StepKind::Documented:
                return sole_operand(shownStep);
            // Its operand, exactly as `Documented`'s is: the selection chose
            // which formula ran, and computed nothing of its own. What it
            // chose goes in the suffix -- see `variant_suffix`.
            case StepKind::VariantSelected:
                return sole_operand(shownStep);
            case StepKind::Round:
                return rounding_call_text(sole_operand(shownStep), shownStep.granularity, unit_symbol_text(shownStep.unit));
            case StepKind::RoundSignificant:
                return "round(" + sole_operand(shownStep) + ", to " + std::to_string(shownStep.granularity) + " sf"
                       + unit_clause(" of ", unit_symbol_text(shownStep.unit)) + ")";
            // The unit only: the granularity belongs with whose rule it is,
            // in the suffix -- see `rounding_rule_suffix`.
            case StepKind::RoundingRuleApplied:
                return "round(" + sole_operand(shownStep) + unit_clause(", in ", unit_symbol_text(shownStep.unit)) + ")";
            case StepKind::NumericValue:
                return "numeric(" + sole_operand(shownStep) + unit_clause(", in ", unit_symbol_text(shownStep.sourceUnit))
                       + ")";
            case StepKind::Conditional:
                return conditional_expression(shownStep);
            case StepKind::Constraint:
                return constraint_expression(shownStep);
            case StepKind::AcceptanceChecked:
                return acceptance_expression(shownStep);
            // The head names are `render()`'s own, and the split between them
            // is the one `render.hpp` makes deliberately: the two *selecting*
            // kinds share `lookup`, and the one that *computes* a number
            // appearing in no row of its table is `interpolate`. A reader
            // checking a derivation against the formula it derives must meet
            // one name per kind, not two.
            case StepKind::BandedLookup:
                return "lookup(" + sole_operand(shownStep) + ")";
            // The key sits where the other two kinds' operand sits, because
            // it plays that part: it is what is being looked up. It is data
            // and not a sub-expression -- an exact lookup has no operand at
            // all -- which is exactly why this step has to carry it.
            case StepKind::ExactLookup:
                return "lookup(" + lookup_key_text(shownStep) + ")";
            case StepKind::InterpolatingLookup:
                return "interpolate(" + sole_operand(shownStep) + ")";
            // The quantity, unmarked: the marker belongs to the formula
            // (`render()`), and a derivation line names what it read. That it
            // is a series shows in the list of elements after the `=`.
            case StepKind::SeriesVariable:
                return std::string { shownStep.symbol };
            // A per-element constant's expression is its values, as a scalar
            // constant's is -- see `series_step_line`.
            case StepKind::SeriesConstant:
                return {};
            case StepKind::ElementwiseNegate:
                return "-" + sole_operand(shownStep);
            case StepKind::ElementwiseAdd:
                return binary_expression(shownStep, "+");
            case StepKind::ElementwiseSubtract:
                return binary_expression(shownStep, "-");
            case StepKind::ElementwiseMultiply:
                return binary_expression(shownStep, "*");
            case StepKind::ElementwiseDivide:
                return binary_expression(shownStep, "/");
            // The end is written, as `render()` writes it: a running total
            // without it is half a derivation.
            case StepKind::CumulativeSum:
                return "cumulative(" + sole_operand(shownStep) + ", "
                       + std::string { describe(shownStep.cumulativeDirection) } + ")";
            case StepKind::SeriesSum:
                return "sum(" + sole_operand(shownStep) + ")";
            // The subject; each element's outcome follows, in the bracket --
            // see `conformity_line`.
            case StepKind::ConformityChecked:
                return "conform(" + sole_operand(shownStep) + ")";
            // The set is `render()`'s to print; the step names where the
            // value landed in its suffix -- see `snap_suffix`.
            case StepKind::SnappedToPermitted:
                return "snap(" + sole_operand(shownStep) + ")";

            // `render()`'s head names; the sample's own shownStep, with every
            // element, is the operand.
            case StepKind::SampleCount:
                return "sample_count(" + sole_operand(shownStep) + ")";
            case StepKind::SampleMean:
                return "sample_mean(" + sole_operand(shownStep) + ")";
            case StepKind::SampleVariance:
                return "sample_variance(" + sole_operand(shownStep) + ")";
            case StepKind::SampleRange:
                return "sample_range(" + sole_operand(shownStep) + ")";
            // Every granularity, in the series' order, in the unit rounded
            // in, as `render()` writes it; the mode goes in the suffix, as
            // for `Round`.
            case StepKind::ElementwiseRound:
                return "round(" + sole_operand(shownStep) + ", to " + granularities_text(shownStep.elementGranularities)
                       + " dp" + unit_clause(" of ", unit_symbol_text(shownStep.unit)) + ")";
            // A declared domain's line is its points, as a per-element
            // constant's is its values -- see `series_step_line`.
            case StepKind::SeriesDomain:
                return {};
            // The two series paired, in order; the pairs follow the `=` --
            // see `curve_step_line`.
            case StepKind::CurvePairing:
                return "curve(" + operands_text(shownStep) + ")";
            // The curve, and where it was read; the segment goes in the
            // suffix -- see `curve_interpolation_suffix`.
            case StepKind::CurveInterpolation:
                return shownStep.operands.size() >= 2 ? "interpolate(" + operand_reference(shownStep.operands[0]) + ", at "
                                                            + operand_reference(shownStep.operands[1]) + ")"
                                                      : "interpolate(" + sole_operand(shownStep) + ")";
            // The direction is always written, as `render()` writes it.
            case StepKind::CurveSplice:
                return "splice(" + operands_text(shownStep) + ", " + std::string { describe(shownStep.monotone) } + ")";
            // The quantity, unmarked, as a series variable's is; the
            // observations follow the `=`.
            case StepKind::ObservationsVariable:
                return std::string { shownStep.symbol };
            // The classes are `render()`'s to print, as a lookup's bands are;
            // the counts follow the `=`.
            case StepKind::Binning:
                return "bin(" + sole_operand(shownStep) + ")";
            // `render()`'s spelling, `round(sqrt(...), to ...)`: one shownStep, and
            // the root inside it, because the root itself was never a value.
            case StepKind::RoundedRoot:
                return rounding_call_text(
                    "sqrt(" + sole_operand(shownStep) + ")", shownStep.granularity, unit_symbol_text(shownStep.unit));
            case StepKind::RoundedNaturalLogarithm:
                return rounded_transcendental_expression(Transcendental::NaturalLogarithm, shownStep);
            case StepKind::RoundedDecimalLogarithm:
                return rounded_transcendental_expression(Transcendental::DecimalLogarithm, shownStep);
            case StepKind::RoundedExponential:
                return rounded_transcendental_expression(Transcendental::Exponential, shownStep);
            // `render()`'s head name. The count is the subject, as a banded
            // lookup's operand is; which row it selected goes in the suffix.
            case StepKind::SampleSizeLookup:
                return "critical(" + sole_operand(shownStep) + ")";
            // `render()`'s spelling: `abs(...)`, never bars, which a Markdown
            // table cell would read as its own delimiter.
            case StepKind::AbsoluteValue:
                return "abs(" + sole_operand(shownStep) + ")";
            // Both read their side-table record, which only `step_line` can
            // reach; see `precision_expression`. Spelled here without it, for
            // a caller that has the step alone.
            case StepKind::PrecisionLevel:
                return "level";
            case StepKind::PrecisionLimit:
                return "precision limit";
            // Words, as `render()` writes them: a symbol could collide with
            // an author's own quantity's.
            case StepKind::PassMean:
                return "pass mean";
            case StepKind::PassCount:
                return "pass n";
            // Each reads its side-table record, which only `step_line` can
            // reach; see `rejection_line`. Spelled here without it.
            case StepKind::RejectionPass:
                return "rejection pass";
            case StepKind::OutlierRejected:
                return "rejected element";
            case StepKind::RejectionSettled:
                return "rejection settled";
            case StepKind::RejectionAborted:
                return "rejection aborted";
            case StepKind::RejectionFailed:
                return "rejection failed";
            case StepKind::RejectionUndecided:
                return "rejection undecided";
            // One lineage attribute compared: the attribute and both keys.
            case StepKind::LineageChecked:
                return lineage_expression(shownStep);
            // The derivation over the other record, then whose record it is.
            // With no record bound there is no operand to name: nothing was
            // read, and the line says so by origin alone.
            case StepKind::RecordScope:
                if (!shownStep.readFrom.has_value())
                    return shownStep.operands.empty() ? std::string { "from another record" }
                                                      : sole_operand(shownStep) + " from another record";
                return shownStep.operands.empty() ? record_origin_text(*shownStep.readFrom)
                                                  : sole_operand(shownStep) + " " + record_origin_text(*shownStep.readFrom);
            // The operation's name is in `Trace::opaqueSteps`, which the line
            // is rendered with (`opaque_call_line`); a step without its row --
            // one built by hand -- still reads as a call on its inputs.
            case StepKind::OpaqueOperation:
                return "opaque(" + operands_text(shownStep) + ")";
            case StepKind::OpaqueOutput:
                return shownStep.operands.empty() ? std::string { "an opaque output" }
                                                  : "output of " + sole_operand(shownStep);
            // `render()`'s spelling, as for a rounded root; the output's name is
            // in the call's row, which `rounded_opaque_output_line` reads.
            case StepKind::RoundedOpaqueOutput:
                return rounding_call_text(shownStep.operands.empty() ? std::string { "an opaque output" }
                                                                     : "output of " + sole_operand(shownStep),
                                          shownStep.granularity,
                                          unit_symbol_text(shownStep.unit));
            // A retry's steps name its result as `render()` does, `w(k)` for
            // an attempt's value and `w(k-1)` for the one before; the
            // attempt's and the retry's own lines are `retry_attempt_line` and
            // `retry_concluded_line`, which begin with these. A recorded
            // determination is `d(k)`, as `render()` writes it.
            case StepKind::RetryAttempt:
            case StepKind::ThisAttempt:
            case StepKind::AttemptInput:
                return attempt_marker<Dialect::Plain>(std::string { shownStep.symbol }, "k");
            case StepKind::PreviousAttempt:
                return attempt_marker<Dialect::Plain>(std::string { shownStep.symbol }, "k-1");
            case StepKind::AttemptNumber:
                return "k";
            case StepKind::RetryConcluded:
                return std::string { shownStep.symbol };
        }
        return "unknown step kind";
    }

    /// What a clause says in place of a citation that names nothing. Every
    /// operation of an overlay takes a citation argument, but an empty one
    /// compiles -- `pin_variant<Cube>({})`, or an operation's aggregate built
    /// directly -- and a clause that then read `fixed by jurisdiction overlay`
    /// would look cited to a reader who does not know it could have said more.
    inline constexpr std::string_view noCitationGiven = "(no citation given)";

    /// ` at element k` for a failed sample statistic, counted from one --
    /// ` at observation k` when the sample is raw observations, as
    /// `FailureSite::InputObservation` says of a series. Over a rejection, k
    /// counts the sample the rejection was given, and its record says how
    /// many that held and whether they were observations. ` at (no such
    /// element)` when the position is not one of the sample's: `Step` is a
    /// public aggregate, and a position past the end of the sample would
    /// name a determination nobody made.
    [[nodiscard]] inline std::string sample_failure_suffix(Trace<Rational> const& trace, Step<Rational> const& recorded)
    {
        std::size_t const at = *recorded.failedElement;
        if (recorded.operands.size() != 1 || recorded.operands.front() >= trace.steps.size())
            return " at (no such element)";
        std::size_t const sampleStep = recorded.operands.front();
        // A rejection's terminal step lists no elements: the position counts
        // the sample the rejection was given, whose size and kind its record
        // keeps.
        if (detail::RejectionRecord<Rational> const* const rejectionRecord =
                record_for_step(trace.rejectionRecords, sampleStep))
        {
            if (at >= rejectionRecord->originalSize)
                return " at (no such element)";
            return (rejectionRecord->ofObservations ? " at observation " : " at element ") + std::to_string(at + 1);
        }
        if (at >= trace.steps[sampleStep].elements.size())
            return " at (no such element)";
        bool const ofObservations = step_counts_observations(trace, sampleStep);
        return (ofObservations ? " at observation " : " at element ") + std::to_string(at + 1);
    }

    /// `#k` for a step a side-table record names, or `(no such step)` when
    /// the index is not one of @p trace's steps: a record is a public
    /// aggregate, and printing a number no line carries -- or one wrapped
    /// round by the `+ 1` -- would point the reader at a step that is not
    /// there.
    [[nodiscard]] inline std::string recorded_step_reference(Trace<Rational> const& trace, std::size_t recordedStep)
    {
        return recordedStep < trace.steps.size() ? operand_reference(recordedStep) : std::string { "(no such step)" };
    }

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
            case detail::PrecisionStepRole::LimitPass: {
                std::string const heading = std::string { precision_render_symbol(precisionRecord->kind) } + " at level "
                                            + recorded_step_reference(trace, precisionRecord->levelStep);
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
        return " [bound by " + recorded_step_reference(trace, *precisionRecord->limitStep) + "]";
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

    /// A value no line can spell, and why: `(not shown: <reason>)`. The one
    /// spelling of it, for a value its unit cannot show and for a value its
    /// style cannot spell in that unit alike.
    [[nodiscard]] inline std::string not_shown_text(ArithmeticError whyNot)
    {
        return "(not shown: " + std::string { describe(whyNot) } + ")";
    }

    /// @p storedValue -- a step's own, or one element of a series step's -- converted
    /// from the coherent unit of @p recorded's dimension into the unit the
    /// step was declared in, with that unit's symbol, or `(not measured)`
    /// (`NotMeasuredText`) when it is empty. Shared by `step_value_text` and
    /// `series_step_line`, so that an element of a series reads exactly as a
    /// single value of the same quantity does.
    ///
    /// The number is spelled in @p numberStyle (`checked_shown_text`: never
    /// padded in a unit nobody declared); a style that cannot spell it in the
    /// step's unit is reported as the conversion's failure is, `(not shown:
    /// ...)`. The fraction style never fails.
    [[nodiscard]] inline std::string value_in_declared_unit(Step<Rational> const& recorded,
                                                            std::optional<Rational> const& storedValue,
                                                            NumberStyle numberStyle)
    {
        if (!storedValue.has_value())
            return std::string { NotMeasuredText };

        std::expected<Rational, ArithmeticError> const shown =
            checked_convert(*storedValue, coherent(recorded.dimension), recorded.unit);
        // Unreachable for a `Step` the recorder built -- it records a unit of
        // the step's own dimension -- but a `Step` is a public aggregate and a
        // caller may fill one in by hand. Refusing to print is the only
        // honest answer: the alternative is a number in a scale the line
        // claims it is not in.
        if (!shown)
            return not_shown_text(shown.error());
        std::expected<NumberText, ArithmeticError> const spelled = checked_shown_text(*shown, numberStyle, recorded.unit);
        if (!spelled)
            return not_shown_text(spelled.error());

        std::string valueText { spelled->view() };
        std::string const unitSymbol = unit_symbol_text(recorded.unit);
        if (!unitSymbol.empty())
            valueText += " " + unitSymbol;
        return valueText;
    }

    /// What a step produced, as a person should read it.
    ///
    /// The value is stored in the coherent unit of the step's dimension;
    /// this converts it back into the unit the step was declared in and
    /// appends that unit's symbol, so an input entered as 180 l reads
    /// `180 l`. A step that failed shows why, and one with no value at all
    /// says so -- absence is not an error and must not be rendered as one.
    [[nodiscard]] inline std::string step_value_text(Step<Rational> const& recorded, NumberStyle numberStyle)
    {
        if (recorded.error.has_value())
            return std::string { describe(*recorded.error) };
        return value_in_declared_unit(recorded, recorded.value, numberStyle);
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

    /// Whether a step of @p stepKind states, as its value, a number typed
    /// rather than computed: a constant -- the author's, the library's
    /// rational `pi`, or one an overlay fixed -- a per-element constant, a
    /// declared domain, the permitted value a snap chose, and the row a
    /// banded, exact or critical-value lookup read from its table. **The one
    /// list of them**, which `value_is_typed` and `curve_part_is_typed` build
    /// on: `escaped_step_line` spells such a value in `exact_only()`, as
    /// every bound, row and limit a line quotes is spelled, so that no style
    /// shows a typed number rounded -- a constant typed as `1/3` never reads
    /// `≈0.333`, nor a snapped value two ways on one line.
    /// An interpolating lookup's value is not here: it is computed between
    /// two rows, and is neither of them.
    [[nodiscard]] constexpr bool states_typed_value(StepKind stepKind) noexcept
    {
        return stepKind == StepKind::Constant || stepKind == StepKind::PiConstant
               || stepKind == StepKind::OverriddenConstant || stepKind == StepKind::SeriesConstant
               || stepKind == StepKind::SeriesDomain || stepKind == StepKind::SnappedToPermitted
               || stepKind == StepKind::BandedLookup || stepKind == StepKind::ExactLookup
               || stepKind == StepKind::SampleSizeLookup;
    }

    /// The step whose value @p trace's step @p stepIndex passes on unchanged,
    /// when it computes nothing of its own: a documented formula, a selected
    /// or a replaced variant and a derived quantity, each over its one
    /// operand; a conditional over the branch that ran, its last operand; a
    /// record's scope over the derivation it read, its last operand that is
    /// not a lineage attribute (as the recorder reads it for the scope's
    /// unit); a precision limit's pass 1 over the level expression and its
    /// pass 2 over the limit expression, each its last operand; and a level
    /// placeholder over the level it read, the step its record names
    /// (`Trace::precisionRecords`).
    ///
    /// Only when that step holds this one's value, in this one's dimension.
    /// A consumer's node that forwards the sink records no step of its own,
    /// so a documented formula or a branch over one claims that node's
    /// operand instead -- a constant under a rise above a reference -- whose
    /// value is not the one passed on. The recorder refuses that operand's
    /// unit for the same reason (`trace.hpp`'s `PassesThroughRecordedStep`);
    /// the value is what tells the two apart here. Empty for every other
    /// step, and for an operand that is not an earlier step: `Trace` is a
    /// public aggregate, and following a later one could go round in a
    /// circle.
    [[nodiscard]] inline std::optional<std::size_t> value_passed_from(Trace<Rational> const& trace, std::size_t stepIndex)
    {
        Step<Rational> const& passing = trace.steps[stepIndex];
        std::optional<std::size_t> passedFrom;
        switch (passing.kind)
        {
            case StepKind::Documented:
            case StepKind::VariantSelected:
            case StepKind::ReplacedVariant:
            case StepKind::DerivedQuantity:
                if (passing.operands.size() == 1)
                    passedFrom = passing.operands.front();
                break;
            case StepKind::Conditional:
                if (passing.branch != Branch::Neither && !passing.operands.empty())
                    passedFrom = passing.operands.back();
                break;
            case StepKind::RecordScope:
                for (std::size_t const operandIndex: passing.operands)
                    if (operandIndex < stepIndex && trace.steps[operandIndex].kind != StepKind::LineageChecked)
                        passedFrom = operandIndex;
                break;
            case StepKind::PrecisionLevel:
            case StepKind::PrecisionLimit:
                if (PrecisionRecord const* const precisionRecord = record_for_step(trace.precisionRecords, stepIndex))
                {
                    if (precisionRecord->role == PrecisionStepRole::LevelPass && !passing.operands.empty())
                        passedFrom = passing.operands.back();
                    // Pass 2 names the level step first and the limit
                    // expression's last, as `precision_expression` reads it.
                    else if (precisionRecord->role == PrecisionStepRole::LimitPass && passing.operands.size() >= 2)
                        passedFrom = passing.operands.back();
                    else if (precisionRecord->role == PrecisionStepRole::Placeholder)
                        passedFrom = precisionRecord->levelStep;
                }
                break;
            default:
                break;
        }
        if (!passedFrom.has_value() || *passedFrom >= stepIndex)
            return std::nullopt;
        Step<Rational> const& passedStep = trace.steps[*passedFrom];
        if (!(passedStep.value == passing.value) || !(passedStep.dimension == passing.dimension))
            return std::nullopt;
        return passedFrom;
    }

    /// Whether the value @p trace's step @p stepIndex states is a number typed
    /// rather than computed: its own kind says so (`states_typed_value`), or
    /// it passes on the value of a step whose kind does (`value_passed_from`),
    /// however many such steps stand between. Decided from the steps
    /// themselves, so that one number reads the same on every line that
    /// states it: a documented constant typed as `1/3` reads `1/3` on both
    /// its lines, not `1/3` and then `≈0.333`.
    [[nodiscard]] inline bool value_is_typed(Trace<Rational> const& trace, std::size_t stepIndex)
    {
        std::size_t stated = stepIndex;
        // Each step passed from is an earlier one, so this ends.
        while (!states_typed_value(trace.steps[stated].kind))
        {
            std::optional<std::size_t> const passedFrom = value_passed_from(trace, stated);
            if (!passedFrom.has_value())
                return false;
            stated = *passedFrom;
        }
        return true;
    }

    /// Which of a curve's two series a question is about: its points or its
    /// values.
    enum class CurvePart : std::uint8_t
    {
        /// Its points, `Step::domainElements`.
        Points,
        /// Its values, `Step::elements`.
        Values,
    };

    /// Whether every one of @p curvePart of the curve step @p stepIndex is a
    /// typed number (`value_is_typed`): a pairing's points are its first
    /// operand's elements and its values its second's -- a declared domain, a
    /// per-element constant -- and a splice's are both its curves'. As for
    /// `value_passed_from`, only when those operands hold what the curve
    /// does: a pairing's points or values equal to its operand's elements,
    /// and every point, or every point with its value, of a splice one of
    /// its two curves'. False for a step of any other kind, and for operands
    /// that are not two earlier steps.
    [[nodiscard]] inline bool curve_part_is_typed(Trace<Rational> const& trace,
                                                  std::size_t stepIndex,
                                                  CurvePart curvePart)
    {
        Step<Rational> const& curveStep = trace.steps[stepIndex];
        if (curveStep.operands.size() != 2 || curveStep.operands.front() >= stepIndex
            || curveStep.operands.back() >= stepIndex)
            return false;
        Step<Rational> const& firstStep = trace.steps[curveStep.operands.front()];
        Step<Rational> const& secondStep = trace.steps[curveStep.operands.back()];
        if (curveStep.kind == StepKind::CurvePairing)
        {
            if (curvePart == CurvePart::Points)
                return curveStep.domainElements == firstStep.elements
                       && value_is_typed(trace, curveStep.operands.front());
            return curveStep.elements == secondStep.elements && value_is_typed(trace, curveStep.operands.back());
        }
        if (curveStep.kind != StepKind::CurveSplice)
            return false;
        // Whether @p spliced's pair at @p at -- its point, and with it its
        // value when the values are asked about -- is one of @p joined's.
        auto const pairFound = [curvePart](Step<Rational> const& spliced, std::size_t at, Step<Rational> const& joined) {
            for (std::size_t each = 0; each < joined.domainElements.size(); ++each)
                if (joined.domainElements[each] == spliced.domainElements[at]
                    && (curvePart == CurvePart::Points
                        || (each < joined.elements.size() && at < spliced.elements.size()
                            && joined.elements[each] == spliced.elements[at])))
                    return true;
            return false;
        };
        for (std::size_t at = 0; at < curveStep.domainElements.size(); ++at)
            if (!pairFound(curveStep, at, firstStep) && !pairFound(curveStep, at, secondStep))
                return false;
        return curve_part_is_typed(trace, curveStep.operands.front(), curvePart)
               && curve_part_is_typed(trace, curveStep.operands.back(), curvePart);
    }

    /// Where a series step's failure arose, counted from one: `at element 3`,
    /// or `at observation 3` when `Step::failureSite` says the position is an
    /// observation -- raw observations and a binning. A binning
    /// that found no class for it says which, and what the classes cover:
    /// `[331 m in no class; the classes cover 0 to under 331 m]` -- both sides
    /// of that comparison in `numberStyle.exact_only()`, so that a value
    /// shown rounded can never read as inside a class it missed.
    [[nodiscard]] inline std::string failed_position_text(Step<Rational> const& recorded, NumberStyle numberStyle)
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
        NumberStyle const comparedStyle = numberStyle.exact_only();
        return positionText + " ["
               + value_in_declared_unit(observationShape, recorded.domainElements[failedAt], comparedStyle)
               + " in no class; the classes cover "
               + half_open_range_text(*recorded.coveredRange, keySymbol, recorded.sourceUnit, comparedStyle) + "]";
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
    [[nodiscard]] inline std::string series_step_line(ShownStep const& recorded,
                                                      std::size_t& budget,
                                                      NumberStyle numberStyle)
    {
        // A per-element constant's line is its values alone, as a scalar
        // constant's is its value alone: `1 kg; 2 kg`, not the tautology
        // `values = 1 kg; 2 kg`.
        bool const listsItself = recorded.kind == StepKind::SeriesConstant || recorded.kind == StepKind::SeriesDomain;
        // A series or observations read from another record says which, after
        // its elements, as a single value read from one does after its value
        // -- see `record_origin_text`. The steps computed from it name it
        // through their operands.
        bool const namesQuantity =
            recorded.kind == StepKind::SeriesVariable || recorded.kind == StepKind::ObservationsVariable;
        std::string originText =
            namesQuantity && recorded.readFrom.has_value() ? ", " + record_origin_text(*recorded.readFrom) : std::string {};
        // A typed-in series says so after its origin, as a single value does:
        // the record qualifies the values, and the source is said of them.
        if (recorded.kind == StepKind::SeriesVariable && recorded.inputSource == ValueSource::ManuallyEntered)
            originText += ", entered by hand";
        std::string lineText = listsItself ? std::string {} : step_expression(recorded) + " = ";
        if (recorded.error.has_value())
            return lineText + std::string { describe(*recorded.error) } + failed_position_text(recorded, numberStyle)
                   + originText;
        std::size_t const elementCount = recorded.elements.size();
        if (elementCount == 0)
            return lineText + "(no elements)" + originText;

        std::size_t const listed = budget < elementCount ? budget : elementCount;
        budget -= listed;
        for (std::size_t at = 0; at < listed; ++at)
        {
            if (at > 0)
                lineText += "; ";
            lineText += value_in_declared_unit(recorded, recorded.elements[at], numberStyle);
        }
        if (listed < elementCount)
            lineText += std::string { listed > 0 ? "; " : "" } + "... " + std::to_string(elementCount - listed) + " more";
        return lineText + originText;
    }

    /// A curve step's line, without its number: the expression, an `=`, and
    /// each point with its value, `7/10 m: 894/25 %`, separated by `; ` --
    /// as many pairs as @p budget allows, one unit each, as a series step's
    /// elements are (`series_step_line`), and `... k more` where `k` is
    /// exactly the number left out. The points are shown in `sourceUnit`, the
    /// values in `unit`.
    ///
    /// A curve's point @p point, in the step's `sourceUnit`.
    [[nodiscard]] inline std::string curve_point_text(Step<Rational> const& recorded,
                                                      std::optional<Rational> const& point,
                                                      NumberStyle numberStyle)
    {
        // A point is shown as a value of the point's own dimension and unit.
        Step<Rational> pointShape {};
        pointShape.dimension = recorded.sourceUnit.dimension;
        pointShape.unit = recorded.sourceUnit;
        return value_in_declared_unit(pointShape, point, numberStyle);
    }

    /// The rule a failed curve broke and the point it broke it at:
    /// `[duplicate domain point 163 m]`, `[domain does not ascend at 113 m]`
    /// or `[breaks non-decreasing at 103 m]`. Nothing when the step names no rule
    /// or holds no point at its failed element.
    [[nodiscard]] inline std::string curve_break_suffix(Step<Rational> const& recorded, NumberStyle numberStyle)
    {
        if (recorded.curveBreak == CurveBreak::None || !recorded.failedElement.has_value()
            || *recorded.failedElement >= recorded.domainElements.size()
            || !recorded.domainElements[*recorded.failedElement].has_value())
            return {};
        std::string const pointText =
            curve_point_text(recorded, recorded.domainElements[*recorded.failedElement], numberStyle);
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
    ///
    /// The points are spelled in @p pointStyle and the values in
    /// @p valueStyle: `escaped_step_line` gives either `exact_only()` when it
    /// comes from a typed series (`curve_part_is_typed`), so that a declared
    /// domain's points read here as on the domain's own line.
    [[nodiscard]] inline std::string curve_step_line(ShownStep const& recorded,
                                                     std::size_t& budget,
                                                     NumberStyle pointStyle,
                                                     NumberStyle valueStyle)
    {
        std::string lineText = step_expression(recorded) + " = ";
        if (recorded.error.has_value())
        {
            lineText += describe(*recorded.error);
            if (recorded.failedElement.has_value())
                lineText += " at element " + std::to_string(*recorded.failedElement + 1)
                            + curve_break_suffix(recorded, pointStyle);
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
            lineText += curve_point_text(recorded, recorded.domainElements[at], pointStyle) + ": "
                        + value_in_declared_unit(recorded, recorded.elements[at], valueStyle);
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
    [[nodiscard]] inline std::string curve_interpolation_suffix(Step<Rational> const& recorded, NumberStyle numberStyle)
    {
        std::string const pointSymbol = unit_symbol_text(recorded.sourceUnit);
        // The points the value lay between, or the ends it lay outside, are
        // one side of the comparison the clause states: `declared_number_text`
        // spells them in `exact_only()`.
        if (recorded.selectedSegment.has_value())
            return " [" + segment_text(*recorded.selectedSegment, pointSymbol, recorded.sourceUnit, numberStyle) + "]";
        if (recorded.coveredRange.has_value())
            return " [outside the curve, which runs "
                   + closed_range_text(*recorded.coveredRange, pointSymbol, recorded.sourceUnit, numberStyle) + "]";
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
    ///
    /// The permitted values are the author's (`declared_number_text`), and
    /// the one it snapped to, named after `nearer`, is one of them: it is
    /// spelled in `numberStyle.exact_only()` too, so that it reads exactly as
    /// the neighbour it names -- and as the step's own value, a typed number
    /// (`states_typed_value`), reads before the bracket.
    [[nodiscard]] inline std::string snap_suffix(Step<Rational> const& recorded, NumberStyle numberStyle)
    {
        std::string const keySymbol = unit_symbol_text(recorded.unit);
        Unit const& keyUnit = recorded.unit;
        if (recorded.selectedSegment.has_value())
        {
            Segment const& neighbours = *recorded.selectedSegment;
            std::string const lowText = number_with_unit(
                declared_number_text(neighbours.low.numerator, neighbours.low.denominator, keyUnit, numberStyle), keySymbol);
            std::string const highText = number_with_unit(
                declared_number_text(neighbours.high.numerator, neighbours.high.denominator, keyUnit, numberStyle),
                keySymbol);
            if (neighbours.low == neighbours.high)
                return " [on " + lowText + "]";
            if (recorded.tieBroken)
                return " [" + lowText + " to " + highText + "; tie, " + std::string { describe(recorded.snapTie) } + "]";
            std::string const nearer = recorded.value.has_value()
                                           ? value_in_declared_unit(recorded, recorded.value, numberStyle.exact_only())
                                           : std::string { "neither" };
            return " [" + lowText + " to " + highText + "; nearer " + nearer + "]";
        }
        if (recorded.coveredRange.has_value())
        {
            LookupRange const& covered = *recorded.coveredRange;
            return " [outside the permitted set, "
                   + number_with_unit(
                       declared_number_text(covered.lowNumerator, covered.lowDenominator, keyUnit, numberStyle), keySymbol)
                   + " to "
                   + number_with_unit(
                       declared_number_text(covered.highNumerator, covered.highDenominator, keyUnit, numberStyle),
                       keySymbol)
                   + "]";
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
    ///
    /// The value and its row are the two sides of the comparison the outcome
    /// states, so both are spelled in `numberStyle.exact_only()`: a value
    /// shown rounded beside a limit could read as on the other side of it.
    [[nodiscard]] inline std::string conformity_line(ShownStep const& recorded,
                                                     std::span<LimitRow const> limits,
                                                     std::size_t& budget,
                                                     NumberStyle numberStyle)
    {
        std::size_t const outcomeCount = recorded.elementOutcomes.size();
        std::size_t const listed = budget < outcomeCount ? budget : outcomeCount;
        budget -= listed;
        NumberStyle const comparedStyle = numberStyle.exact_only();
        std::string lineText = step_expression(recorded) + " [";
        for (std::size_t at = 0; at < listed; ++at)
        {
            if (at > 0)
                lineText += "; ";
            std::string const rowClause =
                at < limits.size()
                    ? " (" + limit_row_text(limits[at], unit_symbol_text(recorded.unit), recorded.unit, comparedStyle) + ")"
                    : std::string {};
            std::string const valueClause =
                at < recorded.elements.size() && recorded.elements[at].has_value()
                    ? ", " + value_in_declared_unit(recorded, recorded.elements[at], comparedStyle)
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

    /// Whether @p stepKind is one of the six steps a rejection records.
    [[nodiscard]] constexpr bool is_rejection_step(StepKind stepKind) noexcept
    {
        return stepKind == StepKind::RejectionPass || stepKind == StepKind::OutlierRejected
               || stepKind == StepKind::RejectionSettled || stepKind == StepKind::RejectionAborted
               || stepKind == StepKind::RejectionFailed || stepKind == StepKind::RejectionUndecided;
    }

    /// What a failed pass failed at, in the words its line uses.
    [[nodiscard]] inline std::string rejection_failure_subject(detail::RejectionFailurePoint point)
    {
        switch (point)
        {
            case detail::RejectionFailurePoint::Mean:
                return "the mean";
            case detail::RejectionFailurePoint::Variance:
                return "the variance";
            case detail::RejectionFailurePoint::Limit:
                return "the limit";
            case detail::RejectionFailurePoint::NegativeLimit:
                return "the limit is negative, which no deviation can be compared with";
            case detail::RejectionFailurePoint::Threshold:
                return "limit^2 * s^2";
            case detail::RejectionFailurePoint::Statistic:
                return "the deviation";
            case detail::RejectionFailurePoint::Range:
                return "the range";
            case detail::RejectionFailurePoint::GapRatio:
                return "the gap / range";
        }
        return "an unknown part of the pass";
    }

    /// @p si, a value in the coherent unit of @p recorded's dimension -- or of
    /// its square, when @p squared -- in @p recorded's unit (or its square),
    /// with the unit's symbol: `27/10 g`, `729/100 g2`. Refuses to print, as
    /// `value_in_declared_unit` does, a value its unit cannot show, or one
    /// @p numberStyle cannot spell in it. A square declares no decimals of its
    /// own, so a squared value is never padded, as a value in a unit nobody
    /// declared is not (`checked_shown_text`); rounded, it would round at the
    /// unit's decimals.
    [[nodiscard]] inline std::string rejection_value_text(Step<Rational> const& recorded,
                                                          Rational si,
                                                          bool squared,
                                                          NumberStyle numberStyle)
    {
        if (!squared)
            return value_in_declared_unit(recorded, si, numberStyle);
        Unit const shownUnit = recorded.unit;
        std::expected<Rational, ArithmeticError> const magnitude =
            Rational::make(shownUnit.magnitudeNumerator, shownUnit.magnitudeDenominator);
        std::expected<Rational, ArithmeticError> const magnitudeSquared =
            magnitude.has_value() ? checked_mul(*magnitude, *magnitude) : magnitude;
        std::expected<Rational, ArithmeticError> const shown =
            magnitudeSquared.has_value() ? checked_div(si, *magnitudeSquared) : magnitudeSquared;
        if (!shown)
            return not_shown_text(shown.error());
        std::expected<NumberText, ArithmeticError> const spelled =
            checked_number_text(*shown, trimmed(numberStyle), shownUnit);
        if (!spelled)
            return not_shown_text(spelled.error());
        std::string valueText { spelled->view() };
        std::string const unitSymbol = unit_symbol_text(shownUnit);
        if (!unitSymbol.empty())
            valueText += " " + unitSymbol + "2";
        return valueText;
    }

    /// A deviation from the mean or its limit, @p si, as `rejection_value_text`
    /// shows it -- in the sample's unit when a difference can be shown in it
    /// (`detail::borrowable`), and otherwise in the coherent one: a deviation
    /// of Celsius readings is a difference, 106/25 kelvin, and in degrees
    /// Celsius it would read as a reading, off by the offset. Squared, in the
    /// coherent unit's square likewise, as the deviation beside it is.
    [[nodiscard]] inline std::string deviation_text(Step<Rational> const& recorded,
                                                    Rational si,
                                                    bool squared,
                                                    NumberStyle numberStyle)
    {
        if (detail::borrowable(recorded.unit))
            return rejection_value_text(recorded, si, squared, numberStyle);
        Step<Rational> differenceShape {};
        differenceShape.dimension = recorded.dimension;
        differenceShape.unit = coherent(recorded.dimension);
        return rejection_value_text(differenceShape, si, squared, numberStyle);
    }

    /// `element 4 of 6`, or `elements 4 and 6 of 6`, or `elements 2, 4 and 6
    /// of 6` -- positions counted from one, as every text shows them; with
    /// @p ofObservations, `observation 4 of 6` and the rest.
    [[nodiscard]] inline std::string elements_text(std::vector<std::size_t> const& positions,
                                                   std::size_t originalSize,
                                                   bool ofObservations)
    {
        std::string listed = ofObservations ? (positions.size() == 1 ? "observation " : "observations ")
                                            : (positions.size() == 1 ? "element " : "elements ");
        for (std::size_t at = 0; at < positions.size(); ++at)
        {
            if (at > 0)
                listed += at + 1 == positions.size() ? " and " : ", ";
            listed += std::to_string(positions[at] + 1);
        }
        return listed + " of " + std::to_string(originalSize);
    }

    /// A rejection step's whole line, read from its side-table record:
    ///
    ///  - a pass: `pass 2: 5 values, mean 1019/25 g`;
    ///  - a rejected determination: `rejected element 4 of 6 (44 g) in pass
    ///    1: abs(x - mean) = 27/10 g > 1239/500 g (deviation from mean)` --
    ///    for `deviation_in_stddevs`, the exact comparison the decision used,
    ///    `(x - mean)^2 = ... g2 > limit^2 * s^2 = ... g2`;
    ///  - settled: `settled: 2 rejected, 4 remain`;
    ///  - aborted: `element 6 of 6 would be rejection 2 of at most 1: discard
    ///    the determinations and repeat the test [Example Standard, 7.4]` --
    ///    naming both bounds when the rejection would pass both; or, before
    ///    any pass, `3 values, fewer than the at least 4 to keep: discard the
    ///    determinations and repeat the test`;
    ///  - failed: `failed in pass 1: the variance: overflow in exact
    ///    arithmetic at element 1 of 6`, or `failed in pass 1: the range:
    ///    overflow in exact arithmetic`;
    ///  - undecided: `no decision in pass 2: the limit is not measured`.
    ///
    /// A step with no record says so rather than guess, and so does one whose
    /// record contradicts itself -- a position past the sample, an abort
    /// naming nobody or passing neither bound, a count that would wrap, a
    /// pass numbered 0 or beyond the sample's size, counts that do not add
    /// up to the sample, a range failure placed at an element: `Trace` and
    /// its records are public aggregates, and a line built from such a
    /// record would print a number no evaluation produced.
    ///
    /// @p recorded is the step's escaped copy (`EscapedStep`). The record is
    /// read from @p trace at @p stepIndex, and its author text -- the verdict
    /// and the citation -- is escaped here, as `EscapedStep` escapes a
    /// step's own.
    ///
    /// Both sides of the comparison a rejection states -- the statistic and
    /// its limit -- are spelled in `numberStyle.exact_only()`: two values
    /// shown rounded could read as equal beside a `>` that decided between
    /// them.
    [[nodiscard]] inline std::string rejection_line(Trace<Rational> const& trace,
                                                    std::size_t stepIndex,
                                                    ShownStep const& recorded,
                                                    NumberStyle numberStyle)
    {
        detail::RejectionRecord<Rational> const* const rejectionRecord = record_for_step(trace.rejectionRecords, stepIndex);
        if (rejectionRecord == nullptr)
            return step_expression(recorded) + " (its record is missing)";
        // Every pass but the last removes a determination, so no rejection of
        // n runs more than n passes (one, for an empty sample), and none runs
        // pass 0.
        bool const passOutOfRange =
            rejectionRecord->pass == 0 || rejectionRecord->pass > std::max<std::size_t>(rejectionRecord->originalSize, 1);
        switch (recorded.kind)
        {
            case StepKind::RejectionPass:
                if (passOutOfRange || rejectionRecord->sampleSize > rejectionRecord->originalSize)
                    return "pass (its record is invalid)";
                return "pass " + std::to_string(rejectionRecord->pass) + ": " + std::to_string(rejectionRecord->sampleSize)
                       + (rejectionRecord->sampleSize == 1 ? " value" : " values") + ", mean "
                       + step_value_text(recorded, numberStyle);
            case StepKind::OutlierRejected: {
                if (!rejectionRecord->position.has_value() || !rejectionRecord->rejectedValue.has_value()
                    || !rejectionRecord->statistic.has_value() || !rejectionRecord->limit.has_value())
                    return "rejected element (its record is incomplete)";
                if (*rejectionRecord->position >= rejectionRecord->originalSize || passOutOfRange)
                    return "rejected element (its record is invalid)";
                std::string const comparison = rejectionRecord->onLimit == OnLimit::Keep ? " > " : " >= ";
                NumberStyle const comparedStyle = numberStyle.exact_only();
                // A gap over a range is a pure number, in no unit.
                std::string const decided =
                    rejectionRecord->criterion == CriterionKind::GapToRange
                        ? "gap / range = " + styled_number_text(*rejectionRecord->statistic, comparedStyle, unit::One)
                              + comparison + styled_number_text(*rejectionRecord->limit, comparedStyle, unit::One)
                              + " (gap to range)"
                    : rejectionRecord->squared
                        ? "(x - mean)^2 = " + deviation_text(recorded, *rejectionRecord->statistic, true, comparedStyle)
                              + comparison + "limit^2 * s^2 = "
                              + deviation_text(recorded, *rejectionRecord->limit, true, comparedStyle)
                              + " (deviation in standard deviations)"
                        : "abs(x - mean) = " + deviation_text(recorded, *rejectionRecord->statistic, false, comparedStyle)
                              + comparison + deviation_text(recorded, *rejectionRecord->limit, false, comparedStyle)
                              + " (deviation from mean)";
                return "rejected "
                       + elements_text({ *rejectionRecord->position },
                                       rejectionRecord->originalSize,
                                       rejectionRecord->ofObservations)
                       + " ("
                       + rejection_value_text(recorded, *rejectionRecord->rejectedValue, false, numberStyle)
                       + ") in pass " + std::to_string(rejectionRecord->pass) + ": " + decided;
            }
            case StepKind::RejectionSettled:
                if (passOutOfRange
                    || rejectionRecord->rejectedCount + rejectionRecord->remaining != rejectionRecord->originalSize)
                    return "rejection settled (its record is invalid)";
                return "settled: " + std::to_string(rejectionRecord->rejectedCount) + " rejected, "
                       + std::to_string(rejectionRecord->remaining) + " remain";
            case StepKind::RejectionAborted: {
                if (rejectionRecord->startedShort)
                {
                    if (rejectionRecord->pass != 0 || !rejectionRecord->wouldReject.empty()
                        || rejectionRecord->rejectedCount != 0 || rejectionRecord->pastAtMost
                        || rejectionRecord->remaining != rejectionRecord->originalSize
                        || rejectionRecord->remaining >= rejectionRecord->keepAtLeast)
                        return "rejection aborted (its record is invalid)";
                    EscapedCitation const cited { rejectionRecord->citation };
                    return std::to_string(rejectionRecord->originalSize)
                           + (rejectionRecord->originalSize == 1 ? " value" : " values") + ", fewer than the at least "
                           + std::to_string(rejectionRecord->keepAtLeast) + " to keep: "
                           + escaped_author_text(rejectionRecord->verdict.label) + citation_suffix(cited.cited());
                }
                bool const namesNobody = rejectionRecord->wouldReject.empty();
                bool const leavesFewerThanNone = rejectionRecord->remaining < rejectionRecord->wouldReject.size();
                bool positionPastSample = false;
                for (std::size_t const wouldGo: rejectionRecord->wouldReject)
                    if (wouldGo >= rejectionRecord->originalSize)
                        positionPastSample = true;
                if (namesNobody || leavesFewerThanNone || positionPastSample || passOutOfRange
                    || rejectionRecord->rejectedCount + rejectionRecord->remaining != rejectionRecord->originalSize
                    || (!rejectionRecord->pastAtMost && !rejectionRecord->belowKeepAtLeast))
                    return "rejection aborted (its record is invalid)";
                std::string reason;
                if (rejectionRecord->pastAtMost)
                {
                    reason = rejectionRecord->wouldReject.size() == 1 ? " would be rejection " : " would be rejections ";
                    for (std::size_t at = 0; at < rejectionRecord->wouldReject.size(); ++at)
                    {
                        if (at > 0)
                            reason += at + 1 == rejectionRecord->wouldReject.size() ? " and " : ", ";
                        reason += std::to_string(rejectionRecord->rejectedCount + at + 1);
                    }
                    reason += " of at most " + std::to_string(rejectionRecord->atMost);
                }
                if (rejectionRecord->belowKeepAtLeast)
                    reason += std::string { rejectionRecord->pastAtMost ? " and" : "" } + " would leave "
                              + std::to_string(rejectionRecord->remaining - rejectionRecord->wouldReject.size())
                              + " of at least " + std::to_string(rejectionRecord->keepAtLeast);
                EscapedCitation const cited { rejectionRecord->citation };
                return elements_text(
                           rejectionRecord->wouldReject, rejectionRecord->originalSize, rejectionRecord->ofObservations)
                       + reason + ": "
                       + escaped_author_text(rejectionRecord->verdict.label) + citation_suffix(cited.cited());
            }
            case StepKind::RejectionFailed: {
                if (!rejectionRecord->failurePoint.has_value() || !recorded.error.has_value())
                    return "rejection failed (its record is incomplete)";
                // A range is no determination's: a record naming one for it
                // contradicts itself.
                if (passOutOfRange
                    || (rejectionRecord->position.has_value()
                        && (*rejectionRecord->position >= rejectionRecord->originalSize
                            || *rejectionRecord->failurePoint == detail::RejectionFailurePoint::Range)))
                    return "rejection failed (its record is invalid)";
                std::string const atElement =
                    rejectionRecord->position.has_value()
                        ? " at " + elements_text({ *rejectionRecord->position },
                                                 rejectionRecord->originalSize,
                                                 rejectionRecord->ofObservations)
                        : std::string {};
                if (*rejectionRecord->failurePoint == detail::RejectionFailurePoint::NegativeLimit)
                    return "failed in pass " + std::to_string(rejectionRecord->pass) + ": "
                           + rejection_failure_subject(*rejectionRecord->failurePoint);
                return "failed in pass " + std::to_string(rejectionRecord->pass) + ": "
                       + rejection_failure_subject(*rejectionRecord->failurePoint) + ": "
                       + std::string { describe(*recorded.error) } + atElement;
            }
            case StepKind::RejectionUndecided:
                if (passOutOfRange)
                    return "rejection undecided (its record is invalid)";
                return "no decision in pass " + std::to_string(rejectionRecord->pass) + ": the limit is not measured";
            default:
                break;
        }
        return step_expression(recorded);
    }

    /// What `render_trace` found in a trace for an opaque step: the call's
    /// row -- the step's own for an `OpaqueOperation` step, and the row of the
    /// call it claimed for an `OpaqueOutput` or `RoundedOpaqueOutput` step --
    /// and, for the latter, which output it selected and whether its operand
    /// **is** a call's step. For a failed call, which input's step carries the
    /// error. Null and empty for every other step, and for a step built by
    /// hand.
    struct OpaqueLine
    {
        OpaqueStepData<Rational> const* call = nullptr;
        std::optional<std::size_t> outputIndex {};
        /// For an `OpaqueOutput` or `RoundedOpaqueOutput` step: whether its
        /// sole operand is an `OpaqueOperation` step, judged by that step's
        /// kind -- the step that says the operation's inside is not shown.
        /// False when a sink that does not hear the opaque hooks recorded the
        /// output over the call's inputs directly, and for a step built by
        /// hand.
        bool overCall = false;
        /// For an `OpaqueOperation` step that relayed a failure: the index of
        /// the input step that carries it -- the last operand, since the call
        /// stops at the first input that fails -- or empty when no operand
        /// step carries an error.
        std::optional<std::size_t> failedInput {};
    };

    /// The coherent unit of @p dimension, spelt from its base units:
    /// `m/s`, `kg/m^3`, `kg/(m s^2)`, `m^(1/2)`; empty for a dimensionless
    /// one. For an opaque output shown in no input's unit, so that a slope in
    /// metres per second does not read as a pure number.
    ///
    /// A named base dimension is spelt by its name -- the name is also the
    /// symbol of its coherent unit -- ahead of the SI units on its side of the
    /// slash, in the dimension's own order: `EUR`, `EUR s^2/(m^2 kg)` for euros
    /// per joule, `1/JPY`, `EUR/JPY`, `EUR^(1/2)`. First, because a tariff is
    /// read as money per energy, not as seconds squared of money per metre.
    /// Each name goes through `escaped_author_text`: `base_dimension()` admits
    /// only letters and digits, but a hand-filled `namedBases` can hold
    /// anything.
    [[nodiscard]] inline std::string coherent_unit_text(Dimension dimension)
    {
        struct BaseUnit
        {
            std::string_view symbol;
            Exponent exponent;
        };
        std::array<BaseUnit, 7> const bases { BaseUnit { "m", dimension.length },      BaseUnit { "kg", dimension.mass },
                                              BaseUnit { "s", dimension.time },        BaseUnit { "A", dimension.current },
                                              BaseUnit { "K", dimension.temperature }, BaseUnit { "mol", dimension.amount },
                                              BaseUnit { "cd", dimension.luminosity } };
        auto const unitPower = [](std::string_view symbolText, std::int32_t numeratorPart, std::int32_t denominatorPart) {
            std::string factorText { symbolText };
            if (denominatorPart != 1)
                factorText += "^(" + std::to_string(numeratorPart) + "/" + std::to_string(denominatorPart) + ")";
            else if (numeratorPart != 1)
                factorText += "^" + std::to_string(numeratorPart);
            return factorText;
        };
        std::string above;
        std::string below;
        std::size_t belowCount = 0;
        auto const place = [&](std::string_view symbolText, Exponent baseExponent) {
            if (baseExponent.numerator > 0)
                above += (above.empty() ? "" : " ")
                         + unitPower(symbolText, baseExponent.numerator, baseExponent.denominator);
            else if (baseExponent.numerator < 0)
            {
                below += (below.empty() ? "" : " ")
                         + unitPower(symbolText, -baseExponent.numerator, baseExponent.denominator);
                ++belowCount;
            }
        };
        for (std::size_t slot = 0; named_base_in_use(dimension, slot); ++slot)
            place(escaped_author_text(view(dimension.namedBases[slot].name)), dimension.namedBases[slot].exponent);
        for (BaseUnit const& base: bases)
            place(base.symbol, base.exponent);
        if (below.empty())
            return above;
        return (above.empty() ? std::string { "1" } : above) + "/" + (belowCount > 1 ? "(" + below + ")" : below);
    }

    /// @p storedValue in @p shownUnit, and -- when that unit has no symbol of
    /// its own but a dimension -- followed by the coherent unit's spelling
    /// (`coherent_unit_text`), for an opaque output.
    [[nodiscard]] inline std::string opaque_value_text(Dimension dimension,
                                                       Unit shownUnit,
                                                       std::optional<Rational> const& storedValue,
                                                       NumberStyle numberStyle)
    {
        Step<Rational> outputShape {};
        outputShape.dimension = dimension;
        outputShape.unit = shownUnit;
        std::string valueText = value_in_declared_unit(outputShape, storedValue, numberStyle);
        if (storedValue.has_value() && view(shownUnit.symbolText).empty() && !(dimension == dim::Scalar))
            valueText += " " + coherent_unit_text(dimension);
        return valueText;
    }

    /// What an opaque call's line says of whose failure it carries, bracketed:
    /// the operation's own, relayed from the one input that failed -- named,
    /// with its element -- or its observation, for raw observations -- counted
    /// from one when the failure had one -- or undetermined.
    [[nodiscard]] inline std::string opaque_failure_suffix(Step<Rational> const& recorded,
                                                           OpaqueFailure carried,
                                                           std::optional<std::size_t> failedInput)
    {
        switch (carried)
        {
            case OpaqueFailure::None:
                return {};
            case OpaqueFailure::Own:
                return " [the operation itself failed, not any input]";
            case OpaqueFailure::Propagated:
            {
                std::string relayed = " [carried up from ";
                relayed += failedInput.has_value() ? operand_reference(*failedInput) : std::string { "an input" };
                if (recorded.failedElement.has_value())
                    relayed +=
                        (recorded.failureSite == FailureSite::InputObservation ? ", at observation " : ", at element ")
                        + std::to_string(*recorded.failedElement + 1);
                return relayed + "]";
            }
            case OpaqueFailure::Undetermined:
                return " [this operation or an input: an input recorded no step]";
        }
        return " [unknown failure]";
    }

    /// @p calledOutputs, each spelled by @p spelled and spending one unit of
    /// @p budget, as a series' elements do, joined by @p joiner; a list cut
    /// short ends `... k more`. For an opaque call's line, which lists its
    /// outputs with their values, or by name alone on the rounded route.
    template <typename Spell>
    [[nodiscard]] std::string opaque_outputs_listed(std::vector<OpaqueOutputValue<Rational>> const& calledOutputs,
                                                    std::size_t& budget,
                                                    std::string_view joiner,
                                                    Spell const& spelled)
    {
        std::size_t const outputCount = calledOutputs.size();
        std::size_t const listed = budget < outputCount ? budget : outputCount;
        budget -= listed;
        std::string listText;
        for (std::size_t at = 0; at < listed; ++at)
            listText += (at > 0 ? std::string { joiner } : std::string {}) + spelled(calledOutputs[at]);
        if (listed < outputCount)
            listText += (listed > 0 ? std::string { joiner } : std::string {}) + "... "
                        + std::to_string(outputCount - listed) + " more";
        return listText;
    }

    /// An opaque call's line, without its number: `series span(#1) = lowest
    /// = 103 g; highest = 191 g; span = 88 g [inside not shown] [Spread of
    /// readings, Example Standard 12, 4.2]`.
    ///
    /// Each output shown spends one unit of @p budget, as a series' elements
    /// do, and a list cut short ends `... k more`. A failed call shows its
    /// error and whose it is (`opaque_failure_suffix`); an absent one,
    /// `(not measured)`. A call stopped at a failing input writes each
    /// input after it, never evaluated, as `(not evaluated)`. A call evaluated
    /// for a rounded output names its outputs without values --
    /// `linear least squares(#3) = intercept, slope: rounded where used` --
    /// since none exists until an output is rounded; an absent one,
    /// `(not measured)`.
    ///
    /// **`[inside not shown]` depends on the step's kind alone**: it is
    /// written for every `OpaqueOperation` step, with or without its row, and
    /// nothing a step or an operation holds can switch it off. The citation
    /// clause is always written too, `(no citation given)` when the call
    /// cited nothing, so that an uncited call never reads as a cited one.
    ///
    /// @p recorded has had its author text escaped already (`step_line`); the
    /// row's names are escaped here, with the same function.
    [[nodiscard]] inline std::string opaque_call_line(ShownStep const& recorded,
                                                      OpaqueLine const& opaqueLine,
                                                      std::size_t& budget,
                                                      NumberStyle numberStyle)
    {
        OpaqueStepData<Rational> const* const callRow = opaqueLine.call;
        std::string lineText = step_expression(recorded);
        if (callRow != nullptr)
        {
            // A call stopped at a failing input lists the inputs after it
            // too, so that it never reads as a call of fewer arguments.
            std::string arguments = operands_text(recorded);
            for (std::size_t skipped = 0; skipped < callRow->inputsNotEvaluated; ++skipped)
                arguments += std::string { arguments.empty() ? "" : ", " } + "(not evaluated)";
            lineText = escaped_author_text(callRow->operationName) + "(" + arguments + ")";
        }
        lineText += " = ";
        if (recorded.error.has_value())
            lineText += describe(*recorded.error);
        else if (callRow != nullptr && callRow->values == OpaqueValues::RoundedWhereUsed)
        {
            // No output holds a value: the names, each spending one unit of
            // the budget as a value would, and how they are reported.
            if (callRow->answer != OpaqueAnswer::Answered || callRow->outputs.empty())
                lineText += NotMeasuredText;
            else
                lineText += opaque_outputs_listed(callRow->outputs,
                                                  budget,
                                                  ", ",
                                                  [](OpaqueOutputValue<Rational> const& shownOutput) {
                                                      return escaped_author_text(shownOutput.name);
                                                  })
                            + ": rounded where used";
        }
        else if (callRow == nullptr || callRow->outputs.empty() || !callRow->outputs.front().value.has_value())
            lineText += NotMeasuredText;
        else
            lineText += opaque_outputs_listed(
                callRow->outputs, budget, "; ", [numberStyle](OpaqueOutputValue<Rational> const& shownOutput) {
                    return escaped_author_text(shownOutput.name) + " = "
                           + opaque_value_text(shownOutput.dimension, shownOutput.unit, shownOutput.value, numberStyle);
                });
        lineText += " [inside not shown]";
        if (callRow != nullptr)
            lineText += opaque_failure_suffix(recorded, callRow->failure, opaqueLine.failedInput);
        std::string const cited = citation_text(recorded.citation);
        lineText += cited.empty() ? " " + std::string { noCitationGiven } : " [" + cited + "]";
        return lineText;
    }

    /// What `render_trace` found in a trace's side tables for a retry's step:
    /// an attempt's number and judgement, or how the retry ended and the
    /// number of its last attempt. Null and empty for every other step, and
    /// for a step built by hand.
    struct RetryLine
    {
        AttemptStepData const* attempt = nullptr;
        RetryStepData const* retry = nullptr;
        /// For a `RetryConcluded` step: the number of the last attempt it
        /// claimed, or empty when none ran.
        std::optional<std::size_t> lastAttempt {};
        /// For an attempt whose judgement failed: the failing side's error,
        /// read off that step -- the attempt's last operand.
        std::optional<ArithmeticError> judgementError {};
    };

    /// A retry attempt's line, without its number: `attempt 4: w(k) = #20 =
    /// 57/5 g; judged #21 >= #23: accepted`. The attempt's value, or its
    /// error; then how it was judged, naming the acceptance's two sides --
    /// or, when one side failed before the other was evaluated, the one that
    /// failed -- and `rejected`, `cannot be judged`, the failing side's error,
    /// or `not judged`.
    [[nodiscard]] inline std::string retry_attempt_line(ShownStep const& recorded,
                                                        RetryLine const& retryLine,
                                                        NumberStyle numberStyle)
    {
        std::string lineText = retryLine.attempt == nullptr
                                   ? std::string { "attempt: " }
                                   : "attempt " + std::to_string(retryLine.attempt->attemptNumber) + ": ";
        lineText += step_expression(recorded);
        if (!recorded.operands.empty())
            lineText += " = " + operand_reference(recorded.operands.front());
        // An attempt that read a determination nobody recorded says so once:
        // as its value when it has none -- the determination's own line reads
        // the same -- and after its value when only its judgement read one.
        bool const notRecorded = retryLine.attempt != nullptr && !recorded.error.has_value()
                                 && retryLine.attempt->judgement == AttemptJudgement::NotRecorded;
        if (notRecorded && !recorded.value.has_value())
            return lineText + " = (not recorded)";
        lineText += " = " + step_value_text(recorded, numberStyle);
        if (retryLine.attempt == nullptr || recorded.error.has_value())
            return lineText;
        AttemptJudgement const judged = retryLine.attempt->judgement;
        if (judged == AttemptJudgement::NotJudged)
            return lineText + "; not judged";
        // Before an absent value's "cannot be judged": what was missing is a
        // recorded determination, not a comparison.
        if (notRecorded)
            return lineText + "; not recorded";
        // An absent value: nothing was compared.
        if (!recorded.value.has_value())
            return lineText + "; cannot be judged";
        lineText += "; judged";
        // The step's own comparison, not the lineage check ShownStep adds.
        if (recorded.operands.size() >= 3)
            lineText += " " + operand_reference(recorded.operands[1]) + " "
                        + std::string { comparison_symbol(recorded.Step<Rational>::comparison) } + " "
                        + operand_reference(recorded.operands[2]);
        else if (recorded.operands.size() == 2)
            lineText += " " + operand_reference(recorded.operands[1]);
        lineText += ": ";
        switch (judged)
        {
            case AttemptJudgement::Accepted:
                return lineText + "accepted";
            case AttemptJudgement::Rejected:
                return lineText + "rejected";
            case AttemptJudgement::JudgementFailed:
                return lineText
                       + (retryLine.judgementError.has_value() ? std::string { describe(*retryLine.judgementError) }
                                                               : std::string { "failed" });
            case AttemptJudgement::NotJudgeable:
            case AttemptJudgement::NotJudged:
            case AttemptJudgement::NotRecorded:
                break;
        }
        return lineText + "cannot be judged";
    }
    /// How a retry ended, its line without its number: `w = retry: accepted
    /// at attempt 4 of 4 = 57/5 g`, `w = retry: exhausted after 3 of 3:
    /// repeat the determination`, `w = retry: failed at attempt 2: division
    /// by zero`, `d_a = retry: attempt 3 not recorded` -- and always its citation, `(no citation given)` when it
    /// cited nothing. The verdict is author text, escaped here, as
    /// `step_line` escapes what a `Step` holds.
    [[nodiscard]] inline std::string retry_concluded_line(ShownStep const& recorded,
                                                          RetryLine const& retryLine,
                                                          NumberStyle numberStyle)
    {
        std::string lineText = step_expression(recorded) + " = retry";
        std::string const attemptWords =
            retryLine.lastAttempt.has_value() ? "attempt " + std::to_string(*retryLine.lastAttempt) : std::string {};
        if (retryLine.retry == nullptr)
            lineText += " = " + step_value_text(recorded, numberStyle);
        else
            switch (retryLine.retry->end)
            {
                case RetryEnd::Accepted:
                    lineText += ": accepted at " + attemptWords + " of " + std::to_string(retryLine.retry->attemptLimit)
                                + " = " + step_value_text(recorded, numberStyle);
                    break;
                // The attempts it claimed, not the limit: a trace that shows
                // fewer never says more ran.
                case RetryEnd::Exhausted:
                    lineText += ": exhausted after " + std::to_string(retryLine.lastAttempt.value_or(0)) + " of "
                                + std::to_string(retryLine.retry->attemptLimit) + ": "
                                + escaped_author_text(retryLine.retry->verdictLabel);
                    break;
                case RetryEnd::NotJudgeable:
                    lineText += ": not judgeable at " + attemptWords;
                    break;
                case RetryEnd::NotRecorded:
                    lineText +=
                        ": " + (attemptWords.empty() ? std::string { "an attempt" } : attemptWords) + " not recorded";
                    break;
                case RetryEnd::Failed:
                    lineText += retryLine.lastAttempt.has_value() ? ": failed at " + attemptWords
                                                                  : std::string { ": failed at its starting value" };
                    lineText += ": " + step_value_text(recorded, numberStyle);
                    break;
                // The recorder never writes this end -- an entered result
                // leaves the trace empty -- but a row built by hand may.
                case RetryEnd::ManuallyEntered:
                    lineText += ": entered by a person";
                    break;
            }
        std::string const cited = citation_text(recorded.citation);
        return lineText + (cited.empty() ? " " + std::string { noCitationGiven } : " [" + cited + "]");
    }

    /// The output an opaque output's line names: `span of #2`, named from its
    /// call's row, or `output of #2` -- `an opaque output` with no operand --
    /// without one.
    [[nodiscard]] inline std::string opaque_output_label(ShownStep const& recorded, OpaqueLine const& opaqueLine)
    {
        if (opaqueLine.call != nullptr && opaqueLine.outputIndex.has_value()
            && *opaqueLine.outputIndex < opaqueLine.call->outputs.size())
            return escaped_author_text(opaqueLine.call->outputs[*opaqueLine.outputIndex].name) + " of "
                   + sole_operand(recorded);
        return recorded.operands.empty() ? std::string { "an opaque output" } : "output of " + sole_operand(recorded);
    }

    /// An opaque output's line, without its number: `span of #2 = 88 g`, the
    /// output named from its call's row; `output of #2` without one.
    ///
    /// Ends `[inside not shown]` unless its operand is the call's own step,
    /// whose line says so: an output recorded by a sink that does not hear the
    /// opaque hooks sits straight over the call's inputs, and without the
    /// clause its line would read as though an input were passed on
    /// unchanged. Judged by the operand step's kind (`OpaqueLine::overCall`),
    /// never by the presence of a row.
    [[nodiscard]] inline std::string opaque_output_line(ShownStep const& recorded,
                                                        OpaqueLine const& opaqueLine,
                                                        NumberStyle numberStyle)
    {
        std::string const outputText = opaque_output_label(recorded, opaqueLine);
        std::string const valueText =
            recorded.error.has_value() ? std::string { describe(*recorded.error) }
                                       : opaque_value_text(recorded.dimension, recorded.unit, recorded.value, numberStyle);
        return outputText + " = " + valueText + (opaqueLine.overCall ? "" : " [inside not shown]");
    }

    /// A rounded opaque output's line, without its number: `round(slope of #4,
    /// to 4 dp of mm/s) = 3393/5000 mm/s [nearest, ties to even]`, the output
    /// named from its call's row, as `opaque_output_line` names one.
    ///
    /// The value is the rounded decimal the step holds, exact, so no style
    /// writes `≈` before it; the mode follows in brackets, as a rounding's
    /// does. A failure the call carried reads as the call's: `[the operation
    /// itself failed, not any input]`, or `[carried up from #k]` naming the
    /// call's step, in place of the mode. A failure of the rounding itself,
    /// after the call answered, keeps the mode, as a failed rounding's line
    /// does. Ends `[inside not shown]` unless its operand is the call's own
    /// step (`OpaqueLine::overCall`), as an output's line does.
    [[nodiscard]] inline std::string rounded_opaque_output_line(ShownStep const& recorded,
                                                                OpaqueLine const& opaqueLine,
                                                                NumberStyle numberStyle)
    {
        std::string lineText = rounding_call_text(opaque_output_label(recorded, opaqueLine),
                                                  recorded.granularity,
                                                  unit_symbol_text(recorded.unit))
                               + " = ";
        bool const callFailed = opaqueLine.call != nullptr && opaqueLine.call->failure != OpaqueFailure::None;
        if (recorded.error.has_value() && callFailed)
            lineText += std::string { describe(*recorded.error) }
                        + opaque_failure_suffix(recorded,
                                                opaqueLine.call->failure,
                                                recorded.operands.empty()
                                                    ? std::nullopt
                                                    : std::optional<std::size_t> { recorded.operands.front() });
        else if (recorded.error.has_value())
            lineText += std::string { describe(*recorded.error) } + rounding_mode_suffix(recorded.mode);
        else
            lineText += opaque_value_text(recorded.dimension, recorded.unit, recorded.value, numberStyle)
                        + rounding_mode_suffix(recorded.mode);
        return lineText + (opaqueLine.overCall ? "" : " [inside not shown]");
    }

    /// What @p trace's side tables hold for the retry step at @p stepIndex
    /// (`RetryLine`).
    [[nodiscard]] inline RetryLine retry_line_of(Trace<Rational> const& trace, std::size_t stepIndex)
    {
        Step<Rational> const& recorded = trace.steps[stepIndex];
        if (recorded.kind == StepKind::RetryAttempt)
        {
            RetryLine attemptLine { .attempt = attempt_data(trace, stepIndex) };
            if (attemptLine.attempt != nullptr && attemptLine.attempt->judgement == AttemptJudgement::JudgementFailed
                && !recorded.operands.empty() && recorded.operands.back() < trace.steps.size())
                attemptLine.judgementError = trace.steps[recorded.operands.back()].error;
            return attemptLine;
        }
        if (recorded.kind != StepKind::RetryConcluded)
            return {};
        RetryLine concludedLine { .retry = retry_data(trace, stepIndex) };
        for (std::size_t const operandIndex: recorded.operands)
            if (AttemptStepData const* const claimed = attempt_data(trace, operandIndex); claimed != nullptr)
                concludedLine.lastAttempt = claimed->attemptNumber;
        return concludedLine;
    }

    /// What @p trace's side tables hold for the opaque step at @p stepIndex:
    /// its own row, or the row of the call an output claimed and which output
    /// it selected (`OpaqueLine`).
    [[nodiscard]] inline OpaqueLine opaque_line_of(Trace<Rational> const& trace, std::size_t stepIndex)
    {
        Step<Rational> const& recorded = trace.steps[stepIndex];
        if (recorded.kind == StepKind::OpaqueOperation)
        {
            OpaqueLine callLine { .call = opaque_data(trace, stepIndex) };
            for (std::size_t const operandIndex: recorded.operands)
                if (operandIndex < trace.steps.size() && trace.steps[operandIndex].error.has_value())
                    callLine.failedInput = operandIndex;
            return callLine;
        }
        if (recorded.kind != StepKind::OpaqueOutput && recorded.kind != StepKind::RoundedOpaqueOutput)
            return {};
        OpaqueLine outputLine {};
        if (OpaqueOutputStepData const* const chosenOutput = opaque_output_data(trace, stepIndex); chosenOutput != nullptr)
            outputLine.outputIndex = chosenOutput->outputIndex;
        if (recorded.operands.size() == 1 && recorded.operands.front() < trace.steps.size()
            && trace.steps[recorded.operands.front()].kind == StepKind::OpaqueOperation)
        {
            outputLine.overCall = true;
            outputLine.call = opaque_data(trace, recorded.operands.front());
        }
        return outputLine;
    }

    /// One step's line, without its number: the expression, an `=`, the value,
    /// and a trailing clause for the kinds that need one -- a citation for
    /// `Documented`, the variant and its discriminator for `VariantSelected`,
    /// a justification for `NumericValue`, the tie-breaking rule
    /// for the three rounding kinds, the granularity, provenance and tie rule
    /// for a method's rounding rule, the overlay that fixed an overridden
    /// constant, `, entered by hand` or `, calculated` for a variable whose
    /// value was not measured, for a `Conditional` whose predicate never
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
    ///
    /// @p numberStyle is the style `render_trace` was given, and every helper
    /// below that spells a number takes it as a parameter -- the one way a
    /// style travels, since several of them show a value through a `Step`
    /// they build themselves, a curve's point or an opaque output. The step's
    /// own value is spelled in its `exact_only()` instead when it is a number
    /// typed rather than computed (`value_is_typed`), and so are a curve's
    /// points or values that come from a typed series (`curve_part_is_typed`).
    [[nodiscard]] inline std::string escaped_step_line(Trace<Rational> const& trace,
                                                       std::size_t stepIndex,
                                                       ShownStep const& recorded,
                                                       std::size_t& budget,
                                                       NumberStyle numberStyle,
                                                       std::span<LimitRow const> limits)
    {
        NumberStyle const valueStyle = value_is_typed(trace, stepIndex) ? numberStyle.exact_only() : numberStyle;
        // A retry's attempt and its conclusion read their side tables.
        if (recorded.kind == StepKind::RetryAttempt || recorded.kind == StepKind::RetryConcluded)
        {
            RetryLine const retryLine = retry_line_of(trace, stepIndex);
            return recorded.kind == StepKind::RetryAttempt ? retry_attempt_line(recorded, retryLine, numberStyle)
                                                           : retry_concluded_line(recorded, retryLine, numberStyle);
        }
        // The previous attempt at the first attempt of a retry with no
        // starting value: the author's mistake, which says so.
        if (recorded.kind == StepKind::PreviousAttempt && recorded.error == ArithmeticError::DomainError)
            return step_expression(recorded) + " = previous attempt: none before attempt 1";
        // A determination nobody recorded: not "not measured", which would
        // say a measurement was due and missed rather than never entered --
        // unless a person typed the series in and left it empty, which reads
        // as a variable typed in empty does.
        if (recorded.kind == StepKind::AttemptInput && !recorded.error.has_value() && !recorded.value.has_value())
            return step_expression(recorded)
                   + (recorded.inputSource == ValueSource::ManuallyEntered ? " = (entered by hand as empty)"
                                                                           : " = (not recorded)");
        OpaqueLine const opaqueLine = opaque_line_of(trace, stepIndex);
        // An opaque call's line is its outputs, and ends saying its inside is
        // not shown; an output's names the output.
        if (recorded.kind == StepKind::OpaqueOperation)
            return opaque_call_line(recorded, opaqueLine, budget, numberStyle);
        if (recorded.kind == StepKind::OpaqueOutput)
            return opaque_output_line(recorded, opaqueLine, numberStyle);
        if (recorded.kind == StepKind::RoundedOpaqueOutput)
            return rounded_opaque_output_line(recorded, opaqueLine, valueStyle);
        // A series first, before anything reads `value`: its values are its
        // elements.
        // A per-element rounding ends with its mode, as a scalar rounding
        // does -- after the elements, however many were shown.
        if (recorded.kind == StepKind::ElementwiseRound)
            return series_step_line(recorded, budget, valueStyle) + rounding_mode_suffix(recorded.mode);
        // A conformity check has outcomes, not a value, and spends the
        // element budget on them as a series does on its elements.
        if (recorded.kind == StepKind::ConformityChecked)
            return conformity_line(recorded, limits, budget, numberStyle);
        if (is_series(recorded.kind))
            return series_step_line(recorded, budget, valueStyle);
        // A curve's values are its pairs, which spend the element budget as
        // a series' elements do.
        if (recorded.kind == StepKind::CurvePairing || recorded.kind == StepKind::CurveSplice)
            return curve_step_line(
                recorded,
                budget,
                curve_part_is_typed(trace, stepIndex, CurvePart::Points) ? numberStyle.exact_only() : numberStyle,
                curve_part_is_typed(trace, stepIndex, CurvePart::Values) ? numberStyle.exact_only() : numberStyle);
        if (recorded.kind == StepKind::Constraint)
            return constraint_expression(recorded) + constraint_outcome_suffix(recorded);
        if (recorded.kind == StepKind::AcceptanceChecked)
            return acceptance_expression(recorded) + acceptance_suffix(recorded);
        if (is_rejection_step(recorded.kind))
            return rejection_line(trace, stepIndex, recorded, numberStyle);
        // A comparison, not a quantity: the attribute and both keys, then the
        // verdict after a comma, as a typed-in input's source is given.
        if (recorded.kind == StepKind::LineageChecked)
            return lineage_expression(recorded) + ", " + constraint_outcome_text(recorded.outcome);

        // An input typed in but left empty was never going to be measured, so
        // "(not measured)" would be false of it, and ", entered by hand"
        // beside it would contradict it. It reads "(entered by hand as
        // empty)": a person's entry, and what it held.
        bool const enteredButEmpty = recorded.kind == StepKind::Variable
                                     && recorded.inputSource == ValueSource::ManuallyEntered
                                     && !recorded.value.has_value() && !recorded.error.has_value();
        // A value the environment calculated was never going to be measured
        // either: an empty one reads "(no value)" -- the calculation had
        // nothing to give it -- and ", calculated" below still says whose
        // value it is.
        bool const calculatedButEmpty = recorded.kind == StepKind::Variable
                                        && recorded.inputSource == ValueSource::Derived
                                        && !recorded.value.has_value() && !recorded.error.has_value();
        // A read from a record that was bound, withheld because a lineage key
        // was unknown: nothing was read, so "(not measured)" would be false of
        // a record whose values may well have been measured. Its line holds no
        // operand but its attribute steps, which `as_rendered` leaves out.
        bool const withheld = recorded.kind == StepKind::RecordScope && recorded.readFrom.has_value()
                              && recorded.readFrom->is_bound() && recorded.operands.empty()
                              && !recorded.value.has_value() && !recorded.error.has_value();
        std::string const valueText = enteredButEmpty      ? std::string { "(entered by hand as empty)" }
                                      : calculatedButEmpty ? std::string { "(no value)" }
                                      : withheld           ? std::string { "(not read: lineage not checked)" }
                                                           : step_value_text(recorded, valueStyle);
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
                 || recorded.kind == StepKind::RoundedRoot || recorded.kind == StepKind::RoundedNaturalLogarithm
                 || recorded.kind == StepKind::RoundedDecimalLogarithm || recorded.kind == StepKind::RoundedExponential)
            annotation = rounding_mode_suffix(recorded.mode);
        else if (recorded.kind == StepKind::RoundingRuleApplied)
            annotation = rounding_rule_suffix(recorded);
        // A constant that replaced a value a person typed in says so, inside
        // the clause that says who fixed it: the typed value was not used, and
        // a reader must not assume it was. An entry typed in empty says that
        // too, in the words a typed-in empty input has: no value was replaced.
        else if (recorded.kind == StepKind::OverriddenConstant)
            annotation = recorded.inputSource == ValueSource::ManuallyEntered
                             ? " [fixed by " + overlay_source_text(recorded.citation)
                                   + (recorded.replacedEntryEmpty ? ", replacing a value entered by hand as empty]"
                                                                  : ", replacing a value entered by hand]")
                             : overridden_constant_suffix(recorded.citation);
        else if (recorded.kind == StepKind::SnappedToPermitted)
            annotation = snap_suffix(recorded, numberStyle);
        else if (recorded.kind == StepKind::CurveInterpolation)
            annotation = curve_interpolation_suffix(recorded, numberStyle);
        // A derived quantity that replaced a value a person typed in says so,
        // in the clause that says who derived it, as a fixed constant does.
        else if (recorded.kind == StepKind::DerivedQuantity)
            annotation = recorded.inputSource == ValueSource::ManuallyEntered
                             ? " [derived by " + overlay_source_text(recorded.citation)
                                   + (recorded.replacedEntryEmpty ? ", replacing a value entered by hand as empty]"
                                                                  : ", replacing a value entered by hand]")
                             : derived_quantity_suffix(recorded.citation);
        else if (recorded.kind == StepKind::ReplacedVariant)
            annotation = replaced_variant_suffix(recorded.citation);
        // A typed-in input says so; a measured one says nothing, since
        // measured is what an input is unless told otherwise. After a comma,
        // not in a bracket: it is a plain statement about where the number
        // came from, not a clause qualifying how it was computed.
        else if ((recorded.kind == StepKind::Variable || recorded.kind == StepKind::AttemptInput)
                 && recorded.inputSource == ValueSource::ManuallyEntered && !enteredButEmpty)
            annotation = ", entered by hand";
        // A value the environment calculated from its other values says so,
        // after a comma, as a typed-in one does: it was neither measured nor
        // typed in, and a reader looking for where it came from looks for its
        // calculation. Said of an empty or a failed value too: where a value
        // came from does not change with what it holds.
        else if (recorded.kind == StepKind::Variable && recorded.inputSource == ValueSource::Derived)
            annotation = ", calculated";
        // Present for a lookup that succeeded as well as for one that failed,
        // unlike the three suffixes above: on a hit it names the band the
        // value fell in, and on a failure it is the only thing separating a
        // miss from a relayed error. See `lookup_suffix`.
        else if (is_lookup(recorded.kind))
            annotation = lookup_suffix(trace, stepIndex, recorded, numberStyle);
        else if (recorded.kind == StepKind::PrecisionLevel)
            annotation = precision_suffix(trace, stepIndex);
        // A mean or a variance whose total overflowed names the determination
        // it overflowed at, counted from one, as a failed series step does.
        else if ((recorded.kind == StepKind::SampleMean || recorded.kind == StepKind::SampleVariance)
                 && recorded.error.has_value() && recorded.failedElement.has_value())
            annotation = sample_failure_suffix(trace, recorded);

        // Whose record a quantity was read from, first among the clauses: it
        // qualifies the number itself, and whatever follows -- typed in by
        // hand, fixed by an overlay -- is said of that record's value. The
        // scope's own step says it in its expression instead.
        bool const namesQuantity = recorded.kind == StepKind::Variable || recorded.kind == StepKind::OverriddenConstant
                                   || recorded.kind == StepKind::DerivedQuantity;
        if (namesQuantity && recorded.readFrom.has_value())
            annotation = ", " + record_origin_text(*recorded.readFrom) + annotation;

        if (recorded.kind == StepKind::Constant)
            return valueText + annotation;
        if (recorded.kind == StepKind::PrecisionLevel || recorded.kind == StepKind::PrecisionLimit)
            return precision_expression(trace, stepIndex, recorded) + " = " + valueText + annotation;
        return step_expression(recorded) + " = " + valueText + annotation;
    }

    /// @p recorded as its line shows it, when that differs from the step as
    /// recorded. A scope's step claims its lineage attribute steps as its
    /// first operands -- that is the record of what was compared before the
    /// read -- but its line names the value it read, so the attribute steps,
    /// each on a line of its own already, are left out of the operands the
    /// line names. Every other step is its own line as recorded, and is
    /// answered empty rather than copied: a series step's elements are not
    /// copied once more for every render.
    [[nodiscard]] inline std::optional<Step<Rational>> as_rendered(Step<Rational> const& recorded,
                                                                   std::vector<Step<Rational>> const& allSteps)
    {
        if (recorded.kind != StepKind::RecordScope)
            return std::nullopt;
        Step<Rational> shown = recorded;
        std::erase_if(shown.operands, [&allSteps](std::size_t operandIndex) {
            return operandIndex < allSteps.size() && allSteps[operandIndex].kind == StepKind::LineageChecked;
        });
        return shown;
    }

    /// One step's line, without its number -- see `escaped_step_line` for
    /// what it holds. The one place author text is escaped: every piece of it
    /// in @p recorded is escaped into an `EscapedStep` here, before anything
    /// reads it, and the line is rendered from that copy.
    ///
    /// @p limits are the rows a `ConformityChecked` step judged against
    /// (`Trace::conformityLimits`), and empty for every other kind. The
    /// record the step was read from and its lineage comparison are read
    /// from @p trace's own side tables (`Trace::origins`,
    /// `Trace::lineageChecks`) into the copy -- see `ShownStep`.
    /// @p numberStyle is how the line spells every number it states, handed
    /// on to `escaped_step_line`.
    [[nodiscard]] inline std::string step_line(Trace<Rational> const& trace,
                                               std::size_t stepIndex,
                                               std::size_t& budget,
                                               NumberStyle numberStyle,
                                               std::span<LimitRow const> limits = {})
    {
        Step<Rational> const& recorded = trace.steps[stepIndex];
        std::optional<Step<Rational>> const asShown = as_rendered(recorded, trace.steps);
        EscapedStep const escaped { asShown.has_value() ? *asShown : recorded, origin_of(trace, recorded),
                                    lineage_of(trace, stepIndex) };
        return escaped_step_line(trace, stepIndex, escaped.step, budget, numberStyle, limits);
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
/// Every number is spelled in @p options.numbers -- fractions unless the
/// caller asks for decimals; see `TraceRenderOptions::numbers` for which
/// numbers are shown exact whatever the style.
///
/// Only an exact (`Rational`) trace can be rendered. Converting a value back
/// into the unit it was declared in is the unit layer's exact
/// multiply-then-divide, and there is no such operation for binary floating
/// point: a `double` trace would need a rounding policy, and choosing one on a
/// caller's behalf is how an audit trail acquires a number nobody can
/// reproduce. Evaluate in `double` by all means; print the exact trace. A
/// value the exact layer cannot hold is no reason to trace in `double`
/// either: a formula declares the precision it is reported at, and the trace
/// shows that exact decimal -- `rounded_sqrt` for a root, `rounded_ln`,
/// `rounded_log10` and `rounded_exp` for a logarithm or an exponential, and
/// `rounded_output` for an output of an opaque operation that computes in
/// wider integers, as `linear_least_squares` does. Any other operation's
/// rounded output is computed in `Rational`, and fails with `Overflow` where
/// the exact output would.
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
        renderedTrace +=
            detail::step_line(trace, shown, budget, options.numbers, detail::conformity_limits_of(trace, shown));
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

namespace detail
{
    /// @p shownText with a backslash, and every character that could end a
    /// line, escaped as `escaped_author_text` escapes them -- and brackets
    /// and semicolons left as they are. For a rendered formula in a
    /// derivation's header: `render` writes its own brackets and
    /// semicolons, which must read as written, while the author's symbols in
    /// it may hold anything, and none of it may end the line.
    [[nodiscard]] inline std::string line_safe_text(std::string_view shownText)
    {
        std::string escaped;
        escaped.reserve(shownText.size());
        for (char const glyph: shownText)
        {
            auto const byte = static_cast<unsigned char>(glyph);
            if (glyph == '\\' || byte < 0x20 || byte == 0x7f)
                escaped += escaped_author_text(std::string_view { &glyph, 1 });
            else
                escaped += glyph;
        }
        return escaped;
    }

    /// Each definition of @p definitionSet as plain text, every symbol
    /// written as @p vocabulary says, in the order given; @p Is counts them.
    template <typename... Ds, Vocabulary V, std::size_t... Is>
    [[nodiscard]] std::array<std::string, sizeof...(Ds)> rendered_definitions(Calculation<Ds...> const& definitionSet,
                                                                              V const& vocabulary,
                                                                              std::index_sequence<Is...>)
    {
        return { line_safe_text(render<Dialect::Plain>(std::get<Is>(definitionSet.definitions).expression, vocabulary))... };
    }

    /// Whether @p shown's step @p stepIndex reads, from the worksheet, a
    /// calculated value whose own block's value is typed: a `Variable` step
    /// whose source is `Derived`, matched to that value's block by the slot it
    /// read (`WorksheetEntry::readSlots`), which @p typedSlots sets. False for
    /// a step whose slot is unknown, as in an entry edited by hand.
    [[nodiscard]] inline bool reads_typed_block(WorksheetEntry const& shown, std::size_t stepIndex, std::uint64_t typedSlots)
    {
        Step<Rational> const& readingStep = shown.trace.steps[stepIndex];
        if (readingStep.kind != StepKind::Variable || readingStep.inputSource != ValueSource::Derived)
            return false;
        if (stepIndex >= shown.readSlots.size() || shown.readSlots[stepIndex] >= 64)
            return false;
        return ((typedSlots >> shown.readSlots[stepIndex]) & 1u) != 0;
    }

    /// Whether the value @p shown's step @p stepIndex states is a number typed
    /// rather than computed: as `value_is_typed` decides within the block, or
    /// where the step, or a step whose value it passes on, reads a calculated
    /// value whose own block's value is typed (`reads_typed_block`). So a
    /// typed value is stated alike wherever the derivation states it: in its
    /// block's header, on its root's line, and on each line of another block
    /// that reads it.
    [[nodiscard]] inline bool derivation_value_is_typed(WorksheetEntry const& shown,
                                                        std::size_t stepIndex,
                                                        std::uint64_t typedSlots)
    {
        std::size_t stated = stepIndex;
        // Each step passed from is an earlier one, so this ends.
        while (!states_typed_value(shown.trace.steps[stated].kind) && !reads_typed_block(shown, stated, typedSlots))
        {
            std::optional<std::size_t> const passedFrom = value_passed_from(shown.trace, stated);
            if (!passedFrom.has_value())
                return false;
            stated = *passedFrom;
        }
        return true;
    }

    /// The slots of @p entries' calculated blocks whose value is typed -- its
    /// root's, as `derivation_value_is_typed` decides -- as a set of bits.
    /// Each block comes before the blocks of the values it reads, so walking
    /// them from the last decides every value a block reads before the block.
    [[nodiscard]] inline std::uint64_t typed_block_slots(std::vector<WorksheetEntry> const& entries)
    {
        std::uint64_t typedSlots = 0;
        for (auto shown = entries.rbegin(); shown != entries.rend(); ++shown)
            if (shown->kind == WorksheetEntryKind::Calculated && !shown->trace.empty() && shown->slot < 64
                && derivation_value_is_typed(*shown, shown->trace.root(), typedSlots))
                typedSlots |= std::uint64_t { 1 } << shown->slot;
        return typedSlots;
    }

    /// The value of @p shown's block as its header states it: in the unit it
    /// was declared in, with that unit's symbol, spelled in @p numberStyle as
    /// a trace line spells a value (`checked_shown_text`), or `(not shown:
    /// ...)` where the style cannot spell it in that unit; why its
    /// calculation failed; or `(no value)`. Exact only when @p typed, the
    /// block's value being a number typed rather than computed
    /// (`derivation_value_is_typed`), so that the header, the block's root
    /// and every line reading the value agree on whether it is typed. That
    /// is all they agree on: a computed root states the value in the
    /// coherent unit, so it may differ from the header in its unit, its
    /// padding and its decimals, and one may read `≈` where the other does
    /// not -- a length of `1 u`, in a unit of a third of a metre, is
    /// `≈0.333` metres at its root.
    [[nodiscard]] inline std::string block_value_text(WorksheetEntry const& shown, NumberStyle numberStyle, bool typed)
    {
        if (shown.error.has_value())
            return std::string { describe(*shown.error) };
        if (!shown.value.has_value())
            return "(no value)";
        std::expected<NumberText, ArithmeticError> const spelled =
            checked_shown_text(*shown.value, typed ? numberStyle.exact_only() : numberStyle, shown.unit);
        if (!spelled)
            return not_shown_text(spelled.error());
        std::string valueText { spelled->view() };
        std::string const unitSymbol = unit_symbol_text(shown.unit);
        if (!unitSymbol.empty())
            valueText += " " + unitSymbol;
        return valueText;
    }

    /// The header of @p shown's block, @p definitionText being its
    /// definition as rendered and its value spelled in @p numberStyle,
    /// exact when @p typed (`block_value_text`): `symbol = definition =
    /// value` for a value calculated, and `symbol = value, entered by hand
    /// in place of definition` for one overridden.
    [[nodiscard]] inline std::string block_header(WorksheetEntry const& shown,
                                                  std::string const& definitionText,
                                                  NumberStyle numberStyle,
                                                  bool typed)
    {
        std::string const symbolText = escaped_author_text(shown.symbol);
        if (shown.kind == WorksheetEntryKind::Overridden)
            return symbolText + " = "
                   + (shown.value.has_value() ? block_value_text(shown, numberStyle, typed) + ", entered by hand"
                                              : std::string { "(entered by hand as empty)" })
                   + " in place of " + definitionText;
        return symbolText + " = " + definitionText + " = " + block_value_text(shown, numberStyle, typed);
    }
} // namespace detail

/// Renders @p explained, a worksheet's derivation, as text a person reads,
/// bounded by @p options.maxSteps.
///
/// Each calculated value's block opens with a header, `symbol = definition =
/// value` -- the definition as `render` writes it, and the value in the unit
/// the quantity was declared in, or why calculating it failed, or `(no
/// value)` -- followed by its steps, indented and numbered from one within
/// the block, each line as `render_trace` writes it. An overridden value's
/// block is the header alone: `symbol = value, entered by hand in place of
/// definition`. The inputs read follow under a line `inputs`, one indented
/// line each, as `render_trace` writes a variable's step. A computed step
/// states its value in the coherent unit of its dimension, as in
/// `render_trace` (`docs/tracing.md`, "Reading a derivation"), so it may
/// read differently from the header above it: `fridge_kwh = fridge_kw *
/// fridge_h = 24/5 kWh` over `3. #1 * #2 = 17280000`, in joules. Under a
/// rounding style the two may differ in their decimals too, one reading `≈`
/// where the other does not.
///
/// **Every number is spelled in @p options.numbers**, fractions unless the
/// caller names another style, as `render_trace` spells a trace's: each
/// step and input line exactly as `render_trace` writes it in that style;
/// each header's definition as `render` writes it under `RenderOptions`
/// with that style, its typed numbers exact and never padded; and each
/// header's value as a trace line states a value in its unit, rounded and
/// marked `≈` only where the style asks and the block's value was computed
/// rather than typed, and `(not shown: ...)` where the style cannot spell it
/// there. A typed value is exact wherever the derivation states it: in its
/// block's header, on its root's line, and on each line of another block
/// that reads it (`WorksheetEntry::readSlots`) -- where it reads as its
/// header does, both in the unit the quantity was declared in.
///
/// **One budget bounds it all.** Every header, step and input line spends
/// one unit of @p options.maxSteps, and a step showing a series spends one
/// more on each element it shows, as in `render_trace`. When it runs out,
/// one last line says how many headers, steps and input lines were left
/// out: `... 12 further steps not shown`. `render_derivation(explained,
/// {})` does not compile, for the reason `StepLimit` gives.
///
/// Nothing is rendered for a derivation with no blocks.
template <Described Result, typename... Ds, Vocabulary V>
[[nodiscard]] std::string render_derivation(ExplainedWorksheet<Result, Calculation<Ds...>, V> const& explained,
                                            TraceRenderOptions options)
{
    using Graph = detail::CalculationGraph<Ds...>;
    std::string derivationText;
    if constexpr (Graph::valid)
    {
        std::array<std::string, sizeof...(Ds)> const definitionTexts =
            detail::rendered_definitions(explained.calculation,
                                         detail::styled(explained.vocabulary, options.numbers),
                                         std::index_sequence_for<Ds...> {});
        std::uint64_t const typedSlots = detail::typed_block_slots(explained.entries);
        std::size_t budget = options.maxSteps.value;
        std::size_t notShown = 0;
        bool inputsHeaded = false;
        for (WorksheetEntry const& shown: explained.entries)
        {
            if (shown.kind == WorksheetEntryKind::Input)
            {
                if (budget == 0)
                {
                    ++notShown;
                    continue;
                }
                --budget;
                if (!inputsHeaded)
                    derivationText += "inputs\n";
                inputsHeaded = true;
                derivationText += "  ";
                derivationText += shown.trace.empty()
                                      ? detail::escaped_author_text(shown.symbol) + " = "
                                            + detail::block_value_text(shown, options.numbers, false)
                                      : detail::step_line(shown.trace, shown.trace.root(), budget, options.numbers);
                derivationText += "\n";
                continue;
            }

            std::size_t const stepCount = shown.kind == WorksheetEntryKind::Calculated ? shown.trace.steps.size() : 0;
            if (budget == 0)
            {
                notShown += 1 + stepCount;
                continue;
            }
            --budget;
            bool const defined = shown.slot >= Graph::inputCount && shown.slot < Graph::slotCount;
            derivationText += detail::block_header(
                shown,
                defined ? definitionTexts[shown.slot - Graph::inputCount] : std::string { "(no definition)" },
                options.numbers,
                !shown.trace.empty() && detail::derivation_value_is_typed(shown, shown.trace.root(), typedSlots));
            derivationText += "\n";
            for (std::size_t stepIndex = 0; stepIndex < stepCount; ++stepIndex)
            {
                if (budget == 0)
                {
                    notShown += stepCount - stepIndex;
                    break;
                }
                --budget;
                derivationText += "  " + std::to_string(stepIndex + 1) + ". ";
                // A line whose value is typed only because it reads a typed
                // value from another block is exact, as that block is.
                bool const readsTyped = !detail::value_is_typed(shown.trace, stepIndex)
                                        && detail::derivation_value_is_typed(shown, stepIndex, typedSlots);
                derivationText += detail::step_line(shown.trace,
                                                    stepIndex,
                                                    budget,
                                                    readsTyped ? options.numbers.exact_only() : options.numbers,
                                                    detail::conformity_limits_of(shown.trace, stepIndex));
                derivationText += "\n";
            }
        }

        if (notShown > 0)
            derivationText += "... " + std::to_string(notShown)
                              + (notShown == 1 ? " further step not shown\n" : " further steps not shown\n");
    }
    return derivationText;
}

} // namespace formula
