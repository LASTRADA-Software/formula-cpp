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
    /// **The branch that ran is the last operand**, when one ran at all:
    /// operands are recorded in evaluation order and the branch is dispatched
    /// after the predicate. What says whether one ran is `step.branch`, not
    /// the operand count -- a predicate side that produced no step of its own
    /// (an untraced extension node; see `binary_expression` above) reduces
    /// the count too, and only `branch` distinguishes the two.
    ///
    /// A predicate side that recorded no step is written `(not evaluated)`,
    /// which is what actually happened: the one arity below two that arises
    /// in practice is a predicate whose left side raised an arithmetic error,
    /// after which the evaluator never dispatched the right one at all. (The
    /// mirror case -- a left side that is an untraced extension node -- would
    /// be labelled the wrong way round here, the same imprecision
    /// `binary_expression` above already accepts for the same cause, and
    /// reachable only by a consumer who has written such a node.)
    template <typename Rep>
    [[nodiscard]] std::string conditional_expression(Step<Rep> const& step)
    {
        bool const branchRan = step.branch != Branch::Neither && !step.operands.empty();
        std::size_t const predicateOperands = step.operands.size() - (branchRan ? 1u : 0u);

        std::string const notEvaluated { "(not evaluated)" };
        std::string const lhs = predicateOperands >= 1 ? operand_reference(step.operands[0]) : notEvaluated;
        std::string const rhs = predicateOperands >= 2 ? operand_reference(step.operands[1]) : notEvaluated;

        std::string text = "if " + lhs + " " + std::string { describe(step.comparison) } + " " + rhs;
        if (branchRan)
            text += " " + std::string { describe(step.branch) } + " " + operand_reference(step.operands.back());
        return text;
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

    /// One step's line, without its number: the expression, an `=`, the value,
    /// and a trailing clause for the four kinds that need one -- a citation
    /// for `Documented`, a justification for `NumericValue`, which branch ran
    /// for `Conditional`, and the tie-breaking rule for the two rounding
    /// kinds.
    [[nodiscard]] inline std::string step_line(Step<Rational> const& step)
    {
        std::string const value = step_value_text(step);
        std::string suffix;
        if (step.kind == StepKind::Documented)
            suffix = citation_suffix(step.citation);
        else if (step.kind == StepKind::NumericValue)
            suffix = justification_suffix(step.justification);
        else if (step.kind == StepKind::Conditional)
            suffix = " [" + std::string { describe(step.branch) } + "]";
        else if (step.kind == StepKind::Round || step.kind == StepKind::RoundSignificant)
            suffix = rounding_mode_suffix(step.mode);

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
