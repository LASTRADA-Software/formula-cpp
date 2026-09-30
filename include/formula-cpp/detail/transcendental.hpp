// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// An integer kernel that encloses the natural logarithm, the decimal logarithm and the exponential of a
/// rational: two ends between which the value certainly lies, computed in 384-bit fixed point with only
/// integer operations the language defines exactly, so that no floating-point mode enters and the same
/// inputs are meant to give the same bits, at compile time and at run time. It is what the rounded forms
/// (`rounded_transcendental.hpp`) round: when both ends round to the same decimal, that is the rounding of
/// the value.
///
/// ## The algorithm and its error bound
///
/// - **Fixed point.** A value v is an integer V in `WideUnsigned<12>` (384 bits) with 128 fraction bits,
///   V = floor(v 2^128). Every operation truncates a non-negative value, so a lower bound stays one; each
///   upper bound is the lower bound plus a slack derived here. No `<cmath>`, no floating point, no
///   intrinsics, no 128-bit type, **and no call of the general `divmod`** (too costly in a constant evaluation
///   over 384 bits): the two scaled quotients are 64-bit long divisions, k is a binary search, and the
///   series' divisors are below 2^32 (`divmod_small`).
/// - **ln(a/b)**, a, b > 0, a != b. For a < b, ln(b/a) is taken and negated: the sign comes from a < b and
///   never from a rounding. So let a > b. B = b 2^k with B <= a < 2B, so k <= 62 and a + B < 2^64. With
///   z = (a - B)/(a + B), 0 <= z < 1/3, ln(a/b) = k ln 2 + 2 atanh(z), atanh(z) = sum_{i>=0} z^(2i+1)/(2i+1).
///   Z = floor(z 2^128); Z2 = floor(Z^2 / 2^128); P_0 = Z, P_{i+1} = floor(P_i Z2 / 2^128);
///   S = sum floor(P_i / (2i+1)) until P_i = 0. Then S <= atanh(z) 2^128. Deficits: Z2 is below z^2 2^128
///   by less than 2z + 1 < 2; the deficit d_i of P_i against z^(2i+1) 2^128 obeys d_0 < 1 and
///   d_{i+1} < 2 z^(2i+1) + z^2 d_i + 1, so d_i < 2 throughout; each term is short by less than
///   1 + d_i/(2i+1); P_i is 0 by i = 41 (z^83 2^128 < 1), after which the tail is below 2.25/(2i+1). So
///   atanh(z) 2^128 - S < 41 + 2 (1 + 1/3 + ... + 1/81) + 2.25/83 < 47 <= `AtanhSlack` = 64. With
///   L = floor(ln 2 2^128): lower = k L + 2S, upper = k (L + 1) + 2 (S + 64) — at most k + 128 <= 190 units
///   of 2^-128 apart, under 2^-120, absolute.
/// - **log10** = ln log10(e). With M = floor(log10(e) 2^128): lower = floor(lower_ln M / 2^128),
///   upper = floor(upper_ln (M + 1) / 2^128) + 1, under 190 · 0.44 + 44 + 2 < 130 units apart, since
///   |ln(a/b)| <= ln(2^63) < 44. The widest product, upper_ln (M + 1), is below 2^261.
/// - **exp(x)**, x = a/b != 0, -43 <= x <= 44 (the rounded forms answer outside it). X = floor(|x| 2^128), exact or one
///   below. For x > 0, k is the largest integer in [0, 63] with k (L + 1) <= X (64 ln 2 > 44 bounds it), and
///   R = X - k (L + 1) <= r 2^128 for r = x - k ln 2; for x < 0, m is the smallest in [1, 63] with
///   m L >= X' (X' = X + 1 when X is inexact; 63 ln 2 > 43 bounds it), R = m L - X' <= r 2^128 for
///   r = m ln 2 - |x|, and exp(x) = 2^-m exp(r). Either way 0 <= R, r < ln 2 + 2^-121, and r 2^128 - R <= 64
///   (one unit for X, one per multiple of ln 2's unit). E = sum T_j, T_0 = 2^128,
///   T_j = floor(floor(T_{j-1} R / 2^128) / j), until T_j = 0 (within 40 terms for r < 0.7), so
///   E <= exp(R 2^-128) 2^128 <= exp(r) 2^128. Each T_j is short by e_j < e_{j-1} r / j + 1 < 2, and the tail
///   after the last term is below 3, so exp(R 2^-128) 2^128 - E < 2 · 40 + 3; the 64 units of r add less than
///   2 · 1.0001 · 64 < 129. So exp(r) 2^128 < E + 212 <= E + `ExponentialSlack` = 512:
///   lower = E 2^k / 2^128, upper = (E + 512) 2^k / 2^128 (for x < 0, denominator 2^(128+m)), under 2^-119
///   relative. The widest numerator, (E + 512) 2^63, is below 2^193; `decide_rounding`'s scaling by up to
///   10^18 keeps it below 2^253.
/// - **Undecided.** When the two ends round differently, `decide_rounding` answers `Overflow`: the rounding
///   needs more bits than the kernel holds. The rounded decimal exists, so `Inexact` would be wrong, and a new
///   error enumerator would break `describe()` and consumers' switches.
///
/// ## What it costs
///
/// Measured on cl 19.51.36257, whose default constant-evaluation budget measured about 1 049 000 steps: one
/// enclosure costs between 21 000 and 29 400 steps (ln 3: 21 200; ln with z near 1/3: 29 100; log10 7:
/// 26 800; exp 1: 20 200; exp -43: 23 800), and a whole rounding of log10 2 to 3 places, with
/// `decide_rounding`, about 239 000.

#include <formula-cpp/detail/checked_int.hpp>
#include <formula-cpp/detail/wide_int.hpp>
#include <formula-cpp/rational.hpp>

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace formula::detail
{
    /// The kernel's width, 384 bits: room for its widest product (under 2^261) and for
    /// `decide_rounding`'s scaling by up to 10^18 of every end it is handed (under 2^253).
    inline constexpr std::size_t KernelLimbs = 12;
    /// A value in the kernel's fixed point.
    using KernelWord = WideUnsigned<KernelLimbs>;
    /// How many of a fixed-point value's bits are fraction.
    inline constexpr std::size_t KernelFractionBits = 128;
    /// How far, in units of 2^-128, the atanh series' lower bound can fall short -- see the file comment.
    inline constexpr std::uint32_t AtanhSlack = 64;
    /// How far the exponential's lower bound can fall short of exp(r), r's own width included.
    inline constexpr std::uint32_t ExponentialSlack = 512;
    /// More terms than the atanh series takes for any z below 1/3; reaching it is refused.
    inline constexpr std::uint32_t AtanhTermLimit = 42;
    /// More terms than the exponential's series takes for any r below 0.7; reaching it is refused.
    inline constexpr std::uint32_t TaylorTermLimit = 40;

    /// Two ends between which a value certainly lies: lower <= value <= upper.
    struct Enclosure
    {
        /// The lower end.
        WideRatio<KernelLimbs> lower;
        /// The upper end.
        WideRatio<KernelLimbs> upper;
    };

    /// @p highHalf * 2^64 + @p lowHalf as a kernel word. 128 bits in 384: neither step can overflow.
    [[nodiscard]] constexpr KernelWord kernel_word(std::uint64_t highHalf, std::uint64_t lowHalf) noexcept
    {
        return *add_checked_or_none(*shift_left_checked_or_none(KernelWord::from_u64(highHalf), 64), KernelWord::from_u64(lowHalf));
    }

    /// One in the kernel's fixed point, 2^128.
    inline constexpr KernelWord KernelOne = *shift_left_checked_or_none(KernelWord::from_u64(1), KernelFractionBits);
    /// floor(ln 2 * 2^128): ln 2 lies in [Ln2Lower, Ln2Upper] * 2^-128. Checked against its published
    /// digits, and re-derived by the kernel's own series, in `transcendental_tests.cpp`.
    inline constexpr KernelWord Ln2Lower = kernel_word(0xB172'17F7'D1CF'79ABULL, 0xC9E3'B398'03F2'F6AFULL);
    /// Ln2Lower + 1.
    inline constexpr KernelWord Ln2Upper = kernel_word(0xB172'17F7'D1CF'79ABULL, 0xC9E3'B398'03F2'F6B0ULL);
    /// floor(log10(e) * 2^128), checked as `Ln2Lower` is.
    inline constexpr KernelWord Log10eLower = kernel_word(0x6F2D'EC54'9B94'38CAULL, 0x9AAD'D557'D699'EE19ULL);
    /// Log10eLower + 1.
    inline constexpr KernelWord Log10eUpper = kernel_word(0x6F2D'EC54'9B94'38CAULL, 0x9AAD'D557'D699'EE1AULL);

    /// floor(dividend * 2^128 / divisor), and whether that is exact.
    struct ScaledQuotient
    {
        /// The quotient, rounded down.
        KernelWord below;
        /// Whether nothing was rounded away.
        bool exact;
    };

    /// @p dividend * 2^128 / @p divisor, by long division in 64-bit words: the whole part, then the 128
    /// fraction bits one at a time. The running remainder stays below the divisor; doubled, it leaves 64
    /// bits only when it is then above the divisor, and the subtraction, taken modulo 2^64, is then exact.
    /// @pre divisor != 0.
    [[nodiscard]] constexpr ScaledQuotient scaled_quotient(std::uint64_t dividend, std::uint64_t divisor) noexcept
    {
        std::uint64_t remaining = dividend % divisor;
        std::uint64_t highBits = 0;
        std::uint64_t lowBits = 0;
        for (std::size_t bit = 0; bit < KernelFractionBits; ++bit)
        {
            bool const carriedOut = (remaining >> 63) != 0;
            remaining <<= 1;
            highBits = (highBits << 1) | (lowBits >> 63);
            lowBits <<= 1;
            if (carriedOut || remaining >= divisor)
            {
                remaining -= divisor;
                lowBits |= 1;
            }
        }
        // The whole part is below 2^64 and the fraction below 2^128: the sum fits.
        KernelWord const whole = *shift_left_checked_or_none(KernelWord::from_u64(dividend / divisor), KernelFractionBits);
        return { *add_checked_or_none(whole, kernel_word(highBits, lowBits)), remaining == 0 };
    }

    /// A lower bound of atanh(z) * 2^128 for z = @p fixedArgument * 2^-128 below 1/3 -- see the file
    /// comment; nothing when a bound failed or the series had not ended by `AtanhTermLimit`.
    [[nodiscard]] constexpr std::optional<KernelWord> atanh_series_lower(KernelWord const& fixedArgument) noexcept
    {
        std::optional<KernelWord> const squaredProduct = mul_checked_or_none(fixedArgument, fixedArgument);
        if (!squaredProduct)
            return std::nullopt;
        KernelWord const squared = shift_right(*squaredProduct, KernelFractionBits);
        KernelWord oddPower = fixedArgument;
        KernelWord partialSum {};
        for (std::uint32_t term = 0; !oddPower.is_zero(); ++term)
        {
            if (term == AtanhTermLimit)
                return std::nullopt;
            std::optional<KernelWord> const added = add_checked_or_none(partialSum, divmod_small(oddPower, 2 * term + 1).quotient);
            std::optional<KernelWord> const product = mul_checked_or_none(oddPower, squared);
            if (!added || !product)
                return std::nullopt;
            partialSum = *added;
            oddPower = shift_right(*product, KernelFractionBits);
        }
        return partialSum;
    }

    /// A lower bound of exp(r) * 2^128 for r = @p fixedArgument * 2^-128 below 0.7 -- see the file
    /// comment; nothing when a bound failed or the series had not ended by `TaylorTermLimit`.
    [[nodiscard]] constexpr std::optional<KernelWord> exponential_series_lower(KernelWord const& fixedArgument) noexcept
    {
        KernelWord term = KernelOne;
        KernelWord partialSum = KernelOne;
        for (std::uint32_t order = 1;; ++order)
        {
            std::optional<KernelWord> const product = mul_checked_or_none(term, fixedArgument);
            if (!product)
                return std::nullopt;
            term = divmod_small(shift_right(*product, KernelFractionBits), order).quotient;
            if (term.is_zero())
                return partialSum;
            if (order == TaylorTermLimit)
                return std::nullopt;
            std::optional<KernelWord> const added = add_checked_or_none(partialSum, term);
            if (!added)
                return std::nullopt;
            partialSum = *added;
        }
    }

    /// Which side of zero a logarithm lies on: below it for an argument below one.
    enum class LogarithmSign : std::uint8_t
    {
        /// The argument is above one.
        Positive,
        /// The argument is below one: the logarithm's ends are its magnitude's, negated and exchanged.
        Negative,
    };

    /// |ln(a/b)| enclosed in units of 2^-128, and on which side of zero ln(a/b) lies.
    struct LogarithmMagnitude
    {
        /// The lower end of |ln(a/b)| * 2^128.
        KernelWord lower;
        /// The upper end.
        KernelWord upper;
        /// `Negative` for a < b.
        LogarithmSign sign;
    };

    /// The enclosure of |ln(@p positive)| -- see the file comment. @pre @p positive > 0 and != 1.
    [[nodiscard]] constexpr std::optional<LogarithmMagnitude> natural_log_magnitude(Rational positive) noexcept
    {
        auto larger = static_cast<std::uint64_t>(positive.numerator());
        auto smaller = static_cast<std::uint64_t>(positive.denominator());
        LogarithmSign const logarithmSign = larger < smaller ? LogarithmSign::Negative : LogarithmSign::Positive;
        if (logarithmSign == LogarithmSign::Negative)
            std::swap(larger, smaller);
        // B = smaller * 2^doublings <= larger < 2B. Both are below 2^63, so the shift stays in 64 bits.
        int doublings = static_cast<int>(std::bit_width(larger)) - static_cast<int>(std::bit_width(smaller));
        if ((smaller << doublings) > larger)
            --doublings;
        std::uint64_t const base = smaller << doublings;
        // z = (a - B) / (a + B), and a + B < 2^64.
        std::optional<KernelWord> const series = atanh_series_lower(scaled_quotient(larger - base, larger + base).below);
        if (!series)
            return std::nullopt;
        auto const multiples = static_cast<std::uint32_t>(doublings);
        std::optional<KernelWord> const twiceSeries = add_checked_or_none(*series, *series);
        std::optional<KernelWord> const ln2Below = mul_small_checked_or_none(Ln2Lower, multiples);
        std::optional<KernelWord> const ln2Above = mul_small_checked_or_none(Ln2Upper, multiples);
        std::optional<KernelWord> const slackedSeries = add_small_checked_or_none(*series, AtanhSlack);
        std::optional<KernelWord> const twiceSlacked = slackedSeries ? add_checked_or_none(*slackedSeries, *slackedSeries) : std::nullopt;
        if (!twiceSeries || !ln2Below || !ln2Above || !twiceSlacked)
            return std::nullopt;
        std::optional<KernelWord> const lowerEnd = add_checked_or_none(*ln2Below, *twiceSeries);
        std::optional<KernelWord> const upperEnd = add_checked_or_none(*ln2Above, *twiceSlacked);
        if (!lowerEnd || !upperEnd)
            return std::nullopt;
        return LogarithmMagnitude { .lower = *lowerEnd, .upper = *upperEnd, .sign = logarithmSign };
    }

    /// An enclosure from the magnitudes of its ends, over 2^128, on the side of zero @p logarithmSign says.
    [[nodiscard]] constexpr Enclosure signed_enclosure(KernelWord const& nearer,
                                                       KernelWord const& farther,
                                                       LogarithmSign logarithmSign) noexcept
    {
        if (logarithmSign == LogarithmSign::Negative)
            return { .lower = { .negative = true, .numerator = farther, .denominator = KernelOne },
                     .upper = { .negative = true, .numerator = nearer, .denominator = KernelOne } };
        return { .lower = { .negative = false, .numerator = nearer, .denominator = KernelOne },
                 .upper = { .negative = false, .numerator = farther, .denominator = KernelOne } };
    }

    /// ln(@p positive), enclosed. @pre @p positive > 0 and != 1.
    [[nodiscard]] constexpr std::optional<Enclosure> natural_log_enclosure(Rational positive) noexcept
    {
        std::optional<LogarithmMagnitude> const natural = natural_log_magnitude(positive);
        if (!natural)
            return std::nullopt;
        return signed_enclosure(natural->lower, natural->upper, natural->sign);
    }

    /// log10(@p positive) = ln(@p positive) * log10(e), enclosed. @pre @p positive > 0, not a power of ten.
    [[nodiscard]] constexpr std::optional<Enclosure> decimal_log_enclosure(Rational positive) noexcept
    {
        std::optional<LogarithmMagnitude> const natural = natural_log_magnitude(positive);
        if (!natural)
            return std::nullopt;
        std::optional<KernelWord> const lowerProduct = mul_checked_or_none(natural->lower, Log10eLower);
        std::optional<KernelWord> const upperProduct = mul_checked_or_none(natural->upper, Log10eUpper);
        if (!lowerProduct || !upperProduct)
            return std::nullopt;
        std::optional<KernelWord> const farther = add_small_checked_or_none(shift_right(*upperProduct, KernelFractionBits), 1U);
        if (!farther)
            return std::nullopt;
        return signed_enclosure(shift_right(*lowerProduct, KernelFractionBits), *farther, natural->sign);
    }

    /// exp(@p argument), enclosed -- see the file comment. @pre @p argument != 0 and -43 <= @p argument <= 44.
    [[nodiscard]] constexpr std::optional<Enclosure> exponential_enclosure(Rational argument) noexcept
    {
        bool const negative = argument.sign() < 0;
        ScaledQuotient const fixedMagnitude =
            scaled_quotient(magnitude(argument.numerator()), static_cast<std::uint64_t>(argument.denominator()));
        std::optional<KernelWord> remainderBelow;
        std::uint32_t shifts = 0;
        if (!negative)
        {
            // The largest k in [0, 63] with k (L + 1) <= X: 64 ln 2 > 44 >= x.
            std::uint32_t below = 0;
            std::uint32_t above = 64;
            while (below + 1 < above)
            {
                std::uint32_t const middle = (below + above) / 2;
                std::optional<KernelWord> const multiple = mul_small_checked_or_none(Ln2Upper, middle);
                if (!multiple)
                    return std::nullopt;
                if (*multiple <= fixedMagnitude.below)
                    below = middle;
                else
                    above = middle;
            }
            std::optional<KernelWord> const multiple = mul_small_checked_or_none(Ln2Upper, below);
            remainderBelow = multiple ? sub_checked_or_none(fixedMagnitude.below, *multiple) : std::nullopt;
            shifts = below;
        }
        else
        {
            // The smallest m in [1, 63] with m L >= X' (X rounded up): 63 ln 2 > 43 >= |x|.
            std::optional<KernelWord> const roundedUp = fixedMagnitude.exact ? std::optional<KernelWord> { fixedMagnitude.below }
                                                                     : add_small_checked_or_none(fixedMagnitude.below, 1U);
            if (!roundedUp)
                return std::nullopt;
            std::uint32_t below = 0;
            std::uint32_t above = 63;
            while (below + 1 < above)
            {
                std::uint32_t const middle = (below + above) / 2;
                std::optional<KernelWord> const multiple = mul_small_checked_or_none(Ln2Lower, middle);
                if (!multiple)
                    return std::nullopt;
                if (*multiple >= *roundedUp)
                    above = middle;
                else
                    below = middle;
            }
            std::optional<KernelWord> const multiple = mul_small_checked_or_none(Ln2Lower, above);
            remainderBelow = multiple ? sub_checked_or_none(*multiple, *roundedUp) : std::nullopt;
            shifts = above;
        }
        if (!remainderBelow)
            return std::nullopt;
        std::optional<KernelWord> const series = exponential_series_lower(*remainderBelow);
        std::optional<KernelWord> const slacked = series ? add_small_checked_or_none(*series, ExponentialSlack) : std::nullopt;
        if (!series || !slacked)
            return std::nullopt;
        if (!negative)
        {
            std::optional<KernelWord> const nearer = shift_left_checked_or_none(*series, shifts);
            std::optional<KernelWord> const farther = shift_left_checked_or_none(*slacked, shifts);
            if (!nearer || !farther)
                return std::nullopt;
            return Enclosure { .lower = { .negative = false, .numerator = *nearer, .denominator = KernelOne },
                               .upper = { .negative = false, .numerator = *farther, .denominator = KernelOne } };
        }
        std::optional<KernelWord> const denominatorPower = shift_left_checked_or_none(KernelOne, shifts);
        if (!denominatorPower)
            return std::nullopt;
        return Enclosure { .lower = { .negative = false, .numerator = *series, .denominator = *denominatorPower },
                           .upper = { .negative = false, .numerator = *slacked, .denominator = *denominatorPower } };
    }
} // namespace formula::detail
