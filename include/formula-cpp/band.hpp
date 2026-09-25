// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Bands: half-open numeric intervals a measured value is bucketed into, and
/// the validation that refuses a table with a gap or an overlap between two
/// of them.
///
/// Bands are declared as int64 numerator/denominator pairs, not `Rational` --
/// the same reason `Unit::Bounds` is (unit.hpp:114): `Rational` keeps its
/// members private, so it is not a *structural* type and cannot be a
/// non-type template parameter (dimension.hpp's comment on `Exponent` says so
/// first, and a spike compiled the rejection on all four compilers). An
/// aggregate of plain `std::int64_t` fields is structural, and so is
/// `std::array<Band, N>` of them -- which is what makes it possible to
/// validate a table's bands at compile time, with `static_assert`, rather
/// than only when it happens to be loaded at runtime.
///
/// Bands are half-open: `[low, high)`. A published table that writes "30 to
/// 40" and then "40 to 50" leaves the value 40 ambiguous on the page; this
/// library resolves it one way, uniformly, rather than guessing which the
/// author of a given table meant. **This is a rule the caller must reconcile
/// against their source document, not an implementation detail to work
/// around.** If a table's last row genuinely means "up to and including the
/// maximum", state its high bound as the smallest value strictly above that
/// maximum the domain can actually take on -- a real, exact number, not an
/// approximation, because a physical measurement is always read to some
/// declared decimal precision (`Unit::decimals`), so "the next tick past the
/// maximum" exists. There is deliberately no separate closed-upper-bound
/// spelling on `Band` itself: adding one would only move the ambiguity from
/// "which bound is closed" to "is the flag set correctly on the right band",
/// and it would give the one validation predicate below two different
/// adjacency rules to reconcile instead of one.
///
/// An empty table (`BandTable<0>`) validates: there is no adjacent pair to
/// check, so it is vacuously free of gaps and overlaps, and a lookup against
/// it simply always misses -- the same "not a value" outcome a banded lookup
/// must already be able to produce for a value that falls between two
/// declared bands. Refusing an empty table at compile time would treat an
/// always-empty table as a different kind of mistake than a
/// mostly-but-not-quite-covering one, when neither is: both leave some or all
/// of the domain undefined, and undefined is the one honest answer this type
/// exists to make representable.

#include <formula-cpp/rational.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace formula
{

/// A half-open band `[low, high)`, in a table's own unit, as exact rationals
/// stated numerator over denominator -- see the file comment for why not
/// `Rational`.
struct Band
{
    /// Numerator of the declared low bound (inclusive).
    std::int64_t lowNumerator = 0;
    /// Denominator of the declared low bound (inclusive).
    std::int64_t lowDenominator = 1;
    /// Numerator of the declared high bound (exclusive).
    std::int64_t highNumerator = 0;
    /// Denominator of the declared high bound (exclusive).
    std::int64_t highDenominator = 1;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(Band const&) const noexcept = default;
};

/// Builds a `Band` from its low (inclusive) and high (exclusive) bound, each
/// as a numerator/denominator pair.
[[nodiscard]] constexpr Band band(std::int64_t lowNumerator,
                                   std::int64_t lowDenominator,
                                   std::int64_t highNumerator,
                                   std::int64_t highDenominator) noexcept
{
    return { lowNumerator, lowDenominator, highNumerator, highDenominator };
}

/// A table of bands, declared in ascending order. An alias template, not a
/// wrapping struct: a spike compiled `template <BandTable Bands>` directly,
/// with alias-template deduction, on all four compilers, so a second type
/// would add nothing but a name to unwrap. Consumers (phase 10 tasks 2-4)
/// name a table either by giving `N` explicitly or by letting it deduce from
/// a braced initialiser.
template <std::size_t N>
using BandTable = std::array<Band, N>;

/// The one predicate gap-and-overlap validation is built on, used both by the
/// `static_assert` wiring below and by any runtime loader (phase 10 tasks
/// 2-4) -- so the two checks cannot drift the way this project's checks have
/// four times before.
///
/// True when `first`'s high bound and `second`'s low bound are the exact same
/// rational number: neither a gap (first ends before second begins) nor an
/// overlap (first ends after second begins) between them. Compares through
/// `Rational::make`, which reduces to lowest terms in the unsigned domain and
/// reports overflow, rather than cross-multiplying the raw numerator and
/// denominator directly -- the same overflow `Rational::operator<=>`'s own
/// comment avoids by using a Euclidean comparison instead. A `Band` whose
/// numerator/denominator pair is not a representable rational (a zero
/// denominator, or an overflow) is treated as not adjacent to anything: a
/// malformed bound is exactly the kind of typo this validation exists to
/// catch, not a case to silently wave through.
[[nodiscard]] constexpr bool bands_are_adjacent(Band const& first, Band const& second) noexcept
{
    auto const firstHigh = Rational::make(first.highNumerator, first.highDenominator);
    auto const secondLow = Rational::make(second.lowNumerator, second.lowDenominator);
    if (!firstHigh || !secondLow)
        return false;
    return *firstHigh == *secondLow;
}

/// True when every adjacent pair of bands in `table` shares its boundary
/// exactly -- the whole table has no gap and no overlap. Checks every pair,
/// not only the last one: a validator that stops at the final adjacency
/// passes a table with a defect anywhere earlier. An empty table and a
/// single-band table both validate, since neither has an adjacent pair that
/// could fail -- see the file comment for why an empty table is treated as
/// valid rather than refused.
template <std::size_t N>
[[nodiscard]] constexpr bool band_table_has_no_gap_or_overlap(BandTable<N> const& table) noexcept
{
    for (std::size_t index = 0; index + 1 < N; ++index)
        if (!bands_are_adjacent(table[index], table[index + 1]))
            return false;
    return true;
}

/// Fails to compile when two adjacent bands are not exactly adjacent -- there
/// is either a gap between them (`First`'s high bound sits below `Second`'s
/// low bound) or an overlap (`First`'s high bound sits above `Second`'s low
/// bound).
///
/// Same shape and same reason as `RequireSameUnitDimension` (unit.hpp):
/// instantiating a named template on the two values makes the compiler print
/// the offending bands, and the wording is ours so the negative-compile
/// harness can assert the reason rather than merely the failure. **It fires
/// only when the type is completed** -- see `RequireSameDimension` in
/// dimension.hpp for the measured five-form table of what does and does not
/// force that; this type is reached the same way, through `::value`.
template <Band First, Band Second>
struct RequireBandsAdjacent
{
    static_assert(bands_are_adjacent(First, Second),
                  "formula: this band table has a gap or overlap between two adjacent bands; "
                  "the earlier band's declared high bound and the later band's declared low "
                  "bound do not match exactly, and the two offending Band values appear in this "
                  "diagnostic as the template arguments First and Second of RequireBandsAdjacent");

    /// Always `true` once reached -- the `static_assert` above already failed
    /// compilation otherwise. Present so `::value` is the spelling that
    /// instantiates the class template; see the class comment for why that
    /// spelling matters.
    static constexpr bool value = true;
};

namespace detail
{
    /// Expands to one `RequireBandsAdjacent<Bands[i], Bands[i+1]>::value` per
    /// index, `&&`-folded together. Every operand of a fold expression is
    /// instantiated to form the expression, independent of the runtime
    /// short-circuit `&&` also performs -- so every adjacent pair is checked,
    /// and each one that fails reports on its own, naming its own two bands.
    /// A defect in an earlier pair does not stop a later, independent
    /// pair from being checked and does not stop it from being reported.
    template <BandTable Bands, std::size_t... Index>
    [[nodiscard]] constexpr bool require_all_bands_adjacent(std::index_sequence<Index...>) noexcept
    {
        return (RequireBandsAdjacent<Bands[Index], Bands[Index + 1]>::value && ...);
    }

    /// Split out of `RequireValidBandTable` so that `Bands.size() - 1` --
    /// which underflows for an empty table -- sits behind `if constexpr` and
    /// is therefore never instantiated for `N < 2`. Guarding it with `||`
    /// instead would not be enough: that operator's short circuit applies to
    /// *evaluation*, not to forming the type of its right-hand operand, and
    /// `std::make_index_sequence<Bands.size() - 1>` for an empty table would
    /// still have to name a sequence of length `SIZE_MAX`.
    template <BandTable Bands>
    [[nodiscard]] constexpr bool band_table_is_valid() noexcept
    {
        if constexpr (Bands.size() < 2)
            return true;
        else
            return require_all_bands_adjacent<Bands>(std::make_index_sequence<Bands.size() - 1> {});
    }
} // namespace detail

/// The static_assert wiring: instantiating this with a `BandTable` that is a
/// compile-time constant enforces, right there, that it has no gap and no
/// overlap -- reusing `bands_are_adjacent`, the same predicate
/// `band_table_has_no_gap_or_overlap` uses for a table that only arrives at
/// runtime, through `RequireBandsAdjacent` above. Consumed by phase 10 tasks
/// 2-4, which declare a banded lookup's bands as a `BandTable` template
/// argument. Reached through `::value`, for the same reason
/// `RequireBandsAdjacent` is.
template <BandTable Bands>
struct RequireValidBandTable
{
    static constexpr bool value = detail::band_table_is_valid<Bands>();
};

} // namespace formula
