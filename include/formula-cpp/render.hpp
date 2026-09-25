// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Turning a formula back into something a person reads.
///
/// The walk brackets a child exactly when it binds more loosely than the
/// context it sits in, and raises that context by one for the right operand of
/// a subtraction or a division -- because `a - (b - c)` is not `(a - b) - c`.
/// Bracketing on equal precedence everywhere would produce `(a / b) * 100`,
/// which is noise; never bracketing on equal precedence would produce
/// `a - b - c` for `a - (b - c)`, which is wrong.
///
/// **This header is deliberately absent from `formula.hpp`.** It pulls
/// `<string>`, and a consumer who only evaluates numbers must not compile a
/// string formatter in every translation unit. Include it when you want text.
///
/// **Ruling, decided here once and binding on every other surface: a
/// half-open interval is spelled `<low> to under <high>`, never `[low,
/// high)`.** A band really is `[10, 20)` (`band.hpp`), and that is exactly
/// the character sequence CommonMark reads as a link label -- the defect
/// phase 8 published, when `round[to 1 dp of mm](d)` reached a page with its
/// operand silently dropped. The guard test in `render_tests.cpp` asserts
/// that no Markdown rendering contains `](` or a bare `[`, and a band written
/// the mathematician's way would defeat it. `to under` is not a compromise
/// spelling: it *says* the exclusion in words, where a reader has to know the
/// bracket convention to see it, and it survives every Markdown flavour
/// because it contains no punctuation at all. Whatever renders a band next --
/// a trace, a `document()` walk, a guide, a gallery -- spells it this way,
/// because two surfaces naming the same thing differently is the phase-8
/// defect itself rather than a matter of taste.

#include <formula-cpp/band.hpp>
#include <formula-cpp/citation.hpp>
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/unit.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <type_traits>

namespace formula
{

/// How a formula should be spelled.
enum class Dialect
{
    /// Plain text, for a terminal or a log.
    Plain,
    /// Markdown: symbols in backticks, so an underscore is not read as emphasis.
    Markdown,
    /// LaTeX: fractions, radicals and braced exponents.
    LaTeX,
};

namespace detail
{
    /// How tightly a node binds. A trait rather than a number threaded through
    /// the walk, so a new node kind is a specialisation rather than an edit to
    /// every renderer.
    enum class Precedence
    {
        /// A comparison or a `when()` -- binds looser than every arithmetic
        /// operator, so either one needs a bracket wherever it sits as the
        /// operand of `+`, `-`, `*`, `/`, unary negation, or a power. Added in
        /// spec phase 8; every other rung keeps its original number.
        Conditional = 0,
        Additive = 1,
        Multiplicative = 2,
        Unary = 3,
        Atom = 4,
    };

    template <typename N>
    struct PrecedenceOf
    {
        static constexpr Precedence value = Precedence::Atom;
    };

    template <BinaryOperator Op, Node Left, Node Right>
    struct PrecedenceOf<BinaryNode<Op, Left, Right>>
    {
        static constexpr Precedence value = (Op == BinaryOperator::Add || Op == BinaryOperator::Subtract)
                                                ? Precedence::Additive
                                                : Precedence::Multiplicative;
    };

    template <UnaryOperator Op, Node Operand>
    struct PrecedenceOf<UnaryNode<Op, Operand>>
    {
        static constexpr Precedence value = Precedence::Unary;
    };

    /// A `when()`'s rendered shape ("if ... then ... else ..." or, in LaTeX, a
    /// `\begin{cases}` block) never changes with the data it carries, unlike
    /// `ConstantNode` below -- so the type-level trait alone is enough, and no
    /// runtime `precedence_of` overload is needed to get the right answer.
    template <Predicate P, Node Then, Node Else>
    struct PrecedenceOf<WhenNode<P, Then, Else>>
    {
        static constexpr Precedence value = Precedence::Conditional;
    };

    /// A comparison, for the same reason: its rendered shape is always
    /// `<lhs> <op> <rhs>` regardless of what the operands hold. `PredicateNode`
    /// is not a `Node`, so this trait is read directly by its `render_node`
    /// overload below rather than through the `Node`-constrained
    /// `precedence_of` runtime function -- see that overload.
    template <Comparison Op, Node Left, Node Right>
    struct PrecedenceOf<PredicateNode<Op, Left, Right>>
    {
        static constexpr Precedence value = Precedence::Conditional;
    };

    /// Forwards the *type-level* precedence of what it wraps. That is correct
    /// as far as it goes, but it is not what makes a citation invisible to
    /// bracketing: a wrapped `ConstantNode` needs its *runtime* answer
    /// forwarded too, which this trait cannot do -- see
    /// `precedence_of(DocumentedNode<Inner> const&)` below, which is the
    /// overload that actually closes the gap.
    template <Node Inner>
    struct PrecedenceOf<DocumentedNode<Inner>>
    {
        static constexpr Precedence value = PrecedenceOf<Inner>::value;
    };

    /// The precedence a node binds at, as a runtime value rather than a type.
    ///
    /// `PrecedenceOf` alone cannot bracket a negative constant correctly: it is
    /// a trait keyed on the node's *type*, and `ConstantNode<U>` is one type
    /// whether the number it holds is `5` or `-5`. But the sign is *data*, held
    /// in the `Rational` at runtime, not something the type system ever sees.
    /// A negative constant's rendered text opens with `-`, which reads like a
    /// unary minus, so it must bracket exactly where a `UnaryNode` would --
    /// `precedence_of` gives every node kind that same runtime answer, falling
    /// back to the type-level trait everywhere the trait is already correct.
    template <Node N>
    [[nodiscard]] constexpr Precedence precedence_of(N const&) noexcept
    {
        return PrecedenceOf<N>::value;
    }

    /// A constant's rendered text is not always an atom in two data-dependent
    /// ways the type does not carry: a negative number opens with a `-` that
    /// reads like a unary minus, and a unit with a symbol renders as *two*
    /// tokens ("150 mm") rather than one -- so `pow<2>` of it would otherwise
    /// read as `150 mm^2`, i.e. `150 * mm^2`, when the tree means `(150 mm)^2`.
    /// Both cases must bracket exactly where a `UnaryNode` would.
    template <Unit U>
    [[nodiscard]] constexpr Precedence precedence_of(ConstantNode<U> const& node) noexcept
    {
        constexpr Unit unit = U;
        bool const hasUnitSymbol = !view(unit.symbolText).empty();
        return node.number.sign() < 0 || hasUnitSymbol ? Precedence::Unary : Precedence::Atom;
    }

    /// A wrapper's *type* answer forwards correctly (`PrecedenceOf` above),
    /// but bracketing is decided from the runtime answer, and the generic
    /// `precedence_of(N const&)` overload would fall back to the type-level
    /// trait -- exactly the answer that is wrong once the wrapped node is a
    /// `ConstantNode`, whose bracketing depends on data the type never sees.
    /// Forwarding the runtime call here, not just the type-level trait above,
    /// is what makes wrapping a negative or unit-bearing constant in
    /// `documented()` bracket the same way the bare constant would.
    template <Node Inner>
    [[nodiscard]] constexpr Precedence precedence_of(DocumentedNode<Inner> const& node) noexcept
    {
        return precedence_of(node.inner);
    }

    /// An exact rational as text: `4`, or `1/4` when it is not whole.
    [[nodiscard]] inline std::string number_text(Rational value)
    {
        if (value.denominator() == 1)
            return std::to_string(value.numerator());
        return std::to_string(value.numerator()) + "/" + std::to_string(value.denominator());
    }

    /// A number followed by its unit's symbol, or the number alone when the
    /// unit has none (`unit::One`) -- `150 mm`, `19/20`.
    ///
    /// Factored out of `render_node(ConstantNode)`, which is the spelling this
    /// library already had, rather than invented for the lookup tables below:
    /// a table states a number in a unit on every one of its rows, and a row
    /// that spelled a number differently from a constant holding that same
    /// number would be two surfaces disagreeing inside one rendered formula.
    [[nodiscard]] inline std::string number_with_unit(std::string const& text, std::string_view symbol)
    {
        return symbol.empty() ? text : text + " " + std::string { symbol };
    }

    /// A bound a table declared as a numerator/denominator pair -- a band's
    /// low or high bound, or a breakpoint's key -- as text.
    ///
    /// Reduced through `Rational::make` first, so a bound typed `10/2` reads
    /// as `5` and `0/1` reads as `0`, exactly as a `ConstantNode` holding the
    /// same number does. The fallback is unreachable for any table that got as
    /// far as a renderer -- every lookup node instantiates its own table's
    /// `Require...` guard, which refuses a bound that names no rational number
    /// at compile time -- and is guarded anyway for the reason
    /// `detail::find_band` (`lookup.hpp`) guards the identical call: printing
    /// back the pair the author typed is better than dereferencing an error.
    [[nodiscard]] inline std::string declared_number_text(std::int64_t numerator, std::int64_t denominator)
    {
        std::expected<Rational, ArithmeticError> const value = Rational::make(numerator, denominator);
        if (value.has_value())
            return number_text(*value);
        return std::to_string(numerator) + "/" + std::to_string(denominator);
    }

    /// A half-open band as text: `10 to under 20 mm`. **The one spelling of a
    /// half-open interval in this library** -- see this file's comment for the
    /// ruling and for the published defect that bought it.
    [[nodiscard]] inline std::string band_text(Band const& value, std::string_view keySymbol)
    {
        return number_with_unit(declared_number_text(value.lowNumerator, value.lowDenominator) + " to under "
                                    + declared_number_text(value.highNumerator, value.highDenominator),
                                keySymbol);
    }

    /// A category key as text: `key 2`.
    ///
    /// **What this costs, stated rather than hidden.** An enumerator's *name*
    /// does not exist at run time in C++ -- there is no portable way to get
    /// `Cylinder` back out of a `Shape` -- so what a reader is given is the
    /// enumerator's underlying value, which is the only thing that does
    /// survive. A reader reconciling this against a published table therefore
    /// has to carry the author's own `enum class` declaration across: `key 2`
    /// says which row, not which *variant*. The alternative would be per-row
    /// labels on the node, and `lookup.hpp` declines those deliberately ("Per-
    /// row labels are not modelled here and are not smuggled into the node"),
    /// because a table's identity is `documented()`'s job. A later task that
    /// wants names has one honest seam: a `Describe`-style trait over the
    /// author's enumeration, opted into the way `Quantity` already is -- not a
    /// field on the node.
    ///
    /// The underlying value, not the row's index: the two coincide only for an
    /// enumeration whose enumerators were left to default, and an author who
    /// numbered theirs `{ Cube = 3, Cylinder = 7 }` would otherwise be shown
    /// numbers that appear nowhere in their own code. The signed and unsigned
    /// casts are spelled separately because an underlying type may be
    /// `unsigned long long`, whose top half no signed type can hold.
    template <typename Key>
    [[nodiscard]] std::string key_text(Key key)
    {
        using Underlying = std::underlying_type_t<Key>;
        if constexpr (std::is_signed_v<Underlying>)
            return "key " + std::to_string(static_cast<long long>(key));
        else
            return "key " + std::to_string(static_cast<unsigned long long>(key));
    }

    /// One row of a rendered lookup table: what selects the row, then what the
    /// row gives. `10 to under 20 mm gives 19/20`, `key 7 gives 1`,
    /// `at 25 mm gives 6/5`.
    [[nodiscard]] inline std::string lookup_row_text(std::string const& selector, std::string const& value)
    {
        return selector + " gives " + value;
    }

    /// A run of words a lookup contributes, as the dialect writes it: a row,
    /// the empty table's own statement, or an exact lookup's key.
    ///
    /// LaTeX sets words as upright text -- `to under`, `gives`, `key` and `at`
    /// are words, and math mode would set each as a product of italic letters
    /// (`k \cdot e \cdot y`) -- so it wraps them in `\text{...}`, the same
    /// construct `render_node(Constraint)` and `render_node(WhenNode)` already
    /// use for the words in their own output. This was found by reading the
    /// LaTeX output rather than by a string comparison, which is exactly the
    /// class of defect a string comparison cannot see.
    ///
    /// Everything a lookup renders goes through here **except the operand of
    /// a banded or an interpolating lookup**, which is a sub-expression and
    /// belongs in math mode -- it has already rendered itself in the dialect.
    ///
    /// **The wrapped text itself is byte-identical in all three dialects**,
    /// which is what lets the cross-dialect test in `render_tests.cpp` compare
    /// the dialects against each other rather than assert each separately.
    template <Dialect D>
    [[nodiscard]] std::string lookup_words_in_dialect(std::string const& words)
    {
        if constexpr (D == Dialect::LaTeX)
            return "\\text{" + words + "}";
        else
            return words;
    }

    /// The separator between a rendered lookup's fields.
    ///
    /// **LaTeX adds `\allowbreak`, and that is a correctness fix rather than
    /// typographic polish.** Each row is one atomic `\text{...}`, and TeX
    /// gives a math comma no break penalty at all -- so without this there is
    /// **no legal break point anywhere in a rendered lookup, at any row
    /// count**. A table does not wrap; it runs off the line, and a wide enough
    /// one runs off the paper. A reader of a truncated formula is told nothing
    /// is missing, which is the same class of defect as the Markdown link
    /// syntax that dropped an operand from a published page -- see this file's
    /// ruling.
    ///
    /// Measured with tectonic 0.17.0 over 22 renderings -- the 20 these tests
    /// build, plus a six-row table and its square, which is the width the
    /// review measured running off the paper -- as overfull `\hbox` against
    /// article's 345pt text block, worst case:
    ///
    /// | context                          | before | after |
    /// |----------------------------------|-------:|------:|
    /// | inline `$...$` inside prose      |  721pt |  88pt |
    /// | inline `$...$` alone in a para    |  549pt | 125pt |
    /// | display `\[...\]`                 |  549pt | 549pt |
    ///
    /// The middle row is the honest caveat: a formula sitting alone in its own
    /// paragraph has no interword glue to justify a broken line with, so TeX
    /// finds every two-line split too loose for `\tolerance` and falls back to
    /// one overfull line. Only the widest cases gain there. That is a TeX
    /// limitation rather than something the emitted text can fix, and it does
    /// not touch the case this library is actually for -- a formula quoted in
    /// a sentence of generated documentation, the top row, where the worst
    /// case drops by 88%.
    ///
    /// **Display math is a separate decision, and the decision is that this
    /// library emits nothing for it.** TeX does not break a display across
    /// lines at all, so `\allowbreak` is inert there -- measured, byte for
    /// byte the same overfull boxes with and without it. The only thing that
    /// could break a display is an explicit `\\`, and that is refused on two
    /// measured grounds, not one: `\\` is a hard error in the far commoner
    /// `$...$` and `\[...\]`, **and** wrapping these in amsmath's `multline*`
    /// does not help anyway (measured: 571pt worst, slightly *worse* than the
    /// plain display, because `multline` also breaks only at an explicit `\\`
    /// and never at `\allowbreak`). An earlier revision of this comment
    /// claimed `multline` fixed it; it does not, and the claim was reasoned
    /// rather than measured.
    ///
    /// What does work, for a caller who genuinely needs a wide table in a
    /// display, is `breqn`'s `dmath`: **0 overfull boxes** on the same
    /// renderings -- and 0 with `\allowbreak` stripped out too, so that is
    /// entirely the caller's package doing the work and owes nothing to this
    /// function. `render_tests.cpp` pins both halves of the decision: that
    /// every field separator carries `\allowbreak`, and that a lookup never
    /// emits `\\`.
    template <Dialect D>
    [[nodiscard]] std::string lookup_separator()
    {
        if constexpr (D == Dialect::LaTeX)
            return ",\\allowbreak ";
        else
            return ", ";
    }

    /// Assembles a rendered lookup: `<name>(<subject>, <row>, <row>, ...)`,
    /// where @p rows is already separator-prefixed and dialect-wrapped, one
    /// field per row.
    ///
    /// **This is not a fourth punctuation style.** It is the shape
    /// `round(d, to 1 dp of mm)` and `numeric(f, in MPa)` already have: a call
    /// whose first field is the thing being operated on, and whose remaining
    /// fields are each a self-describing clause rather than a positional
    /// argument whose meaning a reader has to know. That is what keeps it
    /// clear of `when(#1, #2, #3)`'s defect -- no field here means anything
    /// other than what it says, so having three of them rather than one costs
    /// a reader nothing. And the top-level comma is the same separator
    /// `RoundNode` chose over a bracketed prefix, for the same two reasons:
    /// nothing trails ambiguously into an operand that has no closing
    /// delimiter of its own (a `when()` branch), and a comma means nothing to
    /// any Markdown flavour, where `[`, `]` and `{`, `}` all do.
    ///
    /// An empty table says so. `BandTable<0>`, `KeyTable<Key, 0>` and
    /// `BreakpointTable<0>` are all valid and all always miss (`band.hpp`,
    /// `lookup.hpp`), and rendering one as `lookup(d)` would show a reader a
    /// complete-looking call with the entire table silently absent -- the same
    /// class of lie as the dropped operand this file's ruling exists to stop.
    template <Dialect D>
    [[nodiscard]] std::string lookup_call(std::string_view name, std::string const& subject, std::string const& rows)
    {
        std::string const body =
            rows.empty() ? lookup_separator<D>() + lookup_words_in_dialect<D>("no rows") : rows;
        if constexpr (D == Dialect::LaTeX)
            return "\\operatorname{" + std::string { name } + "}(" + subject + body + ")";
        else
            return std::string { name } + "(" + subject + body + ")";
    }
} // namespace detail

template <Dialect D, Node N>
[[nodiscard]] std::string render(N const& node);

/// Renders @p node in dialect @p D. A `PredicateNode` is not a `Node` -- see
/// `predicate.hpp` -- so it needs this second overload rather than the one
/// above; `WhenNode::render_node` calls this one to render its predicate.
template <Dialect D, Predicate P>
[[nodiscard]] std::string render(P const& node);

/// Renders @p node in dialect @p D. A `Constraint` is not a `Node` either,
/// nor is it itself a `Predicate` -- see `constraint.hpp` -- so it needs
/// this third overload, for the identical reason the second one above does.
template <Dialect D, Predicate P>
[[nodiscard]] std::string render(Constraint<P> const& node);

namespace detail
{
    template <Dialect D, Node Child>
    [[nodiscard]] std::string render_operand(Child const& child, Precedence context)
    {
        std::string text = render<D>(child);
        if (static_cast<int>(precedence_of(child)) < static_cast<int>(context))
            return "(" + text + ")";
        return text;
    }
} // namespace detail

// One overload per node kind. Each is found by argument-dependent lookup from
// `render` below, exactly as the evaluator's overloads are.

/// A variable renders as its quantity's symbol -- backtick-quoted in Markdown.
template <Dialect D, Described Q>
[[nodiscard]] std::string render_node(VarNode<Q> const&)
{
    std::string symbol { Describe<Q>::symbol };
    if constexpr (D == Dialect::Markdown)
        return "`" + symbol + "`";
    else
        return symbol;
}

/// A constant renders as its number, followed by its unit's symbol when it has one.
///
/// The number-and-unit spelling is `detail::number_with_unit`, shared with the
/// lookup tables below so that a table's row states a number exactly as a
/// constant holding the same number does -- see that helper.
template <Dialect D, Unit U>
[[nodiscard]] std::string render_node(ConstantNode<U> const& node)
{
    constexpr Unit unit = U;
    return detail::number_with_unit(detail::number_text(node.number), view(unit.symbolText));
}

/// A unary node renders as its operator followed by its (parenthesised if
/// necessary) operand.
template <Dialect D, UnaryOperator Op, Node Operand>
[[nodiscard]] std::string render_node(UnaryNode<Op, Operand> const& node)
{
    static_assert(Op == UnaryOperator::Negate, "formula: unknown unary operator");
    return "-" + detail::render_operand<D>(node.operand, detail::Precedence::Unary);
}

/// A binary node renders infix, bracketing each side only where its precedence
/// against the parent operator requires it -- see the file comment. Division
/// in `Dialect::LaTeX` renders as `\frac{}{}` instead, which needs no brackets
/// at all because the fraction bar already groups both sides.
template <Dialect D, BinaryOperator Op, Node Left, Node Right>
[[nodiscard]] std::string render_node(BinaryNode<Op, Left, Right> const& node)
{
    constexpr detail::Precedence here = detail::PrecedenceOf<BinaryNode<Op, Left, Right>>::value;
    constexpr detail::Precedence rightContext = (Op == BinaryOperator::Subtract || Op == BinaryOperator::Divide)
                                                    ? static_cast<detail::Precedence>(static_cast<int>(here) + 1)
                                                    : here;

    if constexpr (D == Dialect::LaTeX && Op == BinaryOperator::Divide)
        // \frac groups both sides itself, so neither operand needs a bracket.
        return "\\frac{" + render<D>(node.lhs) + "}{" + render<D>(node.rhs) + "}";
    else
    {
        std::string const lhs = detail::render_operand<D>(node.lhs, here);
        std::string const rhs = detail::render_operand<D>(node.rhs, rightContext);

        if constexpr (Op == BinaryOperator::Add)
            return lhs + " + " + rhs;
        else if constexpr (Op == BinaryOperator::Subtract)
            return lhs + " - " + rhs;
        else if constexpr (Op == BinaryOperator::Multiply)
            return D == Dialect::LaTeX ? lhs + " \\cdot " + rhs : lhs + " * " + rhs;
        else
            return lhs + " / " + rhs;
    }
}

/// A power renders as its base with the exponent superscript -- braced in LaTeX.
template <Dialect D, int Exponent, Node Operand>
[[nodiscard]] std::string render_node(PowerNode<Exponent, Operand> const& node)
{
    std::string const base = detail::render_operand<D>(node.operand, detail::Precedence::Atom);
    if constexpr (D == Dialect::LaTeX)
        return base + "^{" + std::to_string(Exponent) + "}";
    else
        return base + "^" + std::to_string(Exponent);
}

/// A root renders as `\sqrt{}` (or `\sqrt[n]{}` for a degree other than 2) in
/// LaTeX, and as `sqrt(...)` / `rootN(...)` in every other dialect.
template <Dialect D, int Degree, Node Operand>
[[nodiscard]] std::string render_node(RootNode<Degree, Operand> const& node)
{
    std::string const inner = render<D>(node.operand);
    if constexpr (D == Dialect::LaTeX)
    {
        if constexpr (Degree == 2)
            return "\\sqrt{" + inner + "}";
        else
            return "\\sqrt[" + std::to_string(Degree) + "]{" + inner + "}";
    }
    else
    {
        if constexpr (Degree == 2)
            return "sqrt(" + inner + ")";
        else
            return "root" + std::to_string(Degree) + "(" + inner + ")";
    }
}

/// A rounding node renders as a function call, `round(<operand>, to <places>
/// dp of <unit>)` -- braced onto a subscript in LaTeX, the same way a root's
/// degree is. Like `sqrt` and `root` above, the parentheses it always
/// produces already group its own operand, so it needs no `PrecedenceOf`
/// override: the primary template's `Atom` fallback is already the right
/// answer, and unlike `ConstantNode` nothing about that answer depends on the
/// data the node carries, so no runtime `precedence_of` overload either.
///
/// The granularity is a second, comma-separated argument -- `round(d, to 1 dp
/// of mm)`, operand first, in the "value, then how" order `ROUND(value,
/// digits)` already reads to an engineer -- rather than trailing after the
/// operand with nothing between them (`round(<operand> to <places> dp of
/// <unit>)`, the shape this rendered before a review caught it). That
/// trailing shape reads fine for an operand that is a single token, but a
/// `WhenNode` operand has no closing delimiter of its own in Plain or
/// Markdown -- `round(if p then a else b to 1 dp of mm)` reads as if only `b`
/// were "to 1 dp of mm", rounding the else branch alone, when the tree rounds
/// whichever branch predicate `p` selects. A first fix moved the suffix into
/// its own `[...]` in front of the operand's parentheses instead
/// (`round[to 1 dp of mm](d)`), which fixed that misattachment but created a
/// worse one: `[...](...)` right next to each other, with nothing between,
/// is CommonMark link syntax -- a Markdown renderer displays only the link
/// label (`to 1 dp of mm`), and the operand disappears from the visible text
/// entirely rather than merely misattaching. The comma form is safe against
/// both hazards at once: it is a single top-level separator no operand's own
/// rendered text can ever produce (nothing trails into it from a branch with
/// no closing delimiter of its own, the same property the `[...]`-prefix
/// form had), and it contains no character that means anything to any
/// Markdown flavour, unlike `[`, `]`, or `{`/`}` (the latter pair is inert in
/// CommonMark but is consumed by the `attr_list` extension some downstream
/// MkDocs Material setups enable).
///
/// `RoundingMode` deliberately does not appear in this text, in any dialect,
/// and therefore does not appear in `document()` either -- a `Documentation`
/// states its formula through this very function. A formula's rendered text
/// is what a reader checks against a standard, and a standard states a
/// rounding *granularity* -- "to one decimal place" -- without naming a
/// tie-breaking rule.
///
/// Where the mode does appear is the **trace**: `render_trace`
/// (`trace_render.hpp`) writes it as a bracketed clause on the step itself,
/// `round(#1, to 0 dp of mm) = 13 mm [nearest, ties away from zero]`. That is
/// a different document with a different job -- a trace exists to explain why
/// *this* number came out as it did, and the tie rule can be the entire
/// reason a value is 13 rather than 12. Leaving the mode out of the formula
/// text is a decision; leaving it out of the trace would be a defect.
template <Dialect D, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
[[nodiscard]] std::string render_node(RoundNode<U, Places, Mode, Operand> const& node)
{
    std::string const inner = render<D>(node.operand);
    constexpr Unit unit = U;
    std::string const unitSymbol { view(unit.symbolText) };
    std::string const placesText = std::to_string(Places.value);

    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{round}_{" + placesText + "\\,\\mathrm{" + unitSymbol + "}}(" + inner + ")";
    else
        return "round(" + inner + ", to " + placesText + " dp of " + unitSymbol + ")";
}

/// A significant-digits rounding node, spelled the same way as `RoundNode`
/// above but with "sf" (significant figures) in place of "dp" -- see that
/// overload for why no precedence override is needed, why `RoundingMode` is
/// left out, and why the granularity is a comma-separated second argument
/// rather than a trailing suffix or a `[...]` prefix.
template <Dialect D, Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
[[nodiscard]] std::string render_node(RoundSignificantNode<U, Digits, Mode, Operand> const& node)
{
    std::string const inner = render<D>(node.operand);
    constexpr Unit unit = U;
    std::string const unitSymbol { view(unit.symbolText) };
    std::string const digitsText = std::to_string(Digits.value);

    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{round}_{" + digitsText + "\\mathrm{sf},\\,\\mathrm{" + unitSymbol + "}}(" + inner + ")";
    else
        return "round(" + inner + ", to " + digitsText + " sf of " + unitSymbol + ")";
}

/// The numeric-value escape hatch renders as `numeric(<operand>, in <unit>)`,
/// or as a braced quotient in LaTeX. A function call like `RoundNode` above,
/// so the same reasoning applies: no `PrecedenceOf` override needed, none of
/// its data changes that answer, and the unit is a comma-separated second
/// argument for the identical reason `RoundNode` does -- see that overload's
/// comment for both hazards this form avoids (a trailing suffix
/// misattaching to a `WhenNode` operand's else branch, and a `[...]` prefix
/// reading as a Markdown link).
///
/// The justification does not appear here either. It is the compile-time
/// record of *why* a rule needed a bare number instead of a quantity -- an
/// audit trail for the **trace**, not part of the arithmetic this text
/// states. `render_trace` (`trace_render.hpp`) writes it as a bracketed
/// clause on the step itself; `Documentation` has no field for it and
/// `collect()` records none, so `document()` does not carry it either.
template <Dialect D, Unit U, detail::FixedString Justification, Node Operand>
[[nodiscard]] std::string render_node(NumericValueNode<U, Justification, Operand> const& node)
{
    std::string const inner = render<D>(node.operand);
    constexpr Unit unit = U;
    std::string const unitSymbol { view(unit.symbolText) };

    if constexpr (D == Dialect::LaTeX)
        return "\\{" + inner + "/\\mathrm{" + unitSymbol + "}\\}";
    else
        return "numeric(" + inner + ", in " + unitSymbol + ")";
}

/// Pi renders as `\pi` in LaTeX, and as `pi` in every other dialect.
template <Dialect D>
[[nodiscard]] inline std::string render_node(PiNode const&)
{
    if constexpr (D == Dialect::LaTeX)
        return "\\pi";
    else
        return "pi";
}

/// A citation is documentation, not arithmetic: it does not appear in the
/// rendered formula. `document()` is what surfaces it.
template <Dialect D, Node Inner>
[[nodiscard]] std::string render_node(DocumentedNode<Inner> const& node)
{
    return render<D>(node.inner);
}

// ------------------------------------------------------- phase 10: lookups
//
// Three node kinds, one shape: `<name>(<what is looked up>, <row>, <row>,
// ...)`. See `detail::lookup_call` for why that is the existing call shape
// rather than a fourth punctuation style, and this file's comment for the
// ruling on how a half-open band is spelled.
//
// **The two selecting kinds share the head name `lookup`; the computing kind
// is `interpolate`.** The distinction a reader must not miss is the one
// `lookup.hpp` calls out as deliberate: a banded and an exact table *select* a
// number their author wrote down, while an interpolating table *computes* one
// that appears in no row of it. Which of band or key did the selecting is
// visible in every row already (`10 to under 20 mm` against `key 7`), so
// spending the head name on that instead would name the difference a reader
// can see and leave the one they cannot.
//
// **The banded and the interpolating domain render differently because they
// are different**, and this is the other thing a reader must not miss. A band
// is an interval whose top is excluded and says so -- `20 to under 30 mm`.  A
// breakpoint is a row, not a boundary: the table states a value *at* it, the
// last one included, so it renders as the point it is -- `at 30 mm`. Nothing
// in an interpolating rendering excludes anything, because nothing in an
// interpolating table does. `lookup.hpp` pins the two behaviours against each
// other at 30 mm; `render_tests.cpp` pins the two spellings against each other
// on the same number, so that "harmonising" them in either direction fails.
//
// None of the three needs a `PrecedenceOf` override, for `RoundNode`'s reason:
// each always emits its own `(` ... `)`, which groups whatever it holds, so
// the primary template's `Atom` is already right -- and, unlike `ConstantNode`,
// no data any of them carries can change that, so no runtime `precedence_of`
// overload either. An empty table still emits the parentheses (`lookup(d, no
// rows)`), so that stays true for the degenerate table too.

/// A banded lookup renders as `lookup(<operand>, <band> gives <correction>,
/// ...)` -- one field per band, in the table's own declared order.
///
/// The bands come from the type and the corrections from the node's runtime
/// state, and they are rendered interleaved rather than as two lists, because
/// a reader checking a published table reads it row by row: which interval,
/// then what that interval gives. That the structure is compile-time and the
/// contents are not is a fact about how a formula is *built* (`lookup.hpp`),
/// and a reader of the formula's text has no use for it.
template <Dialect D, Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
[[nodiscard]] std::string render_node(BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand> const& node)
{
    constexpr Unit keyUnit = KeyUnit;
    constexpr Unit resultUnit = ResultUnit;

    std::string rows;
    for (std::size_t index = 0; index < Bands.size(); ++index)
        rows += detail::lookup_separator<D>()
                + detail::lookup_words_in_dialect<D>(detail::lookup_row_text(
                    detail::band_text(Bands[index], view(keyUnit.symbolText)),
                    detail::number_with_unit(detail::number_text(node.corrections[index]),
                                             view(resultUnit.symbolText))));

    return detail::lookup_call<D>("lookup", render<D>(node.operand), rows);
}

/// An exact lookup renders as `lookup(<selected key>, <key> gives
/// <correction>, ...)` -- the key this node selects with first, then one field
/// per row of the table, in its own declared order.
///
/// The selected key sits where a banded lookup's operand sits because it plays
/// that part: it is what is being looked up. It is data rather than a
/// sub-expression -- an exact lookup has no operand at all (`lookup.hpp`) --
/// but that is a fact about where the value comes from, not about what the
/// text says, and the text says the same thing either way. It does change one
/// thing, and only in LaTeX: `key 7` is words, not mathematics, so it is set
/// as text like the rows are, where the other two kinds' operands stay in math
/// mode because they really are expressions.
///
/// **A key renders as its underlying value, not its name.** See
/// `detail::key_text` for why no library can do better without help from the
/// author's own enumeration, and for what it costs a reader.
template <Dialect D, KeyTable Keys, Unit ResultUnit>
[[nodiscard]] std::string render_node(ExactLookupNode<Keys, ResultUnit> const& node)
{
    constexpr Unit resultUnit = ResultUnit;

    std::string rows;
    for (std::size_t index = 0; index < Keys.size(); ++index)
        rows += detail::lookup_separator<D>()
                + detail::lookup_words_in_dialect<D>(detail::lookup_row_text(
                    detail::key_text(Keys[index]),
                    detail::number_with_unit(detail::number_text(node.corrections[index]),
                                             view(resultUnit.symbolText))));

    return detail::lookup_call<D>("lookup",
                                 detail::lookup_words_in_dialect<D>(detail::key_text(node.key)),
                                 rows);
}

/// An interpolating lookup renders as `interpolate(<operand>, at <key> gives
/// <value>, ...)` -- one field per breakpoint, in the table's own ascending
/// order.
///
/// `at`, because a breakpoint is a point and not an interval: the table states
/// this value *at* this key, and the values between two rows are what the
/// formula computes rather than anything the table holds. A rendering that
/// spelled these rows as intervals would be claiming the table said something
/// it does not -- and, at the last row, would exclude the one key the table
/// states most directly (`lookup.hpp`).
template <Dialect D, Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
[[nodiscard]] std::string render_node(InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand> const& node)
{
    constexpr Unit keyUnit = KeyUnit;
    constexpr Unit resultUnit = ResultUnit;

    std::string rows;
    for (std::size_t index = 0; index < Points.size(); ++index)
        rows += detail::lookup_separator<D>()
                + detail::lookup_words_in_dialect<D>(detail::lookup_row_text(
                    "at "
                        + detail::number_with_unit(
                            detail::declared_number_text(Points[index].numerator, Points[index].denominator),
                            view(keyUnit.symbolText)),
                    detail::number_with_unit(detail::number_text(node.corrections[index]),
                                             view(resultUnit.symbolText))));

    return detail::lookup_call<D>("interpolate", render<D>(node.operand), rows);
}

/// A predicate renders as `<lhs> <comparison> <rhs>`. Not a `Node`, so it
/// cannot go through `render_operand` -- its own operand context is computed
/// directly from `PrecedenceOf<PredicateNode<...>>` instead, one rung above
/// `Conditional`, exactly the way `BinaryNode`'s right operand above computes
/// its context one rung above its own -- so that a `when()` nested on either
/// side still brackets: `when(...) > threshold` must not read as if the
/// comparison applied only to the else branch.
///
/// `<=`, `>=` and `!=` get the mathematical spelling in LaTeX (`\leq`,
/// `\geq`, `\neq`) rather than the code-like tokens the other dialects use,
/// the same way `BinaryNode`'s `*` becomes `\cdot` there.
template <Dialect D, Comparison Op, Node Left, Node Right>
[[nodiscard]] std::string render_node(PredicateNode<Op, Left, Right> const& node)
{
    constexpr detail::Precedence operandContext =
        static_cast<detail::Precedence>(static_cast<int>(detail::PrecedenceOf<PredicateNode<Op, Left, Right>>::value) + 1);
    std::string const lhs = detail::render_operand<D>(node.lhs, operandContext);
    std::string const rhs = detail::render_operand<D>(node.rhs, operandContext);

    char const* const symbol = [] {
        if constexpr (Op == Comparison::Less)
            return "<";
        else if constexpr (Op == Comparison::LessOrEqual)
            return D == Dialect::LaTeX ? "\\leq" : "<=";
        else if constexpr (Op == Comparison::Greater)
            return ">";
        else if constexpr (Op == Comparison::GreaterOrEqual)
            return D == Dialect::LaTeX ? "\\geq" : ">=";
        else if constexpr (Op == Comparison::Equal)
            return D == Dialect::LaTeX ? "=" : "==";
        else
            return D == Dialect::LaTeX ? "\\neq" : "!=";
    }();

    return lhs + " " + symbol + " " + rhs;
}

/// A constraint renders as its rule alone -- `require <lhs> <comparison>
/// <rhs>` -- never its `Verdict`.
///
/// `require` is not a name borrowed from the public API: `constraint(predicate,
/// verdict, citation)` is a three-argument call, so nothing here reads as a
/// mis-spelled invocation of it. It is the same keyword `constraint_expression`
/// (`trace_render.hpp`) already spells a `Constraint` trace step with, reused
/// here on purpose -- a reader who has seen a constraint in one surface
/// recognises it in the other, and a rendered constraint that agreed with the
/// standard but disagreed with its own trace would be its own kind of
/// misleading text.
///
/// **The verdict stays out**, for the same reason `RoundingMode` stays out of
/// `RoundNode`'s text above, and closer still to why `NumericValueNode`'s
/// justification stays out of both its text and `document()`: this function
/// states the condition a standard asks a reader to check, and a verdict is
/// not part of that condition, it is what a checker does once the condition
/// has already been decided. Unlike a rounding tie rule -- which can be the
/// entire reason a computed number came out as it did -- a verdict never
/// changes whether the predicate holds; `check()` (`constraint.hpp`) reaches
/// `Satisfied` or `Violated` from the predicate alone, and only *attaches*
/// the verdict after that is already settled. That makes it a label on an
/// outcome, not an ingredient of one -- the exact role `NumericValueNode`'s
/// justification plays, and that one is excluded even from `document()`; see
/// its own comment above.
///
/// This project's own two illustrative constraints (`constraint.hpp`'s file
/// comment) do not settle it by prose alone, and are not cited here as if
/// they did: "the two replicates shall agree within 5%" names no consequence
/// at all, while "reject the specimen below 30 MPa" folds one in. A standard
/// is read either way in practice, which is exactly why the deciding fact
/// has to be what `Verdict` structurally *is* on this type, not which of two
/// quoted sentences sounds more natural.
///
/// Where the verdict does appear is the trace: `Constraint::check`
/// (`constraint.hpp`) records a `ConstraintOutcome` carrying it, and
/// `constraint_outcome_suffix` (`trace_render.hpp`) writes it as a bracketed
/// clause on the step, `require #1 >= #2 [reject the specimen]` -- the one
/// place a verdict is a fact about what actually happened, not a rule stated
/// in the abstract. Leaving it out of this text is a decision; leaving it
/// out of the trace would be a defect.
template <Dialect D, Predicate P>
[[nodiscard]] std::string render_node(Constraint<P> const& node)
{
    std::string const predicateText = render<D>(node.predicate);
    if constexpr (D == Dialect::LaTeX)
        return "\\text{require } " + predicateText;
    else
        return "require " + predicateText;
}

/// A conditional renders as `if <predicate> then <then> else <else>` in every
/// dialect but LaTeX, which spells it as a `\begin{cases}` block -- the usual
/// way a case-defined quantity is typeset. Both branches are unambiguous
/// without a bracket at any nesting depth, in every dialect: every `when()`
/// carries a mandatory `else`, so nested if-then-else has none of the
/// dangling-else trouble an *optional* else would cause -- nearest-else-
/// binds-nearest-if always recovers the tree correctly. That is a fact about
/// what a parser can do, though, and this library's rendered text ends up in
/// generated documentation a person checks against a standard; unambiguous
/// to a parser is not the same bar as readable to a person.
///
/// So the **then** branch gets a bracket in Plain and Markdown when it is
/// itself a `WhenNode` -- `if p then (if q then a else b) else c` -- because
/// otherwise a reader has to count `else`s against `then`s to find where the
/// inner conditional stops before "else c" is reached. The **else** branch
/// does not: `if p then a else if q then b else c` is the ordinary else-if
/// chain, already reads fine, and bracketing it would only add noise for
/// nothing. This asymmetry is deliberate -- a future reader who "fixes" it
/// to look symmetric would be undoing the actual readability improvement.
/// LaTeX needs no bracket in either position: its `\begin{cases}` block is a
/// visibly distinct construct nested inside a cell, not text a reader could
/// mistake for a continuation of the outer one.
template <Dialect D, Predicate P, Node Then, Node Else>
[[nodiscard]] std::string render_node(WhenNode<P, Then, Else> const& node)
{
    std::string const predicateText = render<D>(node.predicate);
    std::string const thenText = D == Dialect::LaTeX
                                     ? render<D>(node.thenBranch)
                                     : detail::render_operand<D>(node.thenBranch, detail::Precedence::Additive);
    std::string const elseText = render<D>(node.elseBranch);

    if constexpr (D == Dialect::LaTeX)
        return "\\begin{cases} " + thenText + " & \\text{if } " + predicateText + " \\\\ " + elseText
               + " & \\text{otherwise} \\end{cases}";
    else
        return "if " + predicateText + " then " + thenText + " else " + elseText;
}

/// Renders @p node in dialect @p D.
template <Dialect D, Node N>
[[nodiscard]] std::string render(N const& node)
{
    return render_node<D>(node);
}

/// Renders @p node as plain text.
template <Node N>
[[nodiscard]] std::string render(N const& node)
{
    return render<Dialect::Plain>(node);
}

/// Renders @p node in dialect @p D. See the forward declaration above for why
/// this overload -- for `Predicate`, not `Node` -- exists separately.
template <Dialect D, Predicate P>
[[nodiscard]] std::string render(P const& node)
{
    return render_node<D>(node);
}

/// Renders @p node as plain text.
template <Predicate P>
[[nodiscard]] std::string render(P const& node)
{
    return render<Dialect::Plain>(node);
}

/// Renders @p node in dialect @p D. See the forward declaration above for why
/// this overload -- for `Constraint`, not `Node` or `Predicate` -- exists
/// separately.
template <Dialect D, Predicate P>
[[nodiscard]] std::string render(Constraint<P> const& node)
{
    return render_node<D>(node);
}

/// Renders @p node as plain text.
template <Predicate P>
[[nodiscard]] std::string render(Constraint<P> const& node)
{
    return render<Dialect::Plain>(node);
}

} // namespace formula
