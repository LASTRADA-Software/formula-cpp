// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A string that can be a non-type template parameter.

#include <cstddef>
#include <cstdlib>
#include <string_view>

namespace formula::detail
{

/// A compile-time string, sized to the literal it was built from.
///
/// Exists because a string literal cannot itself be a template argument, and a
/// quantity's symbol and description are part of its type. `N` counts the
/// terminator, so `FixedString { "abc" }` is a `FixedString<4>` whose `view()`
/// is three characters long.
///
/// All members public, deliberately: a class used as a non-type template
/// parameter must be *structural*, which means every non-static data member
/// public, recursively. Do not make `characters` private to "protect" it --
/// that would make this type unusable for its only purpose. The same reasoning,
/// and the same measurement, as `Exponent` in `dimension.hpp`.
///
/// Byte-oriented: `view().size()` is a count of bytes, not of characters, so a
/// multi-byte UTF-8 symbol reports its encoded length.
/// Deliberately NOT `constexpr`. Calling it makes the enclosing expression a
/// non-constant one, so a badly formed `FixedString` is a compile error at the
/// point of use rather than a value that quietly means something else -- and the
/// diagnostic names this function, which is why the name is a sentence. Same
/// mechanism as `exponent()` and `symbol()` use elsewhere in this library. It is
/// defined, not merely declared, because a runtime call must still link.
[[noreturn]] inline void formula_fixed_string_must_be_null_terminated()
{
    std::abort();
}

template <std::size_t N>
struct FixedString
{
    char characters[N] {};

    constexpr FixedString(char const (&text)[N]) noexcept
    {
        // The parameter is any char array, not only a string literal, and
        // `view()` below drops the last byte on the assumption that it is a
        // terminator. Given `char const raw[] = {'a','b','c'}` that assumption
        // is false and the 'c' disappears in silence -- measured on all three
        // compilers before this check existed. Refuse instead.
        if (text[N - 1] != '\0')
            formula_fixed_string_must_be_null_terminated();

        for (std::size_t index = 0; index < N; ++index)
            characters[index] = text[index];
    }

    /// The text without its terminator.
    [[nodiscard]] constexpr std::string_view view() const noexcept
    {
        return std::string_view { characters, N - 1 };
    }
};

template <std::size_t N>
FixedString(char const (&)[N]) -> FixedString<N>;

/// Compares the text, not the capacity.
///
/// Heterogeneous on purpose. `FixedString<4>` and `FixedString<5>` are different
/// types, so a defaulted member `operator==` would not compare them at all and
/// every comparison would silently depend on how long the literals happened to
/// be. Template-argument equivalence does not use this -- it compares members
/// directly -- so this is for callers, not for the language.
template <std::size_t A, std::size_t B>
[[nodiscard]] constexpr bool operator==(FixedString<A> const& lhs, FixedString<B> const& rhs) noexcept
{
    return lhs.view() == rhs.view();
}

} // namespace formula::detail
