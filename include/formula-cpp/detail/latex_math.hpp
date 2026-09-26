// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Text made safe to stand inside `\mathrm{...}` in LaTeX math mode: a unit's
/// symbol, a lookup row's words, a lookup key's name.
///
/// **Why math mode, and not `\text{...}`.** The site renders LaTeX with
/// MathJax 3.2.2, which does not load the `textmacros` extension, so the
/// contents of a `\text{...}` are shown exactly as written: `\text{fit\_2}`
/// reads `fit\_2` on the page, backslash and all. A real TeX engine needs the
/// escape -- a bare `_` in text mode is an error and a bare `%` a comment --
/// so no spelling inside `\text{...}` is right for both. Inside `\mathrm{...}`
/// both read the same escapes the same way, so everything a lookup or a unit
/// contributes goes there instead.

#include <string>
#include <string_view>

namespace formula::detail
{

/// @p authored, escaped for math mode, to be wrapped in `\mathrm{...}`.
///
///  - A space becomes `\ `: math mode ignores spaces, and `key Cylinder`
///    would otherwise read `keyCylinder`.
///  - `_`, `%`, `#`, `&`, `$`, `{` and `}` get a backslash. `%` is TeX's
///    comment character: a unit symbol `%` written bare commented out the
///    rest of the formula, closing braces and all.
///  - `\`, `^`, `~` and the backtick become `\backslash{}`, `\hat{}`,
///    `\tilde{}` and `\grave{}`: math mode has no text-mode
///    `\textbackslash`, and the other three would otherwise be a superscript,
///    a tie and, in the roman math font, an opening curly quote. The three
///    accents sit over nothing, which is how each reads as the character on
///    its own.
///  - `"` becomes `\mathtt{"}`: in the roman math font under OT1 it is a
///    closing curly quote, and the typewriter font has the straight one.
///  - `'` becomes `\text{'}`: in math mode it is a prime whatever the font,
///    and `''` a double prime. A lone apostrophe in `\text{...}` needs no
///    escape in either engine, so this is the one `\text` that is right for
///    both; TeX sets it as a right single quote, which is its apostrophe.
///
/// Every other byte is written as it is: letters, digits, `/`, `.`, `,`, `*`,
/// `-` (a minus, which is what it is in `-23/20`), `<`, `>`, `|` and any byte
/// of a multi-byte UTF-8 sequence. Math mode has no ligatures, so `--` stays
/// two characters without the empty group text mode needs.
///
/// Measured, not reasoned, on every character above and on every formula the
/// gallery publishes: each typesets with tectonic 0.17.0 under
/// `\usepackage[OT1]{fontenc}` and with MathJax 3.2.2 under the site's
/// configuration (`docs/javascripts/mathjax.js`), strictly -- with
/// `noundefined` removed, so an unknown macro is an error -- and MathJax
/// reads each back as written, `\backslash` as U+2216, which its font draws
/// as a backslash. The controls are the spellings this replaced:
/// a percent-valued row in `\text{...}` and a constant `5 %` stop tectonic
/// (`File ended while scanning use of \text@`, `Missing $ inserted`); MathJax
/// drops the constant's `%` as a comment, and shows `\text{key fit\_2}` as
/// `key fit\_2`.
[[nodiscard]] inline std::string latex_math_words(std::string_view authored)
{
    std::string escaped;
    escaped.reserve(authored.size());
    for (char const byte: authored)
    {
        switch (byte)
        {
            case ' ':
                escaped += "\\ ";
                break;
            case '_':
            case '%':
            case '#':
            case '&':
            case '$':
            case '{':
            case '}':
                escaped += '\\';
                escaped += byte;
                break;
            case '\\':
                escaped += "\\backslash{}";
                break;
            case '^':
                escaped += "\\hat{}";
                break;
            case '~':
                escaped += "\\tilde{}";
                break;
            case '`':
                escaped += "\\grave{}";
                break;
            case '"':
                escaped += "\\mathtt{\"}";
                break;
            case '\'':
                escaped += "\\text{'}";
                break;
            default:
                escaped += byte;
                break;
        }
    }
    return escaped;
}

} // namespace formula::detail
