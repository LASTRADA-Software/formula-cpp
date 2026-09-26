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
/// is: nothing checks what it says, so a symbol containing Markdown or LaTeX
/// markup, or text that reads like a trace annotation, reaches the page and
/// the trace as written. The vocabulary is the author's data, like the
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
/// constant expression, which a string literal is and a local buffer is not.
template <Described Q, std::size_t N>
[[nodiscard]] consteval Renames<Q> renames(char const (&symbol)[N]) noexcept;

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

template <Described Q, std::size_t N>
[[nodiscard]] consteval Renames<Q> renames(char const (&symbol)[N]) noexcept
{
    return Renames<Q> { std::string_view { symbol, N - 1 } };
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
    /// The instance a `RecordingSink` built without a vocabulary points at
    /// (`trace.hpp`): static, so the pointer never dangles.
    inline constexpr DefaultVocabulary defaultVocabulary {};

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
