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
/// high)`.** A band really is `[103, 197)` (`band.hpp`), and that is exactly
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
#include <formula-cpp/binning.hpp>
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/citation.hpp>
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/conformity.hpp>
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/critical_value.hpp>
#include <formula-cpp/curve.hpp>
#include <formula-cpp/detail/latex_math.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/number_text.hpp>
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/record.hpp>
#include <formula-cpp/rejection.hpp>
#include <formula-cpp/retry.hpp>
#include <formula-cpp/rounded_root.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/snap.hpp>
#include <formula-cpp/statistics.hpp>
#include <formula-cpp/unit.hpp>
#include <formula-cpp/vocabulary.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

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

    /// How tightly a binary operator binds -- one answer for the scalar
    /// `BinaryNode` and the elementwise `ElementwiseBinaryNode`, which are
    /// spelled alike.
    template <BinaryOperator Op>
    inline constexpr Precedence binary_precedence =
        (Op == BinaryOperator::Add || Op == BinaryOperator::Subtract) ? Precedence::Additive : Precedence::Multiplicative;

    template <BinaryOperator Op, Node Left, Node Right>
    struct PrecedenceOf<BinaryNode<Op, Left, Right>>
    {
        static constexpr Precedence value = binary_precedence<Op>;
    };

    template <BinaryOperator Op, typename Left, typename Right>
    struct PrecedenceOf<ElementwiseBinaryNode<Op, Left, Right>>
    {
        static constexpr Precedence value = binary_precedence<Op>;
    };

    template <UnaryOperator Op, SeriesNode Operand>
    struct PrecedenceOf<ElementwiseUnaryNode<Op, Operand>>
    {
        static constexpr Precedence value = Precedence::Unary;
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

    /// A read from another record, the lowest rung, so that as the operand of
    /// anything it is bracketed: `f_c / (f_c of Reference)`. Without the
    /// bracket, `a / f_c of Reference` could be read as `(a / f_c) of
    /// Reference`, and the words would name the wrong computation.
    template <typename Role, typename Requirement, Node Operand>
    struct PrecedenceOf<RecordScopeNode<Role, Requirement, Operand>>
    {
        static constexpr Precedence value = Precedence::Conditional;
    };

    /// Whether @p N is a read from another record -- the one else branch a
    /// conditional brackets; see `render_node(WhenNode)`.
    template <typename N>
    inline constexpr bool isRecordScope = false;

    template <typename Role, typename Requirement, Node Operand>
    inline constexpr bool isRecordScope<RecordScopeNode<Role, Requirement, Operand>> = true;

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
        constexpr Unit declaredUnit = U;
        bool const hasUnitSymbol = !view(declaredUnit.symbolText).empty();
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

    /// A jurisdiction's replacement formula renders as the formula, so it
    /// brackets as the formula does -- type and runtime answer both, for the
    /// reason `DocumentedNode`'s two forward theirs.
    template <Node Expr>
    struct PrecedenceOf<ReplacedVariantNode<Expr>>
    {
        static constexpr Precedence value = PrecedenceOf<Expr>::value;
    };

    template <Node Expr>
    [[nodiscard]] constexpr Precedence precedence_of(ReplacedVariantNode<Expr> const& node) noexcept
    {
        return precedence_of(node.replacement());
    }

    /// A series node brackets by its type alone: nothing a series holds
    /// changes its rendered shape at run time the way a constant's sign does.
    template <SeriesNode S>
    [[nodiscard]] constexpr Precedence precedence_of(S const&) noexcept
    {
        return PrecedenceOf<S>::value;
    }

    /// How tightly @p node binds as written in dialect @p D: `precedence_of`'s
    /// answer, for every node kind spelled alike in every dialect.
    template <Dialect D, typename N>
    [[nodiscard]] constexpr Precedence precedence_in(N const& node) noexcept
    {
        return precedence_of(node);
    }

    /// A sum is a call, `sum(...)`, that groups itself in plain text and
    /// Markdown, but a large operator in LaTeX, whose reach a reader takes to
    /// run on over anything multiplied after it: `\sum x_i \cdot m_t` reads
    /// as the sum of the products. There it brackets as an additive
    /// expression would.
    template <Dialect D, SeriesNode S>
    [[nodiscard]] constexpr Precedence precedence_in(SumNode<S> const&) noexcept
    {
        return D == Dialect::LaTeX ? Precedence::Additive : Precedence::Atom;
    }

    /// A citation's wrapper renders as what it wraps, so it brackets as that
    /// does, in every dialect -- a documented sum in LaTeX included.
    template <Dialect D, Node Inner>
    [[nodiscard]] constexpr Precedence precedence_in(DocumentedNode<Inner> const& node) noexcept
    {
        return precedence_in<D>(node.inner);
    }

    /// A jurisdiction's replacement renders as the replacement formula, so it
    /// brackets as that does, for `DocumentedNode`'s reason.
    template <Dialect D, Node Expr>
    [[nodiscard]] constexpr Precedence precedence_in(ReplacedVariantNode<Expr> const& node) noexcept
    {
        return precedence_in<D>(node.replacement());
    }

    /// @p quantitySymbol -- already the jurisdiction's, through `symbol_of` -- marked
    /// as a series in dialect @p D: `x_m(i)` in plain text, `` `x_m(i)` `` in
    /// Markdown (the marker inside the backticks, so the code span keeps it
    /// literal), and `{x_m}_{i}` in LaTeX (the whole symbol braced, then
    /// subscripted, so the index attaches to the symbol even when it already
    /// carries a subscript of its own).
    ///
    /// **The one place the marker is spelled.** Every series node that names
    /// a quantity calls this, so the marker cannot drift between node kinds.
    /// Chosen by the phase 12 spike: under MathJax 3.2.2 with the site's
    /// configuration and under tectonic 0.17.0 with `[OT1]{fontenc}`,
    /// `{x_m}_{i}`, `{R}_{i}` and `{f_{c}}_{i}` typeset, while `x_m_i` is a
    /// "Double subscript" error in both; python-markdown 3.10.3 keeps
    /// `` `x_m(i)` `` literal. A symbol that already ends in `)` reads
    /// `w(t)(i)`, and a LaTeX symbol that is not a balanced TeX group breaks
    /// the braces; neither is checked here.
    template <Dialect D>
    [[nodiscard]] std::string series_marker(std::string quantitySymbol)
    {
        if constexpr (D == Dialect::LaTeX)
            return "{" + quantitySymbol + "}_{i}";
        else if constexpr (D == Dialect::Markdown)
            return "`" + quantitySymbol + "(i)`";
        else
            return quantitySymbol + "(i)";
    }

    /// @p quantitySymbol -- already the jurisdiction's, through `symbol_of` --
    /// marked as a retry's value at attempt @p attemptIndex (`k`, `k-1` or
    /// `0`), in dialect @p D: `w(k-1)` in plain text, `` `w(k-1)` `` in
    /// Markdown and `{w}_{k-1}` in LaTeX -- `series_marker`'s family, chosen
    /// by phase 15's spike (step 9) for the same engines.
    ///
    /// **The one place the marker is spelled**, for the render, the document
    /// and the trace (`trace_render.hpp`) alike.
    template <Dialect D>
    [[nodiscard]] std::string attempt_marker(std::string quantitySymbol, std::string_view attemptIndex)
    {
        if constexpr (D == Dialect::LaTeX)
            return "{" + quantitySymbol + "}_{" + std::string { attemptIndex } + "}";
        else if constexpr (D == Dialect::Markdown)
            return "`" + quantitySymbol + "(" + std::string { attemptIndex } + ")`";
        else
            return quantitySymbol + "(" + std::string { attemptIndex } + ")";
    }

    /// Whether @p shownIn is a unit nobody declared: exactly the coherent unit
    /// `coherent()` builds for its dimension -- no symbol, no scale, and
    /// `Unit`'s default of 3 decimals, which nobody chose. A trace shows a
    /// computed value in one, a product in joules or a ratio. A quantity
    /// declared in `unit::One` is the same `Unit` value, so it counts as
    /// unlabelled too.
    [[nodiscard]] constexpr bool is_unlabelled(Unit const& shownIn) noexcept
    {
        return shownIn == coherent(shownIn.dimension);
    }

    /// @p numberStyle with `DecimalPadding::Trimmed`: the same notation and
    /// the same rounding mode, never padded.
    [[nodiscard]] constexpr NumberStyle trimmed(NumberStyle numberStyle) noexcept
    {
        switch (numberStyle.notation())
        {
            case NumberNotation::ExactDecimal:
                return NumberStyle::exact_decimal(DecimalPadding::Trimmed);
            case NumberNotation::ApproximateDecimal:
                return NumberStyle::approximate_decimal(numberStyle.approximation(), DecimalPadding::Trimmed);
            case NumberNotation::Fraction:
                break;
        }
        return numberStyle;
    }

    /// Whether @p spelled is a rounding that came out as zero: `≈0`.
    [[nodiscard]] constexpr bool rounded_to_zero(NumberText const& spelled) noexcept
    {
        std::string_view const spelledText = spelled.view();
        return spelledText.size() == ApproximationMarker.size() + 1 && spelledText.starts_with(ApproximationMarker)
               && spelledText.back() == '0';
    }

    /// `checked_number_text` for a number shown in @p shownIn, except that a
    /// number in a unit nobody declared (`is_unlabelled`) is never padded:
    /// the 3 decimals it would be padded to are a default, not anyone's
    /// statement of precision. An approximating style still rounds it at
    /// those 3 places -- unless they round a value other than zero to `≈0`,
    /// which says nothing of it. The places are then extended to its first
    /// significant digit, up to 18, and the value, rounded there in the
    /// style's mode, stays marked: a tariff in euros per joule,
    /// 3401/33480000000, reads `≈0.0000001`, and 1/11250000 `≈0.00000009`. A
    /// value with no digit within 18 places reads `≈0`. A unit someone
    /// declared keeps its declared places, whatever they round to.
    [[nodiscard]] constexpr std::expected<NumberText, ArithmeticError> checked_shown_text(Rational shownNumber,
                                                                                         NumberStyle numberStyle,
                                                                                         Unit const& shownIn) noexcept
    {
        if (!is_unlabelled(shownIn))
            return checked_number_text(shownNumber, numberStyle, shownIn);
        NumberStyle const unpadded = trimmed(numberStyle);
        std::expected<NumberText, ArithmeticError> const spelled = checked_number_text(shownNumber, unpadded, shownIn);
        if (!spelled.has_value() || shownNumber == Rational { 0 } || !rounded_to_zero(*spelled))
            return spelled;
        // The first significant digit is at the fewest places a truncation
        // leaves something at; rounded there in the style's own mode, the
        // value cannot come out as zero.
        NumberStyle const truncating = NumberStyle::approximate_decimal(RoundingMode::TowardZero);
        for (std::int32_t places = declared_decimals(shownIn).value + 1; places <= ExactDecimalPlaces; ++places)
        {
            Unit finer = shownIn;
            finer.decimals = places;
            std::expected<NumberText, ArithmeticError> const truncated = checked_number_text(shownNumber, truncating, finer);
            if (!truncated.has_value())
                return spelled;
            if (!rounded_to_zero(*truncated))
                return checked_number_text(shownNumber, unpadded, finer);
        }
        return spelled;
    }

    /// @p shownNumber, a number stated in @p shownIn, as @p numberStyle writes
    /// it (`checked_shown_text`), or its exact fraction where that style
    /// cannot write it in that unit.
    ///
    /// For a helper with no clause to say a number is not shown: a table's
    /// bound, an envelope's limit. The fraction style never fails. Any other
    /// can -- a padded or approximated number in a unit whose declared
    /// decimals lie outside the -18 to 18 `DecimalPlaces` spans, for one --
    /// and the fraction is then the one text still exact.
    [[nodiscard]] inline std::string styled_number_text(Rational shownNumber, NumberStyle numberStyle, Unit const& shownIn)
    {
        std::expected<NumberText, ArithmeticError> const spelled = checked_shown_text(shownNumber, numberStyle, shownIn);
        if (spelled.has_value())
            return std::string { spelled->view() };
        NumberText const exactFraction = fraction_text(shownNumber);
        return std::string { exactFraction.view() };
    }

    /// @p typedNumber, a number its author typed in @p typedIn, as a formula's
    /// text under @p vocabulary writes it (`typed_number_style`): `0.863`
    /// under an exact-decimal style, `863/1000` under the default fraction.
    template <Vocabulary V>
    [[nodiscard]] std::string typed_number_text(Rational typedNumber, Unit const& typedIn, V const& vocabulary)
    {
        return styled_number_text(typedNumber, typed_number_style(vocabulary), typedIn);
    }

    /// A number followed by its unit's symbol, or the number alone when the
    /// unit has none (`unit::One`) -- `139 mm`, `863/1000`.
    ///
    /// Factored out of `render_node(ConstantNode)`, which is the spelling this
    /// library already had, rather than invented for the lookup tables below:
    /// a table states a number in a unit on every one of its rows, and a row
    /// that spelled a number differently from a constant holding that same
    /// number would be two surfaces disagreeing inside one rendered formula.
    [[nodiscard]] inline std::string number_with_unit(std::string const& numberText, std::string_view unitSymbol)
    {
        return unitSymbol.empty() ? numberText : numberText + " " + std::string { unitSymbol };
    }

    /// @p lead followed by a unit's symbol -- `, in MPa`, ` dp of mm` -- or
    /// nothing when the unit has none (`unit::One`), so that a rounding or a
    /// numeric value in a dimensionless unit reads `round(x, to 2 dp)` and
    /// `numeric(x)` rather than `round(x, to 2 dp of )` and `numeric(x, in )`.
    /// The clause is dropped rather than a name written in its place, as
    /// `number_with_unit` drops the symbol after a dimensionless number: a
    /// value with no unit is shown with none, everywhere.
    [[nodiscard]] inline std::string unit_clause(std::string_view lead, std::string_view unitSymbol)
    {
        return unitSymbol.empty() ? std::string {} : std::string { lead } + std::string { unitSymbol };
    }

    /// A unit's symbol set upright in LaTeX, `\mathrm{mm}` or `\mathrm{\%}`,
    /// or nothing for a unit with none, for `unit_clause` to drop. Escaped by
    /// `latex_math_words` (`detail/latex_math.hpp`): `%` is TeX's comment
    /// character, and an author's unit may hold any of its specials.
    [[nodiscard]] inline std::string latex_unit(std::string_view unitSymbol)
    {
        return unitSymbol.empty() ? std::string {} : "\\mathrm{" + latex_math_words(unitSymbol) + "}";
    }

    /// A bound a table declared as a numerator/denominator pair -- a band's
    /// low or high bound, or a breakpoint's key -- as text, a number in
    /// @p declaredIn.
    ///
    /// Reduced through `Rational::make` first, so a bound typed `10/2` reads
    /// as `5` and `0/1` reads as `0`, exactly as a `ConstantNode` holding the
    /// same number does. The fallback is unreachable for any table that got as
    /// far as a renderer -- every lookup node instantiates its own table's
    /// `Require...` guard, which refuses a bound that names no rational number
    /// at compile time -- and is guarded anyway for the reason
    /// `detail::find_band` (`lookup.hpp`) guards the identical call: printing
    /// back the pair the author typed is better than dereferencing an error.
    ///
    /// Spelled in `numberStyle.exact_only()`: a number the author typed is
    /// never shown rounded, whatever style the rest of the text is in.
    [[nodiscard]] inline std::string declared_number_text(std::int64_t declaredNumerator,
                                                          std::int64_t declaredDenominator,
                                                          Unit const& declaredIn,
                                                          NumberStyle numberStyle)
    {
        std::expected<Rational, ArithmeticError> const declared = Rational::make(declaredNumerator, declaredDenominator);
        if (declared.has_value())
            return styled_number_text(*declared, numberStyle.exact_only(), declaredIn);
        return std::to_string(declaredNumerator) + "/" + std::to_string(declaredDenominator);
    }

    /// A half-open band as text: `103 to under 197 mm`. **The one spelling of a
    /// half-open interval in this library** -- see this file's comment for the
    /// ruling and for the published defect that bought it.
    ///
    /// @p keySymbol is @p keyUnit's symbol as the caller writes it -- the
    /// trace escapes it, `render()` does not -- and @p keyUnit is the unit the
    /// bounds are numbers in (`declared_number_text`).
    [[nodiscard]] inline std::string band_text(Band const& shownBand,
                                               std::string_view keySymbol,
                                               Unit const& keyUnit,
                                               NumberStyle numberStyle)
    {
        return number_with_unit(
            declared_number_text(shownBand.lowNumerator, shownBand.lowDenominator, keyUnit, numberStyle) + " to under "
                + declared_number_text(shownBand.highNumerator, shownBand.highDenominator, keyUnit, numberStyle),
            keySymbol);
    }

    /// Author-supplied words -- a key's name -- made literal in Markdown, so
    /// that whatever characters they hold are shown rather than obeyed; Plain
    /// changes nothing.
    ///
    /// Needed because a key's name is the one piece of text in a rendering
    /// that this library did not write. A customized spelling
    /// (`EnumeratorName`, `enumerator.hpp`) may hold anything at all --
    /// `[150 mm]`, `*` -- which in Markdown is exactly the link and emphasis
    /// syntax this file's ruling and the guard test in `render_tests.cpp`
    /// exist to keep out.
    ///
    /// **Not LaTeX.** LaTeX sets a key's name inside the `\mathrm{...}` of the
    /// row it stands in, and `lookup_words_in_dialect` escapes the whole row
    /// with `latex_math_words` (`detail/latex_math.hpp`). This function once
    /// escaped for LaTeX text mode, inside `\text{...}` -- `\_`,
    /// `\textbackslash{}`, `{\ttfamily\char34}` -- which a TeX engine reads
    /// and the site's MathJax, without `textmacros`, shows backslash and all.
    ///
    /// **Markdown** backslash-escapes the six characters that open inline
    /// markup -- a backslash, a backtick, `*`, `_`, `[`, `]` -- and writes six
    /// more as entities: `<`, `>` and `&`, which are HTML's; `$`, which the
    /// site's `pymdownx.arithmatex` reads as a maths delimiter; `|`, which
    /// splits a GFM table cell; and `~`, whose pair is GFM strikethrough.
    /// Entities rather than backslashes for those six, because python-markdown
    /// with the site's extensions shows a backslash before `|` or `~`
    /// literally, where CommonMark would drop it.
    ///
    /// Measured, not reasoned: the Markdown set was rendered by
    /// python-markdown with exactly the extensions `mkdocs.yml` enables
    /// (admonition, pymdownx.highlight, pymdownx.superfences,
    /// pymdownx.arithmatex in generic mode), and by pandoc's CommonMark and
    /// GFM readers, in a paragraph and in a GFM table cell, and reads back as
    /// the original in every one. Text with none of these characters is the
    /// same in both dialects, which is what the cross-dialect test relies on.
    template <Dialect D>
        requires(D != Dialect::LaTeX)
    [[nodiscard]] std::string literal_words_in_dialect(std::string_view words)
    {
        if constexpr (D == Dialect::Plain)
            return std::string { words };
        else
        {
            std::string keyText;
            keyText.reserve(words.size());
            for (char const byte: words)
            {
                switch (byte)
                {
                    case '<':
                        keyText += "&lt;";
                        break;
                    case '>':
                        keyText += "&gt;";
                        break;
                    case '&':
                        keyText += "&amp;";
                        break;
                    case '$':
                        keyText += "&#36;";
                        break;
                    case '|':
                        keyText += "&#124;";
                        break;
                    case '~':
                        keyText += "&#126;";
                        break;
                    case '\\':
                    case '`':
                    case '*':
                    case '_':
                    case '[':
                    case ']':
                        keyText += '\\';
                        keyText += byte;
                        break;
                    default:
                        keyText += byte;
                        break;
                }
            }
            return keyText;
        }
    }

    /// A category key as text: `key Cylinder`, in dialect @p D.
    ///
    /// The key's name is `key_name` (`lookup.hpp`): the name of the row of
    /// @p Keys that @p key selects, which is the enumerator's own name as
    /// written in the author's source unless the author spelled it
    /// differently through `EnumeratorName` (`enumerator.hpp`). A reader
    /// reconciling this against a published table reads the author's word for
    /// the row, not a number they would have to look up in the author's code.
    /// The name is made literal in Markdown by `literal_words_in_dialect`,
    /// and in LaTeX by `lookup_words_in_dialect`, which escapes the whole row
    /// the name stands in.
    ///
    /// **The underlying value is the fallback, and only the fallback**: `key
    /// 9` for a key that names no row of the table, which is exactly the key
    /// a missing lookup holds. It may even be a real enumerator the table
    /// has no row for; `key_name` answers only among the table's own keys,
    /// so this does too. The one other case is a row the table declares under
    /// a value that names no enumerator (`static_cast<Shape>(9)` is a valid
    /// key): a hit on it has no name to show either, and shows its value. The
    /// spelling is the one this function used before it had names, so a miss
    /// reads the same as it always has. A reflected name
    /// is an identifier and cannot begin with a digit, so the two spellings
    /// cannot be confused unless an author customizes a name into a number.
    ///
    /// The underlying value, not the row's index: the two coincide only for an
    /// enumeration whose enumerators were left to default, and an author who
    /// numbered theirs `{ Cube = 3, Cylinder = 7 }` would otherwise be shown
    /// numbers that appear nowhere in their own code. The signed and unsigned
    /// casts are spelled separately because an underlying type may be
    /// `unsigned long long`, whose top half no signed type can hold.
    template <Dialect D, KeyTable Keys>
    [[nodiscard]] std::string key_text(KeyOf<Keys> key)
    {
        std::string_view const keyName = key_name<Keys>(key);
        if (!keyName.empty())
        {
            // LaTeX's escape is `lookup_words_in_dialect`'s, applied to the
            // whole row the name stands in.
            if constexpr (D == Dialect::LaTeX)
                return "key " + std::string { keyName };
            else
                return "key " + literal_words_in_dialect<D>(keyName);
        }

        using Underlying = std::underlying_type_t<KeyOf<Keys>>;
        if constexpr (std::is_signed_v<Underlying>)
            return "key " + std::to_string(static_cast<long long>(key));
        else
            return "key " + std::to_string(static_cast<unsigned long long>(key));
    }

    /// One row of a rendered lookup table: what selects the row, then what the
    /// row gives. `103 to under 197 mm gives 863/1000`, `key Cylinder gives
    /// 1127/1000`, `at 241 mm gives 1043/1000`.
    [[nodiscard]] inline std::string lookup_row_text(std::string const& selector, std::string const& correction)
    {
        return selector + " gives " + correction;
    }

    /// A run of words a lookup contributes, as the dialect writes it: a row,
    /// the empty table's own statement, or an exact lookup's key.
    ///
    /// LaTeX sets words upright -- `to under`, `gives`, `key` and `at` are
    /// words, and bare math mode would set each as a product of italic
    /// letters (`k \cdot e \cdot y`) -- so it wraps them in `\mathrm{...}`,
    /// escaped by `latex_math_words` (`detail/latex_math.hpp`): a space as
    /// `\ `, `_` as `\_`, `%` as `\%`, and so on.
    ///
    /// **`\mathrm`, not `\text`.** This was `\text{...}` once, and a row
    /// holds a unit's symbol and a key's name, which may hold TeX's specials.
    /// A bare `%` in `\text` comments out the rest of the formula in a real
    /// TeX engine, so a percent-valued table did not typeset at all; and the
    /// escape a TeX engine needs, `\text{fit\_2}`, is shown backslash and all
    /// by the site's MathJax, which does not load `textmacros`. In
    /// `\mathrm{...}` both engines read the same escapes the same way.
    ///
    /// Everything a lookup renders goes through here **except the operand of
    /// a banded or an interpolating lookup**, which is a sub-expression and
    /// belongs in math mode -- it has already rendered itself in the dialect.
    ///
    /// **The words themselves are the same in all three dialects**, before
    /// LaTeX's escape and wrapper, which is what lets the cross-dialect test
    /// in `render_tests.cpp` compare the dialects against each other rather
    /// than assert each separately.
    template <Dialect D>
    [[nodiscard]] std::string lookup_words_in_dialect(std::string const& words)
    {
        if constexpr (D == Dialect::LaTeX)
            return "\\mathrm{" + latex_math_words(words) + "}";
        else
            return words;
    }

    /// The separator between a rendered lookup's fields.
    ///
    /// **LaTeX adds `\allowbreak`, and that is a correctness fix rather than
    /// typographic polish.** Each row is one atomic `\mathrm{...}`, and TeX
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
    /// What does help, for a caller who genuinely needs a wide table in a
    /// display, is `breqn`'s `dmath`: 5 overfull boxes over the same 22
    /// renderings against a plain display's 16, worst 13.3pt against 549pt.
    ///
    /// **And it helps *because of* this separator, not despite it.** With
    /// `\allowbreak` stripped out, `dmath` falls straight back to 16 boxes
    /// worst 549.0pt -- the plain display's number to the decimal. So the
    /// separator is what makes the caller's remedy available at all, and
    /// deleting it as tidy-up would quietly take the remedy away with it.
    ///
    /// The residual is named rather than rounded off to "clean": all 5 boxes
    /// that survive `dmath` are **exact** lookups, 8.3pt to 13.3pt. An earlier
    /// measurement of `dmath` sampled four renderings and happened to include
    /// no exact lookup at all, which is the degenerate-fixture rule landing on
    /// a measurement set rather than on a test fixture.
    ///
    /// **This paragraph has been wrong twice, both times the same way, and
    /// that is the useful thing in it.** The first draft said amsmath's
    /// `multline` was the remedy; typesetting it gave 571pt, slightly *worse*
    /// than a plain display, because `multline` also breaks only at an
    /// explicit `\\`. The draft that replaced it said `dmath` owed nothing to
    /// `\allowbreak` -- measured against a "control" that turned out to be
    /// byte-identical to the treatment, because the strip silently never
    /// applied. An instrument that cannot report a difference will report no
    /// difference. Anything added here comes from two files diffed before they
    /// are trusted.
    ///
    /// `render_tests.cpp` pins what a unit test can pin: that every field
    /// separator carries `\allowbreak`, and that a lookup never emits `\\`. It
    /// cannot pin any of the numbers above -- those are properties of a TeX
    /// engine, not of the emitted string -- so the stripped case is measured
    /// here and nowhere else.
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

namespace detail
{
    /// A precision limit's symbol: `r` for repeatability, `R` for
    /// reproducibility.
    [[nodiscard]] constexpr std::string_view precision_render_symbol(PrecisionKind precisionKind) noexcept
    {
        return precisionKind == PrecisionKind::Reproducibility ? "R" : "r";
    }
} // namespace detail

template <Dialect D, Node N>
[[nodiscard]] std::string render(N const& node);

/// Renders the series @p node in dialect @p D. A series is not a `Node`
/// (`expression.hpp`), so it needs overloads of its own; each series variable
/// in it is marked as a series (`detail::series_marker`).
template <Dialect D, SeriesNode S>
[[nodiscard]] std::string render(S const& node);

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

/// Renders @p node in dialect @p D, writing every quantity's symbol as
/// @p vocabulary says (`vocabulary.hpp`). The three overloads above are these
/// three with `DefaultVocabulary`, which renames nothing.
template <Dialect D, Node N, Vocabulary V>
[[nodiscard]] std::string render(N const& node, V const& vocabulary);

/// The `Predicate` counterpart of the overload above.
template <Dialect D, Predicate P, Vocabulary V>
[[nodiscard]] std::string render(P const& node, V const& vocabulary);

/// The `SeriesNode` counterpart of the overload above.
template <Dialect D, SeriesNode S, Vocabulary V>
[[nodiscard]] std::string render(S const& node, V const& vocabulary);

/// The `CurveExpression` counterpart of the overload above: a curve is
/// neither a `Node` nor a series (`curve.hpp`).
template <Dialect D, CurveExpression C, Vocabulary V>
[[nodiscard]] std::string render(C const& node, V const& vocabulary);

/// The `Constraint` counterpart of the overload above.
template <Dialect D, Predicate P, Vocabulary V>
[[nodiscard]] std::string render(Constraint<P> const& node, V const& vocabulary);

/// The counterpart for a sample transformer -- a rejection of outliers
/// (`rejection.hpp`) -- which is neither a `Node` nor a series.
template <Dialect D, typename R, Vocabulary V>
    requires detail::is_sample_transformer<R>
[[nodiscard]] std::string render(R const& node, V const& vocabulary);

/// The counterpart for raw observations (`binning.hpp`), neither a `Node` nor
/// a series, when a statistic renders its sample.
template <Dialect D, ObservationsNode O, Vocabulary V>
[[nodiscard]] std::string render(O const& node, V const& vocabulary);

namespace detail
{
    template <Dialect D, typename Child, Vocabulary V>
        requires Node<Child> || SeriesNode<Child>
    [[nodiscard]] std::string render_operand(Child const& child, Precedence context, V const& vocabulary)
    {
        std::string childText = render<D>(child, vocabulary);
        if (static_cast<int>(precedence_in<D>(child)) < static_cast<int>(context))
            return "(" + childText + ")";
        return childText;
    }

    /// A binary operation, infix, bracketing each side only where its
    /// precedence against the operator requires it; division in LaTeX as
    /// `\frac{}{}`, which groups both sides itself. **The one spelling of the
    /// four operators**, shared by the scalar `BinaryNode` and the elementwise
    /// `ElementwiseBinaryNode`, so that `m_r(i) / m_t` is written exactly as
    /// `m_r / m_t` is, and the two cannot drift apart.
    template <Dialect D, BinaryOperator Op, typename Left, typename Right, Vocabulary V>
    [[nodiscard]] std::string render_binary(Left const& leftOperand, Right const& rightOperand, V const& vocabulary)
    {
        constexpr Precedence ownPrecedence = binary_precedence<Op>;
        constexpr Precedence rightContext = (Op == BinaryOperator::Subtract || Op == BinaryOperator::Divide)
                                                ? static_cast<Precedence>(static_cast<int>(ownPrecedence) + 1)
                                                : ownPrecedence;

        if constexpr (D == Dialect::LaTeX && Op == BinaryOperator::Divide)
            // \frac groups both sides itself, so neither operand needs a bracket.
            return "\\frac{" + render<D>(leftOperand, vocabulary) + "}{" + render<D>(rightOperand, vocabulary) + "}";
        else
        {
            std::string const leftText = render_operand<D>(leftOperand, ownPrecedence, vocabulary);
            std::string const rightText = render_operand<D>(rightOperand, rightContext, vocabulary);

            if constexpr (Op == BinaryOperator::Add)
                return leftText + " + " + rightText;
            else if constexpr (Op == BinaryOperator::Subtract)
                return leftText + " - " + rightText;
            else if constexpr (Op == BinaryOperator::Multiply)
                return D == Dialect::LaTeX ? leftText + " \\cdot " + rightText : leftText + " * " + rightText;
            else
                return leftText + " / " + rightText;
        }
    }
} // namespace detail

// One overload per node kind. Each is found by argument-dependent lookup from
// `render` below, exactly as the evaluator's overloads are.
//
// Every overload takes the vocabulary as a second argument, and one whose node
// names a quantity or holds a sub-expression hands it on. The ones whose node
// does neither (a constant, pi, an exact lookup) take it too and ignore it:
// a one-argument `render_node` in this namespace is refused, so that a node
// kind added later cannot render its operands in the declared symbols without
// anything saying so -- see `detail::render_in_vocabulary`.

/// A variable renders as its quantity's symbol under @p vocabulary --
/// backtick-quoted in Markdown. An overridden constant (`overlay.hpp`) renders
/// here too, as the `VarNode` it derives from.
template <Dialect D, Described Q, Vocabulary V>
[[nodiscard]] std::string render_node(VarNode<Q> const&, V const& vocabulary)
{
    std::string quantitySymbol { symbol_of<Q>(vocabulary) };
    if constexpr (D == Dialect::Markdown)
        return "`" + quantitySymbol + "`";
    else
        return quantitySymbol;
}

/// A series variable renders as its quantity's symbol under @p vocabulary,
/// marked as a series in the formula itself (`detail::series_marker`), so a
/// reader can tell `x_m(i)` from the single value `x_m` without the symbol
/// table. The marker wraps the jurisdiction's symbol, never the declared one.
template <Dialect D, Described Q, std::size_t N, Vocabulary V>
[[nodiscard]] std::string render_node(SeriesVarNode<Q, N> const&, V const& vocabulary)
{
    return detail::series_marker<D>(std::string { symbol_of<Q>(vocabulary) });
}

/// A constant renders as its number, followed by its unit's symbol when it has one.
///
/// The number-and-unit spelling is `detail::number_with_unit`, shared with the
/// lookup tables below so that a table's row states a number exactly as a
/// constant holding the same number does -- see that helper. The number is
/// written as @p vocabulary's style says, exact and unpadded
/// (`detail::typed_number_text`): `863/1000` by default, `0.863` under
/// `NumberStyle::exact_decimal()`.
///
/// In LaTeX the symbol is set upright after a thin space, `150\,\mathrm{mm}`
/// and `5\,\mathrm{\%}`, as the rounding clause sets it (`detail::latex_unit`):
/// written bare in math mode, `150 mm` read as the product of two italic
/// letters, and `5 %` commented out the rest of the formula.
template <Dialect D, Unit U, Vocabulary V>
[[nodiscard]] std::string render_node(ConstantNode<U> const& node, V const& vocabulary)
{
    constexpr Unit declaredUnit = U;
    if constexpr (D == Dialect::LaTeX)
        return detail::typed_number_text(node.number, declaredUnit, vocabulary)
               + detail::unit_clause("\\,", detail::latex_unit(view(declaredUnit.symbolText)));
    else
        return detail::number_with_unit(detail::typed_number_text(node.number, declaredUnit, vocabulary),
                                        view(declaredUnit.symbolText));
}

/// A unary node renders as its operator followed by its (parenthesised if
/// necessary) operand.
template <Dialect D, UnaryOperator Op, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(UnaryNode<Op, Operand> const& node, V const& vocabulary)
{
    static_assert(Op == UnaryOperator::Negate, "formula: unknown unary operator");
    return "-" + detail::render_operand<D>(node.operand, detail::Precedence::Unary, vocabulary);
}

/// A binary node renders infix, bracketing each side only where its precedence
/// against the parent operator requires it -- see the file comment. Division
/// in `Dialect::LaTeX` renders as `\frac{}{}` instead, which needs no brackets
/// at all because the fraction bar already groups both sides.
template <Dialect D, BinaryOperator Op, Node Left, Node Right, Vocabulary V>
[[nodiscard]] std::string render_node(BinaryNode<Op, Left, Right> const& node, V const& vocabulary)
{
    return detail::render_binary<D, Op>(node.lhs, node.rhs, vocabulary);
}

/// An elementwise binary node renders exactly as the scalar operator does
/// (`detail::render_binary`): the operation adds no marker of its own, and a
/// series operand carries its own.
template <Dialect D, BinaryOperator Op, typename Left, typename Right, Vocabulary V>
[[nodiscard]] std::string render_node(ElementwiseBinaryNode<Op, Left, Right> const& node, V const& vocabulary)
{
    return detail::render_binary<D, Op>(node.lhs, node.rhs, vocabulary);
}

/// Elementwise negation renders as scalar negation does.
template <Dialect D, UnaryOperator Op, SeriesNode Operand, Vocabulary V>
[[nodiscard]] std::string render_node(ElementwiseUnaryNode<Op, Operand> const& node, V const& vocabulary)
{
    static_assert(Op == UnaryOperator::Negate, "formula: unknown unary operator");
    return "-" + detail::render_operand<D>(node.operand, detail::Precedence::Unary, vocabulary);
}

/// A refused series (`detail::RefusedSeries`) renders as nothing a reader
/// could take for a formula. A program holding one never compiles; this only
/// keeps a `render` of it from adding a second, compiler-worded error.
template <Dialect D, Dimension Dim, Vocabulary V>
[[nodiscard]] std::string render_node(detail::RefusedSeries<Dim> const&, V const&)
{
    return "(refused)";
}

namespace detail
{
    /// A per-element rounding's granularities, `0/0/1`, in the series' order.
    /// A slash rather than a comma, which already separates the call's
    /// fields, and never brackets, which Markdown reads as a link.
    template <auto Places>
    [[nodiscard]] std::string granularities_text()
    {
        std::string listed;
        for (DecimalPlaces const elementPlaces: Places)
        {
            if (!listed.empty())
                listed += "/";
            listed += std::to_string(elementPlaces.value);
        }
        return listed;
    }
} // namespace detail

/// A per-element rounding renders as `RoundNode` does, with every element's
/// granularity in the series' order: `round(p(i), to 0/0/1 dp of %)`, and in
/// LaTeX `\operatorname{round}_{0/-1/2\,\mathrm{mm}}(...)`. The unit clause is
/// `RoundNode`'s: set upright and escaped in LaTeX (`detail::latex_unit`), and
/// dropped for a unit with no symbol. The mode is absent, for `RoundNode`'s
/// reason, and appears in the trace.
template <Dialect D, Unit U, auto Places, RoundingMode Mode, SeriesNode S, Vocabulary V>
[[nodiscard]] std::string render_node(ElementwiseRoundNode<U, Places, Mode, S> const& node, V const& vocabulary)
{
    std::string const inner = render<D>(node.operand, vocabulary);
    constexpr Unit roundedIn = U;
    std::string const unitSymbol { view(roundedIn.symbolText) };
    // Places already refused (`countMatches`) are not a table to list; the
    // text is never seen, since the program does not compile.
    std::string placesText = "(refused)";
    if constexpr (ElementwiseRoundNode<U, Places, Mode, S>::countMatches)
        placesText = detail::granularities_text<Places>();

    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{round}_{" + placesText + detail::unit_clause("\\,", detail::latex_unit(unitSymbol)) + "}("
               + inner + ")";
    else
        return "round(" + inner + ", to " + placesText + " dp" + detail::unit_clause(" of ", unitSymbol) + ")";
}

/// A running total renders as a call naming its end: `cumulative(m_r(i), from
/// last)`, and in LaTeX `\operatorname{cumulative}_{\text{from last}}(...)`,
/// the end on a subscript as a rounding's granularity is. The direction is
/// always written: without it the rendering states half the formula.
template <Dialect D, CumulativeDirection Direction, SeriesNode S, Vocabulary V>
[[nodiscard]] std::string render_node(CumulativeNode<Direction, S> const& node, V const& vocabulary)
{
    std::string const inner = render<D>(node.operand, vocabulary);
    std::string const runsFrom { describe(Direction) };
    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{cumulative}_{\\text{" + runsFrom + "}}(" + inner + ")";
    else
        return "cumulative(" + inner + ", " + runsFrom + ")";
}

/// A sample's count renders as a call on its sample, `sample_count(m(i))`,
/// and in LaTeX as `n({m}_{i})`. The count is one value and carries no series
/// marker; its sample carries its own.
template <Dialect D, SampleSource S, Vocabulary V>
[[nodiscard]] std::string render_node(SampleCountNode<S> const& node, V const& vocabulary)
{
    if constexpr (D == Dialect::LaTeX)
        return "n(" + render<D>(node.sample, vocabulary) + ")";
    else
        return "sample_count(" + render<D>(node.sample, vocabulary) + ")";
}

/// A sample's mean renders as a call on its sample, `sample_mean(m(i))`, and
/// in LaTeX as a bar over it, `\overline{{m}_{i}}`, which groups itself. The
/// mean is one value and carries no series marker; its sample carries its own.
template <Dialect D, SampleSource S, Vocabulary V>
[[nodiscard]] std::string render_node(SampleMeanNode<S> const& node, V const& vocabulary)
{
    if constexpr (D == Dialect::LaTeX)
        return "\\overline{" + render<D>(node.sample, vocabulary) + "}";
    else
        return "sample_mean(" + render<D>(node.sample, vocabulary) + ")";
}

/// A sample's variance renders as a call on its sample,
/// `sample_variance(m(i))`, and in LaTeX as `s^{2}({m}_{i})`, the spelling
/// a spike typeset clean. The variance is one value and carries no series
/// marker; its sample carries its own.
template <Dialect D, SampleSource S, Vocabulary V>
[[nodiscard]] std::string render_node(SampleVarianceNode<S> const& node, V const& vocabulary)
{
    if constexpr (D == Dialect::LaTeX)
        return "s^{2}(" + render<D>(node.sample, vocabulary) + ")";
    else
        return "sample_variance(" + render<D>(node.sample, vocabulary) + ")";
}

/// A sample's range renders as a call on its sample, `sample_range(m(i))`,
/// and in LaTeX as `\operatorname{range}({m}_{i})`.
template <Dialect D, SampleSource S, Vocabulary V>
[[nodiscard]] std::string render_node(SampleRangeNode<S> const& node, V const& vocabulary)
{
    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{range}(" + render<D>(node.sample, vocabulary) + ")";
    else
        return "sample_range(" + render<D>(node.sample, vocabulary) + ")";
}

/// A sum renders as a call on its series, `sum(m_r(i))`, and in LaTeX as the
/// large operator, `\sum {m_r}_{i}`, whose operand already carries the
/// series marker -- the sum itself is one value and carries none. See
/// `detail::precedence_in` for where the LaTeX form is bracketed.
template <Dialect D, SeriesNode S, Vocabulary V>
[[nodiscard]] std::string render_node(SumNode<S> const& node, V const& vocabulary)
{
    if constexpr (D == Dialect::LaTeX)
        return "\\sum " + detail::render_operand<D>(node.operand, detail::Precedence::Multiplicative, vocabulary);
    else
        return "sum(" + render<D>(node.operand, vocabulary) + ")";
}

/// A per-element constant renders as its list of values, `values(0.7 mm,
/// 1.9 mm, ...)`, each spelled as a constant holding it would be
/// (`detail::number_with_unit`), separated as a lookup's rows are. A list
/// already reads as many values, so it carries no index marker. A
/// formula's own values are never truncated.
template <Dialect D, Unit U, std::size_t N, Vocabulary V>
[[nodiscard]] std::string render_node(SeriesConstantNode<U, N> const& node, V const& vocabulary)
{
    constexpr Unit statedIn = U;
    std::string listed;
    for (std::size_t at = 0; at < N; ++at)
    {
        if (at > 0)
            listed += detail::lookup_separator<D>();
        // Each value spelled as a `ConstantNode` holding it is, in every
        // dialect: in LaTeX its unit set upright and escaped.
        std::string const elementText = detail::typed_number_text(node.elements[at], statedIn, vocabulary);
        if constexpr (D == Dialect::LaTeX)
            listed += elementText + detail::unit_clause("\\,", detail::latex_unit(view(statedIn.symbolText)));
        else
            listed += detail::number_with_unit(elementText, view(statedIn.symbolText));
    }
    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{values}(" + listed + ")";
    else
        return "values(" + listed + ")";
}

/// A power renders as its base with the exponent superscript -- braced in LaTeX.
template <Dialect D, int Exponent, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(PowerNode<Exponent, Operand> const& node, V const& vocabulary)
{
    std::string const base = detail::render_operand<D>(node.operand, detail::Precedence::Atom, vocabulary);
    if constexpr (D == Dialect::LaTeX)
        return base + "^{" + std::to_string(Exponent) + "}";
    else
        return base + "^" + std::to_string(Exponent);
}

/// A root renders as `\sqrt{}` (or `\sqrt[n]{}` for a degree other than 2) in
/// LaTeX, and as `sqrt(...)` / `rootN(...)` in every other dialect.
template <Dialect D, int Degree, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(RootNode<Degree, Operand> const& node, V const& vocabulary)
{
    std::string const inner = render<D>(node.operand, vocabulary);
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
/// <unit>)`, the shape this rendered once). That
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
template <Dialect D, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(RoundNode<U, Places, Mode, Operand> const& node, V const& vocabulary)
{
    std::string const inner = render<D>(node.operand, vocabulary);
    constexpr Unit declaredUnit = U;
    std::string const unitSymbol { view(declaredUnit.symbolText) };
    std::string const placesText = std::to_string(Places.value);

    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{round}_{" + placesText + detail::unit_clause("\\,", detail::latex_unit(unitSymbol)) + "}("
               + inner + ")";
    else
        return "round(" + inner + ", to " + placesText + " dp" + detail::unit_clause(" of ", unitSymbol) + ")";
}

/// A significant-digits rounding node, spelled the same way as `RoundNode`
/// above but with "sf" (significant figures) in place of "dp" -- see that
/// overload for why no precedence override is needed, why `RoundingMode` is
/// left out, and why the granularity is a comma-separated second argument
/// rather than a trailing suffix or a `[...]` prefix.
template <Dialect D, Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(RoundSignificantNode<U, Digits, Mode, Operand> const& node, V const& vocabulary)
{
    std::string const inner = render<D>(node.operand, vocabulary);
    constexpr Unit declaredUnit = U;
    std::string const unitSymbol { view(declaredUnit.symbolText) };
    std::string const digitsText = std::to_string(Digits.value);

    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{round}_{" + digitsText + "\\mathrm{sf}"
               + detail::unit_clause(",\\,", detail::latex_unit(unitSymbol)) + "}(" + inner + ")";
    else
        return "round(" + inner + ", to " + digitsText + " sf" + detail::unit_clause(" of ", unitSymbol) + ")";
}

/// A rounded square root renders as what it computes, a rounding of a root:
/// `round(sqrt(<radicand>), to <places> dp of <unit>)`, and in LaTeX
/// `RoundNode`'s subscripted `\operatorname{round}` around `\sqrt{}`. That it
/// is one exact operation rather than two is how it is evaluated, not what it
/// states; a reader checking it against a standard reads "the root, rounded to
/// 0.01 g" either way. See `RoundNode`'s overload above for why the mode is
/// left out, why the granularity is a comma-separated second argument, and why
/// no `PrecedenceOf` override is needed: the call's own parentheses group it,
/// so the primary template's `Atom` is right.
template <Dialect D, Unit U, DecimalPlaces Places, RoundingMode Mode, Node Radicand, Vocabulary V>
[[nodiscard]] std::string render_node(RoundedRootNode<U, Places, Mode, Radicand> const& node, V const& vocabulary)
{
    std::string const inner = render<D>(node.radicand, vocabulary);
    constexpr Unit declaredUnit = U;
    std::string const unitSymbol { view(declaredUnit.symbolText) };
    std::string const placesText = std::to_string(Places.value);

    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{round}_{" + placesText + detail::unit_clause("\\,", detail::latex_unit(unitSymbol))
               + "}(\\sqrt{" + inner + "})";
    else
        return "round(sqrt(" + inner + "), to " + placesText + " dp" + detail::unit_clause(" of ", unitSymbol) + ")";
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
template <Dialect D, Unit U, detail::FixedString Justification, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(NumericValueNode<U, Justification, Operand> const& node, V const& vocabulary)
{
    std::string const inner = render<D>(node.operand, vocabulary);
    constexpr Unit declaredUnit = U;
    std::string const unitSymbol { view(declaredUnit.symbolText) };

    if constexpr (D == Dialect::LaTeX)
        return "\\{" + inner + detail::unit_clause("/", detail::latex_unit(unitSymbol)) + "\\}";
    else
        return "numeric(" + inner + detail::unit_clause(", in ", unitSymbol) + ")";
}

/// Pi renders as `\pi` in LaTeX, and as `pi` in every other dialect.
template <Dialect D, Vocabulary V>
[[nodiscard]] std::string render_node(PiNode const&, V const&)
{
    if constexpr (D == Dialect::LaTeX)
        return "\\pi";
    else
        return "pi";
}

/// A citation is documentation, not arithmetic: it does not appear in the
/// rendered formula. `document()` is what surfaces it.
template <Dialect D, Node Inner, Vocabulary V>
[[nodiscard]] std::string render_node(DocumentedNode<Inner> const& node, V const& vocabulary)
{
    return render<D>(node.inner, vocabulary);
}

/// A variant's formula as a jurisdiction replaced it renders as the
/// replacement: the formula is what runs. That it is a jurisdiction's is the
/// trace's to say, and the citation `document()`'s -- as `DocumentedNode`'s
/// citation is.
template <Dialect D, Node Expr, Vocabulary V>
[[nodiscard]] std::string render_node(ReplacedVariantNode<Expr> const& node, V const& vocabulary)
{
    return render<D>(node.replacement(), vocabulary);
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
// visible in every row already (`103 to under 197 mm` against `key Cylinder`), so
// spending the head name on that instead would name the difference a reader
// can see and leave the one they cannot.
//
// **The banded and the interpolating domain render differently because they
// are different**, and this is the other thing a reader must not miss. A band
// is an interval whose top is excluded and says so -- `209/10 to under 293/10
// mm`. A breakpoint is a row, not a boundary: the table states a value *at*
// it, the last one included, so it renders as the point it is -- `at 293/10
// mm`. Nothing in an interpolating rendering excludes anything, because
// nothing in an interpolating table does. `lookup_tests.cpp` pins the two
// behaviours against each other and `render_tests.cpp` the two spellings, each
// on one shared number of its own, so that "harmonising" them in either
// direction fails.
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
template <Dialect D, Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand> const& node, V const& vocabulary)
{
    constexpr Unit keyUnit = KeyUnit;
    constexpr Unit resultUnit = ResultUnit;

    NumberStyle const tableStyle = typed_number_style(vocabulary);
    std::string rowText;
    for (std::size_t bandIndex = 0; bandIndex < Bands.size(); ++bandIndex)
        rowText += detail::lookup_separator<D>()
                   + detail::lookup_words_in_dialect<D>(detail::lookup_row_text(
                       detail::band_text(Bands[bandIndex], view(keyUnit.symbolText), keyUnit, tableStyle),
                       detail::number_with_unit(
                           detail::typed_number_text(node.corrections[bandIndex], resultUnit, vocabulary),
                           view(resultUnit.symbolText))));

    return detail::lookup_call<D>("lookup", render<D>(node.operand, vocabulary), rowText);
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
/// thing, and only in LaTeX: `key Cylinder` is words, not mathematics, so it is set
/// as text like the rows are, where the other two kinds' operands stay in math
/// mode because they really are expressions.
///
/// **A key renders as its name** -- `key Cylinder`, or the author's own
/// spelling of it through `EnumeratorName` (`enumerator.hpp`) -- and as its
/// underlying value only when it names no row of the table. See
/// `detail::key_text`.
template <Dialect D, KeyTable Keys, Unit ResultUnit, Vocabulary V>
[[nodiscard]] std::string render_node(ExactLookupNode<Keys, ResultUnit> const& node, V const& vocabulary)
{
    constexpr Unit resultUnit = ResultUnit;

    std::string rowText;
    for (std::size_t keyIndex = 0; keyIndex < Keys.size(); ++keyIndex)
        rowText += detail::lookup_separator<D>()
                   + detail::lookup_words_in_dialect<D>(detail::lookup_row_text(
                       detail::key_text<D, Keys>(Keys[keyIndex]),
                       detail::number_with_unit(
                           detail::typed_number_text(node.corrections[keyIndex], resultUnit, vocabulary),
                           view(resultUnit.symbolText))));

    return detail::lookup_call<D>(
        "lookup", detail::lookup_words_in_dialect<D>(detail::key_text<D, Keys>(node.key)), rowText);
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
template <Dialect D, Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand> const& node,
                                      V const& vocabulary)
{
    constexpr Unit keyUnit = KeyUnit;
    constexpr Unit resultUnit = ResultUnit;

    NumberStyle const tableStyle = typed_number_style(vocabulary);
    std::string rowText;
    for (std::size_t pointIndex = 0; pointIndex < Points.size(); ++pointIndex)
        rowText +=
            detail::lookup_separator<D>()
            + detail::lookup_words_in_dialect<D>(detail::lookup_row_text(
                "at "
                    + detail::number_with_unit(
                        detail::declared_number_text(
                            Points[pointIndex].numerator, Points[pointIndex].denominator, keyUnit, tableStyle),
                        view(keyUnit.symbolText)),
                detail::number_with_unit(detail::typed_number_text(node.corrections[pointIndex], resultUnit, vocabulary),
                                         view(resultUnit.symbolText))));

    return detail::lookup_call<D>("interpolate", render<D>(node.operand, vocabulary), rowText);
}

/// A snap renders as `snap(<operand>, to <permitted values> <unit>)`: the set
/// in its declared order, the unit once, shaped as a lookup is
/// (`detail::lookup_call`). **No tie rule**, for `RoundNode`'s reason: a
/// standard states a set of permitted values, not a rule for a value exactly
/// midway; the trace carries the rule where it decided.
template <Dialect D, Unit KeyUnit, BreakpointTable Permitted, SnapTie Tie, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(SnapNode<KeyUnit, Permitted, Tie, Operand> const& node, V const& vocabulary)
{
    constexpr Unit keyUnit = KeyUnit;
    NumberStyle const tableStyle = typed_number_style(vocabulary);
    std::string listed;
    for (std::size_t pointIndex = 0; pointIndex < Permitted.size(); ++pointIndex)
    {
        if (pointIndex > 0)
            listed += ", ";
        listed += detail::declared_number_text(
            Permitted[pointIndex].numerator, Permitted[pointIndex].denominator, keyUnit, tableStyle);
    }
    std::string const permittedField =
        detail::lookup_separator<D>()
        + detail::lookup_words_in_dialect<D>(detail::number_with_unit("to " + listed, view(keyUnit.symbolText)));
    return detail::lookup_call<D>("snap", render<D>(node.operand, vocabulary), permittedField);
}

/// A declared domain renders as its points, `domain(103, 127, 163 mm)`,
/// in their declared order with the unit once, as a snap's set is. A list
/// already reads as many values, so it carries no index marker, as a
/// per-element constant carries none.
template <Dialect D, Unit U, BreakpointTable Points, Vocabulary V>
[[nodiscard]] std::string render_node(DomainNode<U, Points> const&, V const& vocabulary)
{
    constexpr Unit declaredIn = U;
    NumberStyle const tableStyle = typed_number_style(vocabulary);
    std::string listed;
    for (std::size_t pointIndex = 0; pointIndex < Points.size(); ++pointIndex)
    {
        if (pointIndex > 0)
            listed += ", ";
        listed += detail::declared_number_text(
            Points[pointIndex].numerator, Points[pointIndex].denominator, declaredIn, tableStyle);
    }
    std::string const pointsText =
        detail::lookup_words_in_dialect<D>(detail::number_with_unit(listed, view(declaredIn.symbolText)));
    if constexpr (D == Dialect::LaTeX)
        return "\\operatorname{domain}(" + pointsText + ")";
    else
        return "domain(" + pointsText + ")";
}

/// Raw observations render as their quantity's symbol, marked as a series is
/// (`detail::series_marker`): each is one of many values, and the symbol
/// table says how many there can be.
template <Dialect D, Described Q, std::size_t Capacity, Vocabulary V>
[[nodiscard]] std::string render_node(ObservationsVarNode<Q, Capacity> const&, V const& vocabulary)
{
    return detail::series_marker<D>(std::string { symbol_of<Q>(vocabulary) });
}

/// Observations refused already: never seen, since the program does not
/// compile.
template <Dialect D, Vocabulary V>
[[nodiscard]] std::string render_node(detail::RefusedObservations const&, V const&)
{
    return "(refused)";
}

/// A binning renders as `bin(<observations>, <class>, ...)`: one field per
/// class, in the declared order, each in the one spelling of a band
/// (`detail::band_text`), shaped as a lookup is (`detail::lookup_call`).
template <Dialect D, Unit KeyUnit, BandTable Classes, ObservationsNode Obs, Vocabulary V>
[[nodiscard]] std::string render_node(BinnedNode<KeyUnit, Classes, Obs> const& node, V const& vocabulary)
{
    constexpr Unit keyUnit = KeyUnit;
    NumberStyle const tableStyle = typed_number_style(vocabulary);
    std::string classText;
    for (std::size_t classIndex = 0; classIndex < Classes.size(); ++classIndex)
        classText += detail::lookup_separator<D>()
                     + detail::lookup_words_in_dialect<D>(
                         detail::band_text(Classes[classIndex], view(keyUnit.symbolText), keyUnit, tableStyle));
    return detail::lookup_call<D>("bin", render_node<D>(node.source, vocabulary), classText);
}

/// A curve renders as a call on its two series, `curve(d(i), p(i))`, each
/// carrying its series marker, shaped as a lookup is (`detail::lookup_call`).
template <Dialect D, SeriesNode DomainSeries, SeriesNode ValueSeries, Vocabulary V>
[[nodiscard]] std::string render_node(CurveNode<DomainSeries, ValueSeries> const& node, V const& vocabulary)
{
    return detail::lookup_call<D>("curve",
                                  render<D>(node.domainSeries, vocabulary),
                                  detail::lookup_separator<D>() + render<D>(node.valueSeries, vocabulary));
}

/// A splice renders both curves in the order written, and **always** its
/// direction: `splice(curve(...), curve(...), non-decreasing)`. Without the
/// direction the rendering states half the formula, as a running total
/// without its end would.
template <Dialect D, Monotone M, CurveExpression A, CurveExpression B, Vocabulary V>
[[nodiscard]] std::string render_node(SpliceNode<M, A, B> const& node, V const& vocabulary)
{
    return detail::lookup_call<D>("splice",
                                  render<D>(node.first, vocabulary),
                                  detail::lookup_separator<D>() + render<D>(node.second, vocabulary)
                                      + detail::lookup_separator<D>()
                                      + detail::lookup_words_in_dialect<D>(std::string { describe(M) }));
}

/// An interpolation along a curve renders as `interpolate(<curve>, at
/// <point>)` -- `render()`'s head for a value computed between two points, as
/// an interpolating lookup's is. The point is an expression, rendered in the
/// dialect; only the word before it is text.
template <Dialect D, CurveExpression C, Node At, Vocabulary V>
[[nodiscard]] std::string render_node(InterpolateAlongNode<C, At> const& node, V const& vocabulary)
{
    return detail::lookup_call<D>("interpolate",
                                  render<D>(node.along, vocabulary),
                                  detail::lookup_separator<D>() + detail::lookup_words_in_dialect<D>("at ")
                                      + render<D>(node.at, vocabulary));
}

/// A critical-value lookup renders as `critical(<count>, at 3, 4, 5, 6, 8)`:
/// the count, then every size the table declares, in full.
///
/// **The sizes and not the values.** The sizes are the table's structure and
/// part of what the formula says -- which sample sizes it answers for, and so
/// where it misses. The values are the author's data, supplied at runtime, as
/// a lookup's corrections are; a critical value printed into a formula's text
/// would put a table's contents on every page that quotes it.
///
/// The shape is `lookup_call`'s, for its reasons: the subject first, then one
/// field per size, each a legal break point in LaTeX. `at` opens the list
/// once, in words (`\mathrm{at\ }` in LaTeX, as a lookup's words are); the
/// sizes stay numbers. A table of
/// no sizes says so, as an empty lookup does.
template <Dialect D, SampleSizeTable Sizes, Unit ResultUnit, Node Count, Vocabulary V>
[[nodiscard]] std::string render_node(SampleSizeLookupNode<Sizes, ResultUnit, Count> const& node, V const& vocabulary)
{
    std::string rowsText;
    for (std::size_t rowIndex = 0; rowIndex < Sizes.size(); ++rowIndex)
        rowsText += detail::lookup_separator<D>()
                    + (rowIndex == 0 ? detail::lookup_words_in_dialect<D>("at ") : std::string {})
                    + std::to_string(Sizes[rowIndex]);

    return detail::lookup_call<D>("critical", render<D>(node.count, vocabulary), rowsText);
}

/// An absolute value renders as `abs(<operand>)` in plain text and Markdown,
/// and as `\left\lvert <operand>\right\rvert` in LaTeX.
///
/// **Never a `|`, in any dialect.** A bare vertical bar inside a Markdown table
/// cell ends the cell, silently: a spike measured a row whose formula held an
/// absolute value in bars render as a one-cell row holding only the text
/// before the first bar (python-markdown 3.10.3, pymdown-extensions 12.1). A
/// formula is quoted in exactly such tables -- a symbol table, a gallery row,
/// a `document()` page -- so the plain and Markdown spellings are a call, and
/// LaTeX spells its bars `\lvert` and `\rvert`: a LaTeX rendering set in a
/// cell holds no `|` either.
/// Either way the operand is grouped, so no `PrecedenceOf` override is
/// needed: the primary template's `Atom` is right.
template <Dialect D, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(AbsoluteValueNode<Operand> const& node, V const& vocabulary)
{
    std::string const inner = render<D>(node.operand, vocabulary);
    if constexpr (D == Dialect::LaTeX)
        return "\\left\\lvert " + inner + "\\right\\rvert";
    else
        return "abs(" + inner + ")";
}

/// A precision limit's level renders as the word `level` -- `\text{level}`
/// in LaTeX -- and never as a symbol. A symbol such as `L` could collide with
/// an author's own quantity, and symbols are a jurisdiction's to choose; the
/// word belongs to no quantity, and a vocabulary never renames it.
template <Dialect D, Described Q, Vocabulary V>
[[nodiscard]] std::string render_node(PrecisionLevelNode<Q> const&, V const&)
{
    if constexpr (D == Dialect::LaTeX)
        return "\\text{level}";
    else
        return "level";
}

/// The current pass's mean renders as words, `pass mean`, and in LaTeX as
/// `\bar{x}_{\text{pass}}`: a symbol could collide with an author's own
/// quantity's.
template <Dialect D, Described Q, Vocabulary V>
[[nodiscard]] std::string render_node(PassMeanNode<Q> const&, V const&)
{
    if constexpr (D == Dialect::LaTeX)
        return "\\bar{x}_{\\text{pass}}";
    else
        return "pass mean";
}

/// The current pass's size renders as `pass n`, and in LaTeX as
/// `n_{\text{pass}}`.
template <Dialect D, Vocabulary V>
[[nodiscard]] std::string render_node(PassCountNode const&, V const&)
{
    if constexpr (D == Dialect::LaTeX)
        return "n_{\\text{pass}}";
    else
        return "pass n";
}

/// A rejection renders with every parameter that shapes its result stated:
/// `without outliers(m(i); abs(x - pass mean) > 3/50 * pass mean; most
/// extreme per pass; keep on limit; at most 2; keep at least 4)`. A rendering
/// that left one out would state half the rule. A gap criterion reads `gap to
/// range > critical(pass n, at 3, 4, 5, 6, 8) * 1/100`. The deviation is `abs(...)`,
/// never bars, outside LaTeX: a bar inside a Markdown table cell ends the cell;
/// the Markdown guard checks it. In LaTeX the parentheses are plain,
/// not `\left(`...`\right)`: TeX never breaks a line inside that pair, and
/// the `\allowbreak` after each `;` is what lets so long a formula wrap.
template <Dialect D,
          PerPass P,
          OnLimit L,
          typename AtMostT,
          typename KeepAtLeastT,
          typename S,
          typename Criterion,
          Vocabulary V>
[[nodiscard]] std::string render_node(RejectionNode<P, L, AtMostT, KeepAtLeastT, S, Criterion> const& node,
                                      V const& vocabulary)
{
    constexpr bool latex = D == Dialect::LaTeX;
    constexpr bool inStddevs = Criterion::kind == CriterionKind::DeviationInStddevs;
    std::string const deviationText =
        latex ? std::string { "\\left\\lvert x - \\bar{x}_{\\text{pass}}\\right\\rvert" } : std::string { "abs(x - pass mean)" };
    constexpr bool gapToRange = Criterion::kind == CriterionKind::GapToRange;
    std::string const statisticText = gapToRange
                                          ? (latex ? std::string { "\\text{gap to range}" } : std::string { "gap to range" })
                                      : inStddevs ? (latex ? "\\frac{" + deviationText + "}{s}" : deviationText + " / s")
                                                  : deviationText;
    std::string const comparison = L == OnLimit::Keep ? " > " : (latex ? " \\geq " : " >= ");
    std::string const between = latex ? ";\\allowbreak " : "; ";
    auto const inWords = [](std::string_view phrase) {
        return latex ? "\\text{" + std::string { phrase } + "}" : std::string { phrase };
    };
    std::string const perPass = inWords(P == PerPass::MostExtreme ? "most extreme per pass" : "every exceeding per pass");
    std::string const onLimit = inWords(L == OnLimit::Keep ? "keep on limit" : "reject on limit");
    std::string const atMost = latex ? "\\text{at most }" + std::to_string(detail::bound_value<AtMostT>)
                                     : "at most " + std::to_string(detail::bound_value<AtMostT>);
    std::string const keepAtLeast = latex ? "\\text{keep at least }" + std::to_string(detail::bound_value<KeepAtLeastT>)
                                          : "keep at least " + std::to_string(detail::bound_value<KeepAtLeastT>);
    std::string const inside = render<D>(node.sample, vocabulary) + between + statisticText + comparison
                               + render<D>(node.criterion.limit, vocabulary) + between + perPass + between + onLimit
                               + between + atMost + between + keepAtLeast;
    if constexpr (latex)
        return "\\operatorname{without\\ outliers}(" + inside + ")";
    else
        return "without outliers(" + inside + ")";
}

/// A precision limit renders as its symbol applied to its limit expression,
/// with the level it is evaluated at stated beside it:
/// `r(0.1 g + 1/50 * level; level = (x_A + x_B) / 2)`, `R(...)` for
/// reproducibility, and in LaTeX
/// `r\left(... \right)\Big\vert_{\text{level} = ...}`, the evaluation bar
/// typeset clean under MathJax 3.2.2 and tectonic by a spike -- spelt
/// `\vert`, not `|`, so that no `|` reaches a Markdown table cell (see the
/// absolute value's `render_node`).
///
/// **Both passes are on the page.** A reader must be able to see that the
/// limit depends on the results it checks -- the level is written out, not
/// named -- and which expression is the level, so that a rounding the
/// author put on it is visible. `;` separates the two because a comma
/// already separates a call's arguments, and a level expression may hold
/// calls of its own.
template <Dialect D, PrecisionKind K, Node Level, Node Limit, Vocabulary V>
[[nodiscard]] std::string render_node(PrecisionLimitNode<K, Level, Limit> const& node, V const& vocabulary)
{
    std::string const symbolText { detail::precision_render_symbol(K) };
    std::string const limitText = render<D>(node.limit, vocabulary);
    std::string const levelText = render<D>(node.level, vocabulary);
    if constexpr (D == Dialect::LaTeX)
        return symbolText + "\\left(" + limitText + "\\right)\\Big\\vert_{\\text{level} = " + levelText + "}";
    else
        return symbolText + "(" + limitText + "; level = " + levelText + ")";
}

namespace detail
{
    /// One input of an opaque call as its call writes it: a pairing curve as
    /// its two series, points then values -- the two spans `compute` receives
    /// -- and any other input as itself.
    template <Dialect D, typename Input, Vocabulary V>
    [[nodiscard]] std::string opaque_argument_text(Input const& input, V const& vocabulary)
    {
        if constexpr (requires { input.domainSeries; input.valueSeries; })
            return render<D>(input.domainSeries, vocabulary) + ", " + render<D>(input.valueSeries, vocabulary);
        else
            return render<D>(input, vocabulary);
    }
} // namespace detail

/// The attempt number renders as `k`, the method's own counter --
/// backtick-quoted in Markdown, as a variable is.
template <Dialect D, Vocabulary V>
[[nodiscard]] std::string render_node(AttemptNumberNode const&, V const&)
{
    if constexpr (D == Dialect::Markdown)
        return "`k`";
    else
        return "k";
}

/// The previous attempt's value renders as the result's symbol under
/// @p vocabulary, marked `k-1` (`detail::attempt_marker`): `w(k-1)`.
template <Dialect D, Described R, Vocabulary V>
[[nodiscard]] std::string render_node(PreviousAttemptNode<R> const&, V const& vocabulary)
{
    return detail::attempt_marker<D>(std::string { symbol_of<R>(vocabulary) }, "k-1");
}

/// This attempt's value renders as the result's symbol under @p vocabulary,
/// marked `k` (`detail::attempt_marker`): `w(k)`.
template <Dialect D, Described R, Vocabulary V>
[[nodiscard]] std::string render_node(ThisAttemptNode<R> const&, V const& vocabulary)
{
    return detail::attempt_marker<D>(std::string { symbol_of<R>(vocabulary) }, "k");
}

/// The determination recorded for the attempt that is running renders as
/// its quantity's symbol under @p vocabulary, marked `k` as this attempt's
/// value is: `d(k)`, `{d}_{k}`.
template <Dialect D, Described Q, Vocabulary V>
[[nodiscard]] std::string render_node(AttemptInputNode<Q> const&, V const& vocabulary)
{
    return detail::attempt_marker<D>(std::string { symbol_of<Q>(vocabulary) }, "k");
}
/// An opaque output renders as a call to its operation, named as the
/// operation names itself, selecting the output: `linear least squares(t(i),
/// L(i)).slope` in plain text; in Markdown the same, each symbol in its own
/// code span as a series marker writes it; and in LaTeX
/// `\text{linear least squares}({t}_{i}, {L}_{i})_{\text{slope}}`.
///
/// The spellings are phase 15's spike's (step 9), measured under MathJax 3.2.2
/// with the site's configuration, tectonic 0.17.0 with `[OT1]{fontenc}` and
/// python-markdown 3.10.3. The names go in as written: an operation's name and
/// output names hold only ASCII letters, digits and single spaces
/// (`RequireOpaqueNameReadable`), which every dialect shows as they are. The
/// operation's name is its own text, like `numeric(...)`, and no vocabulary
/// renames it; its inputs' symbols follow the vocabulary.
template <Dialect D, std::size_t I, typename Op, typename... Inputs, typename Origin, Vocabulary V>
[[nodiscard]] std::string render_node(OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, Origin> const& node, V const& vocabulary)
{
    std::string arguments;
    std::apply(
        [&](auto const&... inputs) {
            ((arguments += (arguments.empty() ? "" : ", ") + detail::opaque_argument_text<D>(inputs, vocabulary)), ...);
        },
        node.call.inputs);
    std::string const operationName { Op::name };
    std::string const outputName { OpaqueOutputNode<I, OpaqueCall<Op, Inputs...>, Origin>::output };
    if constexpr (D == Dialect::LaTeX)
        return "\\text{" + operationName + "}(" + arguments + ")_{\\text{" + outputName + "}}";
    else
        return operationName + "(" + arguments + ")." + outputName;
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
template <Dialect D, Comparison Op, Node Left, Node Right, Vocabulary V>
[[nodiscard]] std::string render_node(PredicateNode<Op, Left, Right> const& node, V const& vocabulary)
{
    constexpr detail::Precedence operandContext =
        static_cast<detail::Precedence>(static_cast<int>(detail::PrecedenceOf<PredicateNode<Op, Left, Right>>::value) + 1);
    std::string const leftText = detail::render_operand<D>(node.lhs, operandContext, vocabulary);
    std::string const rightText = detail::render_operand<D>(node.rhs, operandContext, vocabulary);

    char const* const comparisonToken = [] {
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

    return leftText + " " + comparisonToken + " " + rightText;
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
/// they did: "the two replicates shall agree within 4.7%" names no consequence
/// at all, while "reject the specimen below 27.3 MPa" folds one in. A standard
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
template <Dialect D, Predicate P, Vocabulary V>
[[nodiscard]] std::string render_node(Constraint<P> const& node, V const& vocabulary)
{
    std::string const predicateText = render<D>(node.predicate, vocabulary);
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
///
/// **One else branch is bracketed: a read from another record.** Its words
/// `of <role>` trail it, and a reader of `if p then a else b of Reference`
/// may attach them to the whole conditional -- the reading
/// `(if p then a else b) of Reference`, which is a different formula, taking
/// the predicate and the then branch from that record too. So it reads
/// `if p then a else (b of Reference)`.
template <Dialect D, Predicate P, Node Then, Node Else, Vocabulary V>
[[nodiscard]] std::string render_node(WhenNode<P, Then, Else> const& node, V const& vocabulary)
{
    std::string const predicateText = render<D>(node.predicate, vocabulary);
    std::string const thenText = D == Dialect::LaTeX
                                     ? render<D>(node.thenBranch, vocabulary)
                                     : detail::render_operand<D>(node.thenBranch, detail::Precedence::Additive, vocabulary);
    std::string const elseText = D != Dialect::LaTeX && detail::isRecordScope<Else>
                                     ? "(" + render<D>(node.elseBranch, vocabulary) + ")"
                                     : render<D>(node.elseBranch, vocabulary);

    if constexpr (D == Dialect::LaTeX)
        return "\\begin{cases} " + thenText + " & \\text{if } " + predicateText + " \\\\ " + elseText
               + " & \\text{otherwise} \\end{cases}";
    else
        return "if " + predicateText + " then " + thenText + " else " + elseText;
}

/// A read from another record renders as its operand and the words `of
/// <role>`: `f_c of Reference`, and `(F / A) of Reference` when the operand is
/// more than one symbol -- bracketed, `\left(...\right)` in LaTeX, so that
/// the role is read as qualifying the whole computation.
///
/// The role's name is author text (`tag_name<Role>()`), and identifier-like:
/// ASCII letters, digits, underscores and single spaces, or the scope is
/// refused (`RequireIdentifierLikeRoleName`, `record.hpp`). Of those, only
/// `_` and the space need anything, per dialect: as-is in Plain; through the
/// author-words escaping lookup keys use in Markdown; and in LaTeX in math
/// mode, `\ \text{of }\mathrm{...}`, through `detail::latex_math_words` --
/// not inside `\text{}`, where the site's MathJax shows a text-mode escape
/// literally.
///
/// A lineage requirement is not rendered: it gates whether the value is read,
/// and the trace records every attribute it compared.
template <Dialect D, typename Role, typename Requirement, Node Operand, Vocabulary V>
[[nodiscard]] std::string render_node(RecordScopeNode<Role, Requirement, Operand> const& node, V const& vocabulary)
{
    std::string const operandText = render<D>(node.operand, vocabulary);
    bool const bracketed = static_cast<int>(detail::precedence_of(node.operand))
                           < static_cast<int>(detail::Precedence::Atom);
    constexpr std::string_view roleName = tag_name<Role>();
    if constexpr (D == Dialect::LaTeX)
        return (bracketed ? "\\left(" + operandText + "\\right)" : operandText) + "\\ \\text{of }\\mathrm{"
               + detail::latex_math_words(roleName) + "}";
    else
        return (bracketed ? "(" + operandText + ")" : operandText) + " of "
               + detail::literal_words_in_dialect<D>(roleName);
}

/// A refused series-valued read from another record
/// (`detail::RefusedSeriesScope`) renders as a refused series does, as
/// nothing a reader could take for a formula. A program holding one never
/// compiles; this only keeps a `render` of it from adding a second,
/// compiler-worded error to the refusal.
template <Dialect D, typename Operand, Vocabulary V>
[[nodiscard]] std::string render_node(detail::RefusedSeriesScope<Operand> const&, V const&)
{
    return "(refused)";
}

namespace detail
{
    /// Renders @p node through the `render_node` it has, and refuses a node of
    /// this library's whose `render_node` would not take the vocabulary.
    ///
    /// **This library's nodes** all render through the two-argument form,
    /// `render_node(node, vocabulary)`. Which overloads are this library's is
    /// answered by *qualified* lookup, `::formula::render_node`: that finds
    /// only the overloads declared in `formula` before this point, which is
    /// every one this library defines, and never a consumer's -- a consumer's
    /// lives in their own namespace, and is found only by the unqualified,
    /// argument-dependent calls. A one-argument overload of this library's is
    /// refused outright: it would render its node's operands in the declared
    /// symbols under every vocabulary, a defect found
    /// waiting for the join with derived and replaced variants, and one no
    /// test that lacks such a node would see.
    ///
    /// **A consumer's nodes** keep the extension point every earlier phase
    /// published, `template <Dialect D> std::string render_node(TheirNode
    /// const&)`. Passing the vocabulary as a second argument would leave every
    /// such overload unreachable -- measured by the phase-11 spike on clang++
    /// 20.1.8 ("no matching function for call to 'render_node'"), and again on
    /// cl 19.51 by deleting the fallback below (C2672). So, in order:
    ///
    ///  1. a two-argument overload that is not this library's -- the consumer
    ///     opted in -- is called with the vocabulary;
    ///  2. otherwise a one-argument overload, which by then can only be the
    ///     consumer's, is called without it. `sink.hpp`'s `detail::dispatch`
    ///     makes the same two-step choice for evaluation;
    ///  3. otherwise this library's two-argument overload is called.
    ///
    /// Step 2 comes before step 3 for a consumer's node that **derives from
    /// one of this library's**, `struct Labelled: formula::VarNode<Q>`: it
    /// renders through its own one-argument overload, as it did before
    /// vocabularies existed, rather than as the base it derives from. That
    /// node's own text is then in the declared symbols; to receive the
    /// vocabulary it defines the two-argument form **instead**. Were it to
    /// define both, the one-argument form would win, because its two-argument
    /// overload cannot be told apart from its base's by lookup alone.
    ///
    /// **What the fallback cannot do** is carry the vocabulary into a
    /// one-argument overload: whatever such a node renders of its own, and any
    /// operand it renders with `render<D>(operand)`, is written in the default
    /// vocabulary. A consumer who wants their node's operands renamed writes
    /// `template <Dialect D, Vocabulary V> std::string render_node(TheirNode
    /// const&, V const& vocabulary)`, and hands the vocabulary on with
    /// `render<D>(operand, vocabulary)`.
    template <Dialect D, typename N, Vocabulary V>
    [[nodiscard]] std::string render_in_vocabulary(N const& node, V const& vocabulary)
    {
        constexpr bool libraryTakesVocabulary = requires { ::formula::render_node<D>(node, vocabulary); };
        constexpr bool libraryIgnoresVocabulary = requires { ::formula::render_node<D>(node); };
        static_assert(!libraryIgnoresVocabulary,
                      "formula: a render_node of this library takes no vocabulary, so it would render this node "
                      "in the declared symbols under every vocabulary. Give it the second parameter, "
                      "Vocabulary V const& vocabulary, and hand it on to every operand it renders");

        if constexpr (!libraryTakesVocabulary && requires { render_node<D>(node, vocabulary); })
            return render_node<D>(node, vocabulary);
        else if constexpr (requires { render_node<D>(node); })
            return render_node<D>(node);
        else
            return render_node<D>(node, vocabulary);
    }
} // namespace detail

/// Renders @p node in dialect @p D, writing symbols as @p vocabulary says.
template <Dialect D, Node N, Vocabulary V>
[[nodiscard]] std::string render(N const& node, V const& vocabulary)
{
    return detail::render_in_vocabulary<D>(node, vocabulary);
}

/// Renders @p node as plain text, writing symbols as @p vocabulary says.
template <Node N, Vocabulary V>
[[nodiscard]] std::string render(N const& node, V const& vocabulary)
{
    return render<Dialect::Plain>(node, vocabulary);
}

/// Renders @p node in dialect @p D.
template <Dialect D, Node N>
[[nodiscard]] std::string render(N const& node)
{
    return render<D>(node, DefaultVocabulary {});
}

/// Renders @p node as plain text.
template <Node N>
[[nodiscard]] std::string render(N const& node)
{
    return render<Dialect::Plain>(node);
}

/// Renders the series @p node in dialect @p D, writing symbols as
/// @p vocabulary says.
template <Dialect D, SeriesNode S, Vocabulary V>
[[nodiscard]] std::string render(S const& node, V const& vocabulary)
{
    return detail::render_in_vocabulary<D>(node, vocabulary);
}

/// Renders the sample transformer @p node in dialect @p D, writing symbols as
/// @p vocabulary says.
template <Dialect D, typename R, Vocabulary V>
    requires detail::is_sample_transformer<R>
[[nodiscard]] std::string render(R const& node, V const& vocabulary)
{
    return detail::render_in_vocabulary<D>(node, vocabulary);
}

/// Renders the raw observations @p node in dialect @p D, writing symbols as
/// @p vocabulary says.
template <Dialect D, ObservationsNode O, Vocabulary V>
[[nodiscard]] std::string render(O const& node, V const& vocabulary)
{
    return detail::render_in_vocabulary<D>(node, vocabulary);
}

/// Renders the sample transformer @p node as plain text, writing symbols as
/// @p vocabulary says.
template <typename R, Vocabulary V>
    requires detail::is_sample_transformer<R>
[[nodiscard]] std::string render(R const& node, V const& vocabulary)
{
    return render<Dialect::Plain>(node, vocabulary);
}

/// Renders the sample transformer @p node as plain text.
template <typename R>
    requires detail::is_sample_transformer<R>
[[nodiscard]] std::string render(R const& node)
{
    return render<Dialect::Plain>(node, DefaultVocabulary {});
}

/// Renders the sample transformer @p node in dialect @p D.
template <Dialect D, typename R>
    requires detail::is_sample_transformer<R>
[[nodiscard]] std::string render(R const& node)
{
    return render<D>(node, DefaultVocabulary {});
}

/// Renders the series @p node as plain text, writing symbols as
/// @p vocabulary says.
template <SeriesNode S, Vocabulary V>
[[nodiscard]] std::string render(S const& node, V const& vocabulary)
{
    return render<Dialect::Plain>(node, vocabulary);
}

/// Renders the series @p node in dialect @p D.
template <Dialect D, SeriesNode S>
[[nodiscard]] std::string render(S const& node)
{
    return render<D>(node, DefaultVocabulary {});
}

/// Renders the series @p node as plain text.
template <SeriesNode S>
[[nodiscard]] std::string render(S const& node)
{
    return render<Dialect::Plain>(node);
}

/// Renders the curve @p node in dialect @p D, writing symbols as
/// @p vocabulary says.
template <Dialect D, CurveExpression C, Vocabulary V>
[[nodiscard]] std::string render(C const& node, V const& vocabulary)
{
    return detail::render_in_vocabulary<D>(node, vocabulary);
}

/// Renders the curve @p node as plain text, writing symbols as
/// @p vocabulary says.
template <CurveExpression C, Vocabulary V>
[[nodiscard]] std::string render(C const& node, V const& vocabulary)
{
    return render<Dialect::Plain>(node, vocabulary);
}

/// Renders the curve @p node in dialect @p D.
template <Dialect D, CurveExpression C>
[[nodiscard]] std::string render(C const& node)
{
    return render<D>(node, DefaultVocabulary {});
}

/// Renders the curve @p node as plain text.
template <CurveExpression C>
[[nodiscard]] std::string render(C const& node)
{
    return render<Dialect::Plain>(node);
}

/// Renders @p node in dialect @p D, writing symbols as @p vocabulary says.
/// See the forward declaration above for why this overload -- for
/// `Predicate`, not `Node` -- exists separately.
template <Dialect D, Predicate P, Vocabulary V>
[[nodiscard]] std::string render(P const& node, V const& vocabulary)
{
    return detail::render_in_vocabulary<D>(node, vocabulary);
}

/// Renders @p node as plain text, writing symbols as @p vocabulary says.
template <Predicate P, Vocabulary V>
[[nodiscard]] std::string render(P const& node, V const& vocabulary)
{
    return render<Dialect::Plain>(node, vocabulary);
}

/// Renders @p node in dialect @p D. See the forward declaration above for why
/// this overload -- for `Predicate`, not `Node` -- exists separately.
template <Dialect D, Predicate P>
[[nodiscard]] std::string render(P const& node)
{
    return render<D>(node, DefaultVocabulary {});
}

/// Renders @p node as plain text.
template <Predicate P>
[[nodiscard]] std::string render(P const& node)
{
    return render<Dialect::Plain>(node);
}

/// Renders @p node in dialect @p D, writing symbols as @p vocabulary says.
/// See the forward declaration above for why this overload -- for
/// `Constraint`, not `Node` or `Predicate` -- exists separately.
template <Dialect D, Predicate P, Vocabulary V>
[[nodiscard]] std::string render(Constraint<P> const& node, V const& vocabulary)
{
    return detail::render_in_vocabulary<D>(node, vocabulary);
}

/// Renders @p node as plain text, writing symbols as @p vocabulary says.
template <Predicate P, Vocabulary V>
[[nodiscard]] std::string render(Constraint<P> const& node, V const& vocabulary)
{
    return render<Dialect::Plain>(node, vocabulary);
}

/// Renders @p node in dialect @p D. See the forward declaration above for why
/// this overload -- for `Constraint`, not `Node` or `Predicate` -- exists
/// separately.
template <Dialect D, Predicate P>
[[nodiscard]] std::string render(Constraint<P> const& node)
{
    return render<D>(node, DefaultVocabulary {});
}

/// Renders @p node as plain text.
template <Predicate P>
[[nodiscard]] std::string render(Constraint<P> const& node)
{
    return render<Dialect::Plain>(node);
}

namespace detail
{
    /// One row of an envelope as the range it permits, the unit after the
    /// last number: `from 30 to 40 %`, `at least 60 %`, `at most 5 mm`, or
    /// `any value` for a row unbounded on both sides.
    ///
    /// @p unitSymbol is @p limitsIn's symbol as the caller writes it -- the
    /// trace escapes it, `render()` does not -- and @p limitsIn is the unit
    /// the limits are numbers in. Spelled in `numberStyle.exact_only()`: a
    /// limit is one side of the comparison a check states, and is never shown
    /// rounded.
    [[nodiscard]] inline std::string limit_row_text(LimitRow limitRow,
                                                    std::string_view unitSymbol,
                                                    Unit const& limitsIn,
                                                    NumberStyle numberStyle)
    {
        NumberStyle const limitStyle = numberStyle.exact_only();
        std::optional<Rational> const lowerValue = limitRow.lower.value();
        std::optional<Rational> const upperValue = limitRow.upper.value();
        if (lowerValue.has_value() && upperValue.has_value())
            return "from " + styled_number_text(*lowerValue, limitStyle, limitsIn) + " to "
                   + number_with_unit(styled_number_text(*upperValue, limitStyle, limitsIn), unitSymbol);
        if (lowerValue.has_value())
            return "at least " + number_with_unit(styled_number_text(*lowerValue, limitStyle, limitsIn), unitSymbol);
        if (upperValue.has_value())
            return "at most " + number_with_unit(styled_number_text(*upperValue, limitStyle, limitsIn), unitSymbol);
        return "any value";
    }
} // namespace detail

/// Renders a conformity check in dialect @p D: `conform(<subject>, <row>,
/// ...)`, one field per element in the series' order, each the range it
/// permits (`detail::limit_row_text`), shaped as a lookup is
/// (`detail::lookup_call`). The subject carries its series marker.
///
/// **The verdict stays out**, for `Constraint`'s reason: it is what a checker
/// does once each element is decided, not part of what is checked. It
/// appears in the trace.
template <Dialect D, Unit U, SeriesNode S, Vocabulary V>
[[nodiscard]] std::string render(Conformity<U, S> const& conformityCheck, V const& vocabulary)
{
    constexpr Unit limitsIn = U;
    NumberStyle const limitStyle = typed_number_style(vocabulary);
    std::string rowFields;
    for (std::size_t at = 0; at < S::length; ++at)
        rowFields += detail::lookup_separator<D>()
                     + detail::lookup_words_in_dialect<D>(detail::limit_row_text(
                         conformityCheck.envelope[at], view(limitsIn.symbolText), limitsIn, limitStyle));
    return detail::lookup_call<D>("conform", render<D>(conformityCheck.subject, vocabulary), rowFields);
}

/// Renders a conformity check as plain text, writing symbols as @p vocabulary
/// says.
template <Unit U, SeriesNode S, Vocabulary V>
[[nodiscard]] std::string render(Conformity<U, S> const& node, V const& vocabulary)
{
    return render<Dialect::Plain>(node, vocabulary);
}

/// Renders a conformity check in dialect @p D.
template <Dialect D, Unit U, SeriesNode S>
[[nodiscard]] std::string render(Conformity<U, S> const& node)
{
    return render<D>(node, DefaultVocabulary {});
}

/// Renders a conformity check as plain text.
template <Unit U, SeriesNode S>
[[nodiscard]] std::string render(Conformity<U, S> const& node)
{
    return render<Dialect::Plain>(node);
}

namespace detail
{
    /// @p sentence set word by word in LaTeX: each `\mathrm{...}`, escaped by
    /// `latex_math_words`, and joined by `\ \allowbreak `, so that a line may
    /// break between any two -- a whole sentence in one `\mathrm{...}` is one
    /// atom, which never breaks. For a retry's words and its verdict.
    [[nodiscard]] inline std::string latex_breakable_words(std::string_view sentence)
    {
        std::string brokenWords;
        std::size_t wordStart = 0;
        while (wordStart <= sentence.size())
        {
            std::size_t wordEnd = sentence.find(' ', wordStart);
            if (wordEnd == std::string_view::npos)
                wordEnd = sentence.size();
            if (wordEnd > wordStart)
                brokenWords += (brokenWords.empty() ? "" : "\\ \\allowbreak ") + std::string { "\\mathrm{" }
                               + latex_math_words(sentence.substr(wordStart, wordEnd - wordStart)) + "}";
            wordStart = wordEnd + 1;
        }
        return brokenWords;
    }
} // namespace detail
/// Renders a retry in dialect @p D, writing symbols as @p vocabulary says:
/// `up to 4 attempts: w(k) = 152/25 g + w(k-1) / 2, starting from w(0) = 0 g;
/// accept when w(k-1) - w(k) >= -19/25 g; otherwise: repeat the
/// determination`. The words are this library's, set in `\mathrm{...}` in
/// LaTeX as a lookup's are; the verdict is author text, made literal in
/// Markdown and escaped in LaTeX. A retry judged from its second attempt
/// says so: `accept from attempt 2 when ...`.
template <Dialect D, Described R, std::size_t Max, FirstJudged J, typename Start, typename A, typename P, Vocabulary V>
[[nodiscard]] std::string render(Retry<R, Max, J, Start, A, P> const& retrying, V const& vocabulary)
{
    std::string const resultSymbol { symbol_of<R>(vocabulary) };
    auto const inDialect = [](std::string const& libraryWords) {
        if constexpr (D == Dialect::LaTeX)
            return detail::latex_breakable_words(libraryWords);
        else
            return libraryWords;
    };
    // `\allowbreak` after each clause, as `lookup_separator` gives a lookup's
    // fields, and between words (`latex_breakable_words`): a sentence of
    // `\mathrm{...}` atoms otherwise has no legal break. Measured with
    // tectonic 0.17.0: a ten-word verdict overflowed the line by 152 pt, and
    // the clauses by 11 pt, in one atom each; split, nothing overflows.
    std::string const clauseSeparator = D == Dialect::LaTeX ? std::string { ";\\ \\allowbreak " } : std::string { "; " };
    std::string renderedRetry = inDialect("up to " + std::to_string(Max) + (Max == 1 ? " attempt:" : " attempts:"))
                                + (D == Dialect::LaTeX ? "\\ " : " ") + detail::attempt_marker<D>(resultSymbol, "k") + " = "
                                + render<D>(retrying.attempt, vocabulary);
    if constexpr (detail::StartTraits<Start>::states)
        renderedRetry += (D == Dialect::LaTeX ? ",\\ \\allowbreak " : ", ") + inDialect("starting from")
                         + (D == Dialect::LaTeX ? "\\ " : " ") + detail::attempt_marker<D>(resultSymbol, "0") + " = "
                         + render<D>(retrying.start.expression, vocabulary);
    renderedRetry += clauseSeparator
                     + inDialect(J == FirstJudged::AtSecondAttempt ? std::string { "accept from attempt 2 when" }
                                                                   : std::string { "accept when" })
                     + (D == Dialect::LaTeX ? "\\ " : " ") + render<D>(retrying.accept, vocabulary);
    std::string verdictText;
    if constexpr (D == Dialect::LaTeX)
        verdictText = detail::latex_breakable_words(retrying.onExhausted.label);
    else if constexpr (D == Dialect::Markdown)
        verdictText = detail::literal_words_in_dialect<D>(retrying.onExhausted.label);
    else
        verdictText = std::string { retrying.onExhausted.label };
    return renderedRetry + clauseSeparator + inDialect("otherwise:") + (D == Dialect::LaTeX ? "\\ " : " ") + verdictText;
}

/// Renders a retry as plain text, writing symbols as @p vocabulary says.
template <Described R, std::size_t Max, FirstJudged J, typename Start, typename A, typename P, Vocabulary V>
[[nodiscard]] std::string render(Retry<R, Max, J, Start, A, P> const& node, V const& vocabulary)
{
    return render<Dialect::Plain>(node, vocabulary);
}

/// Renders a retry in dialect @p D, in the default vocabulary.
template <Dialect D, Described R, std::size_t Max, FirstJudged J, typename Start, typename A, typename P>
[[nodiscard]] std::string render(Retry<R, Max, J, Start, A, P> const& node)
{
    return render<D>(node, DefaultVocabulary {});
}

/// Renders a retry as plain text.
template <Described R, std::size_t Max, FirstJudged J, typename Start, typename A, typename P>
[[nodiscard]] std::string render(Retry<R, Max, J, Start, A, P> const& node)
{
    return render<Dialect::Plain>(node);
}

namespace detail
{
    /// Each definition of the calculation of @p Ds as its line, `symbol =
    /// expression`, in the order the definitions were given; @p Is counts
    /// them.
    template <Dialect D, typename... Ds, Vocabulary V, std::size_t... Is>
    [[nodiscard]] std::array<std::string, sizeof...(Ds)> definition_lines(Calculation<Ds...> const& definitionSet,
                                                                          V const& vocabulary,
                                                                          std::index_sequence<Is...>)
    {
        return { (render<D>(var<typename Ds::quantity>, vocabulary) + " = "
                  + render<D>(std::get<Is>(definitionSet.definitions).expression, vocabulary))... };
    }

    /// @p symbolText as a quoted identifier of the DOT language: between
    /// double quotes, each `"` and `\` in it preceded by a `\`.
    [[nodiscard]] inline std::string dot_quoted(std::string_view symbolText)
    {
        std::string quoted = "\"";
        for (char const glyph: symbolText)
        {
            if (glyph == '"' || glyph == '\\')
                quoted += '\\';
            quoted += glyph;
        }
        return quoted + "\"";
    }
} // namespace detail

/// Renders the calculation @p definitionSet in dialect @p D, writing every
/// symbol as @p vocabulary says: one line per definition, `symbol =
/// expression`, in the order the calculation calculates them -- each after
/// the values it reads. The lines are separated by a newline, and by a blank
/// line in Markdown, where lines a single newline apart run on as one
/// paragraph; the last has none after it. In LaTeX too they are separated by
/// a plain newline, which a typeset LaTeX document does not break on: the
/// caller wraps the lines in an environment that does, or splits them.
/// Nothing for a calculation refused where it was written.
template <Dialect D, typename... Ds, Vocabulary V>
[[nodiscard]] std::string render(Calculation<Ds...> const& definitionSet, V const& vocabulary)
{
    using Graph = detail::CalculationGraph<Ds...>;
    std::string renderedCalculation;
    if constexpr (Graph::valid)
    {
        std::array<std::string, sizeof...(Ds)> const definitionLines =
            detail::definition_lines<D>(definitionSet, vocabulary, std::index_sequence_for<Ds...> {});
        for (std::size_t placed = Graph::inputCount; placed < Graph::slotCount; ++placed)
        {
            if (!renderedCalculation.empty())
                renderedCalculation += D == Dialect::Markdown ? "\n\n" : "\n";
            renderedCalculation += definitionLines[Graph::order[placed] - Graph::inputCount];
        }
    }
    return renderedCalculation;
}

/// Renders a calculation as plain text, writing symbols as @p vocabulary
/// says.
template <typename... Ds, Vocabulary V>
[[nodiscard]] std::string render(Calculation<Ds...> const& node, V const& vocabulary)
{
    return render<Dialect::Plain>(node, vocabulary);
}

/// Renders a calculation in dialect @p D, in the default vocabulary.
template <Dialect D, typename... Ds>
[[nodiscard]] std::string render(Calculation<Ds...> const& node)
{
    return render<D>(node, DefaultVocabulary {});
}

/// Renders a calculation as plain text.
template <typename... Ds>
[[nodiscard]] std::string render(Calculation<Ds...> const& node)
{
    return render<Dialect::Plain>(node);
}

/// The dependency graph of @p definitionSet as plain text, every symbol
/// written as @p vocabulary says: a first line naming the inputs, in the
/// order the calculation numbers them -- `inputs: fridge_w, fridge_h` --
/// and then one line per defined quantity, in the order it is calculated
/// in, naming what it reads, in that order too -- `fridge_kwh <- fridge_h,
/// fridge_kw`. The symbol each such line begins with is padded with spaces
/// to the longest of them, so that the arrows line up. The padding counts
/// bytes, so a symbol with a character UTF-8 spells in several bytes, such
/// as `σ`, is padded short. `none` stands for no input, and `nothing` for a
/// definition that reads nothing. Every line ends in a newline. Nothing for
/// a calculation refused where it was written.
template <typename... Ds, Vocabulary V = DefaultVocabulary>
[[nodiscard]] std::string describe_graph(Calculation<Ds...> const&, V const& vocabulary = V {})
{
    using Graph = detail::CalculationGraph<Ds...>;
    std::string graphText;
    if constexpr (Graph::valid)
    {
        std::array<std::string_view, Graph::slotCount> const everySymbol =
            detail::slot_symbols(static_cast<typename Graph::slots const*>(nullptr), vocabulary);
        graphText += "inputs: ";
        for (std::size_t placed = 0; placed < Graph::inputCount; ++placed)
            graphText += std::string { placed == 0 ? "" : ", " } + std::string { everySymbol[Graph::order[placed]] };
        if (Graph::inputCount == 0)
            graphText += "none";
        graphText += "\n";

        std::size_t widestSymbol = 0;
        for (std::size_t placed = Graph::inputCount; placed < Graph::slotCount; ++placed)
            if (everySymbol[Graph::order[placed]].size() > widestSymbol)
                widestSymbol = everySymbol[Graph::order[placed]].size();
        for (std::size_t placed = Graph::inputCount; placed < Graph::slotCount; ++placed)
        {
            std::size_t const definedSlot = Graph::order[placed];
            std::string definedLine { everySymbol[definedSlot] };
            definedLine.resize(widestSymbol, ' ');
            definedLine += " <- ";
            bool readsAny = false;
            for (std::size_t readPlace = 0; readPlace < Graph::slotCount; ++readPlace)
            {
                std::size_t const readSlot = Graph::order[readPlace];
                if (((Graph::reads[definedSlot] >> readSlot) & 1u) == 0)
                    continue;
                definedLine += std::string { readsAny ? ", " : "" } + std::string { everySymbol[readSlot] };
                readsAny = true;
            }
            if (!readsAny)
                definedLine += "nothing";
            graphText += definedLine + "\n";
        }
    }
    return graphText;
}

/// The dependency graph of @p definitionSet in the DOT language of Graphviz,
/// every symbol written as @p vocabulary says: `dot -Tsvg` draws it, the
/// inputs on the left as boxes and the calculated values as ellipses, each
/// arrow from a value to one that reads it. It opens `digraph calculation {`,
/// then `rankdir=LR;` and `node [fontname="Helvetica"];`, then one line per
/// node -- `q0 [label="x_0", shape=box];` -- and one per arrow -- `q0 ->
/// q1;` -- each indented by two spaces, and closes with `}`; every line ends
/// in a newline.
///
/// Each node is named by its quantity's position among the calculation's
/// quantities, `q0` for the first, and labelled with its symbol, so that two
/// quantities written alike stay two nodes; a `"` or `\` in a symbol is
/// preceded by a `\`. The nodes come in the order the quantities are
/// calculated in, the inputs first, and the arrows by the value that reads,
/// in that order too; the arrows into one value come in the order the
/// values it reads are calculated in. Nothing for a calculation refused
/// where it was written.
template <typename... Ds, Vocabulary V = DefaultVocabulary>
[[nodiscard]] std::string to_dot(Calculation<Ds...> const&, V const& vocabulary = V {})
{
    using Graph = detail::CalculationGraph<Ds...>;
    std::string dotText;
    if constexpr (Graph::valid)
    {
        std::array<std::string_view, Graph::slotCount> const everySymbol =
            detail::slot_symbols(static_cast<typename Graph::slots const*>(nullptr), vocabulary);
        dotText += "digraph calculation {\n  rankdir=LR;\n  node [fontname=\"Helvetica\"];\n";
        for (std::size_t placed = 0; placed < Graph::slotCount; ++placed)
        {
            std::size_t const shownSlot = Graph::order[placed];
            dotText += "  q" + std::to_string(shownSlot) + " [label=" + detail::dot_quoted(everySymbol[shownSlot])
                       + (shownSlot < Graph::inputCount ? ", shape=box];\n" : ", shape=ellipse];\n");
        }
        for (std::size_t placed = Graph::inputCount; placed < Graph::slotCount; ++placed)
        {
            std::size_t const definedSlot = Graph::order[placed];
            for (std::size_t readPlace = 0; readPlace < Graph::slotCount; ++readPlace)
            {
                std::size_t const readSlot = Graph::order[readPlace];
                if (((Graph::reads[definedSlot] >> readSlot) & 1u) != 0)
                    dotText += "  q" + std::to_string(readSlot) + " -> q" + std::to_string(definedSlot) + ";\n";
            }
        }
        dotText += "}\n";
    }
    return dotText;
}

/// How `render()` and `document()` write a formula's numbers, for the
/// overloads that take it: under `{ .numbers = NumberStyle::exact_decimal() }`
/// a constant holding 863/1000 reads `0.863` where it read `863/1000`.
///
/// **A formula states its numbers as its author typed them.** Every number in
/// a formula's text was typed -- a constant, a table's bound or row, a
/// permitted value, a limit -- so it is written exactly, and never padded,
/// whatever `numbers` says. `NumberStyle::exact_decimal(DecimalPadding::Padded)`
/// and `NumberStyle::approximate_decimal(mode)` are accepted, and act here as
/// `NumberStyle::exact_decimal()`:
///
///  - an approximating style writes a number as `NumberStyle::exact_decimal()`
///    would, so `1/3` stays `1/3` and no `≈` appears in a formula;
///  - the style's padding is ignored, so a constant in a unit that declares
///    decimals reads as typed, `5 kJ`, and a pure number such as
///    `number(Rational { 1, 2 })` reads `0.5`, never `0.500`.
///
/// `typed_number_style(vocabulary)` (`vocabulary.hpp`) is that style: the one
/// every node of this library writes a formula's number in, and the one a
/// consumer's own `render_node` should write one in.
///
/// **A trace is different, on purpose.** Its lines state values in a column,
/// where a uniform number of decimals is what padding is for, so under
/// `Padded` a trace writes that same typed `5 kJ` as `5.0 kJ`
/// (`TraceRenderOptions::numbers`, `trace_render.hpp`). It pads only a value
/// in a unit that declares decimals: a pure number, whose unit (`unit::One`)
/// nobody declared, stays `0.5` there too. And a trace may show a computed
/// value rounded, which a formula has none of.
struct RenderOptions
{
    /// The notation of every number in the formula's text. The default,
    /// `NumberStyle::fraction()`, writes exactly what `render()` without
    /// options writes.
    NumberStyle numbers = NumberStyle::fraction();
};

/// Renders @p node in dialect @p D -- plain text unless one is named --
/// writing every symbol as @p vocabulary says and every number as
/// @p renderOptions says (`RenderOptions`). Takes whatever `render<D>(node,
/// vocabulary)` takes, and hands every node @p vocabulary carrying the style,
/// so that a consumer's own `render_node` reads it with `number_style_of`,
/// or with `typed_number_style` for a number its author typed.
template <Dialect D = Dialect::Plain, typename X, Vocabulary V>
    requires requires(X const& written, V const& writtenIn) { render<D>(written, writtenIn); }
[[nodiscard]] std::string render(X const& node, V const& vocabulary, RenderOptions renderOptions)
{
    return render<D>(node, detail::styled(vocabulary, renderOptions.numbers));
}
} // namespace formula
