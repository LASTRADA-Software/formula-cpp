// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Byte-level helpers shared by the two places the library recovers a name
/// from the compiler and lets an author replace it: an enumerator's
/// (`enumerator.hpp`, `detail/enum_name.hpp`) and a method tag's (`tag.hpp`,
/// `detail/type_name.hpp`).
///
/// One statement of each rule, so that the two customization points cannot
/// come to disagree about what an identifier byte is or what makes a spelling
/// safe to keep.

#include <cstddef>
#include <string_view>

namespace formula::detail
{

/// True for a byte that can appear in an identifier the compiler printed: an
/// ASCII letter, a digit, an underscore, or any byte of a multi-byte UTF-8
/// sequence -- C++23 identifiers may be Unicode, and all four compilers print
/// `Uni::Größe` as UTF-8 rather than as a universal-character-name.
[[nodiscard]] constexpr bool is_identifier_byte(char c) noexcept
{
    auto const byte = static_cast<unsigned char>(c);
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9') || byte == '_'
           || byte >= 0x80;
}

/// Reads every character of @p text and answers `true`. Only interesting
/// inside a constant expression, where reading a character that is not
/// there -- a destroyed local, freed storage, a mutable static -- makes the
/// whole expression not a constant expression. The first gate `EnumeratorName`
/// and `TagName` both describe.
[[nodiscard]] constexpr bool every_character_readable(std::string_view text) noexcept
{
    std::size_t read = 0;
    for (char const c: text)
        read += c == '\0' ? 0 : 1;
    return read <= text.size();
}

} // namespace formula::detail
