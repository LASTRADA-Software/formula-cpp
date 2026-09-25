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
/// Nothing is shared with the enumerator case next door
/// (`detail/enum_name.hpp`) beyond the signature trick itself. That parse
/// reads one identifier back from a fixed tail; this one has to walk a whole
/// type, template arguments included, and rewrite it -- the two have no step
/// in common that would not be a coincidence of spelling.
///
/// The public face of this is `formula::tag_name` (`tag.hpp`), which consults
/// the author's `TagName` customization first. Nothing outside that header
/// should call into here directly.

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
    constexpr std::string_view head = "type_signature<";
    constexpr std::string_view tail = ">(void)";
#else
    constexpr std::string_view head = "T = ";
    constexpr std::string_view tail = "]";
#endif
    std::size_t const start = signature.find(head);
    if (start == std::string_view::npos || !signature.ends_with(tail))
        return {};
    std::size_t const first = start + head.size();
    if (first > signature.size() - tail.size())
        return {};
    return signature.substr(first, signature.size() - tail.size() - first);
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
};

/// True when @p text at @p position begins with @p word followed by a space.
[[nodiscard]] constexpr bool starts_with_word(std::string_view text, std::size_t position, std::string_view word) noexcept
{
    return text.substr(position).starts_with(word) && position + word.size() < text.size()
           && text[position + word.size()] == ' ';
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
/// `Ch<'x'>`, `ByEnum<E::A>` -- and `long long` as `__int64`, where clang
/// prints `long long` and GCC `long long int`. No parse of the text can recover what cl did not print.
/// (Nor is a pointer argument's spacing evened out -- `Global *` from clang
/// and cl, `Global*` from GCC -- since no tag has a reason to take one.)
/// A tag whose name must read the same on every compiler, and a template
/// specialization with such arguments is one, customizes its spelling
/// through `TagName` (`tag.hpp`).
template <std::size_t Capacity>
[[nodiscard]] consteval NormalizedTypeName<Capacity> normalized_type_name(std::string_view argument) noexcept
{
    NormalizedTypeName<Capacity> result {};
    // Where the component being written began, per level of `<` nesting.
    std::array<std::size_t, Capacity + 1> componentStart {};
    std::size_t depth = 0;

    auto const push = [&](char c) {
        if (result.size < Capacity)
            result.chars[result.size++] = c;
    };

    std::size_t position = 0;
    while (position < argument.size())
    {
        char const c = argument[position];
        bool const atComponentStart = result.size == componentStart[depth];

        if (atComponentStart
            && (starts_with_word(argument, position, "struct") || starts_with_word(argument, position, "class")
                || starts_with_word(argument, position, "union") || starts_with_word(argument, position, "enum")))
        {
            while (argument[position] != ' ')
                ++position;
            ++position;
        }
        else if (argument.substr(position).starts_with("::"))
        {
            result.size = componentStart[depth];
            position += 2;
        }
        else if (c == '<')
        {
            push(c);
            ++depth;
            componentStart[depth] = result.size;
            ++position;
        }
        else if (c == '>')
        {
            if (result.size > 0 && result.chars[result.size - 1] == ' ')
                --result.size;
            push(c);
            if (depth > 0)
                --depth;
            ++position;
        }
        else if (c == ',')
        {
            push(',');
            push(' ');
            ++position;
            while (position < argument.size() && argument[position] == ' ')
                ++position;
            componentStart[depth] = result.size;
        }
        else
        {
            push(c);
            ++position;
        }
    }
    return result;
}

/// The first `Size - 1` bytes of @p name, followed by a nul.
template <std::size_t Size, std::size_t Capacity>
[[nodiscard]] consteval std::array<char, Size> trimmed(NormalizedTypeName<Capacity> const& name) noexcept
{
    std::array<char, Size> chars {};
    for (std::size_t index = 0; index + 1 < Size && index < name.size; ++index)
        chars[index] = name.chars[index];
    return chars;
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

    /// Room for the normalized name: the argument's own size, plus one byte
    /// for every comma, since `,` may become `, `.
    static constexpr std::size_t capacity = argument.size() * 2 + 1;

    /// The normalized name, before it is trimmed to its size.
    static constexpr NormalizedTypeName<capacity> normalized = normalized_type_name<capacity>(argument);

    /// The name's bytes, trimmed, and nul-terminated for a debugger's sake.
    static constexpr std::array<char, normalized.size + 1> chars = trimmed<normalized.size + 1>(normalized);
};

/// The name of class type @p T as written in its declaration, unqualified --
/// `Cylinder`, never `specimen::Cylinder` or `(anonymous namespace)::Cylinder`
/// -- with a template specialization's arguments kept and spelt as
/// `normalized_type_name` describes. Empty when the compiler's signature is
/// not in the shape `type_argument_text` expects, so that the caller falls
/// back rather than shows a fragment.
///
/// Every view this returns points into `TypeNameStorage<T>::chars`, which has
/// static storage duration.
template <typename T>
[[nodiscard]] consteval std::string_view reflected_type_name() noexcept
{
    return std::string_view { TypeNameStorage<T>::chars.data(), TypeNameStorage<T>::normalized.size };
}

} // namespace formula::detail
