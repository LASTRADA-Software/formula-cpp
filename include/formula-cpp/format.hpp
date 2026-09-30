// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// `std::format` for a `Rational` and a `Measured<Q>`: `std::format("{}",
/// Rational { 3, 5 })` is `0.6`, and a measured 5.2 in a unit whose symbol is
/// `kJ` formats as `5.2 kJ`.
///
/// **Opt-in.** This header is not included by `formula.hpp`: it includes
/// `<format>`, which the umbrella deliberately keeps out, so that a consumer
/// who only evaluates numbers does not compile it in every translation unit.
///
///     #include <formula-cpp/format.hpp>
///
/// **Include it in every translation unit that formats a `Rational` or a
/// `Measured`, or asks whether it can** (`std::formattable`). What it
/// declares are explicit specialisations of `std::formatter`, and an explicit
/// specialisation must be seen before any use that would otherwise
/// instantiate the primary template. A translation unit that asks without it
/// gets `std::formatter`'s disabled primary for a type another translation
/// unit formats, and a program whose translation units disagree on that is
/// ill-formed, with no diagnostic required.
///
/// **One rule, the library's throughout** (`number_text.hpp`): a decimal is
/// written only when it is the exact value, and a rounded one only when the
/// format asks for it by naming a rounding mode. `{}` of `1/3` is `1/3`, never
/// a decimal that is not the value; `{:~.3HalfEven}` of it is `≈0.333`, the
/// `≈` saying it was rounded; `{:.3HalfEven}` is `0.333`, a rounding the
/// format asked for outright.
///
/// **The reference is on the two specialisations**,
/// `std::formatter<formula::Rational, char>` and
/// `std::formatter<formula::Measured<Q>, char>`: the spec's grammar, one
/// example per form with the text it writes, the seven rounding-mode names
/// and why no mode is assumed, how the width counts, how a spec the grammar
/// does not allow fails, and when writing a value throws. The guide
/// `docs/display.md`, section "Formatting with `std::format`", sets it out
/// for a reader with the output of a real program beside each form.
///
/// **The library owns these two specialisations of `std::formatter`.** A
/// consumer who specialises `std::formatter<formula::Rational, char>` or
/// `std::formatter<formula::Measured<Q>, char>` as well defines one entity
/// twice, which breaks the one-definition rule. Only `char` formatting is
/// provided: a unit's symbol is UTF-8 bytes.

#include <formula-cpp/dimension.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/number_text.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/unit.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace formula::detail
{
/// Refuses a number format that rounds but names no rounding mode: `{:.2}`,
/// `{:~.3}`, `{:~}`. Not `constexpr`, so that a literal format string calling
/// it fails to compile, naming it; at run time it throws.
/// @throws std::format_error always.
[[noreturn]] inline void formula_number_format_needs_a_rounding_mode()
{
    throw std::format_error(
        "formula: this number format rounds but names no rounding mode -- write one after the places, as in "
        "{:.2HalfEven}; there is no default, because one number rounds differently under different methods");
}

/// Refuses a number format that rounds to more than 18 decimal places, or
/// that rounds a `Measured` at its unit's decimals when those lie outside -18
/// to 18. Not `constexpr`, for the reason
/// `formula_number_format_needs_a_rounding_mode` gives.
/// @throws std::format_error always.
[[noreturn]] inline void formula_number_format_places_out_of_range()
{
    throw std::format_error(
        "formula: a number format rounds to 0 to 18 decimal places, and a unit's declared decimals must lie "
        "within -18 to 18 for ~Mode to round at them -- write .0 to .18");
}

/// Refuses a number format the grammar does not allow (see the two
/// `std::formatter` specialisations below). Not `constexpr`, for the reason
/// `formula_number_format_needs_a_rounding_mode` gives.
/// @throws std::format_error always.
[[noreturn]] inline void formula_number_format_spec_not_understood()
{
    throw std::format_error(
        "formula: this number format is not one formula-cpp understands -- after an optional fill and "
        "alignment and a width, write nothing, /, .N and a rounding mode, or ~ with an optional .N and a "
        "rounding mode (~ without .N only for a Measured value, whose unit declares the places)");
}

/// Refuses to write a value the format could not spell: `~Mode` on a
/// `Measured` whose unit declares negative decimals, for a value it must
/// round (one with no exact decimal of at most 18 places) that exact
/// arithmetic cannot divide by 10^-decimals (see `formatter<Measured<Q>>`)
/// -- the one case the parser's checks cannot see. Only `format` reaches it.
/// @throws std::format_error always.
[[noreturn]] inline void number_format_failed(ArithmeticError spellingFailure)
{
    if (spellingFailure == ArithmeticError::Overflow)
        throw std::format_error(
            "formula: this number cannot be spelled as the format asks: overflow in exact arithmetic");
    throw std::format_error("formula: this number cannot be spelled as the format asks");
}

/// What a number format's body asks for (see the grammar on the
/// `std::formatter` specialisations below).
enum class NumberFormatBody : std::uint8_t
{
    /// Nothing: the exact decimal, else the fraction.
    ExactOrFraction,
    /// `/`: the fraction.
    Fraction,
    /// `.N Mode`: rounded to N places, padded, unmarked.
    Rounded,
    /// `~[.N] Mode`: the exact decimal, else rounded and marked `≈`.
    Approximated,
};

/// Where a formatted number sits within its width.
enum class NumberFormatAlign : std::uint8_t
{
    /// `<`: the text first, the fill after it.
    Left,
    /// `>`: the fill first -- the default.
    Right,
    /// `^`: the fill split around the text, the odd one after it.
    Centre,
};

/// A number format spec, parsed (`parse_number_format`).
struct NumberFormatSpec
{
    /// The fill's UTF-8 bytes, `fillLength` of them.
    char fill[4] { ' ', '\0', '\0', '\0' };
    /// How many of `fill`'s bytes are the fill: 1 to 4.
    std::size_t fillLength = 1;
    /// Where the text sits within `minimumWidth`.
    NumberFormatAlign align = NumberFormatAlign::Right;
    /// The width in code points the text is filled to; 0 for none.
    std::size_t minimumWidth = 0;
    /// What is written.
    NumberFormatBody body = NumberFormatBody::ExactOrFraction;
    /// The places `.N` named, if it did.
    std::optional<int> places {};
    /// The rounding mode named; meaningful for `Rounded` and `Approximated`.
    RoundingMode roundingMode = RoundingMode::HalfEven;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(NumberFormatSpec const&) const noexcept = default;
};

/// A rounding mode's name as a format spec spells it: its enumerator's.
struct RoundingModeName
{
    /// The name.
    std::string_view name;
    /// The mode it names.
    RoundingMode roundingMode;
};

/// The seven names a format spec accepts, one per `RoundingMode`.
inline constexpr RoundingModeName RoundingModeNames[] {
    { "HalfAwayFromZero", RoundingMode::HalfAwayFromZero },
    { "HalfTowardZero", RoundingMode::HalfTowardZero },
    { "HalfEven", RoundingMode::HalfEven },
    { "Ceiling", RoundingMode::Ceiling },
    { "Floor", RoundingMode::Floor },
    { "TowardZero", RoundingMode::TowardZero },
    { "AwayFromZero", RoundingMode::AwayFromZero },
};

/// The number of bytes of the Unicode scalar value @p utf8Text starts with,
/// in well-formed UTF-8, or 0 when it starts with none: a stray continuation
/// byte, a sequence cut short, an overlong encoding (a lead byte of 0xC0 or
/// 0xC1, or 0xE0 or 0xF0 followed by too small a second byte), a surrogate
/// (0xED followed by 0xA0 or more), or a value past U+10FFFF (0xF4 followed
/// by 0x90 or more, or a lead byte of 0xF5 or more).
[[nodiscard]] constexpr std::size_t scalar_value_length(std::string_view utf8Text) noexcept
{
    if (utf8Text.empty())
        return 0;
    auto const byteOf = [utf8Text](std::size_t byteAt) { return static_cast<unsigned char>(utf8Text[byteAt]); };
    unsigned const lead = byteOf(0);
    if (lead < 0x80U)
        return 1;
    std::size_t encodedLength = 0;
    unsigned lowestSecond = 0x80U;
    unsigned highestSecond = 0xBFU;
    if (lead >= 0xC2U && lead <= 0xDFU)
        encodedLength = 2;
    else if (lead >= 0xE0U && lead <= 0xEFU)
    {
        encodedLength = 3;
        lowestSecond = lead == 0xE0U ? 0xA0U : lowestSecond;
        highestSecond = lead == 0xEDU ? 0x9FU : highestSecond;
    }
    else if (lead >= 0xF0U && lead <= 0xF4U)
    {
        encodedLength = 4;
        lowestSecond = lead == 0xF0U ? 0x90U : lowestSecond;
        highestSecond = lead == 0xF4U ? 0x8FU : highestSecond;
    }
    else
        return 0;
    if (utf8Text.size() < encodedLength || byteOf(1) < lowestSecond || byteOf(1) > highestSecond)
        return 0;
    for (std::size_t continuationAt = 2; continuationAt < encodedLength; ++continuationAt)
        if ((byteOf(continuationAt) & 0xC0U) != 0x80U)
            return 0;
    return encodedLength;
}

/// How many code points @p utf8Text holds: its bytes that do not continue a
/// character.
[[nodiscard]] constexpr std::size_t code_points(std::string_view utf8Text) noexcept
{
    std::size_t codePointCount = 0;
    for (char const glyph: utf8Text)
        if ((static_cast<unsigned char>(glyph) & 0xC0U) != 0x80U)
            ++codePointCount;
    return codePointCount;
}

/// Whether @p glyph is an alignment: `<`, `>` or `^`.
[[nodiscard]] constexpr bool is_number_format_align(char glyph) noexcept
{
    return glyph == '<' || glyph == '>' || glyph == '^';
}

/// @p specText -- the part of a replacement field after its `:`, up to its
/// `}` -- parsed as the specialisations' grammar says. Refuses, by calling
/// the guard named for the mistake, anything that grammar does not allow: at
/// compile time that is a compile error, at run time a `std::format_error`.
///
/// Whether a body fits the type formatted -- `~Mode` without `.N` needs a
/// `Measured` -- is the formatter's to check, not this function's.
[[nodiscard]] constexpr NumberFormatSpec parse_number_format(std::string_view specText)
{
    NumberFormatSpec parsed {};
    std::size_t at = 0;

    // A fill is one Unicode scalar value followed by an alignment; an
    // alignment alone is one too. Bytes that are no scalar value are no fill,
    // and nothing else the grammar allows starts with them: they are refused
    // below, as not understood.
    if (!specText.empty())
    {
        std::size_t const fillLength = scalar_value_length(specText);
        if (fillLength > 0 && fillLength < specText.size() && is_number_format_align(specText[fillLength]))
        {
            if (specText[0] == '{' || specText[0] == '}')
                formula_number_format_spec_not_understood();
            for (std::size_t byteAt = 0; byteAt < fillLength; ++byteAt)
                parsed.fill[byteAt] = specText[byteAt];
            parsed.fillLength = fillLength;
            at = fillLength;
        }
        if (at < specText.size() && is_number_format_align(specText[at]))
        {
            parsed.align = specText[at] == '<'   ? NumberFormatAlign::Left
                           : specText[at] == '^' ? NumberFormatAlign::Centre
                                                 : NumberFormatAlign::Right;
            ++at;
        }
    }

    // A width: a positive whole number of at most nine digits. A leading
    // zero would be `std::format`'s zero-padding, which a fraction cannot
    // take.
    if (at < specText.size() && specText[at] >= '1' && specText[at] <= '9')
    {
        std::size_t widthDigits = 0;
        while (at < specText.size() && specText[at] >= '0' && specText[at] <= '9')
        {
            if (widthDigits == 9)
                formula_number_format_spec_not_understood();
            parsed.minimumWidth = parsed.minimumWidth * 10 + static_cast<std::size_t>(specText[at] - '0');
            ++widthDigits;
            ++at;
        }
    }

    if (at == specText.size())
        return parsed;

    if (specText[at] == '/')
    {
        if (at + 1 != specText.size())
            formula_number_format_spec_not_understood();
        parsed.body = NumberFormatBody::Fraction;
        return parsed;
    }

    if (specText[at] == '~')
    {
        parsed.body = NumberFormatBody::Approximated;
        ++at;
    }
    else if (specText[at] == '.')
        parsed.body = NumberFormatBody::Rounded;
    else
        formula_number_format_spec_not_understood();

    // `.N`: required after nothing but itself, optional after `~`.
    if (at < specText.size() && specText[at] == '.')
    {
        ++at;
        if (at == specText.size() || specText[at] < '0' || specText[at] > '9')
            formula_number_format_spec_not_understood();
        int placesAsked = 0;
        bool beyondRange = false;
        while (at < specText.size() && specText[at] >= '0' && specText[at] <= '9')
        {
            if (!beyondRange)
                placesAsked = placesAsked * 10 + (specText[at] - '0');
            beyondRange = beyondRange || placesAsked > 18;
            ++at;
        }
        if (beyondRange)
            formula_number_format_places_out_of_range();
        parsed.places = placesAsked;
    }

    std::string_view const modeText = specText.substr(at);
    if (modeText.empty())
        formula_number_format_needs_a_rounding_mode();
    for (RoundingModeName const& named: RoundingModeNames)
        if (named.name == modeText)
        {
            parsed.roundingMode = named.roundingMode;
            return parsed;
        }
    formula_number_format_spec_not_understood();
}

/// A replacement field's spec, parsed: the text from @p parseContext's
/// beginning up to its `}` (`parse_number_format`), and where it ended, which
/// is what `std::formatter::parse` returns.
[[nodiscard]] constexpr std::format_parse_context::iterator parse_number_format_field(
    std::format_parse_context& parseContext, NumberFormatSpec& parsed)
{
    auto specEnd = parseContext.begin();
    while (specEnd != parseContext.end() && *specEnd != '}')
        ++specEnd;
    parsed = parse_number_format(std::string_view { parseContext.begin(), specEnd });
    return specEnd;
}

/// @p shownValue, a number in @p shownIn, spelled as @p formatSpec's body
/// asks -- the number alone, without the unit's symbol. Throws
/// `std::format_error` when it cannot be spelled (`number_format_failed`).
[[nodiscard]] inline NumberText spell_formatted_number(Rational shownValue,
                                                       Unit const& shownIn,
                                                       NumberFormatSpec const& formatSpec)
{
    auto const spelling = [&]() -> std::expected<NumberText, ArithmeticError> {
        switch (formatSpec.body)
        {
            case NumberFormatBody::Fraction:
                return fraction_text(shownValue);
            case NumberFormatBody::Rounded:
                return checked_decimal_text(shownValue,
                                            DecimalPlaces { formatSpec.places.value_or(0) },
                                            formatSpec.roundingMode,
                                            DecimalPadding::Padded);
            case NumberFormatBody::Approximated: {
                Unit roundedIn = shownIn;
                if (formatSpec.places.has_value())
                    roundedIn.decimals = *formatSpec.places;
                return checked_number_text(shownValue,
                                           NumberStyle::approximate_decimal(formatSpec.roundingMode),
                                           roundedIn);
            }
            case NumberFormatBody::ExactOrFraction:
                break;
        }
        return checked_number_text(shownValue, NumberStyle::exact_decimal(), shownIn);
    };
    std::expected<NumberText, ArithmeticError> const spelled = spelling();
    if (!spelled)
        number_format_failed(spelled.error());
    return *spelled;
}

/// Writes @p numberPart, then -- when @p unitSymbol is not empty -- a space
/// and @p unitSymbol, to @p destination, filled and aligned as @p formatSpec
/// says, the width counted in code points.
template <typename OutputIterator>
[[nodiscard]] OutputIterator write_formatted_number(std::string_view numberPart,
                                                    std::string_view unitSymbol,
                                                    NumberFormatSpec const& formatSpec,
                                                    OutputIterator destination)
{
    std::size_t const shownWidth = code_points(numberPart) + (unitSymbol.empty() ? 0 : 1 + code_points(unitSymbol));
    std::size_t const padding = formatSpec.minimumWidth > shownWidth ? formatSpec.minimumWidth - shownWidth : 0;
    std::size_t const paddingBefore = formatSpec.align == NumberFormatAlign::Left    ? 0
                                      : formatSpec.align == NumberFormatAlign::Right ? padding
                                                                                     : padding / 2;
    auto const writeFill = [&](std::size_t fillCount) {
        for (std::size_t filled = 0; filled < fillCount; ++filled)
            for (std::size_t byteAt = 0; byteAt < formatSpec.fillLength; ++byteAt)
                *destination++ = formatSpec.fill[byteAt];
    };
    writeFill(paddingBefore);
    for (char const glyph: numberPart)
        *destination++ = glyph;
    if (!unitSymbol.empty())
    {
        *destination++ = ' ';
        for (char const glyph: unitSymbol)
            *destination++ = glyph;
    }
    writeFill(padding - paddingBefore);
    return destination;
}

/// Reads a `Measured<Q>` or `Outcome<Q>` replacement field's spec, as
/// `parse_number_format_field` does, and refuses `~Mode` without `.N` when
/// @p Q's unit declares decimals outside the -18 to 18 that `DecimalPlaces`
/// spans.
template <Described Q>
[[nodiscard]] constexpr std::format_parse_context::iterator parse_measured_format_field(
    std::format_parse_context& parseContext, NumberFormatSpec& parsed)
{
    auto const specEnd = parse_number_format_field(parseContext, parsed);
    constexpr int declaredPlaces = Describe<Q>::unit.decimals;
    if (parsed.body == NumberFormatBody::Approximated && !parsed.places.has_value()
        && (declaredPlaces > 18 || declaredPlaces < -18))
        formula_number_format_places_out_of_range();
    return specEnd;
}

/// Writes @p shownMeasured to @p destination as @p formatSpec says: the
/// number in @p Q's declared unit and its symbol, or `(not measured)` when it
/// is absent. Throws `std::format_error` when the number cannot be spelled as
/// asked (`spell_formatted_number`).
template <Described Q, typename OutputIterator>
[[nodiscard]] OutputIterator format_measured(Measured<Q> const& shownMeasured,
                                             NumberFormatSpec const& formatSpec,
                                             OutputIterator destination)
{
    if (shownMeasured.is_absent())
        return write_formatted_number(NotMeasuredText, std::string_view {}, formatSpec, destination);
    Unit const shownIn = Describe<Q>::unit;
    NumberText const spelled = spell_formatted_number(*shownMeasured.stored(), shownIn, formatSpec);
    return write_formatted_number(spelled.view(), view(shownIn.symbolText), formatSpec, destination);
}

/// Appends @p baseName and @p exponentValue to @p spelled as `L^2` or
/// `L^(1/2)`, after a space when @p spelled is not empty; nothing when the
/// exponent is zero.
inline void append_exponent_text(std::string& spelled, std::string_view baseName, Exponent exponentValue)
{
    if (is_zero(exponentValue))
        return;
    if (!spelled.empty())
        spelled += ' ';
    spelled += baseName;
    spelled += '^';
    if (is_integer(exponentValue))
        spelled += std::to_string(exponentValue.numerator);
    else
        spelled += '(' + std::to_string(exponentValue.numerator) + '/' + std::to_string(exponentValue.denominator) + ')';
}

/// A dimension's exponents, as `std::format` writes them -- see
/// `formatter<Dimension>`.
[[nodiscard]] inline std::string dimension_text(Dimension const& shownDimension)
{
    std::string spelled;
    append_exponent_text(spelled, "L", shownDimension.length);
    append_exponent_text(spelled, "M", shownDimension.mass);
    append_exponent_text(spelled, "T", shownDimension.time);
    append_exponent_text(spelled, "I", shownDimension.current);
    append_exponent_text(spelled, "Theta", shownDimension.temperature);
    append_exponent_text(spelled, "N", shownDimension.amount);
    append_exponent_text(spelled, "J", shownDimension.luminosity);
    for (NamedBase const& namedBase: shownDimension.namedBases)
        append_exponent_text(spelled, view(namedBase.name), namedBase.exponent);
    if (is_dimensionless(shownDimension))
        spelled = "(dimensionless)";
    return spelled;
}
} // namespace formula::detail

// The specialisations are declared inside `namespace std` rather than as
// `struct std::formatter<...>` at global scope: both are standard C++, but
// Doxygen 1.9.8 finds no scope for the qualified form and fails the API build.
namespace std
{
/// `std::format` of a `formula::Rational`, in the spellings `number_text`
/// gives: a decimal only where it is the exact value, and a rounded one only
/// where the format names a rounding mode.
///
///     spec  ::= [[fill] align] [width] [body]         fill: one Unicode scalar value; align: < > ^ (default >)
///     body  ::= ''                   exact decimal, else fraction      0.6   1/3    157.4 g
///             | '/'                  fraction                           3/5   1/3
///             | '.' N Mode           rounded to N (0..18), padded       {:.2HalfEven} -> 118.26
///             | '~' ['.' N] Mode     exact where exact, else ≈ rounded  {:~.3HalfEven} -> ≈0.333
///     Mode  ::= HalfAwayFromZero | HalfTowardZero | HalfEven | Ceiling | Floor | TowardZero | AwayFromZero
///
/// One example per form, each call with the text it produces:
///
///     std::format("{}", Rational { 3, 5 })                          0.6
///     std::format("{}", Rational { 1, 3 })                          1/3
///     std::format("{:/}", Rational { 3, 5 })                        3/5
///     std::format("{:.2HalfEven}", Rational { 23653, 200 })         118.26
///     std::format("{:.2HalfAwayFromZero}", Rational { 23653, 200 }) 118.27
///     std::format("{:.2HalfEven}", Rational { 4 })                  4.00
///     std::format("{:~.3HalfEven}", Rational { 1, 3 })              ≈0.333
///     std::format("{:~.3HalfEven}", Rational { 3, 5 })              0.6
///     std::format("{:>8}", Rational { 3, 5 })                       "     0.6"
///     std::format("{:*^7}", Rational { 3, 5 })                      **0.6**
///
/// `.N Mode` asks for a rounding outright, so it pads to N places and writes
/// no `≈`, even when rounding changed the value. `~.N Mode` writes the exact
/// decimal where the value has one, and otherwise rounds to N places and
/// marks the result `≈`. A `Rational` has no unit to take the places from,
/// so `~` needs `.N`: `{:~HalfEven}` is refused.
///
/// **The modes** are `RoundingMode`'s enumerators, spelled exactly as they
/// are there. **There is no default mode**: the same number rounds
/// differently under different methods -- 2.5 is 3 under `HalfAwayFromZero`
/// and 2 under `HalfEven` -- and which one applies is for the method's author
/// to decide, not for a format to assume.
///
/// **Width counts code points, not bytes**: `{:>8~.3HalfEven}` of 1/3 is
/// `"  ≈0.333"`, two fill characters, although `≈` is three bytes. The fill
/// is one Unicode scalar value in well-formed UTF-8, any but `{` and `}`;
/// `<` aligns left, `>` right (the default) and `^` in the middle, the odd
/// fill character going after the text. The width is at most nine digits,
/// written in the spec.
///
/// **A bad spec** calls the guard named for the mistake:
/// `formula_number_format_needs_a_rounding_mode` for a rounding with no mode
/// (`{:.2}`), `formula_number_format_places_out_of_range` for more than 18
/// places (`{:.19HalfEven}`), and `formula_number_format_spec_not_understood`
/// for anything else the grammar does not allow (`{:x}`, `{:08}`, a width
/// from an argument `{:{}}`, `{:~HalfEven}`). In a literal format string that
/// is a compile error naming the guard; under `std::vformat` the guard throws
/// `std::format_error`, whose `what()` starts `formula: `.
///
/// The guide, `docs/display.md`, section "Formatting with `std::format`",
/// sets this reference out with a real program's output beside each form.
///
/// Owned by this library: a consumer's own specialisation of it would define
/// it twice, which breaks the one-definition rule.
template <>
struct formatter<formula::Rational, char>
{
    /// Reads the spec up to its `}`. A spec the grammar does not allow calls
    /// the guard named for the mistake -- a compile error in a literal format
    /// string, `std::format_error` under `std::vformat`.
    constexpr auto parse(std::format_parse_context& parseContext)
    {
        auto const specEnd = formula::detail::parse_number_format_field(parseContext, _spec);
        if (_spec.body == formula::detail::NumberFormatBody::Approximated && !_spec.places.has_value())
            formula::detail::formula_number_format_spec_not_understood();
        return specEnd;
    }

    /// Writes @p shown as the spec says. Every form spells a `Rational` at 0
    /// to 18 places, where exact arithmetic cannot overflow, so this does not
    /// throw; the spelling's result is checked rather than assumed all the
    /// same, as the `Measured` overload's must be.
    template <typename FormatContext>
    auto format(formula::Rational const& shown, FormatContext& formatContext) const
    {
        formula::NumberText const spelled = formula::detail::spell_formatted_number(shown, formula::unit::One, _spec);
        return formula::detail::write_formatted_number(spelled.view(), std::string_view {}, _spec, formatContext.out());
    }

  private:
    formula::detail::NumberFormatSpec _spec {};
};

/// `std::format` of a `formula::Measured<Q>`: the number in `Q`'s declared
/// unit, in the spellings `number_text` gives, then a space and the unit's
/// symbol when it has one -- or `(not measured)` when the value is absent,
/// whatever the spec's body.
///
///     spec  ::= [[fill] align] [width] [body]         fill: one Unicode scalar value; align: < > ^ (default >)
///     body  ::= ''                   exact decimal, else fraction      0.6   1/3    157.4 g
///             | '/'                  fraction                           3/5   1/3
///             | '.' N Mode           rounded to N (0..18), padded       {:.2HalfEven} -> 118.26
///             | '~' ['.' N] Mode     exact where exact, else ≈ rounded  {:~.3HalfEven} -> ≈0.333
///     Mode  ::= HalfAwayFromZero | HalfTowardZero | HalfEven | Ceiling | Floor | TowardZero | AwayFromZero
///
/// One example per form, each call with the text it produces -- here with
/// `Q` declared in `unit::Kilojoule`, whose symbol is `kJ` and which declares
/// one decimal:
///
///     std::format("{}", Measured<Q> { Rational { 26, 5 } })            5.2 kJ
///     std::format("{}", Measured<Q> { Rational { 1, 3 } })             1/3 kJ
///     std::format("{:/}", Measured<Q> { Rational { 26, 5 } })          26/5 kJ
///     std::format("{:.3HalfEven}", Measured<Q> { Rational { 26, 5 } }) 5.200 kJ
///     std::format("{:~HalfEven}", Measured<Q> { Rational { 1, 3 } })   ≈0.3 kJ
///     std::format("{:~.3HalfEven}", Measured<Q> { Rational { 1, 3 } }) ≈0.333 kJ
///     std::format("{:>10}", Measured<Q> { Rational { 26, 5 } })        "    5.2 kJ"
///     std::format("{}", Measured<Q>::absent())                         (not measured)
///
/// `{}` and `{:/}` spell what `number_text` does with
/// `NumberStyle::exact_decimal()` and `NumberStyle::fraction()`. `.N Mode`
/// asks for a rounding outright, so it pads to N places and writes no `≈`.
/// `~Mode` without `.N` rounds at the decimals `Q`'s unit declares, exactly as
/// `number_text(measured, NumberStyle::approximate_decimal(Mode))` does, and
/// `~.N Mode` at N places instead; both write the exact decimal where the
/// value has one, and mark a rounding `≈`.
///
/// **The modes** are `RoundingMode`'s enumerators, spelled exactly as they
/// are there. **There is no default mode**: the same number rounds
/// differently under different methods -- 2.5 is 3 under `HalfAwayFromZero`
/// and 2 under `HalfEven` -- and which one applies is for the method's author
/// to decide, not for a format to assume.
///
/// **Width counts code points, not bytes**, the symbol's included: `{:>8}` of
/// 21.3 in degrees Celsius is `" 21.3 °C"`, one fill character, although `°`
/// is two bytes. The fill is one Unicode scalar value in well-formed UTF-8,
/// any but `{` and `}`; `<` aligns left, `>` right (the default) and `^` in
/// the middle, the odd fill character going after the text. The width is at
/// most nine digits, written in the spec.
///
/// **A bad spec** calls the guard named for the mistake:
/// `formula_number_format_needs_a_rounding_mode` for a rounding with no mode
/// (`{:.2}`), `formula_number_format_places_out_of_range` for more than 18
/// places (`{:.19HalfEven}`) or for `~Mode` on a `Q` whose unit declares
/// decimals outside -18 to 18, and `formula_number_format_spec_not_understood`
/// for anything else the grammar does not allow (`{:x}`, `{:08}`, a width
/// from an argument `{:{}}`). In a literal format string that is a compile
/// error naming the guard; under `std::vformat` the guard throws
/// `std::format_error`, whose `what()` starts `formula: `.
///
/// **A value exact arithmetic cannot round** is refused when it is written,
/// not when the spec is read. `~Mode` on a `Q` whose unit declares negative
/// decimals writes a value with an exact decimal of at most 18 places as it
/// is -- 1/10^18 at -3 decimals is `0.000000000000000001` -- and rounds any
/// other by dividing it by 10^-decimals. For such a value whose denominator
/// times that power of ten, less any factor of it the numerator cancels,
/// exceeds the integer range, the division overflows --
/// `from_double_exact(0.1)` at -3 decimals is one -- and `format` throws
/// `std::format_error`, whose `what()` starts `formula: this number cannot be
/// spelled as the format asks`.
///
/// The guide, `docs/display.md`, section "Formatting with `std::format`",
/// sets this reference out with a real program's output beside each form.
///
/// Owned by this library: a consumer's own specialisation of it would define
/// it twice, which breaks the one-definition rule.
template <formula::Described Q>
struct formatter<formula::Measured<Q>, char>
{
    /// Reads the spec up to its `}`, as `formatter<Rational>` does. `~Mode`
    /// without `.N` rounds at `Q`'s declared decimals, so a unit whose declared
    /// decimals lie outside the -18 to 18 that `DecimalPlaces` spans is
    /// refused here.
    constexpr auto parse(std::format_parse_context& parseContext)
    {
        return formula::detail::parse_measured_format_field<Q>(parseContext, _spec);
    }

    /// Writes @p shown as the spec says, or `(not measured)` when it is
    /// absent.
    /// @throws std::format_error when `~Mode` rounds @p shown at a unit
    ///         declaring negative decimals and exact arithmetic overflows
    ///         there: for a value with no exact decimal of at most 18 places
    ///         -- one with such a decimal is written as it is, never rounded
    ///         -- whose denominator times 10^-decimals, less any factor of it
    ///         the numerator cancels, exceeds the integer range.
    template <typename FormatContext>
    auto format(formula::Measured<Q> const& shown, FormatContext& formatContext) const
    {
        return formula::detail::format_measured<Q>(shown, _spec, formatContext.out());
    }

  private:
    formula::detail::NumberFormatSpec _spec {};
};

/// `std::format` of a `formula::Outcome<Q>`, in the grammar of
/// `formatter<formula::Measured<Q>>`: a value as a `Measured<Q>` is written,
/// an empty outcome as `(not measured)`, and a verdict or an invalid outcome
/// as its label, filled, aligned and padded to the spec's width. A rounding in
/// the spec does not apply to words, but a spec the grammar does not allow is
/// refused as it is for a `Measured<Q>`.
///
///     std::format("{}", Outcome<Q>::value(Measured<Q> { Rational { 26, 5 } }, ValueSource::Derived))  5.2 kJ
///     std::format("{}", Outcome<Q>::empty())                                                           (not measured)
///     std::format("{:>18}", Outcome<Q>::verdict({ "repeat the test" }))                                "   repeat the test"
///
/// Owned by this library: a consumer's own specialisation of it would define
/// it twice, which breaks the one-definition rule.
template <formula::Described Q>
struct formatter<formula::Outcome<Q>, char>
{
    /// Reads the spec up to its `}`, as `formatter<formula::Measured<Q>>` does.
    constexpr auto parse(std::format_parse_context& parseContext)
    {
        return formula::detail::parse_measured_format_field<Q>(parseContext, _spec);
    }

    /// Writes @p shown as the spec says.
    /// @throws std::format_error as `formatter<formula::Measured<Q>>` does,
    ///         for a value.
    template <typename FormatContext>
    auto format(formula::Outcome<Q> const& shown, FormatContext& formatContext) const
    {
        if (shown.is_verdict())
            return formula::detail::write_formatted_number(
                shown.verdict_label(), std::string_view {}, _spec, formatContext.out());
        if (shown.is_invalid())
            return formula::detail::write_formatted_number(
                shown.reason_label(), std::string_view {}, _spec, formatContext.out());
        return formula::detail::format_measured<Q>(shown.measurement(), _spec, formatContext.out());
    }

  private:
    formula::detail::NumberFormatSpec _spec {};
};

/// `std::format` of a `formula::Unit`: its symbol, filled and aligned as a
/// string is.
template <>
struct formatter<formula::Unit, char>: formatter<string_view, char>
{
    /// Writes the symbol of @p shownIn.
    template <typename FormatContext>
    auto format(formula::Unit const& shownIn, FormatContext& formatContext) const
    {
        return formatter<string_view, char>::format(formula::view(shownIn.symbolText), formatContext);
    }
};

/// `std::format` of a `formula::Dimension`: its exponents joined by spaces,
/// `L^2 M^-3` or `L^(1/2)` (base names `L`, `M`, `T`, `I`, `Theta`, `N`, `J`,
/// each left out at exponent 0), then any named base by its name, and
/// `(dimensionless)` for a pure number. Filled and aligned as a string is.
template <>
struct formatter<formula::Dimension, char>: formatter<string_view, char>
{
    /// Writes @p shown.
    template <typename FormatContext>
    auto format(formula::Dimension const& shown, FormatContext& formatContext) const
    {
        std::string const spelled = formula::detail::dimension_text(shown);
        return formatter<string_view, char>::format(spelled, formatContext);
    }
};

/// `std::format` of a formula enumeration that `detail::formats_by_describe`
/// lists: its `describe()` words, filled and aligned as a string is. The
/// enumeration's own header must be included.
template <typename E>
    requires formula::detail::formats_by_describe<E>
struct formatter<E, char>: formatter<string_view, char>
{
    /// Writes `describe(shown)`.
    template <typename FormatContext>
    auto format(E shown, FormatContext& formatContext) const
    {
        return formatter<string_view, char>::format(describe(shown), formatContext);
    }
};
} // namespace std
