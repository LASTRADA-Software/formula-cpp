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
/// `std::uint64_t` on the value's magnitude and denominator, never
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
/// spells is 59 bytes -- the marker, a sign, 19 whole digits, a point and 18
/// places, then a space and a unit symbol of `SymbolCapacity` bytes -- and a
/// `static_assert` below keeps that true.
inline constexpr std::size_t NumberTextCapacity = 64;

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
        /// writes more than `LongestNumberText` bytes, which the
        /// `static_assert` below keeps within the buffer.
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

        /// Appends @p wholeNumber in decimal: at most 20 digits.
        static constexpr void put_whole(NumberText& spelled, std::uint64_t wholeNumber) noexcept
        {
            char reversed[20] {};
            std::size_t produced = 0;
            do
            {
                reversed[produced] = static_cast<char>('0' + wholeNumber % 10U);
                ++produced;
                wholeNumber /= 10U;
            } while (wholeNumber != 0U);
            while (produced > 0)
            {
                --produced;
                put(spelled, reversed[produced]);
            }
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

    /// The longest text this header spells: the marker, a sign, the 19 digits
    /// of 2^63, a point and 18 places, a space, and a unit symbol of
    /// `SymbolCapacity` bytes -- `view(Symbol const&)` returns that many from a
    /// symbol with no terminator.
    inline constexpr std::size_t LongestNumberText = ApproximationMarker.size() + 1 + 19 + 1 + 18 + 1 + SymbolCapacity;
    static_assert(LongestNumberText <= NumberTextCapacity,
                  "formula: NumberTextCapacity is too small for the longest number this library spells");

    /// 10^18, the largest power of ten `Rational::Int` holds: the finest
    /// scale this header writes a decimal on.
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
    [[nodiscard]] constexpr std::optional<NumberText> exact_decimal_text(Rational shownValue, int minimumPlaces) noexcept
    {
        auto const divisor = static_cast<std::uint64_t>(shownValue.denominator());
        if (ExactDecimalScale % divisor != 0U)
            return std::nullopt;

        std::uint64_t const magnitudeShown = magnitude(shownValue.numerator());
        std::uint64_t fractional = (magnitudeShown % divisor) * (ExactDecimalScale / divisor);
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
        NumberTextAccess::put_whole(spelled, magnitudeShown / divisor);
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
    [[nodiscard]] constexpr std::expected<bool, ArithmeticError> moves_away_from_zero(std::uint64_t remainderLeft,
                                                                                     std::uint64_t divisor,
                                                                                     bool negative,
                                                                                     bool lastKeptOdd,
                                                                                     RoundingMode roundingMode) noexcept
    {
        if (remainderLeft == 0U)
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

        std::uint64_t const distanceUp = divisor - remainderLeft;
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
    return detail::ExactDecimalScale % static_cast<std::uint64_t>(shownValue.denominator()) == 0U;
}

/// @p shownValue as its exact decimal, with no trailing zeros: `0.6`,
/// `18.8822`, `-1.75`, `4`. Nothing when it has none of 18 places or fewer
/// -- see `has_exact_decimal` -- rather than a decimal that is not the value.
[[nodiscard]] constexpr std::optional<NumberText> exact_decimal_text(Rational shownValue) noexcept
{
    return detail::exact_decimal_text(shownValue, 0);
}

/// @p shownValue as a fraction in lowest terms, or as a whole number when its
/// denominator is one: `3/5`, `4`, `-1/3`. Always exact.
[[nodiscard]] constexpr NumberText fraction_text(Rational shownValue) noexcept
{
    NumberText spelled = detail::NumberTextAccess::blank();
    if (shownValue.sign() < 0)
        detail::NumberTextAccess::put(spelled, '-');
    detail::NumberTextAccess::put_whole(spelled, detail::magnitude(shownValue.numerator()));
    if (shownValue.denominator() != 1)
    {
        detail::NumberTextAccess::put(spelled, '/');
        detail::NumberTextAccess::put_whole(spelled, static_cast<std::uint64_t>(shownValue.denominator()));
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
/// magnitude and denominator, in `std::uint64_t`, so the text exists even
/// where `checked_round`'s own arithmetic overflows -- `IntMax/3` to 18
/// places is `3074457345618258602.333333333333333333`, while `checked_round`
/// reports `Overflow` for it. Each digit is found without forming
/// `remainder * 10`: the remainder is added ten times, taking the
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
        std::optional<NumberText> spelledRounded = detail::exact_decimal_text(*rounded, 0);
        if (!spelledRounded)
            return std::unexpected { ArithmeticError::DomainError };
        if (!(*rounded == unrounded))
            detail::NumberTextAccess::mark_rounded(*spelledRounded);
        return *spelledRounded;
    }

    auto const divisor = static_cast<std::uint64_t>(unrounded.denominator());
    std::uint64_t const magnitudeShown = detail::magnitude(unrounded.numerator());
    std::uint64_t wholePart = magnitudeShown / divisor;
    std::uint64_t remainderLeft = magnitudeShown % divisor;

    char fractionDigits[detail::ExactDecimalPlaces] {};
    for (int place = 0; place < places.value; ++place)
    {
        // The next digit is floor(remainderLeft * 10 / divisor), and the
        // next remainder remainderLeft * 10 mod divisor. Both are found by
        // adding remainderLeft ten times: before each addition the sum is
        // below divisor, so after it the sum is below 2 * divisor < 2^64.
        std::uint64_t tenfold = 0;
        int nextDigit = 0;
        for (int added = 0; added < 10; ++added)
        {
            tenfold += remainderLeft;
            if (tenfold >= divisor)
            {
                tenfold -= divisor;
                ++nextDigit;
            }
        }
        fractionDigits[place] = static_cast<char>('0' + nextDigit);
        remainderLeft = tenfold;
    }

    bool const negative = unrounded.sign() < 0;
    bool const lastKeptOdd =
        places.value > 0 ? (fractionDigits[places.value - 1] - '0') % 2 != 0 : wholePart % 2U != 0U;
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
            fractionDigits[place] = static_cast<char>(fractionDigits[place] + 1);
        else
            ++wholePart; // at most 2^62 + 1: a value with a remainder has a denominator of at least 2
    }

    bool roundedToZero = wholePart == 0U;
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
    if (remainderLeft != 0U)
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
    std::optional<NumberText> const exact = detail::exact_decimal_text(shownValue, minimumPlaces);
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

/// @p shownMeasurement as @p shownStyle writes it in `Q`'s declared unit,
/// followed by a space and the unit's symbol when it has one -- `5.2 kJ`,
/// `3/5` in `unit::One` -- or `NotMeasuredText` when it is absent.
///
/// @return any error of `checked_number_text(Rational, NumberStyle, Unit const&)`;
///         never one for an absent value.
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

    Unit const shownIn = Describe<Q>::unit;
    std::expected<NumberText, ArithmeticError> spelled =
        checked_number_text(*shownMeasurement.stored(), shownStyle, shownIn);
    std::string_view const unitSymbol = formula::view(shownIn.symbolText);
    if (spelled && !unitSymbol.empty())
    {
        detail::NumberTextAccess::put(*spelled, ' ');
        detail::NumberTextAccess::put(*spelled, unitSymbol);
    }
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
