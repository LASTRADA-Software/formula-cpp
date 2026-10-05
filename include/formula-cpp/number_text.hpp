// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A number spelled as text -- `3/5`, `0.6`, `118.26`, `≈0.333` -- into a
/// fixed buffer, without allocating and at compile time as well as at run
/// time.
///
/// Three notations, chosen by a `NumberStyle`: a fraction, which is always
/// exact; an exact decimal, shown only when it *is* the exact value and
/// otherwise falling back to the fraction; and an approximate decimal, which a
/// caller has to ask for by naming the rounding mode, which rounds at the
/// unit's declared decimals, and which always carries `ApproximationMarker`.
/// So a decimal without the marker is always the exact value, and a value
/// whose decimal expansion never ends is never passed off as one that does.
///
/// A core header: it includes no `<string>`, and the arithmetic is plain
/// 128-bit unsigned arithmetic on the value's magnitude and denominator, never
/// `Rational::make`, so spelling a number reports nothing to the overflow
/// census (`detail/checked_int.hpp`). The one exception is rounding to a
/// negative number of places, which is `checked_round`'s own arithmetic --
/// see `checked_decimal_text`.

#include <formula-cpp/error.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/unit.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>

namespace formula
{

/// Bytes of text a `NumberText` can hold. The longest text this header
/// spells is a fraction -- a sign, a 39-digit numerator, a slash, a 39-digit
/// denominator, a space and a unit symbol of `SymbolCapacity` bytes -- of 113
/// bytes, and a `static_assert` below keeps it within this. A fraction is
/// never marked approximate, and the longest marked decimal, at 18 places, is
/// shorter. A value in a dimensioned unit with no symbol is followed by its
/// coherent unit's spelling instead, which no such bound covers: a text that
/// would not fit is refused with `ArithmeticError::Overflow`.
inline constexpr std::size_t NumberTextCapacity = 128;

/// The one spelling of "approximately": U+2248, `≈`, in UTF-8. Not `~`: a
/// pair of those is GFM strikethrough, as `render.hpp`'s Markdown escaping
/// notes.
inline constexpr std::string_view ApproximationMarker = "\xe2\x89\x88";

/// The one spelling of absence -- what `number_text` writes for a `Measured`
/// value nobody measured, in the same words the trace uses for one.
inline constexpr std::string_view NotMeasuredText = "(not measured)";

class NumberText;

namespace detail
{
    struct NumberTextAccess;
} // namespace detail

/// A number spelled into a fixed buffer: usable at compile time, and never
/// allocating.
///
/// Made only by the functions in this header, or copied from one they
/// returned: its default constructor is private and it has no public member
/// that writes into it, so `is_exact()` is this header's statement about how
/// the text was made, not a caller's.
class NumberText
{
  public:
    /// The text. Lvalues only, like `view(Symbol const&)`: the view points
    /// into this object, and a temporary is gone before a caller could read it.
    [[nodiscard]] constexpr std::string_view view() const& noexcept
    {
        return std::string_view { _characters, _length };
    }

    /// Deleted: see above. Bind the `NumberText` to a named local first, or
    /// compare the temporary directly with `==`.
    std::string_view view() const&& = delete;

    /// False when rounding changed the value: the text is an approximation,
    /// and a text an approximating style made then starts with
    /// `ApproximationMarker`. True for a fraction, an exact decimal and
    /// `NotMeasuredText`.
    [[nodiscard]] constexpr bool is_exact() const noexcept { return _exact; }

    /// True when the text is exactly @p comparedText.
    [[nodiscard]] friend constexpr bool operator==(NumberText const& spelled, std::string_view comparedText) noexcept
    {
        return spelled.view() == comparedText;
    }

  private:
    friend struct detail::NumberTextAccess;

    constexpr NumberText() noexcept = default;

    char _characters[NumberTextCapacity] {};
    std::size_t _length = 0;
    bool _exact = true;
};

/// Whether a decimal is shown with trailing zeros.
enum class DecimalPadding : std::uint8_t
{
    /// No trailing zeros, and no point when no places are left: `0.6`, `4`.
    Trimmed,
    /// Zeros up to the places asked for: `0.600`, `4.00`. For a style, those
    /// are the unit's declared decimals, and an exact decimal with more places
    /// than that is shown in full, never cut short.
    Padded,
};

/// How a `NumberStyle` writes a number.
enum class NumberNotation : std::uint8_t
{
    /// Always a fraction in lowest terms: `3/5`, `4`, `-1/3`.
    Fraction,
    /// A decimal when that is the exact value, otherwise the fraction: `0.6`,
    /// but `1/3`.
    ExactDecimal,
    /// A decimal when that is the exact value, otherwise a rounded decimal
    /// that starts with `ApproximationMarker`: `0.6`, `≈0.333`.
    ApproximateDecimal,
};

/// How a number is shown: its notation, the rounding mode an approximate
/// decimal is rounded in, and its padding.
///
/// Built only through the three factories, so an approximating style cannot
/// exist without the caller having named its rounding mode.
class NumberStyle
{
  public:
    /// Fractions: the same style as `fraction()`.
    constexpr NumberStyle() noexcept = default;

    /// Always a fraction.
    [[nodiscard]] static constexpr NumberStyle fraction() noexcept { return NumberStyle {}; }

    /// A decimal when that is the exact value, the fraction otherwise.
    [[nodiscard]] static constexpr NumberStyle exact_decimal(
        DecimalPadding decimalPadding = DecimalPadding::Trimmed) noexcept
    {
        NumberStyle made {};
        made._notation = NumberNotation::ExactDecimal;
        made._padding = decimalPadding;
        return made;
    }

    /// A decimal when that is the exact value, otherwise one rounded in
    /// @p approximationMode at the unit's declared decimals and marked with
    /// `ApproximationMarker`.
    [[nodiscard]] static constexpr NumberStyle approximate_decimal(
        RoundingMode approximationMode, DecimalPadding decimalPadding = DecimalPadding::Trimmed) noexcept
    {
        NumberStyle made {};
        made._notation = NumberNotation::ApproximateDecimal;
        made._approximation = approximationMode;
        made._padding = decimalPadding;
        return made;
    }

    /// How this style writes a number.
    [[nodiscard]] constexpr NumberNotation notation() const noexcept { return _notation; }

    /// The rounding mode an approximate decimal is rounded in. Meaningful only
    /// when `notation()` is `ApproximateDecimal`; every other style rounds
    /// nothing, and reports `RoundingMode::HalfEven` here.
    [[nodiscard]] constexpr RoundingMode approximation() const noexcept { return _approximation; }

    /// Whether a decimal this style writes is padded with trailing zeros.
    [[nodiscard]] constexpr DecimalPadding padding() const noexcept { return _padding; }

    /// This style without its approximation: an `ApproximateDecimal` style
    /// becomes `exact_decimal` with the same padding, and any other style is
    /// returned unchanged. For a number that must never be shown rounded -- one
    /// an author typed, or either side of a stated comparison.
    [[nodiscard]] constexpr NumberStyle exact_only() const noexcept
    {
        if (_notation == NumberNotation::ApproximateDecimal)
            return exact_decimal(_padding);
        return *this;
    }

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(NumberStyle const&) const noexcept = default;

  private:
    NumberNotation _notation = NumberNotation::Fraction;
    RoundingMode _approximation = RoundingMode::HalfEven;
    DecimalPadding _padding = DecimalPadding::Trimmed;
};

namespace detail
{
    /// The one writer of a `NumberText`, and its only friend: every function
    /// in this header builds its text through this.
    struct NumberTextAccess
    {
        /// An empty text, marked exact.
        [[nodiscard]] static constexpr NumberText blank() noexcept { return NumberText {}; }

        /// Appends @p written. Never past the end: no caller in this header
        /// writes more than `LongestNumberText` bytes through it, which the
        /// `static_assert` below keeps within the buffer; a coherent unit's
        /// spelling goes through `put_within`.
        static constexpr void put(NumberText& spelled, char written) noexcept
        {
            spelled._characters[spelled._length] = written;
            ++spelled._length;
        }

        /// Appends every byte of @p written.
        static constexpr void put(NumberText& spelled, std::string_view written) noexcept
        {
            for (char const each: written)
                put(spelled, each);
        }

        /// Appends every byte of @p written when all of them fit in the
        /// buffer, and nothing when they do not: for text whose length no
        /// bound in this header covers, a coherent unit's spelling.
        /// @return whether they fit.
        [[nodiscard]] static constexpr bool put_within(NumberText& spelled, std::string_view written) noexcept
        {
            if (written.size() > NumberTextCapacity - spelled._length)
                return false;
            put(spelled, written);
            return true;
        }

        /// Appends @p wholeNumber in decimal: at most 39 digits.
        static constexpr void put_whole(NumberText& spelled, UInt128 wholeNumber) noexcept
        {
            DecimalSpelling const written = u128_decimal(wholeNumber);
            for (int at = 0; at < written.length; ++at)
                put(spelled, written.characters[at]);
        }

        /// Appends a point and the first @p shownPlaces of @p fractionDigits,
        /// or nothing when @p shownPlaces is zero.
        static constexpr void put_fraction(NumberText& spelled, char const* fractionDigits, int shownPlaces) noexcept
        {
            if (shownPlaces <= 0)
                return;
            put(spelled, '.');
            for (int place = 0; place < shownPlaces; ++place)
                put(spelled, fractionDigits[place]);
        }

        /// Marks @p spelled as rounded: `is_exact()` is then false.
        static constexpr void mark_rounded(NumberText& spelled) noexcept { spelled._exact = false; }
    };

    /// The longest text this header spells: a fraction of a sign, the 39
    /// digits of 2^127, a slash, a 39-digit denominator, a space, and a unit
    /// symbol of `SymbolCapacity` bytes -- `view(Symbol const&)` returns that
    /// many from a symbol with no terminator. A fraction is never marked
    /// approximate; the longest marked decimal -- the marker, a sign, 38
    /// whole digits (a marked decimal's denominator is at least 3, and
    /// 2^127 / 3 has 38), a point and 18 places, a space and the symbol -- is
    /// shorter.
    inline constexpr std::size_t LongestNumberText = 1 + 39 + 1 + 39 + 1 + SymbolCapacity;
    static_assert(LongestNumberText <= NumberTextCapacity,
                  "formula: NumberTextCapacity is too small for the longest number this library spells");

    /// 10^18, the finest scale this header writes a decimal on: rounding's
    /// places stop at 18, as `DecimalPlaces` does, though `Rational::Int`
    /// holds powers of ten up to 10^38.
    inline constexpr std::uint64_t ExactDecimalScale = 1'000'000'000'000'000'000ULL;

    /// The places `ExactDecimalScale` spans -- also the largest
    /// `DecimalPlaces` `checked_round` accepts.
    inline constexpr int ExactDecimalPlaces = 18;

    /// @p shownValue as an exact decimal with at least @p minimumPlaces
    /// places, or nothing when its decimal expansion needs more than 18.
    ///
    /// The denominator divides 10^18, so the fraction part is the remainder
    /// times `10^18 / denominator`, which stays below 10^18: the remainder is
    /// below the denominator.
    ///
    /// @pre `0 <= minimumPlaces <= 18`.
    [[nodiscard]] constexpr std::optional<NumberText> exact_decimal_digits(Rational shownValue, int minimumPlaces) noexcept
    {
        UInt128 const divisor = wide_magnitude(shownValue.denominator());
        UInt128Division const scaleSplit = u128_divmod(UInt128::from_u64(ExactDecimalScale), divisor);
        if (!scaleSplit.remainder.is_zero())
            return std::nullopt;

        UInt128Division const shownSplit = u128_divmod(wide_magnitude(shownValue.numerator()), divisor);
        // The remainder is below the divisor, which divides 10^18, so both it
        // and the cofactor 10^18 / divisor fit 64 bits, and so does their
        // product, which stays below 10^18.
        std::uint64_t fractional = shownSplit.remainder.lowWord * scaleSplit.quotient.lowWord;
        char fractionDigits[ExactDecimalPlaces] {};
        for (int place = ExactDecimalPlaces - 1; place >= 0; --place)
        {
            fractionDigits[place] = static_cast<char>('0' + fractional % 10U);
            fractional /= 10U;
        }
        int shownPlaces = ExactDecimalPlaces;
        while (shownPlaces > minimumPlaces && fractionDigits[shownPlaces - 1] == '0')
            --shownPlaces;

        NumberText spelled = NumberTextAccess::blank();
        if (shownValue.sign() < 0)
            NumberTextAccess::put(spelled, '-');
        NumberTextAccess::put_whole(spelled, shownSplit.quotient);
        NumberTextAccess::put_fraction(spelled, fractionDigits, shownPlaces);
        return spelled;
    }

    /// Whether a magnitude cut short after its last kept digit, with
    /// @p remainderLeft of @p divisor still over, moves one unit of that digit
    /// away from zero under @p roundingMode.
    ///
    /// `checked_round_to_int`'s decision, taken on the magnitude rather than on
    /// a floored quotient: the sign matters only to `Floor` and `Ceiling`, and
    /// a tie is found by comparing @p remainderLeft with
    /// `divisor - remainderLeft`, with no doubling. A tie under a mode that is
    /// none of the seven is refused with `DomainError`, as there.
    [[nodiscard]] constexpr std::expected<bool, ArithmeticError> moves_away_from_zero(UInt128 remainderLeft,
                                                                                     UInt128 divisor,
                                                                                     bool negative,
                                                                                     bool lastKeptOdd,
                                                                                     RoundingMode roundingMode) noexcept
    {
        if (remainderLeft.is_zero())
            return false;

        switch (roundingMode)
        {
            case RoundingMode::Floor:
                return negative;
            case RoundingMode::Ceiling:
                return !negative;
            case RoundingMode::TowardZero:
                return false;
            case RoundingMode::AwayFromZero:
                return true;
            default:
                break;
        }

        UInt128 const distanceUp = u128_sub(divisor, remainderLeft);
        if (remainderLeft < distanceUp)
            return false;
        if (remainderLeft > distanceUp)
            return true;

        switch (roundingMode)
        {
            case RoundingMode::HalfAwayFromZero:
                return true;
            case RoundingMode::HalfTowardZero:
                return false;
            case RoundingMode::HalfEven:
                return lastKeptOdd;
            default:
                break;
        }
        return std::unexpected { ArithmeticError::DomainError };
    }
} // namespace detail

/// Whether @p shownValue has an exact decimal of at most 18 places: whether
/// its denominator divides 10^18. `1/262144` (2^18) does; `1/524288` (2^19)
/// and `1/3` do not.
[[nodiscard]] constexpr bool has_exact_decimal(Rational shownValue) noexcept
{
    return detail::u128_divmod(detail::UInt128::from_u64(detail::ExactDecimalScale),
                               detail::wide_magnitude(shownValue.denominator()))
        .remainder.is_zero();
}

/// @p shownValue as its exact decimal, with no trailing zeros: `0.6`,
/// `18.8822`, `-1.75`, `4`. Nothing when it has none of 18 places or fewer
/// -- see `has_exact_decimal` -- rather than a decimal that is not the value.
[[nodiscard]] constexpr std::optional<NumberText> exact_decimal_text(Rational shownValue) noexcept
{
    return detail::exact_decimal_digits(shownValue, 0);
}

/// @p shownValue as a fraction in lowest terms, or as a whole number when its
/// denominator is one: `3/5`, `4`, `-1/3`. Always exact.
[[nodiscard]] constexpr NumberText fraction_text(Rational shownValue) noexcept
{
    NumberText spelled = detail::NumberTextAccess::blank();
    if (shownValue.sign() < 0)
        detail::NumberTextAccess::put(spelled, '-');
    detail::NumberTextAccess::put_whole(spelled, detail::wide_magnitude(shownValue.numerator()));
    if (shownValue.denominator() != 1)
    {
        detail::NumberTextAccess::put(spelled, '/');
        detail::NumberTextAccess::put_whole(spelled, detail::wide_magnitude(shownValue.denominator()));
    }
    return spelled;
}

/// @p unrounded rounded to @p places decimal places under @p roundingMode,
/// as text: `118.26`, `4.00`, `-0.01`. `is_exact()` is false when rounding
/// changed the value; no `ApproximationMarker` is written here -- that is
/// `checked_number_text`'s, which adds it.
///
/// The text is exactly what `checked_round` would round to, but not by way
/// of it: for 0 to 18 places the digits come from long division on the
/// magnitude and denominator, in 128-bit unsigned integers, so the text
/// exists even where `checked_round`'s own arithmetic overflows -- the
/// largest `Rational::Int` over 3, to 18 places, is
/// `56713727820156410577229101238628035242.333333333333333333`, while
/// `checked_round` reports `Overflow` for it. Each digit is found without
/// forming `remainder * 10`: the remainder is added ten times, taking the
/// denominator off whenever the sum reaches it, so the sum stays below twice
/// the denominator. A value that rounds to zero is written without a `-`.
///
/// A negative @p places -- whole tens, hundreds -- is `checked_round` itself,
/// then `exact_decimal_text` of what it rounded to: `125` to
/// `DecimalPlaces { -1 }` is `120` in `HalfEven` and `130` in
/// `HalfAwayFromZero`. That path runs
/// `checked_round`'s arithmetic, so it can overflow where the rounding does,
/// and it is the only one here that reports to the overflow census.
///
/// @return `Overflow` for more than 18 places -- `checked_round`'s own
///         refusal, since 10^-19 is not a representable step -- or wherever
///         `checked_round` overflows at a negative @p places; `DomainError`
///         for a tie under a rounding mode that is none of the seven.
///         Nothing saturates.
[[nodiscard]] constexpr std::expected<NumberText, ArithmeticError> checked_decimal_text(
    Rational unrounded, DecimalPlaces places, RoundingMode roundingMode, DecimalPadding decimalPadding) noexcept
{
    if (places.value > detail::ExactDecimalPlaces)
        return std::unexpected { ArithmeticError::Overflow };

    if (places.value < 0)
    {
        std::expected<Rational, ArithmeticError> const rounded = checked_round(unrounded, places, roundingMode);
        if (!rounded)
            return std::unexpected { rounded.error() };
        // A multiple of a power of ten is whole, so this always has one.
        std::optional<NumberText> spelledRounded = detail::exact_decimal_digits(*rounded, 0);
        if (!spelledRounded)
            return std::unexpected { ArithmeticError::DomainError };
        if (!(*rounded == unrounded))
            detail::NumberTextAccess::mark_rounded(*spelledRounded);
        return *spelledRounded;
    }

    detail::UInt128 const divisor = detail::wide_magnitude(unrounded.denominator());
    detail::UInt128Division const shownSplit = detail::u128_divmod(detail::wide_magnitude(unrounded.numerator()), divisor);
    detail::UInt128 wholePart = shownSplit.quotient;
    detail::UInt128 remainderLeft = shownSplit.remainder;

    char fractionDigits[detail::ExactDecimalPlaces] {};
    for (int place = 0; place < places.value; ++place)
    {
        // The next digit is floor(remainderLeft * 10 / divisor), and the
        // next remainder remainderLeft * 10 mod divisor. Both are found by
        // adding remainderLeft ten times: before each addition the sum is
        // below divisor, so after it the sum is below 2 * divisor < 2^128.
        detail::UInt128 tenfold {};
        int nextDigit = 0;
        for (int added = 0; added < 10; ++added)
        {
            tenfold = detail::u128_add(tenfold, remainderLeft);
            if (!(tenfold < divisor))
            {
                tenfold = detail::u128_sub(tenfold, divisor);
                ++nextDigit;
            }
        }
        fractionDigits[place] = static_cast<char>('0' + nextDigit);
        remainderLeft = tenfold;
    }

    bool const negative = unrounded.sign() < 0;
    bool const lastKeptOdd =
        places.value > 0 ? (fractionDigits[places.value - 1] - '0') % 2 != 0 : (wholePart.lowWord & 1U) != 0U;
    std::expected<bool, ArithmeticError> const awayFromZero =
        detail::moves_away_from_zero(remainderLeft, divisor, negative, lastKeptOdd, roundingMode);
    if (!awayFromZero)
        return std::unexpected { awayFromZero.error() };

    if (*awayFromZero)
    {
        int place = places.value - 1;
        while (place >= 0 && fractionDigits[place] == '9')
        {
            fractionDigits[place] = '0';
            --place;
        }
        if (place >= 0)
        {
            fractionDigits[place] = static_cast<char>(fractionDigits[place] + 1);
        }
        else
        {
            // Below 2^127: a value with a remainder has a denominator of at
            // least 2.
            wholePart = detail::u128_add(wholePart, detail::UInt128::from_u64(1));
        }
    }

    bool roundedToZero = wholePart.is_zero();
    for (int place = 0; place < places.value; ++place)
        roundedToZero = roundedToZero && fractionDigits[place] == '0';

    int shownPlaces = places.value;
    while (decimalPadding == DecimalPadding::Trimmed && shownPlaces > 0 && fractionDigits[shownPlaces - 1] == '0')
        --shownPlaces;

    NumberText spelled = detail::NumberTextAccess::blank();
    if (negative && !roundedToZero)
        detail::NumberTextAccess::put(spelled, '-');
    detail::NumberTextAccess::put_whole(spelled, wholePart);
    detail::NumberTextAccess::put_fraction(spelled, fractionDigits, shownPlaces);
    if (!remainderLeft.is_zero())
        detail::NumberTextAccess::mark_rounded(spelled);
    return spelled;
}

/// Throwing spelling of `checked_decimal_text`.
/// @throws ArithmeticException when the checked form would report an error.
[[nodiscard]] constexpr NumberText decimal_text(Rational unrounded,
                                                DecimalPlaces places,
                                                RoundingMode roundingMode,
                                                DecimalPadding decimalPadding)
{
    return detail::or_throw(checked_decimal_text(unrounded, places, roundingMode, decimalPadding));
}

/// @p shownValue, a value in @p shownIn, as @p shownStyle writes it -- the
/// number alone; the unit decides only the decimals. `number_text` of a
/// `Measured` adds the unit's symbol.
///
/// - `Fraction`: `fraction_text`.
/// - `ExactDecimal`: `exact_decimal_text`, and the fraction when there is none:
///   `1/3` stays `1/3`.
/// - `ApproximateDecimal`: `exact_decimal_text`, and when there is none,
///   `ApproximationMarker` then `checked_decimal_text` at
///   `declared_decimals(shownIn)` in the style's rounding mode: `1/3` in
///   `unit::One` (3 decimals) is `≈0.333`. Such a value never has an exact
///   decimal, so the rounding always changes it and the marker is never
///   wrong.
///
/// `DecimalPadding::Padded` pads an exact decimal with zeros up to the unit's
/// declared decimals, and never cuts one short: `3/5` in a unit of 3 decimals
/// is `0.600`, `123/1000` in a unit of 1 decimal is `0.123`.
///
/// @return `Overflow` when the style reads the unit's decimals -- it pads, or
///         it approximates -- and they lie outside the -18 to 18 that
///         `DecimalPlaces` spans, whatever the value; otherwise any error of
///         `checked_decimal_text`.
[[nodiscard]] constexpr std::expected<NumberText, ArithmeticError> checked_number_text(Rational shownValue,
                                                                                      NumberStyle shownStyle,
                                                                                      Unit const& shownIn) noexcept
{
    if (shownStyle.notation() == NumberNotation::Fraction)
        return fraction_text(shownValue);

    DecimalPlaces const declaredPlaces = declared_decimals(shownIn);
    bool const readsDecimals = shownStyle.notation() == NumberNotation::ApproximateDecimal
                               || shownStyle.padding() == DecimalPadding::Padded;
    if (readsDecimals
        && (declaredPlaces.value > detail::ExactDecimalPlaces || declaredPlaces.value < -detail::ExactDecimalPlaces))
        return std::unexpected { ArithmeticError::Overflow };

    int const minimumPlaces =
        shownStyle.padding() == DecimalPadding::Padded && declaredPlaces.value > 0 ? declaredPlaces.value : 0;
    std::optional<NumberText> const exact = detail::exact_decimal_digits(shownValue, minimumPlaces);
    if (exact)
        return *exact;
    if (shownStyle.notation() == NumberNotation::ExactDecimal)
        return fraction_text(shownValue);

    std::expected<NumberText, ArithmeticError> const approximated =
        checked_decimal_text(shownValue, declaredPlaces, shownStyle.approximation(), shownStyle.padding());
    if (!approximated)
        return approximated;
    NumberText marked = detail::NumberTextAccess::blank();
    detail::NumberTextAccess::put(marked, ApproximationMarker);
    detail::NumberTextAccess::put(marked, approximated->view());
    detail::NumberTextAccess::mark_rounded(marked);
    return marked;
}

/// Throwing spelling of `checked_number_text(Rational, NumberStyle, Unit const&)`.
/// @throws ArithmeticException when the checked form would report an error.
[[nodiscard]] constexpr NumberText number_text(Rational shownValue, NumberStyle shownStyle, Unit const& shownIn)
{
    return detail::or_throw(checked_number_text(shownValue, shownStyle, shownIn));
}

namespace detail
{
    /// Whether a value of @p dimension in @p declared is shown in the coherent
    /// unit, spelt from its base units (`put_coherent_unit`), rather than in
    /// @p declared: when @p declared has no symbol and @p dimension is not
    /// dimensionless. A unit with no symbol cannot say what scale its number
    /// is on, so the number is moved into the one scale its spelling names.
    /// The one rule for every place a number is written with its unit: in a
    /// trace, a step's value, a squared deviation and a derivation's header;
    /// in a trace and in `render()` alike, every number a formula declares --
    /// a constant, a per-element constant's values, a bound or a row a table,
    /// a curve or a permitted set declared, a limit (`shown_number`,
    /// `shown_bound_text`, `shown_limit_row`); and a `Measured` value's text,
    /// here and through `std::format`. So every number is in the unit written
    /// after it.
    ///
    /// A dimensionless unit with no symbol is always at scale 1 here: one with
    /// a scale is refused where it is written (`RequireNamedScaledScalar`,
    /// `unit.hpp`), so its bare number is the value.
    [[nodiscard]] constexpr bool spells_coherent_unit(Unit const& declared, Dimension dimension)
    {
        return formula::view(declared.symbolText).empty() && !(dimension == dim::Scalar);
    }

    /// Whether a number declared in @p declared is followed by a unit: its
    /// symbol, or the coherent unit's spelling (`spells_coherent_unit`).
    /// Only a dimensionless unit with no symbol writes none.
    [[nodiscard]] constexpr bool writes_a_unit(Unit const& declared)
    {
        return !formula::view(declared.symbolText).empty() || spells_coherent_unit(declared, declared.dimension);
    }

    /// The unit a value of @p dimension declared in @p declared is shown in:
    /// the coherent unit where `spells_coherent_unit` says so, @p declared
    /// otherwise.
    [[nodiscard]] constexpr Unit shown_unit_of(Unit const& declared, Dimension dimension)
    {
        return spells_coherent_unit(declared, dimension) ? coherent(dimension) : declared;
    }

    /// @p declaredNumber, a number of @p dimension declared in @p declared,
    /// moved exactly into the unit it is shown in (`shown_unit_of`): the
    /// coherent unit for a dimensioned unit with no symbol, and @p declared,
    /// unchanged, otherwise.
    ///
    /// **The one rule for every number written with its unit**, in a
    /// formula's text and in its trace alike: a bound or a row a table
    /// declares, a permitted value, a limit, a constant and a per-element
    /// constant's values, so that no number is in a scale the text after it
    /// does not name; and a `Measured` value's text. Only the move can fail
    /// -- for a malformed unit, or a unit with an offset whose sum overflows
    /// -- and the caller then writes `not_shown_text` (`render.hpp`) or
    /// returns the error, never the number in the wrong scale.
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> shown_number(Rational declaredNumber,
                                                                                  Unit const& declared,
                                                                                  Dimension dimension)
    {
        if (!spells_coherent_unit(declared, dimension))
            return declaredNumber;
        return checked_convert(declaredNumber, declared, coherent(dimension));
    }

    /// Whether @p shownIn is a unit nobody declared: exactly the coherent unit
    /// `coherent()` builds for its dimension -- no symbol, no scale, and
    /// `Unit`'s default of 3 decimals, which nobody chose. A trace shows a
    /// computed value in one, a product in joules or a ratio. A quantity
    /// declared in `unit::One` is the same `Unit` value, so it counts as
    /// unlabelled too.
    [[nodiscard]] constexpr bool is_unlabelled(Unit const& shownIn) noexcept
    {
        return shownIn == coherent(shownIn.dimension);
    }

    /// @p numberStyle with `DecimalPadding::Trimmed`: the same notation and
    /// the same rounding mode, never padded.
    [[nodiscard]] constexpr NumberStyle trimmed(NumberStyle numberStyle) noexcept
    {
        switch (numberStyle.notation())
        {
            case NumberNotation::ExactDecimal:
                return NumberStyle::exact_decimal(DecimalPadding::Trimmed);
            case NumberNotation::ApproximateDecimal:
                return NumberStyle::approximate_decimal(numberStyle.approximation(), DecimalPadding::Trimmed);
            case NumberNotation::Fraction:
                break;
        }
        return numberStyle;
    }

    /// Whether @p spelled is a rounding that came out as zero: `≈0`.
    [[nodiscard]] constexpr bool rounded_to_zero(NumberText const& spelled) noexcept
    {
        std::string_view const spelledText = spelled.view();
        return spelledText.size() == ApproximationMarker.size() + 1 && spelledText.starts_with(ApproximationMarker)
               && spelledText.back() == '0';
    }

    /// `checked_number_text` for a number shown in @p shownIn, except that a
    /// number in a unit nobody declared (`is_unlabelled`) is never padded:
    /// the 3 decimals it would be padded to are a default, not anyone's
    /// statement of precision. An approximating style still rounds it at
    /// those 3 places -- unless they round a value other than zero to `≈0`,
    /// which says nothing of it. The places are then extended to its first
    /// significant digit, up to 18, and the value, rounded there in the
    /// style's mode, stays marked: a tariff in euros per joule,
    /// 3401/33480000000, reads `≈0.0000001`, and 1/11250000 `≈0.00000009`. A
    /// value with no digit within 18 places reads `≈0`. A unit someone
    /// declared keeps its declared places, whatever they round to.
    ///
    /// The one spelling of a value a trace line shows, and of a `Measured`
    /// value moved into the coherent unit (`checked_shown_value_text`), so
    /// that `number_text`, `std::format` and a trace write such a value alike.
    [[nodiscard]] constexpr std::expected<NumberText, ArithmeticError> checked_shown_text(Rational shownNumber,
                                                                                         NumberStyle numberStyle,
                                                                                         Unit const& shownIn) noexcept
    {
        if (!is_unlabelled(shownIn))
            return checked_number_text(shownNumber, numberStyle, shownIn);
        NumberStyle const unpadded = trimmed(numberStyle);
        std::expected<NumberText, ArithmeticError> const spelled = checked_number_text(shownNumber, unpadded, shownIn);
        if (!spelled.has_value() || shownNumber == Rational { 0 } || !rounded_to_zero(*spelled))
            return spelled;
        // The first significant digit is at the fewest places a truncation
        // leaves something at; rounded there in the style's own mode, the
        // value cannot come out as zero.
        NumberStyle const truncating = NumberStyle::approximate_decimal(RoundingMode::TowardZero);
        for (std::int32_t places = declared_decimals(shownIn).value + 1; places <= ExactDecimalPlaces; ++places)
        {
            Unit finer = shownIn;
            finer.decimals = places;
            std::expected<NumberText, ArithmeticError> const truncated = checked_number_text(shownNumber, truncating, finer);
            if (!truncated.has_value())
                return spelled;
            if (!rounded_to_zero(*truncated))
                return checked_number_text(shownNumber, unpadded, finer);
        }
        return spelled;
    }

    /// @p shownValue, a value of a quantity declared in @p declaredIn that
    /// `shown_number` has already moved into the unit it is shown in, as
    /// @p numberStyle writes it there -- the number alone. A value moved into
    /// the coherent unit is spelled as a trace line spells it
    /// (`checked_shown_text`): its places are a default nobody declared, so
    /// they are never padded, and never round a value other than zero to
    /// `≈0`. A value in any other unit is spelled at the decimals that unit
    /// declares (`checked_number_text`), as before.
    [[nodiscard]] constexpr std::expected<NumberText, ArithmeticError> checked_shown_value_text(
        Rational shownValue, NumberStyle numberStyle, Unit const& declaredIn) noexcept
    {
        if (spells_coherent_unit(declaredIn, declaredIn.dimension))
            return checked_shown_text(shownValue, numberStyle, coherent(declaredIn.dimension));
        return checked_number_text(shownValue, numberStyle, declaredIn);
    }

    /// Writes the power a base unit's factor is raised to, as plain text and a
    /// trace write it -- `^-1`, `^(1/2)`, `^(-1/2)`, and nothing for a power of
    /// 1 -- through @p writer, which takes each piece as a `std::string_view`.
    /// The one spelling of it, for `plain_unit_power` (`render.hpp`) and for a
    /// number's text here alike.
    template <typename Writer>
    constexpr void write_plain_unit_power(Writer& writer, std::int32_t numeratorPart, std::int32_t denominatorPart)
    {
        auto const writeWhole = [&writer](std::int32_t wholePart) {
            std::int64_t const widened = wholePart;
            if (widened < 0)
                writer("-");
            DecimalSpelling const written =
                u128_decimal(UInt128::from_u64(static_cast<std::uint64_t>(widened < 0 ? -widened : widened)));
            writer(std::string_view { written.characters, static_cast<std::size_t>(written.length) });
        };
        if (denominatorPart != 1)
        {
            writer("^(");
            writeWhole(numeratorPart);
            writer("/");
            writeWhole(denominatorPart);
            writer(")");
        }
        else if (numeratorPart != 1)
        {
            writer("^");
            writeWhole(numeratorPart);
        }
    }

    /// Writes the coherent unit of @p dimension, spelt from its base units, to
    /// @p unitSink: the one order and shape of that spelling, which
    /// `coherent_unit_spelling` (`render.hpp`) sets in each notation and a
    /// number's text here writes plain, so the two cannot drift.
    ///
    /// The named bases come first, in the dimension's own order, then the SI
    /// base units, `m kg s A K mol cd`. Factors with a positive exponent stand
    /// above a slash and the rest below it, with their exponents negated and
    /// bracketed when there are two or more: `kg/(m s^2)`. A dimension with no
    /// positive exponent is written with negative exponents and no slash,
    /// `kg^-1`; a dimensionless one writes nothing.
    ///
    /// @p unitSink takes three calls: `factor(symbolText, namedBase,
    /// numeratorPart, denominatorPart)` for one base unit and its power, where
    /// `namedBase` says that `symbolText` is a named base dimension's name;
    /// `between()` between two factors on one side of the slash; and
    /// `put(text)` for the slash and the brackets.
    template <typename UnitSink>
    constexpr void put_coherent_unit(Dimension const& dimension, UnitSink& unitSink)
    {
        struct BaseUnit
        {
            std::string_view symbolText;
            Exponent exponent;
        };
        BaseUnit const bases[] { BaseUnit { "m", dimension.length },      BaseUnit { "kg", dimension.mass },
                                 BaseUnit { "s", dimension.time },        BaseUnit { "A", dimension.current },
                                 BaseUnit { "K", dimension.temperature }, BaseUnit { "mol", dimension.amount },
                                 BaseUnit { "cd", dimension.luminosity } };
        auto const forEachFactor = [&](auto const& visit) {
            for (std::size_t slot = 0; named_base_in_use(dimension, slot); ++slot)
                visit(formula::view(dimension.namedBases[slot].name), true, dimension.namedBases[slot].exponent);
            for (BaseUnit const& base: bases)
                visit(base.symbolText, false, base.exponent);
        };

        std::size_t aboveCount = 0;
        std::size_t belowCount = 0;
        forEachFactor([&](std::string_view, bool, Exponent baseExponent) {
            if (baseExponent.numerator > 0)
                ++aboveCount;
            else if (baseExponent.numerator < 0)
                ++belowCount;
        });
        auto const writeSide = [&](bool aboveTheSlash, bool negated) {
            bool firstOnSide = true;
            forEachFactor([&](std::string_view symbolText, bool namedBase, Exponent baseExponent) {
                if (aboveTheSlash ? baseExponent.numerator <= 0 : baseExponent.numerator >= 0)
                    return;
                if (!firstOnSide)
                    unitSink.between();
                firstOnSide = false;
                unitSink.factor(symbolText,
                                namedBase,
                                negated ? -baseExponent.numerator : baseExponent.numerator,
                                baseExponent.denominator);
            });
        };

        if (aboveCount == 0 || belowCount == 0)
        {
            // One side only, every exponent as it is, and no slash.
            writeSide(belowCount == 0, false);
            return;
        }
        writeSide(true, false);
        unitSink.put("/");
        if (belowCount > 1)
            unitSink.put("(");
        writeSide(false, true);
        if (belowCount > 1)
            unitSink.put(")");
    }

    /// A `put_coherent_unit` sink that writes plain text, `kg/(m s^2)`, through
    /// a `Writer`, which takes each piece as a `std::string_view`: each symbol
    /// and name as it is, a power as `write_plain_unit_power` writes it, and
    /// a space between two factors.
    template <typename Writer>
    struct PlainUnitSink
    {
        /// Where the text goes.
        Writer& writer;

        /// Writes a slash or a bracket.
        constexpr void put(std::string_view written) { writer(written); }

        /// Writes the space between two factors.
        constexpr void between() { writer(" "); }

        /// Writes one base unit's symbol, or a named base's name, and its power.
        constexpr void factor(std::string_view symbolText, bool, std::int32_t numeratorPart, std::int32_t denominatorPart)
        {
            writer(symbolText);
            write_plain_unit_power(writer, numeratorPart, denominatorPart);
        }
    };

    /// Writes the unit after a number declared in @p declared, in plain text,
    /// through @p writer, which takes each piece as a `std::string_view`: the
    /// coherent unit's spelling where `spells_coherent_unit` says so,
    /// @p declared's symbol otherwise, and so nothing for a dimensionless unit
    /// with no symbol. The number before it is the one `shown_number` moved.
    template <typename Writer>
    constexpr void write_shown_unit(Writer& writer, Unit const& declared)
    {
        if (spells_coherent_unit(declared, declared.dimension))
        {
            PlainUnitSink<Writer> plainSink { writer };
            put_coherent_unit(declared.dimension, plainSink);
        }
        else
            writer(formula::view(declared.symbolText));
    }

    /// A writer for `write_shown_unit` that appends to a `NumberText` through
    /// `NumberTextAccess::put_within`. A piece that does not fit sets
    /// `overflowed`, and nothing after it is written.
    struct NumberTextWriter
    {
        /// The text appended to.
        NumberText& spelled;
        /// Whether a piece did not fit.
        bool overflowed = false;

        /// Appends @p written, unless it or an earlier piece did not fit.
        constexpr void operator()(std::string_view written) noexcept
        {
            overflowed = overflowed || !NumberTextAccess::put_within(spelled, written);
        }
    };
} // namespace detail

/// @p shownMeasurement as @p shownStyle writes it, followed by a space and
/// its unit -- `5.2 kJ`, `3/5` in `unit::One` -- or `NotMeasuredText` when it
/// is absent.
///
/// The number is in `Q`'s declared unit, followed by its symbol -- unless
/// that unit has no symbol and a dimension (`detail::spells_coherent_unit`):
/// the number is then moved exactly into the coherent unit and followed by
/// that unit's spelling, `3/1000 kg` for 3 of a unit of 1/1000 kg, so it is
/// never on a scale nothing after it names. Such a value is spelled as a
/// trace line spells it (`detail::checked_shown_value_text`): never padded to
/// the coherent unit's default 3 places, and never rounded to `≈0` when it is
/// not zero -- 1/3 of a unit of 1/1000 kg reads `≈0.0003 kg`. A dimensionless
/// unit with no symbol writes the number alone.
///
/// @return any error of `checked_number_text(Rational, NumberStyle, Unit const&)`;
///         any error of the move into the coherent unit (`checked_convert`),
///         never the number in the declared unit's scale; `Overflow` when the
///         coherent unit's spelling does not fit a `NumberText`. Never one
///         for an absent value.
template <Described Q>
[[nodiscard]] constexpr std::expected<NumberText, ArithmeticError> checked_number_text(
    Measured<Q> const& shownMeasurement, NumberStyle shownStyle) noexcept
{
    if (shownMeasurement.is_absent())
    {
        NumberText absentText = detail::NumberTextAccess::blank();
        detail::NumberTextAccess::put(absentText, NotMeasuredText);
        return absentText;
    }

    Unit const declaredIn = Describe<Q>::unit;
    std::expected<Rational, ArithmeticError> const shownValue =
        detail::shown_number(*shownMeasurement.stored(), declaredIn, declaredIn.dimension);
    if (!shownValue)
        return std::unexpected { shownValue.error() };
    std::expected<NumberText, ArithmeticError> spelled =
        detail::checked_shown_value_text(*shownValue, shownStyle, declaredIn);
    if (!spelled || !detail::writes_a_unit(declaredIn))
        return spelled;
    detail::NumberTextWriter appendTo { *spelled };
    appendTo(" ");
    detail::write_shown_unit(appendTo, declaredIn);
    if (appendTo.overflowed)
        return std::unexpected { ArithmeticError::Overflow };
    return spelled;
}

/// Throwing spelling of `checked_number_text(Measured<Q> const&, NumberStyle)`.
/// @throws ArithmeticException when the checked form would report an error.
template <Described Q>
[[nodiscard]] constexpr NumberText number_text(Measured<Q> const& shownMeasurement, NumberStyle shownStyle)
{
    return detail::or_throw(checked_number_text(shownMeasurement, shownStyle));
}

} // namespace formula
