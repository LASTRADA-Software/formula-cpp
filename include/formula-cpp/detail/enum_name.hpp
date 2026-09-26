// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// An enumerator's name as written in the source, recovered at compile time.
///
/// Adapted from `detail::GetName` in `contour-terminal/reflection-cpp`
/// (`include/reflection-cpp/reflection.hpp`), which is licensed Apache-2.0,
/// the same licence as this repository. Only the enumerator case is taken,
/// and its parsing is rewritten; see `reflected_enumerator_name` for what
/// changed and why.
///
/// The public face of this is `formula::enumerator_name` (`enumerator.hpp`),
/// which consults the author's `EnumeratorName` customization first. Nothing
/// outside that header should call into here directly.

#include <formula-cpp/detail/name_text.hpp>

#include <cstddef>
#include <string_view>
#include <type_traits>

namespace formula::detail
{

/// The compiler's own spelling of this function's signature, which names the
/// template argument @p E -- `auto formula::detail::enumerator_signature()
/// [E = Shape::Cube]` from clang and GCC, `auto __cdecl
/// formula::detail::enumerator_signature<Shape::Cube>(void)` from cl.
///
/// **The return type is `auto`, and that is load bearing.** Spelled
/// `std::string_view`, GCC 13 appends the typedef it expanded --
/// `[with auto E = Shape::Cube; std::string_view =
/// std::basic_string_view<char>]` -- so the text before the final `]` is no
/// longer the argument. With `auto` -- as the reference's own function is
/// declared -- there is nothing to expand. **Not `noexcept` either, for the same
/// kind of reason:** cl appends ` noexcept` to `__FUNCSIG__` after the
/// parameter list, which would move the tail `reflected_enumerator_name`
/// looks for. Both measured on cl 19.51, clang-cl 22, clang++ 20 and g++ 13.
///
/// `__FUNCSIG__` on cl and `__PRETTY_FUNCTION__` everywhere else, rather than
/// `std::source_location::function_name()`. On cl the two are identical; on
/// clang-cl they are not (`source_location` adds `__cdecl` and `(void)`),
/// though both end in the same `[E = ...]`. The builtins need no header, and
/// on clang-cl `__PRETTY_FUNCTION__` is the same clang format the non-cl
/// branch of the parse already reads.
template <auto E>
[[nodiscard]] consteval auto enumerator_signature()
{
#if defined(_MSC_VER) && !defined(__clang__)
    return std::string_view { __FUNCSIG__ };
#else
    return std::string_view { __PRETTY_FUNCTION__ };
#endif
}

/// The name of enumerator @p E as written in its declaration -- `Cylinder`,
/// never `Shape::Cylinder` -- or an empty view when @p E is a value that
/// names no enumerator, such as `static_cast<Shape>(9)`.
///
/// **Parsed from the end, not from the front.** Everything up to the
/// template argument is the compiler's business (return type, calling
/// convention, the enumeration's qualification, which differs per compiler
/// for an anonymous namespace: `(anonymous namespace)::` from clang,
/// `<unnamed>::` from GCC, `anonymous-namespace'::` behind a backtick from
/// cl), while the text after it is a fixed tail per compiler: `]` from clang
/// and GCC, `>(void)` from cl. So the tail is stripped, and the name is the
/// run of identifier bytes that ends there. That run stops at the `::` before
/// the enumerator, at the space after `E =` for an unscoped enumerator (every
/// compiler prints `PlainB`, not `Plain::PlainB`), or at the `<` before it on
/// cl. The reference reads forward from `E = ` to the first `]` instead,
/// which keeps the qualification on clang and GCC (`Shape::Cylinder`), and
/// cuts a name short at a `]` inside the enumeration's own qualification
/// (`Tmpl<int[3]>::E`).
///
/// **A value that names no enumerator is printed as a cast**: `(Shape)9` from
/// clang and GCC, `(enum Shape)0x9` from cl, `(Neg)-4` and `(enum Neg)0xfc`
/// for a negative one. Two rules turn each of those into an empty answer
/// rather than a fragment of it, and each is needed on its own:
///
///   - **A run right after a `)` is the value of a cast.** Usually that run is
///     a number, but not always: cl prints a `bool`-based enumeration's
///     non-enumerator as `(enum Flag)true`, whose run is the identifier-like
///     `true`. Measured on cl 19.51; clang and GCC print `(Flag)1`.
///   - **A run that begins with a digit is a number.** An identifier cannot,
///     and a negative value puts a `-` between the cast and its digits --
///     `(Neg)-4` on clang and GCC -- so the rule above does not see it.
///
/// Two enumerators with the same value are the same template argument, so
/// the compiler prints the first one declared for both: `Alias::Second`,
/// declared after `Alias::First = 1` with the same value, is named `First`.
/// That is a property of the language, not of this parse, and all four
/// compilers agree on it.
///
/// A signature that does not end in the tail this expects yields an empty
/// view rather than a guess, so the caller falls back to the underlying value.
/// That is the only format change detected: a compiler that kept the tail but
/// spelled the argument differently would not be noticed here -- the test
/// suite is what would notice.
template <auto E>
    requires std::is_enum_v<decltype(E)>
[[nodiscard]] consteval std::string_view reflected_enumerator_name() noexcept
{
#if defined(_MSC_VER) && !defined(__clang__)
    constexpr std::string_view tail = ">(void)";
#else
    constexpr std::string_view tail = "]";
#endif

    std::string_view signature = enumerator_signature<E>();
    if (!signature.ends_with(tail))
        return {};
    signature.remove_suffix(tail.size());

    std::size_t start = signature.size();
    while (start > 0 && is_identifier_byte(signature[start - 1]))
        --start;

    std::string_view const name = signature.substr(start);
    if (name.empty() || (start > 0 && signature[start - 1] == ')'))
        return {};
    if (name.front() >= '0' && name.front() <= '9')
        return {};
    return name;
}

} // namespace formula::detail
