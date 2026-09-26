// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Jurisdiction-scoped vocabularies: which symbol names which quantity, where.
///
/// The same written symbol can name different quantities in different
/// countries -- in the case that made this necessary, two jurisdictions use
/// one pair of symbols **crossed over**, each one's word meaning the other's
/// quantity. A page rendered with the wrong vocabulary is then not merely
/// unfamiliar, it states the wrong formula, so a symbol is resolved through a
/// vocabulary rather than read straight off `Describe<Q>::symbol`.
///
///     constexpr auto north = formula::vocabulary(formula::renames<Strength>("R"),
///                                                formula::renames<Modulus>("E"));
///     formula::render(f, north);                      // R / E
///     formula::document(f, north);                    // the symbol table agrees
///     formula::RecordingSink sink { trace, north };   // and so does the trace
///
/// **`Describe<Q>` is not changed, and is not bypassed.** A vocabulary wraps
/// it: every quantity it does not rename is written as `Describe<Q>::symbol`
/// says, and `DefaultVocabulary`, which renames nothing, is the default
/// argument of every surface that takes one -- so a caller who never names a
/// vocabulary gets exactly the text they got before one existed.
///
/// **Only the symbol changes, never the meaning.** A vocabulary renames how a
/// quantity is *written*; its description and its unit are properties of the
/// quantity itself and stay `Describe<Q>`'s. That is what makes the
/// crossed-over case come out right: `document()` under the northern
/// vocabulary lists `R` as the strength, and under the southern one lists `E`
/// as the strength -- the word moves, the quantity it names does not.
///
/// **The three surfaces that write a symbol all take one, and all three must
/// be given the same one:** `render()` and `document()` (which write it when
/// asked for text), and `RecordingSink` (`trace.hpp`), which writes a trace
/// step's symbol while the formula is being *evaluated*, long before anything
/// renders it. `render_trace` (`trace_render.hpp`) takes none: it reads the
/// symbol the sink already recorded. So a trace recorded without a vocabulary
/// cannot be rendered into one afterwards, and a page rendered with one does
/// not make the trace agree with it.
///
/// **A renamed symbol is written verbatim**, exactly as a `Describe` symbol
/// is: beyond refusing an empty one, one that is all whitespace, and one
/// holding a NUL, a square bracket or a control character (see `renames`),
/// nothing checks what it says, so a symbol containing other Markdown or LaTeX
/// markup reaches the page and the trace as written. The vocabulary is the author's data, like the
/// quantity declarations it wraps.
///
/// **What a vocabulary does not rename.** A variant's tag (`TagName`,
/// `tag.hpp`) and a lookup key's enumerator (`EnumeratorName`,
/// `enumerator.hpp`) are words with their own customisation traits, and they
/// name a case or a row, not a quantity; a vocabulary leaves both alone. Nor
/// does it change `Measured<Q>::quantity_symbol()` (`measured.hpp`), which
/// reports `Describe<Q>`'s metadata and renders nothing.

#include <formula-cpp/quantity.hpp>

#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>

/// `[[no_unique_address]]`, spelled for the compiler reading it: cl and
/// clang-cl ignore the standard spelling under the MSVC ABI, and honour
/// `[[msvc::no_unique_address]]` instead. Lets a member of an empty type --
/// `DefaultVocabulary` held by `RecordingSink` (`trace.hpp`) -- take no space.
#if defined(_MSC_VER)
    #define FORMULA_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
    #define FORMULA_NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

namespace formula
{

template <typename Q>
class Renames;

/// Writes @p Q as @p symbol, in a vocabulary built from it.
///
/// **`consteval`, and a character array rather than a `std::string_view`, on
/// purpose.** A trace step keeps the symbol as a `std::string_view` for as long
/// as the trace lives (`Step::symbol`, `trace.hpp`), which is only safe if the
/// characters live as long as the program -- the guarantee a `Describe`
/// symbol has. A `std::string_view` parameter would accept a view of a
/// `std::string` that dies at the end of the statement. Taken as an array
/// reference by an immediate function, the argument has to be usable in a
/// constant expression, which a string literal is and a buffer on the stack
/// is not. A buffer that is not `const` -- whose text could change after a
/// trace kept a view of it -- is refused by the overload below, in this
/// library's words, whatever its storage. Without that overload it would
/// still be refused, because this one reads the characters and a constant
/// expression cannot read an object that is not `const`, but in the
/// compiler's words (measured on cl 19.51 by deleting the overload).
///
/// **Refused, because a formula rendered with it cannot be read:**
///
///  - an empty symbol, `renames<Q>("")`;
///  - a symbol holding a NUL before its end, `renames<Q>("\0")`, or an array
///    with no terminating NUL -- see
///    `detail::renames_symbol_must_be_a_string_with_no_embedded_nul`;
///  - a symbol that is all whitespace -- see
///    `detail::renames_symbol_must_not_be_all_whitespace`;
///  - a symbol holding a square bracket or a control character (a newline, a
///    tab, any byte below 0x20, or 0x7f) -- see
///    `detail::renames_symbol_must_not_hold_a_bracket_or_a_control_character`.
///
/// The last is refused because of what a trace line is: a numbered line
/// whose provenance is a bracketed clause at its end, `1. k_s = 97/100 [fixed
/// by jurisdiction overlay]`. A symbol holding `[` or `]` could write such a
/// clause itself, and one holding a newline could write a whole line, so a
/// vocabulary could make a trace claim an overlay fixed, derived or replaced
/// something no overlay touched -- provenance only the library may state. A
/// bracket is also CommonMark link syntax, which no Markdown rendering may
/// contain (`render.hpp`). Nothing else about the text is checked -- see the
/// file comment.
template <Described Q, std::size_t N>
[[nodiscard]] consteval Renames<Q> renames(char const (&symbol)[N]) noexcept;

/// Refuses a symbol in a buffer that is not `const`. A better match than the
/// overload above for any such array, so it is the one chosen, and it never
/// compiles.
template <Described Q, std::size_t N>
[[nodiscard]] consteval Renames<Q> renames(char (&symbol)[N]) noexcept;

/// One entry of a vocabulary: quantity @p Q is written as `symbol()`.
///
/// Made only by `renames<Q>("symbol")`, which is where the guarantee
/// `symbol()` needs is enforced -- see there. The constructor is private so
/// that an entry cannot be aggregate-initialised around that guarantee from a
/// view of a temporary string.
template <typename Q>
class Renames
{
  public:
    /// The quantity this entry renames.
    using quantity = Q;

    /// How @p Q is written under this vocabulary. Static storage, so a trace
    /// step may keep the view.
    [[nodiscard]] constexpr std::string_view symbol() const noexcept
    {
        return _symbol;
    }

  private:
    constexpr explicit Renames(std::string_view symbol) noexcept:
        _symbol { symbol }
    {
    }

    template <Described R, std::size_t N>
    friend consteval Renames<R> renames(char const (&symbol)[N]) noexcept;

    std::string_view _symbol;
};

namespace detail
{
    /// Called only from `renames`, when its symbol is not a string: a NUL
    /// before the end, or none at the end. Deliberately not `constexpr`, so
    /// that the immediate call cannot complete and the compiler names this
    /// function in refusing it: its name is the refusal. The shape
    /// `published_positions_must_be_distinct_and_below_the_published_count`
    /// (`method.hpp`) has, for the same reason -- the text is a value, not a
    /// type, so no `static_assert` can state it. Never called at run time.
    inline void renames_symbol_must_be_a_string_with_no_embedded_nul() noexcept {}

    /// Called only from `renames`, when every character of its symbol is
    /// whitespace: a blank where the quantity stands, as an empty symbol
    /// would leave. See the function above for why a name.
    inline void renames_symbol_must_not_be_all_whitespace() noexcept {}

    /// Called only from `renames`, when its symbol holds `[`, `]` or a control
    /// character -- text that could forge a trace line's provenance clause, or
    /// a trace line. See the first function above for why a name.
    inline void renames_symbol_must_not_hold_a_bracket_or_a_control_character() noexcept {}

    /// True for `[`, `]`, and the ASCII control characters: below 0x20, and
    /// 0x7f.
    [[nodiscard]] constexpr bool is_forbidden_in_symbol(char character) noexcept
    {
        auto const byte = static_cast<unsigned char>(character);
        return character == '[' || character == ']' || byte < 0x20 || byte == 0x7f;
    }

    /// True for the characters `std::isspace` answers true for in the "C"
    /// locale, which is not `constexpr`.
    [[nodiscard]] constexpr bool is_blank(char character) noexcept
    {
        return character == ' ' || character == '\t' || character == '\n' || character == '\r' || character == '\v'
               || character == '\f';
    }

    /// A `false` that depends on @p N, for an assertion that must fire only
    /// when its template is instantiated.
    template <std::size_t N>
    inline constexpr bool dependentFalse = false;
} // namespace detail

template <Described Q, std::size_t N>
[[nodiscard]] consteval Renames<Q> renames(char const (&symbol)[N]) noexcept
{
    // The length is in the argument's type, so this is a type-level fact and
    // an ordinary assertion can state it. The characters are values, checked
    // below.
    static_assert(N > 1,
                  "formula: renames<Q>(\"\") gives the quantity an empty symbol, which would leave a blank "
                  "where it stands in every rendered formula and trace line");
    // Gated on the assertion above, so that an empty symbol -- which has no
    // characters, and so is vacuously all whitespace -- is refused once, as
    // empty, and not a second time here.
    if constexpr (N > 1)
    {
        bool blank = true;
        for (std::size_t index = 0; index + 1 < N; ++index)
        {
            if (symbol[index] == '\0')
                detail::renames_symbol_must_be_a_string_with_no_embedded_nul();
            if (detail::is_forbidden_in_symbol(symbol[index]))
                detail::renames_symbol_must_not_hold_a_bracket_or_a_control_character();
            blank = blank && detail::is_blank(symbol[index]);
        }
        if (symbol[N - 1] != '\0')
            detail::renames_symbol_must_be_a_string_with_no_embedded_nul();
        if (blank)
            detail::renames_symbol_must_not_be_all_whitespace();
    }
    return Renames<Q> { std::string_view { symbol, N - 1 } };
}

template <Described Q, std::size_t N>
[[nodiscard]] consteval Renames<Q> renames(char (&)[N]) noexcept
{
    static_assert(detail::dependentFalse<N>,
                  "formula: renames<Q> was given a buffer that is not const. A trace keeps a view of the symbol "
                  "for as long as it lives, so the symbol must be a string literal nothing can overwrite");
    return renames<Q>("?");
}

namespace detail
{
    template <typename E>
    inline constexpr bool isRenames = false;

    template <typename Q>
    inline constexpr bool isRenames<Renames<Q>> = true;

    /// How many of @p Es rename @p Q. cv-qualification is looked through, as
    /// it is when a symbol is resolved: `renames<Q const>` and `renames<Q>`
    /// would both apply to `var<Q>`.
    template <typename Q, typename... Es>
    inline constexpr std::size_t renamesOf =
        (std::size_t { 0 } + ...
         + (std::is_same_v<std::remove_cv_t<typename Es::quantity>, std::remove_cv_t<Q>> ? std::size_t { 1 }
                                                                                         : std::size_t { 0 }));

    /// The position among @p Es of the one entry renaming @p Q. Read only
    /// once `renamesOf<Q, Es...>` is known to be exactly one.
    template <typename Q, typename... Es>
    inline constexpr std::size_t renamingIndex = [] {
        constexpr bool matches[] = { std::is_same_v<std::remove_cv_t<typename Es::quantity>, std::remove_cv_t<Q>>... };
        std::size_t index = 0;
        while (!matches[index])
            ++index;
        return index;
    }();

    /// True when no two of @p Es rename the same quantity -- and, so that a
    /// pack holding something other than an entry is told that once rather
    /// than twice, also for such a pack: `ScopedVocabulary` refuses it on its
    /// own.
    template <typename... Es>
    [[nodiscard]] consteval bool renames_distinct_quantities() noexcept
    {
        if constexpr (!(isRenames<Es> && ...))
            return true;
        else
            return ((renamesOf<typename Es::quantity, Es...> == 1) && ...);
    }

    /// Fails to compile, in this library's words, when @p Es are not the
    /// entries of a vocabulary: something other than a `renames<Q>(...)`, or
    /// one quantity renamed twice.
    ///
    /// A class of its own rather than two assertions in `ScopedVocabulary`'s
    /// body, for the reason `RequireDistinctQuantities` (`environment.hpp`)
    /// is: a failed assertion in a class's own body leaves clang treating the
    /// class as invalid, and every later use of it -- the `constexpr`
    /// variable holding it, the `symbol_of` reading it -- as a further error.
    /// Measured on clang++ 20.1.8: three errors for one duplicate, where this
    /// shape gives one.
    template <typename... Es>
    struct RequireVocabularyEntries
    {
        static_assert((isRenames<Es> && ...), "formula: a vocabulary holds only entries made by renames<Q>(symbol)");

        // Gated inside `renames_distinct_quantities` on the assertion above,
        // so a pack refused there is not refused a second time here.
        static_assert(renames_distinct_quantities<Es...>(),
                      "formula: this vocabulary renames the same quantity twice, so nothing can say which "
                      "of the two symbols it means. Give each quantity at most one renames<Q>(symbol)");

        static constexpr bool value = true;
    };
} // namespace detail

/// The vocabulary that renames nothing: every quantity is written as
/// `Describe<Q>::symbol` says. The default everywhere a vocabulary is taken.
struct DefaultVocabulary
{
    /// How @p Q is written: `Describe<Q>::symbol`.
    template <Described Q>
    [[nodiscard]] constexpr std::string_view symbol() const noexcept
    {
        return Describe<Q>::symbol;
    }
};

/// A jurisdiction's vocabulary: the quantities it writes differently, each
/// once, and `Describe<Q>::symbol` for every other quantity.
///
/// Built by `vocabulary(renames<Q>("..."), ...)`. Plain data holding views of
/// static storage, so it can be `constexpr` and is cheap to copy.
///
/// **A quantity renamed twice is refused**, because nothing could say which
/// of the two spellings the jurisdiction meant. Two *different* quantities
/// renamed to one spelling are not: whether that is wise is the author's
/// judgement, exactly as it is for two `Describe` symbols that coincide, and
/// `document()` still gives each its own row (`SymbolEntry`, `document.hpp`).
template <typename... Es>
struct ScopedVocabulary
{
    static_assert(detail::RequireVocabularyEntries<Es...>::value);

    /// The entries, one per renamed quantity.
    std::tuple<Es...> entries;

    /// How @p Q is written under this vocabulary: its entry's symbol, or
    /// `Describe<Q>::symbol` when it has none.
    template <Described Q>
    [[nodiscard]] constexpr std::string_view symbol() const noexcept
    {
        // Nested, not one `&&`: a pack `RequireVocabularyEntries` has already
        // refused must not reach `renamesOf` and be refused again there, in
        // the compiler's words -- measured on clang++ 20.1.8, four further
        // errors for one non-entry without this.
        if constexpr ((detail::isRenames<Es> && ...))
        {
            if constexpr (detail::renamesOf<Q, Es...> == 1)
                return std::get<detail::renamingIndex<Q, Es...>>(entries).symbol();
            else
                return Describe<Q>::symbol;
        }
        else
            return Describe<Q>::symbol;
    }
};

/// Builds a vocabulary from its entries: `vocabulary(renames<Strength>("R"),
/// renames<Modulus>("E"))`.
template <typename... Es>
[[nodiscard]] constexpr ScopedVocabulary<Es...> vocabulary(Es... entries) noexcept
{
    return ScopedVocabulary<Es...> { std::tuple<Es...> { entries... } };
}

namespace detail
{
    template <typename V>
    inline constexpr bool isVocabulary = false;

    template <>
    inline constexpr bool isVocabulary<DefaultVocabulary> = true;

    template <typename... Es>
    inline constexpr bool isVocabulary<ScopedVocabulary<Es...>> = true;
} // namespace detail

/// A vocabulary this library knows how to read: `DefaultVocabulary` or a
/// `ScopedVocabulary`. Closed, because every surface that takes one resolves
/// symbols through `symbol_of` and nothing else.
template <typename V>
concept Vocabulary = detail::isVocabulary<std::remove_cv_t<V>>;

/// How @p Q is written under @p vocabulary -- the one call every surface that
/// writes a quantity's symbol goes through.
template <Described Q, Vocabulary V>
[[nodiscard]] constexpr std::string_view symbol_of(V const& vocabulary) noexcept
{
    return vocabulary.template symbol<Q>();
}

} // namespace formula
