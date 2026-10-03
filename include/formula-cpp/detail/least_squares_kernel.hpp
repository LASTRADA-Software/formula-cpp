// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// The numerics behind a least-squares fit over raw observations: a line
/// through as many points as were observed, or a plane through several
/// regressors, each answering the coefficients, the coefficient of
/// determination R² and the number of points. No node, no unit, no trace:
/// bare numbers in the coherent unit in, bare numbers out, for the operations
/// in `least_squares.hpp` that give them their dimensions.
///
/// **The exact route** (`exact_regression`) works in the wide integers of
/// `detail/wide_int.hpp`, so that the answer is the exact fraction whatever its
/// size, for `rounded_output` to round. Every column is brought to a common
/// denominator, which turns each element into an integer; the sums, the cross
/// sums and the centred sums (`n` times a cross sum less the product of two
/// totals, so that no mean, which would be a fraction, is formed) are integer
/// arithmetic in `regressionSumLimbs` limbs, one pass over the rows and never an
/// array of their number. For one regressor the line is the closed form of
/// those sums. For two or more, the centred normal equations `M g = v` are
/// solved by fraction-free (Bareiss) elimination with row exchanges: every
/// entry stays an integer, and a column with no non-zero pivot is an exact
/// verdict of a singular design. The back-substitution is exact by Cramer's
/// rule, each numerator over the determinant, and the constant and R² follow
/// from them. Everything stays in integers until the fractions are handed out.
///
/// **The width is an estimate; every wide operation checks it and answers
/// `Overflow` beyond it.** For realistic data -- at most 1024 observations,
/// each at most 6 decimals and 10^6 in its declared unit, with a coherent
/// factor of numerator and denominator at most 2^22 -- a column's common
/// denominator is at most 2^42, a scaled element at most 2^62, a total at most
/// 2^72 and a cross total at most 2^134. A centred sum is `n` times a centred
/// cross sum, so by Cauchy–Schwarz at most `n` times the square root of the
/// product of two cross totals: 2^10 2^134 = 2^144. One regressor then needs
/// 288 = 2 * 144 bits, R² -- a centred sum squared over the product of two --
/// being the widest, and 12 limbs of 32 bits hold it with room for a rounding.
/// With K regressors, a K x K minor of the centred sums is at most d(K) =
/// 144 K + ceil((K / 2) log2 K) bits (Hadamard's bound); the elimination and
/// the back-substitution form products of two minors and a sum of K of them,
/// so 2 d(K) + 4 bits: 582 bits for two regressors (19 limbs), 874 for three
/// (28), 1164, 1456, 1748, 2040 and 2332 for four to eight (37, 46, 55, 64
/// and 73). The sums, the centred sums and the totals are formed in 12 limbs
/// whatever K is, and widened for the solve. Data outside that envelope, with
/// many distinct denominators, can outgrow it, and the answer is then
/// `Overflow`, never a wrapped number.
///
/// **The approximate route** (`approximate_regression`) is the same fit in any
/// representation, for `double`. It centres on the means in a first pass, forms
/// the centred sums in a second, and solves the centred normal equations by
/// square-root-free Cholesky, `C = L D L^T`. Rounded data cannot decide whether
/// a design is singular, so a pivot at or below 1e-9 of its diagonal entry -- 1
/// less R² of a regressor on the ones before it, below 1e-9 -- is taken for
/// singular and refused with `DomainError`: an exactly non-singular design that
/// is nearly collinear is answered by the exact route and refused by this one.
/// It promises no digits: the ones it gives are as good as the design is
/// conditioned.
///
/// Both routes decide the same pre-checks first, on the values as given with
/// `==` alone (`regression_is_posed`): as many regressor values as responses,
/// more responses than regressors, responses not all equal, no regressor all
/// equal. Each failure is `DomainError`, never a division by zero.

#include <formula-cpp/detail/wide_int.hpp>
#include <formula-cpp/detail/wide_rounding.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/rational.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace formula::detail
{

/// The most regressors a fit takes: the exact solve is sized for this many.
inline constexpr std::size_t maxRegressors = 8;

/// The width, in 32-bit limbs, in which a fit forms its sums and centred sums,
/// whatever its number of regressors, and the whole of a line's solve.
inline constexpr std::size_t regressionSumLimbs = 12;

/// The width, in 32-bit limbs, of the exact solve for @p regressorCount
/// regressors, 1 to `maxRegressors`: see the file comment's estimate.
[[nodiscard]] consteval std::size_t regression_limbs(std::size_t regressorCount) noexcept
{
    constexpr std::array<std::size_t, maxRegressors> limbsFor { 12, 19, 28, 37, 46, 55, 64, 73 };
    return limbsFor[regressorCount - 1];
}

/// @p dividend over @p divisor when that divides exactly; nothing for a zero
/// divisor or a remainder. `WideSigned` adds, subtracts and multiplies; the
/// solve also divides, and only ever exactly.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideSigned<L>> exact_quotient(WideSigned<L> const& dividend,
                                                                    WideSigned<L> const& divisor) noexcept
{
    if (divisor.magnitude.is_zero())
        return std::nullopt;
    WideDivision<L> const divided = divmod(dividend.magnitude, divisor.magnitude);
    if (!divided.remainder.is_zero())
        return std::nullopt;
    return WideSigned<L> { dividend.negative != divisor.negative && !divided.quotient.is_zero(), divided.quotient };
}

/// @p narrow in a type of more limbs, the same value: the sums are formed in
/// `regressionSumLimbs` and the solve for K >= 2 in more.
template <std::size_t Wide, std::size_t Narrow>
    requires(Wide >= Narrow)
[[nodiscard]] constexpr WideSigned<Wide> widened(WideSigned<Narrow> const& narrow) noexcept
{
    std::array<std::uint32_t, Wide> widenedLimbs {};
    for (std::size_t limbAt = 0; limbAt < Narrow; ++limbAt)
        widenedLimbs[limbAt] = narrow.magnitude.limb(limbAt);
    return WideSigned<Wide> { narrow.negative, WideUnsigned<Wide>::from_limbs(widenedLimbs) };
}

/// The least common multiple of every element's denominator: the number
/// every element of @p observedColumn multiplies to an integer. Nothing when
/// it leaves `L` limbs. A denominator that already divides the running one --
/// a repeated one, say -- leaves it as it is; below 2^32 that is decided by
/// `divmod_small`, one step per limb, rather than by the lcm's gcd and long
/// division.
template <std::size_t L>
    requires(L >= 4)
[[nodiscard]] constexpr std::optional<WideUnsigned<L>> common_denominator(std::span<Rational const> observedColumn) noexcept
{
    WideUnsigned<L> common = WideUnsigned<L>::from_u64(1);
    for (Rational const& observed: observedColumn)
    {
        UInt128 const denominatorValue = wide_magnitude(observed.denominator());
        if (denominatorValue.fits_u64() && denominatorValue.lowWord <= 0xFFFF'FFFFU
            && divmod_small(common, static_cast<std::uint32_t>(denominatorValue.lowWord)).remainder == 0)
            continue;
        std::optional<WideUnsigned<L>> const grown =
            lcm_checked_or_none(common, WideUnsigned<L>::from_u128(denominatorValue));
        if (!grown.has_value())
            return std::nullopt;
        common = *grown;
    }
    return common;
}

/// @p runningTotal plus @p addend; nothing when either overflowed. The
/// `type_identity_t` keeps `L` deduced from the total alone, so a plain
/// `WideSigned` converts to the optional.
template <std::size_t L>
[[nodiscard]] constexpr std::optional<WideSigned<L>> accumulate(
    WideSigned<L> const& runningTotal, std::type_identity_t<std::optional<WideSigned<L>>> const& addend) noexcept
{
    if (!addend.has_value())
        return std::nullopt;
    return add_checked_or_none(runningTotal, *addend);
}

/// Whether every value of @p observed equals the first: compared with `==`
/// only, on the values as given. An empty span counts as all equal: the
/// caller counts first.
template <typename Rep>
[[nodiscard]] constexpr bool all_equal_to_first(std::span<Rep const> observed) noexcept
{
    return observed.empty()
           || std::ranges::all_of(observed.subspan(1), [&observed](Rep const& each) { return each == observed.front(); });
}

/// Whether a fit of @p responses on @p regressorColumns has an answer to look
/// for: every regressor as long as the responses, more responses than
/// regressors, the responses not all equal, no regressor all equal. Decided
/// with `==` on the values as given, before any sum, so it holds in every
/// `Rep`: in `double`, three values of 0.1 have a mean that is not 0.1 and a
/// spread that is rounding noise, not zero.
template <typename Rep, std::size_t K>
[[nodiscard]] constexpr bool regression_is_posed(std::array<std::span<Rep const>, K> const& regressorColumns,
                                                 std::span<Rep const> responses) noexcept
{
    if (responses.size() < K + 1)
        return false;
    for (std::span<Rep const> const& observedColumn: regressorColumns)
        if (observedColumn.size() != responses.size() || all_equal_to_first<Rep>(observedColumn))
            return false;
    return !all_equal_to_first<Rep>(responses);
}

/// The integer sums of a fit, each column scaled by its common denominator:
/// what one pass over the rows accumulates.
template <std::size_t K>
struct RegressionSums
{
    /// Each regressor column's common denominator.
    std::array<WideUnsigned<regressionSumLimbs>, K> regressorDenominators;
    /// The responses' common denominator.
    WideUnsigned<regressionSumLimbs> responseDenominator;
    /// The sum of each scaled regressor.
    std::array<WideSigned<regressionSumLimbs>, K> regressorTotals;
    /// The sum of the scaled responses.
    WideSigned<regressionSumLimbs> responseTotal;
    /// The sums of products of scaled regressors, upper triangle: `[a][b]` for `a <= b`.
    std::array<std::array<WideSigned<regressionSumLimbs>, K>, K> crossTotals;
    /// The sums of products of a scaled regressor and a scaled response.
    std::array<WideSigned<regressionSumLimbs>, K> responseCrossTotals;
    /// The sum of squares of the scaled responses.
    WideSigned<regressionSumLimbs> responseSquareTotal;
};

/// The denominators first, one pass per column, then one pass over the rows
/// forming each row's scaled regressors and response and accumulating every
/// total and product. `Overflow` when anything outgrows `regressionSumLimbs`.
/// @pre `regression_is_posed`.
template <std::size_t K>
[[nodiscard]] constexpr std::expected<RegressionSums<K>, ArithmeticError> regression_sums(
    std::array<std::span<Rational const>, K> const& regressorColumns, std::span<Rational const> responses) noexcept
{
    using Sum = WideSigned<regressionSumLimbs>;
    RegressionSums<K> sums;
    for (std::size_t regressorAt = 0; regressorAt < K; ++regressorAt)
    {
        std::optional<WideUnsigned<regressionSumLimbs>> const columnDenominator =
            common_denominator<regressionSumLimbs>(regressorColumns[regressorAt]);
        if (!columnDenominator.has_value())
            return std::unexpected { ArithmeticError::Overflow };
        sums.regressorDenominators[regressorAt] = *columnDenominator;
    }
    std::optional<WideUnsigned<regressionSumLimbs>> const responseCommon = common_denominator<regressionSumLimbs>(responses);
    if (!responseCommon.has_value())
        return std::unexpected { ArithmeticError::Overflow };
    sums.responseDenominator = *responseCommon;

    for (std::size_t rowAt = 0; rowAt < responses.size(); ++rowAt)
    {
        std::array<Sum, K> scaledRegressors;
        for (std::size_t regressorAt = 0; regressorAt < K; ++regressorAt)
        {
            std::optional<Sum> const made =
                scaled_to_denominator(regressorColumns[regressorAt][rowAt], sums.regressorDenominators[regressorAt]);
            if (!made.has_value())
                return std::unexpected { ArithmeticError::Overflow };
            scaledRegressors[regressorAt] = *made;
        }
        std::optional<Sum> const scaledResponse = scaled_to_denominator(responses[rowAt], sums.responseDenominator);
        if (!scaledResponse.has_value())
            return std::unexpected { ArithmeticError::Overflow };

        std::optional<Sum> const responseTotal = accumulate(sums.responseTotal, scaledResponse);
        std::optional<Sum> const responseSquareTotal =
            accumulate(sums.responseSquareTotal, mul_checked_or_none(*scaledResponse, *scaledResponse));
        if (!responseTotal.has_value() || !responseSquareTotal.has_value())
            return std::unexpected { ArithmeticError::Overflow };
        sums.responseTotal = *responseTotal;
        sums.responseSquareTotal = *responseSquareTotal;
        for (std::size_t regressorAt = 0; regressorAt < K; ++regressorAt)
        {
            std::optional<Sum> const regressorTotal =
                accumulate(sums.regressorTotals[regressorAt], scaledRegressors[regressorAt]);
            std::optional<Sum> const responseCrossTotal = accumulate(
                sums.responseCrossTotals[regressorAt], mul_checked_or_none(scaledRegressors[regressorAt], *scaledResponse));
            if (!regressorTotal.has_value() || !responseCrossTotal.has_value())
                return std::unexpected { ArithmeticError::Overflow };
            sums.regressorTotals[regressorAt] = *regressorTotal;
            sums.responseCrossTotals[regressorAt] = *responseCrossTotal;
            for (std::size_t otherAt = regressorAt; otherAt < K; ++otherAt)
            {
                std::optional<Sum> const crossTotal =
                    accumulate(sums.crossTotals[regressorAt][otherAt],
                               mul_checked_or_none(scaledRegressors[regressorAt], scaledRegressors[otherAt]));
                if (!crossTotal.has_value())
                    return std::unexpected { ArithmeticError::Overflow };
                sums.crossTotals[regressorAt][otherAt] = *crossTotal;
            }
        }
    }
    return sums;
}

/// The centred sums of a fit: `n` times a cross sum less the product of the
/// two totals, which is `n^2` times the centred cross sum over both
/// denominators, and an integer.
template <std::size_t K>
struct CentredSums
{
    /// M, symmetric: the centred cross sums of the regressors.
    std::array<std::array<WideSigned<regressionSumLimbs>, K>, K> regressorSpread;
    /// v: the centred cross sums of each regressor with the responses.
    std::array<WideSigned<regressionSumLimbs>, K> responseCoSpread;
    /// T: the centred sum of squares of the responses.
    WideSigned<regressionSumLimbs> responseSpread;
};

/// `rowCount * crossTotal - totalOfOne * totalOfOther`; nothing on overflow.
/// The narrow row count is the left factor of its product.
[[nodiscard]] constexpr std::optional<WideSigned<regressionSumLimbs>> centred_sum(
    WideSigned<regressionSumLimbs> const& rowCount,
    WideSigned<regressionSumLimbs> const& crossTotal,
    WideSigned<regressionSumLimbs> const& totalOfOne,
    WideSigned<regressionSumLimbs> const& totalOfOther) noexcept
{
    std::optional<WideSigned<regressionSumLimbs>> const scaledCross = mul_checked_or_none(rowCount, crossTotal);
    std::optional<WideSigned<regressionSumLimbs>> const totalsProduct = mul_checked_or_none(totalOfOne, totalOfOther);
    if (!scaledCross.has_value() || !totalsProduct.has_value())
        return std::nullopt;
    return sub_checked_or_none(*scaledCross, *totalsProduct);
}

/// The centred sums, M mirrored across its diagonal; `Overflow` when one
/// outgrows `regressionSumLimbs`.
template <std::size_t K>
[[nodiscard]] constexpr std::expected<CentredSums<K>, ArithmeticError> centred_sums(
    RegressionSums<K> const& sums, WideSigned<regressionSumLimbs> const& rowCount) noexcept
{
    CentredSums<K> made;
    for (std::size_t regressorAt = 0; regressorAt < K; ++regressorAt)
    {
        for (std::size_t otherAt = regressorAt; otherAt < K; ++otherAt)
        {
            std::optional<WideSigned<regressionSumLimbs>> const centredValue =
                centred_sum(rowCount,
                            sums.crossTotals[regressorAt][otherAt],
                            sums.regressorTotals[regressorAt],
                            sums.regressorTotals[otherAt]);
            if (!centredValue.has_value())
                return std::unexpected { ArithmeticError::Overflow };
            made.regressorSpread[regressorAt][otherAt] = *centredValue;
            made.regressorSpread[otherAt][regressorAt] = *centredValue;
        }
        std::optional<WideSigned<regressionSumLimbs>> const coSpread = centred_sum(
            rowCount, sums.responseCrossTotals[regressorAt], sums.regressorTotals[regressorAt], sums.responseTotal);
        if (!coSpread.has_value())
            return std::unexpected { ArithmeticError::Overflow };
        made.responseCoSpread[regressorAt] = *coSpread;
    }
    std::optional<WideSigned<regressionSumLimbs>> const responseSpreadOfAll =
        centred_sum(rowCount, sums.responseSquareTotal, sums.responseTotal, sums.responseTotal);
    if (!responseSpreadOfAll.has_value())
        return std::unexpected { ArithmeticError::Overflow };
    made.responseSpread = *responseSpreadOfAll;
    return made;
}

/// @p dividend over @p over as a wide fraction, its sign the product of the
/// two, never set for a zero numerator. Not reduced.
template <std::size_t L>
[[nodiscard]] constexpr WideRatio<L> wide_ratio_of(WideSigned<L> const& dividend, WideSigned<L> const& over) noexcept
{
    return WideRatio<L> { dividend.negative != over.negative && !dividend.magnitude.is_zero(),
                          dividend.magnitude,
                          over.magnitude };
}

/// The number of points, @p rowCount over one.
template <std::size_t L>
[[nodiscard]] constexpr WideRatio<L> points_ratio(std::size_t rowCount) noexcept
{
    return WideRatio<L> { .negative = false,
                          .numerator = WideUnsigned<L>::from_u64(rowCount),
                          .denominator = WideUnsigned<L>::from_u64(1) };
}

/// A non-negative integer as a signed one.
template <std::size_t L>
[[nodiscard]] constexpr WideSigned<L> as_signed(WideUnsigned<L> const& magnitudeOnly) noexcept
{
    return WideSigned<L> { .negative = false, .magnitude = magnitudeOnly };
}

/// The line through the points, in the closed form of the sums: the
/// intercept `(M S_y - v S_x) / (n D_y M)`, the slope `v D_x / (M D_y)`, R²
/// `v^2 / (M T)` and the number of points. M and T are positive, by the
/// pre-checks; a wide operation that overflowed is `Overflow`. The parameters
/// are named for what they hold, not `points`, which a consumer may declare.
[[nodiscard]] constexpr std::expected<std::array<WideRatio<regressionSumLimbs>, 4>, ArithmeticError> line_outputs(
    RegressionSums<1> const& sums, CentredSums<1> const& centred, std::size_t rowCount) noexcept
{
    using Sum = WideSigned<regressionSumLimbs>;
    Sum const rowTotal = as_signed(WideUnsigned<regressionSumLimbs>::from_u64(rowCount));
    Sum const& regressorSpread = centred.regressorSpread[0][0];
    Sum const& coSpread = centred.responseCoSpread[0];
    Sum const regressorDenominator = as_signed(sums.regressorDenominators[0]);
    Sum const responseDenominator = as_signed(sums.responseDenominator);

    std::optional<Sum> const slopeNumerator = mul_checked_or_none(regressorDenominator, coSpread);
    std::optional<Sum> const slopeDenominator = mul_checked_or_none(responseDenominator, regressorSpread);

    std::optional<Sum> const heightTerm = mul_checked_or_none(sums.responseTotal, regressorSpread);
    std::optional<Sum> const slopeTerm = mul_checked_or_none(sums.regressorTotals[0], coSpread);
    std::optional<Sum> const interceptNumerator =
        heightTerm.has_value() && slopeTerm.has_value() ? sub_checked_or_none(*heightTerm, *slopeTerm) : std::nullopt;
    std::optional<Sum> const scaledDenominator = mul_checked_or_none(rowTotal, responseDenominator);
    std::optional<Sum> const interceptDenominator =
        scaledDenominator.has_value() ? mul_checked_or_none(*scaledDenominator, regressorSpread) : std::nullopt;

    std::optional<Sum> const determinationNumerator = mul_checked_or_none(coSpread, coSpread);
    std::optional<Sum> const determinationDenominator = mul_checked_or_none(regressorSpread, centred.responseSpread);

    if (!slopeNumerator || !slopeDenominator || !interceptNumerator || !interceptDenominator || !determinationNumerator
        || !determinationDenominator)
        return std::unexpected { ArithmeticError::Overflow };
    return std::array<WideRatio<regressionSumLimbs>, 4> {
        wide_ratio_of(*interceptNumerator, *interceptDenominator),
        wide_ratio_of(*slopeNumerator, *slopeDenominator),
        wide_ratio_of(*determinationNumerator, *determinationDenominator),
        points_ratio<regressionSumLimbs>(rowCount),
    };
}

/// Solves M g = v, given as the augmented rows [M | v], by fraction-free
/// (Bareiss) elimination: every entry stays an integer, each division by the
/// previous pivot is exact, and the last pivot is the determinant of the
/// rows as exchanged. The answer is g_k = N_k / det, returned as N_0 ...
/// N_{K-1} and then det. A column with no non-zero pivot at or below its
/// stage is a singular matrix: `DomainError`. An inexact division cannot
/// happen (Bareiss' theorem, Cramer's rule); it is a backstop, `Overflow`,
/// so that a slip here fails rather than answers.
template <std::size_t K, std::size_t L>
[[nodiscard]] constexpr std::expected<std::array<WideSigned<L>, K + 1>, ArithmeticError> fraction_free_solve(
    std::array<std::array<WideSigned<L>, K + 1>, K> augmented) noexcept
{
    WideSigned<L> previousPivot { .negative = false, .magnitude = WideUnsigned<L>::from_u64(1) };
    for (std::size_t stage = 0; stage < K; ++stage)
    {
        std::size_t pivotAt = stage;
        while (pivotAt < K && augmented[pivotAt][stage].magnitude.is_zero())
            ++pivotAt;
        if (pivotAt == K)
            return std::unexpected { ArithmeticError::DomainError };
        std::swap(augmented[stage], augmented[pivotAt]);
        for (std::size_t below = stage + 1; below < K; ++below)
        {
            for (std::size_t across = stage + 1; across <= K; ++across)
            {
                std::optional<WideSigned<L>> const kept =
                    mul_checked_or_none(augmented[stage][stage], augmented[below][across]);
                std::optional<WideSigned<L>> const removed =
                    mul_checked_or_none(augmented[below][stage], augmented[stage][across]);
                if (!kept.has_value() || !removed.has_value())
                    return std::unexpected { ArithmeticError::Overflow };
                std::optional<WideSigned<L>> const difference = sub_checked_or_none(*kept, *removed);
                if (!difference.has_value())
                    return std::unexpected { ArithmeticError::Overflow };
                std::optional<WideSigned<L>> const eliminated = exact_quotient(*difference, previousPivot);
                if (!eliminated.has_value())
                    return std::unexpected { ArithmeticError::Overflow };
                augmented[below][across] = *eliminated;
            }
            augmented[below][stage] = WideSigned<L> {};
        }
        previousPivot = augmented[stage][stage];
    }
    std::array<WideSigned<L>, K + 1> solved;
    WideSigned<L> const determinant = augmented[K - 1][K - 1];
    solved[K] = determinant;
    solved[K - 1] = augmented[K - 1][K];
    for (std::size_t stage = K - 1; stage-- > 0;)
    {
        std::optional<WideSigned<L>> accumulated = mul_checked_or_none(augmented[stage][K], determinant);
        for (std::size_t later = stage + 1; later < K && accumulated.has_value(); ++later)
        {
            std::optional<WideSigned<L>> const known = mul_checked_or_none(augmented[stage][later], solved[later]);
            accumulated = known.has_value() ? sub_checked_or_none(*accumulated, *known) : std::nullopt;
        }
        if (!accumulated.has_value())
            return std::unexpected { ArithmeticError::Overflow };
        std::optional<WideSigned<L>> const numeratorAt = exact_quotient(*accumulated, augmented[stage][stage]);
        if (!numeratorAt.has_value())
            return std::unexpected { ArithmeticError::Overflow };
        solved[stage] = *numeratorAt;
    }
    return solved;
}

/// The fit of two or more regressors: M, v, T, the totals and the
/// denominators widened to `regression_limbs(K)`, `[M | v]` solved, then
/// `c_k = N_k D_k / (det D_y)`, the constant `(det S_y - sum N_k S_k) / (n D_y
/// det)`, R² `(sum N_k v_k) / (det T)` and the number of points. Why: the
/// centred normal equations `sum_k M_jk / (n D_j D_k) c_k = v_j / (n D_j
/// D_y)` become `M g = v` with `g_k = c_k D_y / D_k`; the constant is `ybar -
/// sum c_k xbar_k = (S_y - sum g_k S_k) / (n D_y)`; and `R² = sum c_k C_ky /
/// C_yy = sum g_k v_k / T`. A singular design is `DomainError`.
template <std::size_t K>
    requires(K >= 2)
[[nodiscard]] constexpr std::expected<std::array<WideRatio<regression_limbs(K)>, K + 3>, ArithmeticError> solved_outputs(
    RegressionSums<K> const& sums, CentredSums<K> const& centred, std::size_t rowCount) noexcept
{
    constexpr std::size_t W = regression_limbs(K);
    using Sum = WideSigned<W>;
    std::array<std::array<Sum, K + 1>, K> augmented;
    for (std::size_t stage = 0; stage < K; ++stage)
    {
        for (std::size_t across = 0; across < K; ++across)
            augmented[stage][across] = widened<W>(centred.regressorSpread[stage][across]);
        augmented[stage][K] = widened<W>(centred.responseCoSpread[stage]);
    }
    std::expected<std::array<Sum, K + 1>, ArithmeticError> const solved = fraction_free_solve<K, W>(augmented);
    if (!solved.has_value())
        return std::unexpected { solved.error() };
    Sum const& determinant = (*solved)[K];
    Sum const rowTotal { .negative = false, .magnitude = WideUnsigned<W>::from_u64(rowCount) };
    Sum const responseDenominator = widened<W>(as_signed(sums.responseDenominator));
    Sum const responseTotal = widened<W>(sums.responseTotal);
    Sum const responseSpread = widened<W>(centred.responseSpread);

    std::array<WideRatio<W>, K + 3> outputs;
    std::optional<Sum> const scaledDenominator = mul_checked_or_none(responseDenominator, determinant);
    if (!scaledDenominator.has_value())
        return std::unexpected { ArithmeticError::Overflow };
    // The constant: (det S_y - sum N_k S_k) / (n D_y det); R²'s numerator sum N_k v_k.
    std::optional<Sum> constantNumerator = mul_checked_or_none(responseTotal, determinant);
    Sum determinationNumerator {};
    for (std::size_t stage = 0; stage < K; ++stage)
    {
        Sum const denominatorOfStage = widened<W>(as_signed(sums.regressorDenominators[stage]));
        std::optional<Sum> const numeratorTimesDenominator = mul_checked_or_none(denominatorOfStage, (*solved)[stage]);
        if (!numeratorTimesDenominator.has_value())
            return std::unexpected { ArithmeticError::Overflow };
        outputs[1 + stage] = wide_ratio_of(*numeratorTimesDenominator, *scaledDenominator);

        std::optional<Sum> const totalTimesNumerator =
            mul_checked_or_none(widened<W>(sums.regressorTotals[stage]), (*solved)[stage]);
        constantNumerator = constantNumerator.has_value() && totalTimesNumerator.has_value()
                                ? sub_checked_or_none(*constantNumerator, *totalTimesNumerator)
                                : std::nullopt;
        std::optional<Sum> const grownDetermination = accumulate(
            determinationNumerator, mul_checked_or_none(widened<W>(centred.responseCoSpread[stage]), (*solved)[stage]));
        if (!grownDetermination.has_value())
            return std::unexpected { ArithmeticError::Overflow };
        determinationNumerator = *grownDetermination;
    }
    std::optional<Sum> const rowsTimesDenominator = mul_checked_or_none(rowTotal, responseDenominator);
    std::optional<Sum> const constantDenominator =
        rowsTimesDenominator.has_value() ? mul_checked_or_none(*rowsTimesDenominator, determinant) : std::nullopt;
    std::optional<Sum> const determinationDenominator = mul_checked_or_none(responseSpread, determinant);
    if (!constantNumerator.has_value() || !constantDenominator.has_value() || !determinationDenominator.has_value())
        return std::unexpected { ArithmeticError::Overflow };
    outputs[0] = wide_ratio_of(*constantNumerator, *constantDenominator);
    outputs[K + 1] = wide_ratio_of(determinationNumerator, *determinationDenominator);
    outputs[K + 2] = points_ratio<W>(rowCount);
    return outputs;
}

/// The exact fit of @p responses on @p regressorColumns, with a constant:
/// `{constant, c_1 ... c_K, R², points}` as wide fractions, not reduced.
/// `DomainError` when the fit is not posed (`regression_is_posed`) or the
/// design is singular; `Overflow` when a sum or the solve outgrows its width.
/// The coefficients are per coherent unit of their regressor, the constant is
/// the response where every regressor is zero.
template <std::size_t K>
[[nodiscard]] constexpr std::expected<std::array<WideRatio<regression_limbs(K)>, K + 3>, ArithmeticError> exact_regression(
    std::array<std::span<Rational const>, K> const& regressorColumns, std::span<Rational const> responses) noexcept
{
    if (!regression_is_posed<Rational, K>(regressorColumns, responses))
        return std::unexpected { ArithmeticError::DomainError };
    std::expected<RegressionSums<K>, ArithmeticError> const sums = regression_sums<K>(regressorColumns, responses);
    if (!sums.has_value())
        return std::unexpected { sums.error() };
    WideSigned<regressionSumLimbs> const rowCount { .negative = false,
                                                    .magnitude =
                                                        WideUnsigned<regressionSumLimbs>::from_u64(responses.size()) };
    std::expected<CentredSums<K>, ArithmeticError> const centred = centred_sums<K>(*sums, rowCount);
    if (!centred.has_value())
        return std::unexpected { centred.error() };
    if constexpr (K == 1)
        return line_outputs(*sums, *centred, responses.size());
    else
        return solved_outputs<K>(*sums, *centred, responses.size());
}

/// Every wide fraction of @p wideOutputs as a `Rational`, or the first error:
/// every output fits, or the call fails.
template <std::size_t L, std::size_t M>
[[nodiscard]] constexpr std::expected<std::array<Rational, M>, ArithmeticError> narrowed_all(
    std::array<WideRatio<L>, M> const& wideOutputs) noexcept
{
    std::array<Rational, M> narrowed;
    for (std::size_t outputAt = 0; outputAt < M; ++outputAt)
    {
        std::expected<Rational, ArithmeticError> const one = narrow_wide_ratio(wideOutputs[outputAt]);
        if (!one.has_value())
            return std::unexpected { one.error() };
        narrowed[outputAt] = *one;
    }
    return narrowed;
}

/// Which `RepTraits` operation `rep_apply` makes.
enum class RepOperation : std::uint8_t
{
    /// `RepTraits<Rep>::add`.
    Add,
    /// `RepTraits<Rep>::subtract`.
    Subtract,
    /// `RepTraits<Rep>::multiply`.
    Multiply,
    /// `RepTraits<Rep>::divide`.
    Divide,
};

/// One `RepTraits` call, @p Operation, on answers that may already have
/// failed: the first failure is passed on untouched. Named by an enumerator,
/// not handed a `RepTraits` function, so that a representation's traits may
/// overload theirs.
template <RepOperation Operation, typename Rep>
[[nodiscard]] constexpr std::expected<Rep, ArithmeticError> rep_apply(
    std::expected<Rep, ArithmeticError> const& leftOperand, std::expected<Rep, ArithmeticError> const& rightOperand) noexcept
{
    if (!leftOperand.has_value())
        return leftOperand;
    if (!rightOperand.has_value())
        return rightOperand;
    if constexpr (Operation == RepOperation::Add)
        return RepTraits<Rep>::add(*leftOperand, *rightOperand);
    else if constexpr (Operation == RepOperation::Subtract)
        return RepTraits<Rep>::subtract(*leftOperand, *rightOperand);
    else if constexpr (Operation == RepOperation::Multiply)
        return RepTraits<Rep>::multiply(*leftOperand, *rightOperand);
    else
        return RepTraits<Rep>::divide(*leftOperand, *rightOperand);
}

/// The fit in any representation, for `checked_evaluate_si<double>`: the
/// same pre-checks, the means, then one pass of centred sums -- never an
/// n-sized array -- then the centred normal equations solved by
/// square-root-free Cholesky, C = L D L^T. A pivot D_k at or below 1e-9 times
/// C_kk -- that is, 1 - R^2 of regressor k on the ones before it below 1e-9 --
/// is taken for a singular design: `DomainError`. Rounded data cannot decide
/// singularity exactly, and a design near that line has few trustworthy
/// digits either way.
///
/// Every operation is its own `RepTraits` call, never `a * b + c` in one
/// expression: ISO C++ lets a compiler fuse a multiply and an add only within
/// one expression. g++ in its GNU dialect (`-std=gnu++23`) contracts across
/// statements too, so a consumer's build there can differ in the last bits;
/// the route is approximate either way, and never traced.
///
/// Returns `{constant, c_1 ... c_K, R², points}`.
template <typename Rep, std::size_t K>
[[nodiscard]] constexpr std::expected<std::array<Rep, K + 3>, ArithmeticError> approximate_regression(
    std::array<std::span<Rep const>, K> const& regressorColumns, std::span<Rep const> responses) noexcept
{
    using Traits = RepTraits<Rep>;
    using Answer = std::expected<Rep, ArithmeticError>;
    using enum RepOperation;
    if (!regression_is_posed<Rep, K>(regressorColumns, responses))
        return std::unexpected { ArithmeticError::DomainError };
    std::size_t const rowCount = responses.size();
    Answer const zero = Traits::from(Rational { 0 });
    Answer const rowTally = Traits::from(Rational { static_cast<Rational::Int>(rowCount) });
    Answer const pivotFloorScale = Traits::from(Rational { 1, 1'000'000'000 });

    // The means.
    std::array<Answer, K> regressorMeans;
    for (std::size_t regressorAt = 0; regressorAt < K; ++regressorAt)
    {
        Answer runningTotal = zero;
        for (std::size_t rowAt = 0; rowAt < rowCount; ++rowAt)
            runningTotal = rep_apply<Add, Rep>(runningTotal, regressorColumns[regressorAt][rowAt]);
        regressorMeans[regressorAt] = rep_apply<Divide, Rep>(runningTotal, rowTally);
    }
    Answer responseTotal = zero;
    for (std::size_t rowAt = 0; rowAt < rowCount; ++rowAt)
        responseTotal = rep_apply<Add, Rep>(responseTotal, responses[rowAt]);
    Answer const responseMean = rep_apply<Divide, Rep>(responseTotal, rowTally);

    // The centred sums, one pass over the rows.
    std::array<std::array<Answer, K>, K> centredCross;
    std::array<Answer, K> centredWithResponse;
    for (std::size_t regressorAt = 0; regressorAt < K; ++regressorAt)
    {
        centredWithResponse[regressorAt] = zero;
        for (std::size_t otherAt = 0; otherAt < K; ++otherAt)
            centredCross[regressorAt][otherAt] = zero;
    }
    Answer responseSpreadSum = zero;
    for (std::size_t rowAt = 0; rowAt < rowCount; ++rowAt)
    {
        std::array<Answer, K> regressorDeviations;
        for (std::size_t regressorAt = 0; regressorAt < K; ++regressorAt)
            regressorDeviations[regressorAt] =
                rep_apply<Subtract, Rep>(regressorColumns[regressorAt][rowAt], regressorMeans[regressorAt]);
        Answer const responseDeviation = rep_apply<Subtract, Rep>(responses[rowAt], responseMean);
        responseSpreadSum =
            rep_apply<Add, Rep>(responseSpreadSum, rep_apply<Multiply, Rep>(responseDeviation, responseDeviation));
        for (std::size_t regressorAt = 0; regressorAt < K; ++regressorAt)
        {
            centredWithResponse[regressorAt] =
                rep_apply<Add, Rep>(centredWithResponse[regressorAt],
                                    rep_apply<Multiply, Rep>(regressorDeviations[regressorAt], responseDeviation));
            for (std::size_t otherAt = regressorAt; otherAt < K; ++otherAt)
                centredCross[regressorAt][otherAt] = rep_apply<Add, Rep>(
                    centredCross[regressorAt][otherAt],
                    rep_apply<Multiply, Rep>(regressorDeviations[regressorAt], regressorDeviations[otherAt]));
        }
    }
    for (std::size_t regressorAt = 0; regressorAt < K; ++regressorAt)
        for (std::size_t otherAt = 0; otherAt < regressorAt; ++otherAt)
            centredCross[regressorAt][otherAt] = centredCross[otherAt][regressorAt];

    // C = L D L^T, L unit lower triangular: unitLower[later][stage] for later > stage.
    std::array<std::array<Answer, K>, K> unitLower;
    std::array<Answer, K> pivots;
    for (std::size_t stage = 0; stage < K; ++stage)
    {
        Answer pivotValue = centredCross[stage][stage];
        for (std::size_t earlier = 0; earlier < stage; ++earlier)
        {
            Answer const squared = rep_apply<Multiply, Rep>(unitLower[stage][earlier], unitLower[stage][earlier]);
            pivotValue = rep_apply<Subtract, Rep>(pivotValue, rep_apply<Multiply, Rep>(squared, pivots[earlier]));
        }
        Answer const floorValue = rep_apply<Multiply, Rep>(pivotFloorScale, centredCross[stage][stage]);
        if (!pivotValue.has_value())
            return std::unexpected { pivotValue.error() };
        if (!floorValue.has_value())
            return std::unexpected { floorValue.error() };
        if (!(*floorValue < *pivotValue))
            return std::unexpected { ArithmeticError::DomainError };
        pivots[stage] = pivotValue;
        for (std::size_t later = stage + 1; later < K; ++later)
        {
            Answer entryValue = centredCross[later][stage];
            for (std::size_t earlier = 0; earlier < stage; ++earlier)
            {
                Answer const weighted = rep_apply<Multiply, Rep>(unitLower[later][earlier], unitLower[stage][earlier]);
                entryValue = rep_apply<Subtract, Rep>(entryValue, rep_apply<Multiply, Rep>(weighted, pivots[earlier]));
            }
            unitLower[later][stage] = rep_apply<Divide, Rep>(entryValue, pivots[stage]);
        }
    }

    // L z = C_y, then D w = z, then L^T c = w.
    std::array<Answer, K> forward;
    for (std::size_t stage = 0; stage < K; ++stage)
    {
        Answer forwardValue = centredWithResponse[stage];
        for (std::size_t earlier = 0; earlier < stage; ++earlier)
            forwardValue = rep_apply<Subtract, Rep>(forwardValue,
                                                    rep_apply<Multiply, Rep>(unitLower[stage][earlier], forward[earlier]));
        forward[stage] = forwardValue;
    }
    std::array<Answer, K> coefficientAnswers;
    for (std::size_t stage = K; stage-- > 0;)
    {
        Answer backValue = rep_apply<Divide, Rep>(forward[stage], pivots[stage]);
        for (std::size_t later = stage + 1; later < K; ++later)
            backValue = rep_apply<Subtract, Rep>(
                backValue, rep_apply<Multiply, Rep>(unitLower[later][stage], coefficientAnswers[later]));
        coefficientAnswers[stage] = backValue;
    }

    // The constant, R² and the number of points.
    Answer constantValue = responseMean;
    Answer explainedSum = zero;
    for (std::size_t stage = 0; stage < K; ++stage)
    {
        constantValue = rep_apply<Subtract, Rep>(constantValue,
                                                 rep_apply<Multiply, Rep>(coefficientAnswers[stage], regressorMeans[stage]));
        explainedSum = rep_apply<Add, Rep>(explainedSum,
                                           rep_apply<Multiply, Rep>(coefficientAnswers[stage], centredWithResponse[stage]));
    }
    std::array<Answer, K + 3> produced;
    produced[0] = constantValue;
    for (std::size_t stage = 0; stage < K; ++stage)
        produced[1 + stage] = coefficientAnswers[stage];
    produced[K + 1] = rep_apply<Divide, Rep>(explainedSum, responseSpreadSum);
    produced[K + 2] = rowTally;

    std::array<Rep, K + 3> outputs;
    for (std::size_t outputAt = 0; outputAt < K + 3; ++outputAt)
    {
        if (!produced[outputAt].has_value())
            return std::unexpected { produced[outputAt].error() };
        outputs[outputAt] = *produced[outputAt];
    }
    return outputs;
}

} // namespace formula::detail
