// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// How an enumerator is spelled wherever the library shows one.
///
/// An exact lookup's key is an enumerator of the author's own scoped
/// enumeration (`lookup.hpp`), and `render()` and a rendered trace both show
/// it. By default they show the enumerator's name as written in the source --
/// `key Cylinder` -- recovered at compile time from the compiler's own
/// spelling of a function signature (`detail/enum_name.hpp`). An author whose
/// published table words a row differently specializes `EnumeratorName` to
/// say so.
///
/// **A header of its own**, rather than a section of `render.hpp`, because
/// both `render.hpp` and `trace.hpp` need it and neither may include the
/// other (`trace.hpp` must not pull `<string>`), and because an author
/// customizing a spelling is describing their enumeration -- a fact that
/// belongs beside the type, in a header that costs nothing to include --
/// not asking for text. It pulls only `<concepts>`, `<cstddef>`,
/// `<string_view>` and `<type_traits>`, and `lookup.hpp` includes it, so
/// anyone who can declare an exact lookup already has it.

#include <formula-cpp/detail/enum_name.hpp>

#include <concepts>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace formula
{

namespace detail
{
    /// The base only `EnumeratorName`'s primary template has. How the library
    /// tells "the author wrote no specialization" from "the author wrote one
    /// with the wrong shape": the second must be refused, and an empty
    /// primary would be indistinguishable from it. A specialization that has
    /// no `of` at all, or a misspelt one, is still a specialization, and
    /// still lacks this base.
    struct EnumeratorNameNotCustomized
    {
    };
} // namespace detail

/// Customization point: how an enumerator of `Enum` is spelled wherever the
/// library shows one (a lookup key in `render()` and in a trace).
///
/// The primary template customizes nothing, and the library falls back to
/// the enumerator's own name as written in the source. Specialize it to
/// spell an enumerator differently, for example to match a published
/// table's wording:
///
///     template <>
///     struct formula::EnumeratorName<Shape>
///     {
///         static constexpr std::string_view of(Shape s) noexcept
///         {
///             switch (s)
///             {
///                 case Shape::Cube: return "cube 150 mm";
///                 default: return {};
///             }
///         }
///     };
///
/// **`of` is read at compile time.** It must be a `static constexpr` (or
/// `consteval`) function taking an `Enum` and returning something convertible
/// to `std::string_view`, and it must be usable in a constant expression for
/// every enumerator the library asks about -- which, for an exact lookup, is
/// every key of the table. A specialization of any other shape is refused
/// with this library's own message, never read as "not customized": a
/// misspelt `of` that silently fell back to the reflected name would be the
/// author's wording quietly not appearing anywhere.
///
/// **Declare the specialization next to the enumeration, before anything uses
/// it, and on the unqualified type.** That promise covers a specialization the
/// library can see; it cannot cover one it cannot. Like every trait, a
/// specialization declared after the first `render()` or trace that asks for
/// the name -- or in a header that translation unit does not include -- is
/// simply not there when the name is computed, and the enumerator's own name
/// is used instead, with nothing to say so. (A specialization declared after
/// an implicit instantiation is ill-formed, no diagnostic required.) The
/// cv-qualified form, `EnumeratorName<Shape const>`, is refused with this
/// library's message: the library only ever asks about the unqualified type,
/// so such a specialization could never take effect.
///
/// **Return an empty view to leave an enumerator alone.** A specialization
/// may cover some enumerators and return `{}` for the rest; those fall back
/// to the enumerator's own name. A `switch` with no `default` that falls off
/// its end is not usable in a constant expression, and is refused rather than
/// read as empty.
///
/// **What `of` returns must be readable at compile time, and so have static
/// storage duration** -- a string literal, or a view of a `constexpr` array.
/// A trace keeps the view (`Step::lookupKeyName`, `trace.hpp`) for as long as
/// the trace lives, which may be long after the formula that recorded it is
/// gone. This is enforced, not merely requested, by three independent gates,
/// any one of which refuses a view it cannot vouch for:
///
///   1. `RequireEnumeratorName` reads every character of the view in a
///      constant expression, and refuses in this library's words when it
///      cannot. A view of a local buffer, of a `std::string` returned by
///      value, of a mutable static, or of an immutable but non-`constexpr`
///      array (`const char name[]` at namespace scope, whose characters are
///      not usable in a constant expression) all fail it on cl, clang-cl and
///      clang.
///   2. `enumerator_name` is `consteval`, so its result must be a permitted
///      result of a constant expression: a pointer into an object with static
///      storage duration. The compiler refuses anything else in its own
///      words. g++ 13 lets the local-buffer case through gate 1 -- it reads
///      the dead buffer as a constant -- and refuses it here instead, as
///      "is not a constant expression".
///   3. The table of a lookup's key names (`detail::keyNames`, `lookup.hpp`)
///      is itself a `constexpr` variable, so its initializer is held to the
///      same rule again, on the path `render()` and a trace actually take.
///
/// The refusal is always safe; what differs is whose words it is in.
template <typename Enum>
struct EnumeratorName: detail::EnumeratorNameNotCustomized
{
};

/// True when `EnumeratorName<Enum>` has the shape the library reads: a static
/// `of` callable with an `Enum` and returning something convertible to
/// `std::string_view`. Whether it is usable in a constant expression is a
/// separate question, asked per enumerator by `RequireEnumeratorName`.
template <typename Enum>
concept EnumeratorNameCustomization = requires(Enum value) {
    { EnumeratorName<Enum>::of(value) } -> std::convertible_to<std::string_view>;
};

namespace detail
{
    /// True when the author specialized `EnumeratorName<Enum>` at all -- with
    /// any shape. See `EnumeratorNameNotCustomized`.
    template <typename Enum>
    inline constexpr bool customizesEnumeratorName = !std::is_base_of_v<EnumeratorNameNotCustomized, EnumeratorName<Enum>>;

    /// The customized spelling of @p E, as a view. The conversion to
    /// `std::string_view` happens here, in a `return`, so a customization
    /// that returns a `std::string` by value yields a view whose characters
    /// are already gone by the time anyone reads them -- exactly what the
    /// check below is there to catch.
    ///
    /// Constrained on the shape, so that asking whether a wrongly shaped
    /// specialization is constant is a plain "no" rather than a hard error
    /// from inside this body.
    template <auto E>
        requires EnumeratorNameCustomization<decltype(E)>
    [[nodiscard]] constexpr std::string_view customized_enumerator_name()
    {
        return EnumeratorName<decltype(E)>::of(E);
    }

    /// Reads every character of @p text and answers `true`. Only interesting
    /// inside a constant expression, where reading a character that is not
    /// there -- a destroyed local, freed storage, a mutable static -- makes
    /// the whole expression not a constant expression.
    [[nodiscard]] constexpr bool every_character_readable(std::string_view text) noexcept
    {
        std::size_t read = 0;
        for (char const c: text)
            read += c == '\0' ? 0 : 1;
        return read <= text.size();
    }

    /// True when `EnumeratorName`'s spelling of @p E can be computed at
    /// compile time and every character of it read there. Gate 1 of the three
    /// described on `EnumeratorName`: on cl, clang-cl and clang it refuses
    /// every view without static storage duration that was measured, but g++
    /// 13 lets a view of a dead local buffer through, which gate 2 then
    /// refuses.
    template <auto E>
    concept ConstantEnumeratorName = EnumeratorNameCustomization<decltype(E)> && requires {
        typename std::bool_constant<every_character_readable(customized_enumerator_name<E>())>;
    };
} // namespace detail

/// Fails to compile when `EnumeratorName<decltype(E)>` is specialized but the
/// library cannot read a spelling of @p E out of it: the specialization has
/// the wrong shape (no static `of`, a misspelt one, a return type that is not
/// a string), or its `of(E)` is not usable in a constant expression, or the
/// characters of what it returns cannot be read at compile time.
///
/// Instantiated by `enumerator_name` whenever a specialization exists, so a
/// broken one is refused the first time anything asks for a name -- in
/// practice, the first `render()` or trace of an exact lookup keyed on it.
template <auto E>
struct RequireEnumeratorName
{
    static_assert(EnumeratorNameCustomization<decltype(E)>,
                  "formula: this EnumeratorName specialisation does not have the shape the library "
                  "reads -- it needs a static constexpr member function of(Enum) returning something "
                  "convertible to std::string_view; the enumeration appears in this diagnostic as the "
                  "type of template argument E of RequireEnumeratorName -- a specialisation the library "
                  "cannot read is refused rather than treated as no customization, so a misspelt of() "
                  "cannot silently drop the author's wording");

    // `!EnumeratorNameCustomization || ...` so a wrongly shaped specialization
    // gets the message above and not this one as well --
    // `enumerator_name_misspelt_of.cpp` REJECTs this message to pin that.
    static_assert(!EnumeratorNameCustomization<decltype(E)> || detail::ConstantEnumeratorName<E>,
                  "formula: EnumeratorName<Enum>::of(E) is not usable in a constant expression for this "
                  "enumerator, or returns a view whose characters cannot be read at compile time -- a "
                  "string literal or a constexpr array can be, a local buffer, a std::string returned by "
                  "value or a non-constexpr array cannot, and a trace keeps the view; the enumerator "
                  "appears in this diagnostic as template argument E of RequireEnumeratorName -- make of() "
                  "constexpr, return a string literal, and return an empty string_view for an enumerator "
                  "it does not spell");

    /// Always `true` once reached -- see `RequireBandsAdjacent::value`.
    static constexpr bool value = true;
};

/// Fails to compile when `EnumeratorName` is specialized for a cv-qualified
/// @p Enum -- `EnumeratorName<Shape const>`. The library only ever asks
/// `EnumeratorName<Shape>`, so that specialization could never take effect,
/// and the author's wording would be silently absent.
///
/// Instantiated by `enumerator_name` for every enumerator it names,
/// customized or not, since the point is to catch a specialization that
/// otherwise looks like no customization at all.
template <typename Enum>
struct RequireUnqualifiedEnumeratorName
{
    static_assert(!detail::customizesEnumeratorName<Enum const> && !detail::customizesEnumeratorName<Enum volatile>
                      && !detail::customizesEnumeratorName<Enum const volatile>,
                  "formula: EnumeratorName is specialised for a const- or volatile-qualified enumeration, "
                  "which the library never asks about, so it would silently never be used; the enumeration "
                  "appears in this diagnostic as template argument Enum of RequireUnqualifiedEnumeratorName "
                  "-- specialise EnumeratorName for the unqualified enumeration instead");

    /// Always `true` once reached -- see `RequireBandsAdjacent::value`.
    static constexpr bool value = true;
};

/// The spelling of enumerator @p E: `EnumeratorName<decltype(E)>`'s, if it
/// customizes @p E; otherwise the enumerator's own name as written in its
/// declaration (`Cylinder`, never `Shape::Cylinder`); empty if @p E names no
/// enumerator, such as `static_cast<Shape>(9)`.
///
/// Every view this returns has static storage duration: a reflected name
/// points into the compiler's function-signature literal, and a customized
/// one has passed the gates described on `EnumeratorName`. It is therefore
/// safe to keep for as long as the image that computed it stays loaded --
/// for an ordinary program, until it exits. A view recorded by code in a
/// shared library or plugin points into that library's read-only data, and
/// dangles once the library is unloaded.
///
/// Compile time only, and deliberately so. Every enumerator this library
/// shows is a key of a table fixed at compile time, so the set is always
/// known; there is no runtime value-to-name scan over an assumed range of
/// values. Two enumerators with the same value are one template argument,
/// and are both named after the first one declared -- see
/// `detail::reflected_enumerator_name`.
template <auto E>
    requires std::is_enum_v<decltype(E)>
[[nodiscard]] consteval std::string_view enumerator_name() noexcept
{
    using Enum = decltype(E);
    static_assert(RequireUnqualifiedEnumeratorName<Enum>::value);
    if constexpr (detail::customizesEnumeratorName<Enum>)
    {
        static_assert(RequireEnumeratorName<E>::value);
        // Guarded again so a refused specialization is reported once, in the
        // library's words, and not followed by the raw error from calling it
        // -- `enumerator_name_misspelt_of.cpp` and
        // `enumerator_name_not_constexpr.cpp` REJECT each compiler's raw
        // wording to pin that.
        if constexpr (detail::ConstantEnumeratorName<E>)
        {
            std::string_view const customized = detail::customized_enumerator_name<E>();
            if (!customized.empty())
                return customized;
        }
    }
    return detail::reflected_enumerator_name<E>();
}

} // namespace formula
