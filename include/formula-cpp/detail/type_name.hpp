// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A class type's name as written in the source, recovered at compile time.
///
/// Adapted from `TypeNameOf` in `contour-terminal/reflection-cpp`
/// (`include/reflection-cpp/reflection.hpp`), which is licensed Apache-2.0,
/// the same licence as this repository. The idea is the reference's -- read
/// the template argument back out of the compiler's own spelling of a
/// function signature -- and so is the reason the signature function returns
/// `auto`. What is kept of the argument is not: the reference returns it
/// verbatim, qualification and all, where this strips it to the name a
/// method's author wrote -- see `normalized_type_name`.
///
/// Little is shared with the enumerator case next door
/// (`detail/enum_name.hpp`) beyond the signature trick itself and what an
/// identifier byte is (`detail/name_text.hpp`). That parse reads one
/// identifier back from a fixed tail; this one has to walk a whole type,
/// template arguments included, and rewrite it.
///
/// The public face of this is `formula::tag_name` (`tag.hpp`), which consults
/// the author's `TagName` customization first. Nothing outside that header
/// should call into here directly.

#include <formula-cpp/detail/name_text.hpp>

#include <array>
#include <cstddef>
#include <string_view>

namespace formula::detail
{

/// The compiler's own spelling of this function's signature, which names the
/// template argument @p T -- `auto formula::detail::type_signature() [T =
/// Cube]` from clang, `consteval auto formula::detail::type_signature() [with
/// T = Cube]` from GCC, `auto __cdecl
/// formula::detail::type_signature<struct Cube>(void)` from cl.
///
/// `auto` and not `noexcept`, for the reasons `enumerator_signature` gives:
/// GCC would otherwise append the `std::string_view` typedef it expanded
/// after the argument, and cl would append ` noexcept` after `(void)`.
template <typename T>
[[nodiscard]] consteval auto type_signature()
{
#if defined(_MSC_VER) && !defined(__clang__)
    return std::string_view { __FUNCSIG__ };
#else
    return std::string_view { __PRETTY_FUNCTION__ };
#endif
}

/// The template argument's text inside a `type_signature` spelling, exactly
/// as the compiler printed it, or an empty view when the signature is not in
/// the shape this expects.
///
/// Read between a fixed head and a fixed tail, both of which this library
/// wrote and no user text can contain: `T = ` up to the final `]` on clang
/// and GCC, `type_signature<` up to the final `>(void)` on cl. Unlike the
/// enumerator case this cannot be parsed from the end alone, because a type's
/// spelling may itself end in `>`, `]` or an identifier.
[[nodiscard]] constexpr std::string_view type_argument_text(std::string_view signature) noexcept
{
#if defined(_MSC_VER) && !defined(__clang__)
    constexpr std::string_view opener = "type_signature<";
    constexpr std::string_view closer = ">(void)";
#else
    constexpr std::string_view opener = "T = ";
    constexpr std::string_view closer = "]";
#endif
    std::size_t const opening = signature.find(opener);
    if (opening == std::string_view::npos || !signature.ends_with(closer))
        return {};
    std::size_t const nameStart = opening + opener.size();
    if (nameStart > signature.size() - closer.size())
        return {};
    return signature.substr(nameStart, signature.size() - closer.size() - nameStart);
}

/// A type's name with what the compiler adds around it taken away, and at
/// most @p Capacity bytes of it -- `size` says how many were used.
template <std::size_t Capacity>
struct NormalizedTypeName
{
    /// The bytes of the name; only the first `size` mean anything.
    std::array<char, Capacity> chars {};
    /// How many of `chars` are the name.
    std::size_t size = 0;
    /// Whether `const` or `volatile` qualifies a part of the type the name
    /// shows -- see `normalized_type_name`.
    bool qualified = false;
};

/// True when @p spelling at @p at begins with @p keyword as a whole word --
/// neither preceded nor followed by an identifier byte.
[[nodiscard]] constexpr bool word_at(std::string_view spelling, std::size_t at, std::string_view keyword) noexcept
{
    if (!spelling.substr(at).starts_with(keyword))
        return false;
    std::size_t const wordEnd = at + keyword.size();
    return (at == 0 || !is_identifier_byte(spelling[at - 1]))
           && (wordEnd == spelling.size() || !is_identifier_byte(spelling[wordEnd]));
}

/// True when @p spelling at @p at begins with @p keyword followed by a space.
[[nodiscard]] constexpr bool starts_with_word(std::string_view spelling, std::size_t at, std::string_view keyword) noexcept
{
    return spelling.substr(at).starts_with(keyword) && at + keyword.size() < spelling.size()
           && spelling[at + keyword.size()] == ' ';
}

/// @p argument -- a type as `type_argument_text` found it -- reduced to the
/// spelling a method's author wrote: `Cylinder`, `Sized<150>`, `Box<Anon>`.
///
/// **Every qualification goes, at every level.** Measured on cl 19.51,
/// clang-cl 22.1.3, clang++ 20.1.8, g++ 13.3 and g++-14 14.2, the four
/// compilers agree on an ordinary qualifier -- `outer::inner::Nested` -- and
/// disagree about an anonymous namespace: `(anonymous namespace)::` from clang
/// and clang-cl, `{anonymous}::` from GCC, and from cl `` `anonymous-namespace'::``
/// at the top level but `` `anonymous namespace'::`` inside a template
/// argument list.
/// Rather than recognise each, a name component is cut back to its start at
/// every `::`, so whatever precedes the last `::` of a component -- a
/// namespace, an enclosing class, an enclosing class template's
/// specialization, an anonymous namespace however spelt -- is dropped
/// without having to be understood. An enumerator argument loses its
/// enumeration the same way, so `ByEnum<Shape::Cube>` reads `ByEnum<Cube>`,
/// as a lookup key reads `key Cube` -- on the compilers that print the
/// enumerator at all; see below for cl.
///
/// **Three things of cl's are undone as well**, so that the common cases
/// read alike on every compiler: the class-key cl writes before every class
/// type (`struct Box<struct Global>`), its missing space after a comma
/// (`Pair<int,double>`), and the space cl and GCC both leave between two
/// closing angle brackets (`Box<Box<int> >`).
///
/// **What is not undone, because it is not spelling but meaning.** cl prints
/// a `bool`, `char` or enumeration template argument as its number --
/// `Flag<1>`, `Ch<120>`, `ByEnum<0>` where the others print `Flag<true>`,
/// `Ch<'x'>`, `ByEnum<E::A>` (a `char` argument never reaches a trace on
/// clang or GCC, since `is_plain_type_name` refuses the `'`, but cl's
/// `Ch<120>` does) -- and `long long` as `__int64`, where clang
/// prints `long long` and GCC `long long int`. No parse of the text can recover what cl did not print.
/// Nor can a defaulted template argument: cl prints it and clang and GCC
/// leave it out, so `Opt<Cube>` for `template <typename T, typename U = void>
/// struct Opt` reads `Opt<Cube, void>` on cl and `Opt<Cube>` elsewhere --
/// measured on all four. (Nor is a pointer argument's spacing evened out --
/// `Global *` from clang and cl, `Global*` from GCC -- though a pointer
/// argument never reaches a trace: `is_plain_type_name` refuses the `*`.)
/// A tag whose name must read the same on every compiler, and a template
/// specialization with such arguments is one, customizes its spelling
/// through `TagName` (`tag.hpp`).
///
/// **`qualified` says whether a `const` or `volatile` belongs to what the
/// name shows**, which the name itself cannot be trusted to say: the `::`
/// cut takes a leading `const ` with the scope it cuts, so `TagBox<const
/// ns::A>` reads `TagBox<A>` on clang and GCC -- the name of a different
/// type. A cv word counts when it qualifies a component the name keeps, and
/// not when it sits in the argument list of a scope the name discards:
/// `Outer<const int>::Inner` reads `Inner`, and the `const` qualified
/// `Outer`'s argument, not `Inner`. Hence three flags per level -- a word
/// read in the component itself survives that component's `::` cut, since it
/// qualifies the name that follows; a word from a closed inner list does
/// not, since that list belonged to the scope cut away; and a word from an
/// earlier component of the same list is already settled. cl prints a cv
/// qualifier after the type, `TagBox<struct ns::A const >`, where it is kept
/// text, and also prints defaulted arguments, so `Opt<Cube>` for `template
/// <typename T, typename U = const int> struct Opt` is `Opt<Cube, int
/// const>` there -- qualified on cl alone, measured on all four.
template <std::size_t Capacity>
[[nodiscard]] consteval NormalizedTypeName<Capacity> normalized_type_name(std::string_view argument) noexcept
{
    NormalizedTypeName<Capacity> normalized {};
    // Where the component being written began, per level of `<` nesting.
    std::array<std::size_t, Capacity + 1> componentStart {};
    std::size_t nesting = 0;

    // Where a `const` or `volatile` was read, per level: in the component
    // being written itself (`own`), inside an argument list that component
    // has closed (`nested`), or in an earlier, finished component of the
    // same list (`listed`). See the function comment for why the three.
    std::array<bool, Capacity + 1> own {};
    std::array<bool, Capacity + 1> nested {};
    std::array<bool, Capacity + 1> listed {};

    auto const push = [&](char byte) {
        if (normalized.size < Capacity)
            normalized.chars[normalized.size++] = byte;
    };

    std::size_t cursor = 0;
    while (cursor < argument.size())
    {
        char const byte = argument[cursor];
        bool const atComponentStart = normalized.size == componentStart[nesting];

        if (word_at(argument, cursor, "const") || word_at(argument, cursor, "volatile"))
            own[nesting] = true;

        if (atComponentStart
            && (starts_with_word(argument, cursor, "struct") || starts_with_word(argument, cursor, "class")
                || starts_with_word(argument, cursor, "union") || starts_with_word(argument, cursor, "enum")))
        {
            while (argument[cursor] != ' ')
                ++cursor;
            ++cursor;
        }
        else if (argument.substr(cursor).starts_with("::"))
        {
            normalized.size = componentStart[nesting];
            // The scope being cut away is discarded, and so is anything its
            // own argument list said; a cv word of this level still
            // qualifies the name that follows.
            nested[nesting] = false;
            cursor += 2;
        }
        else if (byte == '<')
        {
            push(byte);
            ++nesting;
            componentStart[nesting] = normalized.size;
            own[nesting] = nested[nesting] = listed[nesting] = false;
            ++cursor;
        }
        else if (byte == '>')
        {
            if (normalized.size > 0 && normalized.chars[normalized.size - 1] == ' ')
                --normalized.size;
            push(byte);
            if (nesting > 0)
            {
                bool const closedQualified = own[nesting] || nested[nesting] || listed[nesting];
                --nesting;
                nested[nesting] = nested[nesting] || closedQualified;
            }
            ++cursor;
        }
        else if (byte == ',')
        {
            push(',');
            push(' ');
            ++cursor;
            while (cursor < argument.size() && argument[cursor] == ' ')
                ++cursor;
            componentStart[nesting] = normalized.size;
            listed[nesting] = listed[nesting] || own[nesting] || nested[nesting];
            own[nesting] = nested[nesting] = false;
        }
        else
        {
            push(byte);
            ++cursor;
        }
    }
    normalized.qualified = own[0] || nested[0] || listed[0];
    return normalized;
}

/// True when a normalized name reads as a class name and its template
/// arguments, and nothing else: it begins with an ASCII letter or `_`, and
/// contains none of the bytes `(`, `)`, `{`, `}`, a backtick, a backslash,
/// `/`, `.`, `*`, `&`, `'` or `"`.
///
/// Those are the bytes through which what the compiler printed escapes the
/// normalizer's grammar, which tracks nesting through `<` and `>` alone.
/// Measured on cl 19.51, clang-cl 22.1.3, clang++ 20.1.8, g++ 13.3 and
/// g++-14 14.2, each of these reaches this function as something other than
/// the type:
///
///  - a function-type argument, `TagBox<void(ns::A)>`, is `TagBox<A)>` on
///    all four -- the `::` cuts back through the `(`;
///  - a pointer to member, `TagBox<int ns::A::*>`, is `TagBox<*>`;
///  - a cast non-type argument, `ByEnum<(ns::E)5>`, is `ByEnum<E)5>` from
///    clang and GCC (cl prints `ByEnum<5>`, which passes);
///  - a lambda is `(lambda at file.cpp:10:12)` from clang and clang-cl,
///    `<lambda()>` from GCC, and from cl `<lambda_1>`, or `<lambda>@name`
///    for the closure type of a named variable;
///  - an unnamed class is `(unnamed struct at file.cpp:12:17)` from clang,
///    `<unnamed struct>` from GCC and `<unnamed-type-member>` from cl. A
///    class named for linkage by a typedef, `typedef struct { } Foo;`, is
///    `Foo` on all four, and passes;
///  - either of the last two as a template ARGUMENT keeps its placeholder
///    once the `::` cut has taken its scope: `TagBox<<lambda_1_>>` and
///    `TagBox<<unnamed-type-member>>` from cl, `TagBox<<unnamed struct>>`
///    from GCC. Hence the second rule: every argument starts with a letter,
///    a digit, `_` or `-`, never a `<`;
///  - and three kinds of readable non-type argument are refused as well,
///    because the compilers disagree about them: a `char`, `Ch<'x'>` from
///    clang and GCC and `Ch<120>` from cl (which passes there); a
///    floating-point value, `Real<1.5e+0>` from GCC, `Real<1.500000e+00>`
///    from clang and `Real<1.500000>` from cl; and a class-type value,
///    `Sized<Dim{3}>`, `Sized<Dim{int:3}>` on cl.
///
/// A name, or an argument, that begins with a non-ASCII letter -- a legal
/// C++23 identifier -- is refused too, which is the price of stating the
/// rule in bytes.
[[nodiscard]] constexpr bool is_plain_type_name(std::string_view typeName) noexcept
{
    if (typeName.empty())
        return false;
    char const leadingCharacter = typeName.front();
    if (!((leadingCharacter >= 'a' && leadingCharacter <= 'z') || (leadingCharacter >= 'A' && leadingCharacter <= 'Z')
          || leadingCharacter == '_'))
        return false;
    // Every argument starts with a letter, a digit, `_` or `-` (a negative
    // number) -- never a `<`, which is how a compiler's placeholder survives
    // inside an argument list once the `::` cut has taken its scope.
    for (std::size_t characterIndex = 0; characterIndex < typeName.size(); ++characterIndex)
    {
        bool const startsArgument =
            typeName[characterIndex] == '<'
            || (typeName[characterIndex] == ' ' && characterIndex > 0 && typeName[characterIndex - 1] == ',');
        if (!startsArgument)
            continue;
        if (characterIndex + 1 == typeName.size())
            return false;
        char const following = typeName[characterIndex + 1];
        // An empty list, `Box<>`, is a specialization whose every argument
        // was defaulted, as clang and GCC print it.
        if (typeName[characterIndex] == '<' && following == '>')
            continue;
        if (!((following >= 'a' && following <= 'z') || (following >= 'A' && following <= 'Z')
              || (following >= '0' && following <= '9') || following == '_' || following == '-'))
            return false;
    }
    return typeName.find_first_of("(){}`\\/.*&'\"") == std::string_view::npos;
}

/// The first `Size - 1` bytes of @p name, followed by a nul.
template <std::size_t Size, std::size_t Capacity>
[[nodiscard]] consteval std::array<char, Size> trimmed(NormalizedTypeName<Capacity> const& name) noexcept
{
    std::array<char, Size> trimmedChars {};
    for (std::size_t characterIndex = 0; characterIndex + 1 < Size && characterIndex < name.size; ++characterIndex)
        trimmedChars[characterIndex] = name.chars[characterIndex];
    return trimmedChars;
}

/// The name of @p T as `normalized_type_name` spells it, held in storage of
/// its own.
///
/// **A static data member, and that is the point.** The name is not a
/// substring of the compiler's signature literal -- stripping a qualifier
/// from the middle of a template argument list leaves no contiguous run to
/// point at -- so it has to be written somewhere, and `Step::variantTag`
/// (`trace.hpp`) keeps a view of it for as long as a trace lives. A `static
/// constexpr` member of a class template has static storage duration, and is
/// implicitly inline, so every translation unit sees one object.
template <typename T>
struct TypeNameStorage
{
    /// The argument as the compiler printed it.
    static constexpr std::string_view argument = type_argument_text(type_signature<T>());

    /// Room for the normalized name: twice the argument's size, plus one. The
    /// name is never longer than the argument plus one byte per comma, since
    /// `,` may become `, ` and nothing else grows, so this is generous -- it
    /// is bounded by the signature, not by the name, and costs nothing at run
    /// time, since only `chars` below is ever emitted.
    static constexpr std::size_t capacity = argument.size() * 2 + 1;

    /// The normalized name, before it is trimmed to its size.
    static constexpr NormalizedTypeName<capacity> normalized = normalized_type_name<capacity>(argument);

    /// The name's bytes, trimmed, and nul-terminated for a debugger's sake.
    static constexpr std::array<char, normalized.size + 1> chars = trimmed<normalized.size + 1>(normalized);

    /// Whether the compiler's signature was in the shape `type_argument_text`
    /// reads. When it was not, there is no name, and the caller falls back.
    static constexpr bool recognised = !argument.empty();

    /// Whether the name, once found, can be shown as it stands -- see
    /// `is_plain_type_name`. Meaningful only when `recognised`.
    static constexpr bool plain =
        is_plain_type_name(std::string_view { chars.data(), normalized.size }) && !normalized.qualified;
};

/// The name of class type @p T as written in its declaration, unqualified --
/// `Cylinder`, never `specimen::Cylinder` or `(anonymous namespace)::Cylinder`
/// -- with a template specialization's arguments kept and spelt as
/// `normalized_type_name` describes. Empty when the compiler's signature is
/// not in the shape `type_argument_text` expects, so that the caller falls
/// back rather than shows a fragment.
///
/// Whether a name that WAS found can be shown as it stands is a separate
/// question -- `TypeNameStorage<T>::plain` -- which `tag_name` (`tag.hpp`)
/// asks, and refuses to compile when it cannot be.
///
/// Every view this returns points into `TypeNameStorage<T>::chars`, which has
/// static storage duration.
template <typename T>
[[nodiscard]] consteval std::string_view reflected_type_name() noexcept
{
    return std::string_view { TypeNameStorage<T>::chars.data(), TypeNameStorage<T>::normalized.size };
}

} // namespace formula::detail
