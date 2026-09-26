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
#include <formula-cpp/detail/name_text.hpp>

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
/// library can see; it cannot cover one it cannot.
///
///   - **Later in the same translation unit** than a use, it is normally a
///     hard error, because any use of `enumerator_name` has already
///     instantiated `EnumeratorName<Shape>`: "explicit specialization ...
///     after instantiation" from clang and clang-cl, "specialization ... after
///     instantiation" from GCC, C2908 from cl. Measured with the use at
///     namespace scope, in an inline function, and in a function template that
///     is never instantiated. cl alone accepts the last of those and then uses
///     the specialization -- it defers the template body -- while clang,
///     clang-cl and GCC reject it too.
///   - **In a header another translation unit does not include**, it is simply
///     not there when that unit computes a name, and that unit uses the
///     enumerator's own name with nothing to say so. Two units disagreeing
///     about a specialization is ill-formed, no diagnostic required, as for
///     any trait.
///
/// The const-qualified form, `EnumeratorName<Shape const>` with no specialization
/// for `Shape` itself, is refused with this library's message: the library
/// only ever asks about the unqualified type, so such a specialization could
/// never take effect.
///
/// **A spelling may not hold `[`, `]` or a control character** -- a newline,
/// a tab, any byte below 0x20, or 0x7f. A key is written into a trace line
/// that ends in a bracketed clause saying where things came from, so such a
/// spelling could write a clause no overlay made, or a line of its own; see
/// `RequireEnumeratorNameSpelling`.
///
/// **Return an empty view to leave an enumerator alone.** A specialization
/// may cover some enumerators and return `{}` for the rest; those fall back
/// to the enumerator's own name. A `switch` with no `default` that falls off
/// its end is not usable in a constant expression, and is refused rather than
/// read as empty.
///
/// **What `of` returns must be readable at compile time, and so have static
/// storage duration** -- a string literal, or a view of a namespace-scope or
/// `static` `constexpr` array. A `constexpr` array local to `of` is a local
/// buffer like any other, and is refused.
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
    // `enumerator_name_misspelt_of.cpp` REJECTs this message to pin that,
    // on cl and GCC; clang stops at the first failed static_assert of a class
    // template instantiation, so it never shows this one either way.
    static_assert(!EnumeratorNameCustomization<decltype(E)> || detail::ConstantEnumeratorName<E>,
                  "formula: EnumeratorName<Enum>::of(E) is not usable in a constant expression for this "
                  "enumerator, or returns a view whose characters cannot be read at compile time -- a "
                  "string literal or a namespace-scope or static constexpr array can be, a local buffer "
                  "(constexpr or not), a std::string returned by value or a non-constexpr array cannot, and "
                  "a trace keeps the view; the enumerator "
                  "appears in this diagnostic as template argument E of RequireEnumeratorName -- make of() "
                  "constexpr, return a string literal, and return an empty string_view for an enumerator "
                  "it does not spell");

    /// Always `true` once reached -- see `RequireBandsAdjacent::value`.
    static constexpr bool value = true;
};

/// Fails to compile when `EnumeratorName`'s spelling of @p E holds `[`, `]`
/// or a control character -- the rule `RequireTagNameSpelling` (`tag.hpp`)
/// states for a tag, for the same reason: an exact lookup's key is written
/// into a trace line, `lookup(key steel] [replaced by jurisdiction overlay:
/// ...`, whose bracketed clause could then claim a replacement no overlay
/// made, and a newline would write a line that is no step. Measured before
/// the rule, on cl 19.51: exactly that line was printed.
///
/// Asked only of a spelling the library could read at all
/// (`RequireEnumeratorName`), so a broken specialization gets that message
/// alone.
template <auto E>
struct RequireEnumeratorNameSpelling
{
    static_assert(!detail::holds_character_forbidden_in_trace_name(detail::customized_enumerator_name<E>()),
                  "formula: this EnumeratorName spelling holds a square bracket or a control character (a newline, "
                  "a tab, any byte below 0x20, or 0x7f); a trace line ends in a bracketed clause saying where "
                  "a value came from, and a line ends at a newline, so such a spelling could make a trace "
                  "claim an overlay replaced or fixed something no overlay touched, or add a line that is no "
                  "step -- the enumerator appears in this diagnostic as template argument E of "
                  "RequireEnumeratorNameSpelling -- spell it without them");

    /// Always `true` once reached -- see `RequireBandsAdjacent::value`.
    static constexpr bool value = true;
};

/// Fails to compile when `EnumeratorName` is specialized for a const-qualified
/// @p Enum -- `EnumeratorName<Shape const>` -- and not for @p Enum itself.
/// The library only ever asks `EnumeratorName<Shape>`, so that specialization
/// could never take effect, and the author's wording would be silently absent.
///
/// **Only when the unqualified form is not customized.** A constrained partial
/// specialization covering every enumeration -- say one bridging to a
/// codebase's own `to_string` -- also matches `Shape const`, since
/// `std::is_enum_v` is true of a cv-qualified enumeration; it customizes
/// `Shape` as well, and must not be refused.
///
/// **`volatile` is deliberately not checked.** Asking whether
/// `EnumeratorName<Shape volatile>` is specialized instantiates it, and for
/// exactly that generic bridge that declares `of(Shape volatile)`: a
/// volatile-qualified parameter, which clang and clang-cl report as
/// deprecated (`-Wdeprecated-volatile`, on by default); g++ 13 and cl stay
/// quiet. The check would turn an ordinary bridge into a build failure under
/// `-Werror` on clang, to catch a specialization nobody writes. Measured on
/// all four; with this library's own warning flags it failed to compile the
/// bridge test in `enumerator_tests.cpp` on clang 20.
///
/// Instantiated by `enumerator_name` for every enumerator it names,
/// customized or not, since the point is to catch a specialization that
/// otherwise looks like no customization at all.
template <typename Enum>
struct RequireUnqualifiedEnumeratorName
{
    static_assert(detail::customizesEnumeratorName<Enum> || !detail::customizesEnumeratorName<Enum const>,
                  "formula: EnumeratorName is specialised for a const-qualified enumeration, "
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
            static_assert(RequireEnumeratorNameSpelling<E>::value);
            std::string_view const customized = detail::customized_enumerator_name<E>();
            if (!customized.empty())
                return customized;
        }
    }
    return detail::reflected_enumerator_name<E>();
}

} // namespace formula
