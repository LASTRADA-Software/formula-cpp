// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Critical values keyed by sample size: a method publishes one value per
/// number of determinations, and the count of a sample selects the row.
///
/// The same split as every lookup in `lookup.hpp`, and the same miss rule:
///
///  - **The sizes are structure** and live in the type: a `SampleSizeTable`,
///    strictly ascending, every size at least 1, validated where the node is
///    declared.
///  - **The values are contents**: `Corrections<K>`, one per size, supplied
///    by the author at runtime and never by this library. No table here
///    carries a value from any published standard, and none may.
///  - **A count that is not a declared size is a miss**, reported as
///    `ArithmeticError::DomainError`: never the nearest row, never an
///    interpolation between two rows, never the first or the last row for a
///    count off either end. A table without a row for 7 has said nothing
///    about 7, and a library that answered anyway would be making up a
///    critical value.
///  - **A count that is not a whole, non-negative number** names no row
///    either, and is the same `DomainError`: 5.5 determinations is not a
///    sample size, and neither truncating nor rounding it is the author's
///    rule.
///
/// Not an `exact_lookup`: an exact table's keys are compile-time scoped
/// enumerators (`RequireScopedEnumKey`), while a sample size is a runtime
/// integer that, in an outlier rejection, changes from one pass to the next.
///
/// `Rational`-only, like every lookup: the count is compared exactly.

#include <formula-cpp/dimension.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <type_traits>

namespace formula
{

/// The sample sizes a critical-value table declares a row for, in ascending
/// order -- an alias over `std::array`, for the reason `BandTable` and
/// `KeyTable` are aliases: `template <SampleSizeTable Sizes>` deduces the row
/// count from the argument.
template <std::size_t K>
using SampleSizeTable = std::array<std::size_t, K>;

/// Whether @p candidateSize can be a row of a sample-size table: a sample of no
/// determinations has no critical value to look up.
///
/// One of the two predicates `sample_size_table_is_well_formed` and
/// `RequireValidSampleSizeTable` both use, so that the check a runtime table
/// passes and the one a declared table passes cannot drift apart.
[[nodiscard]] constexpr bool sample_size_is_well_formed(std::size_t candidateSize) noexcept
{
    return candidateSize >= 1;
}

/// Whether @p earlierSize may come directly before @p laterSize in a
/// sample-size table: strictly smaller. A repeated size makes the later row
/// unreachable, and a descending pair makes the table's order a lie.
///
/// The other shared predicate -- see `sample_size_is_well_formed`.
[[nodiscard]] constexpr bool sample_sizes_ascend(std::size_t earlierSize, std::size_t laterSize) noexcept
{
    return earlierSize < laterSize;
}

namespace detail
{
    /// The position of the first row of @p sizes that breaks the table -- a
    /// zero size, or a size not above the one before it -- or `K` when none
    /// does.
    ///
    /// **The one loop both checks run**: `sample_size_table_is_well_formed`
    /// for a table that arrives at runtime, and `RequireValidSampleSizeTable`
    /// for a declared one, which also takes the position of its first defect
    /// from here. Sharing the predicates alone would leave two loops that
    /// could disagree about where the pairs start.
    template <std::size_t K>
    [[nodiscard]] constexpr std::size_t first_bad_sample_size(SampleSizeTable<K> const& sizes) noexcept
    {
        for (std::size_t rowIndex = 0; rowIndex < K; ++rowIndex)
        {
            if (!sample_size_is_well_formed(sizes[rowIndex]))
                return rowIndex;
            if (rowIndex > 0 && !sample_sizes_ascend(sizes[rowIndex - 1], sizes[rowIndex]))
                return rowIndex;
        }
        return K;
    }
} // namespace detail

/// Whether @p sizes is a usable table: every size at least 1, and the sizes
/// strictly ascending. For a table that arrives at runtime; a declared one is
/// checked by `RequireValidSampleSizeTable`, through the same loop.
template <std::size_t K>
[[nodiscard]] constexpr bool sample_size_table_is_well_formed(SampleSizeTable<K> const& sizes) noexcept
{
    return detail::first_bad_sample_size(sizes) == K;
}

/// Fails to compile when a declared table holds a size of zero. The offending
/// size and its position appear in the diagnostic as the template arguments.
template <std::size_t Size, std::size_t Position>
struct RequireSampleSizeWellFormed
{
    static_assert(sample_size_is_well_formed(Size),
                  "formula: this sample-size table declares a size of zero, and a sample of no determinations "
                  "has no critical value; the size and its zero-based position appear in this diagnostic as the "
                  "template arguments Size and Position of RequireSampleSizeWellFormed");

    /// Always `true` once reached -- see `RequireBandsAdjacent::value`.
    static constexpr bool value = true;
};

/// Fails to compile when two neighbouring sizes of a declared table do not
/// strictly ascend -- a repeat, whose later row could never be selected, or
/// a descending pair. The two sizes appear in the diagnostic as the template
/// arguments, as `RequireBandsAdjacent`'s two bands do.
template <std::size_t Earlier, std::size_t Later>
struct RequireSampleSizesAscend
{
    static_assert(sample_sizes_ascend(Earlier, Later),
                  "formula: this sample-size table's sizes do not strictly ascend; a repeated size makes its "
                  "later row unreachable, and a descending one breaks the table's order; the offending pair "
                  "appears in this diagnostic as the template arguments Earlier and Later of "
                  "RequireSampleSizesAscend");

    /// Always `true` once reached -- see `RequireBandsAdjacent::value`.
    static constexpr bool value = true;
};

namespace detail
{
    /// Refuses @p Sizes for its **first** defect only, and says which one.
    ///
    /// Not a fold over every pair, as `RequireValidBandTable` is: a
    /// descending table breaks every pair at once, and five messages for one
    /// table written backwards is four more than the author needs. The first
    /// defect is the one to fix first, and fixing it shows the next.
    template <SampleSizeTable Sizes>
    [[nodiscard]] constexpr bool sample_size_table_is_valid() noexcept
    {
        constexpr std::size_t badRow = first_bad_sample_size(Sizes);
        if constexpr (badRow == Sizes.size())
            return true;
        else if constexpr (!sample_size_is_well_formed(Sizes[badRow]))
            return RequireSampleSizeWellFormed<Sizes[badRow], badRow>::value;
        else
            return RequireSampleSizesAscend<Sizes[badRow - 1], Sizes[badRow]>::value;
    }
} // namespace detail

/// The static_assert wiring for a declared sample-size table: instantiating
/// this refuses a zero size or a pair that does not ascend, naming it, through
/// the same two predicates `sample_size_table_is_well_formed` uses. Reached
/// through `::value`, as `RequireValidBandTable` is.
template <SampleSizeTable Sizes>
struct RequireValidSampleSizeTable
{
    /// Always `true` once reached.
    static constexpr bool value = detail::sample_size_table_is_valid<Sizes>();
};

namespace detail
{
    /// Fails to compile when a critical value's count is not a bare number:
    /// a count of determinations has no unit, and a mass or a length in its
    /// place is a category error, not a miss.
    template <typename Count>
    struct RequireSampleCountScalar
    {
        static_assert(Count::dimension == dim::Scalar,
                      "formula: this critical value's count is not dimensionless; a sample size is a bare number "
                      "of determinations; the count's expression appears in this diagnostic as the template "
                      "argument of RequireSampleCountScalar");

        static constexpr bool value = true;
    };

    /// Whether @p N is a `NumericValueNode`.
    template <typename N>
    inline constexpr bool is_numeric_value = false;

    template <Unit U, FixedString Justification, Node Operand>
    inline constexpr bool is_numeric_value<NumericValueNode<U, Justification, Operand>> = true;

    /// The unit a count's *result* is stated in, when its node names one: a
    /// quantity's declared unit, or a constant's. Nothing for any other
    /// expression -- a sum, a `sample_count` -- whose value is a bare number
    /// by construction; nor for `numeric_value_of`, whose `unit` is the unit
    /// its operand is read in, while what it yields is a bare number.
    template <typename Count>
    [[nodiscard]] consteval std::optional<Unit> count_unit() noexcept
    {
        if constexpr (is_numeric_value<std::remove_cv_t<Count>>)
            return std::nullopt;
        else if constexpr (requires { typename Count::quantity; })
            return Describe<typename Count::quantity>::unit;
        else if constexpr (requires { Count::unit; })
            return Count::unit;
        else
            return std::nullopt;
    }

    /// Fails to compile when a count is stated in a dimensionless unit other
    /// than a bare number -- in percent, say. 300 % is 3 in the coherent unit,
    /// so such a count would read row 3; but a count of determinations stated
    /// in percent is a mistake in the count's declaration, not a count, and a
    /// table read through it would be read at a number nobody wrote.
    ///
    /// Asked only once the count is dimensionless (@p Dimensionless), so that
    /// a mass in the count's place draws `RequireSampleCountScalar`'s message
    /// alone.
    template <typename Count, bool Dimensionless>
    struct RequireSampleCountInOne
    {
        static constexpr std::optional<Unit> stated = count_unit<Count>();
        static_assert(!Dimensionless || !stated.has_value()
                          || (stated->magnitudeNumerator == stated->magnitudeDenominator && stated->offsetNumerator == 0),
                      "formula: this critical value's count is stated in a unit other than a bare number, such as "
                      "percent; a sample size is a number of determinations -- declare the count in unit::One; the "
                      "count's expression appears in this diagnostic as the template argument of "
                      "RequireSampleCountInOne");

        static constexpr bool value = true;
    };

    /// Stands in for `Corrections<K>` when the table itself is refused, and
    /// accepts any list silently. **The gate that keeps one mistake to one
    /// message:** with a malformed table, a corrections list of the wrong
    /// length is a consequence of the same mistake -- a table written wrong
    /// has as many rows as it has -- and `RequireCorrectionCountMatches`
    /// firing on top of `RequireValidSampleSizeTable` would ask the author to
    /// fix a count against a table that is about to change. Never reached for
    /// a table that is valid.
    template <std::size_t K>
    struct UncheckedCorrections
    {
        /// Any list, of any length, without a word: the table's own refusal
        /// is the one message.
        template <typename... Rs>
        constexpr UncheckedCorrections(Rs...) noexcept
        {
        }

        /// Never read: a node holding this has already failed to compile.
        std::array<Rational, K> values {};

        /// Mirrors `Corrections<K>::operator[]`, so that the node's other
        /// members keep one spelling.
        [[nodiscard]] constexpr Rational operator[](std::size_t rowIndex) const noexcept
        {
            return values[rowIndex];
        }
    };

    /// The corrections type of a critical-value table over @p Sizes:
    /// `Corrections<K>` for a valid table, and `UncheckedCorrections<K>` for
    /// one `RequireValidSampleSizeTable` is about to refuse.
    template <SampleSizeTable Sizes>
    using SampleSizeCorrections = std::conditional_t<first_bad_sample_size(Sizes) == Sizes.size(),
                                                     Corrections<Sizes.size()>,
                                                     UncheckedCorrections<Sizes.size()>>;

    static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t),
                  "formula: a table's sample size is widened through a 64-bit count to the 128 bits it is "
                  "compared as, and this platform's std::size_t is wider than 64 bits");

    /// The row of @p Sizes whose size is @p sampleSize, or nothing. A linear
    /// scan, for `find_band`'s reason, and an equality: never the nearest row.
    ///
    /// Compared as 128 bits, into which every `std::size_t` widens
    /// losslessly: the count is never narrowed to the table's type, so where
    /// `std::size_t` is 32 bits a count of 2^32 + 3 misses rather than wrap
    /// onto the row for 3.
    template <SampleSizeTable Sizes>
    [[nodiscard]] constexpr std::optional<std::size_t> find_sample_size(UInt128 sampleSize) noexcept
    {
        for (std::size_t rowIndex = 0; rowIndex < Sizes.size(); ++rowIndex)
            if (UInt128::from_u64(static_cast<std::uint64_t>(Sizes[rowIndex])) == sampleSize)
                return rowIndex;
        return std::nullopt;
    }

    /// @p evaluatedCount as a sample size, or nothing when it is not a whole,
    /// non-negative number. The count is read in the coherent unit of its
    /// dimension, which is a bare number. A whole count larger than any size
    /// a table can declare -- beyond 2^64 - 1 included -- is still a count,
    /// and misses in `find_sample_size`.
    [[nodiscard]] constexpr std::optional<UInt128> as_sample_size(Rational evaluatedCount) noexcept
    {
        if (evaluatedCount.denominator() != 1 || evaluatedCount.numerator() < 0)
            return std::nullopt;
        return wide_magnitude(evaluatedCount.numerator());
    }

    /// How many characters `sample_size_list<Sizes>` spells.
    template <SampleSizeTable Sizes>
    [[nodiscard]] consteval std::size_t sample_size_list_length() noexcept
    {
        std::size_t sizeCount = 0;
        for (std::size_t rowIndex = 0; rowIndex < Sizes.size(); ++rowIndex)
        {
            if (rowIndex > 0)
                sizeCount += 2;
            std::size_t digitsLeft = Sizes[rowIndex];
            do
            {
                ++sizeCount;
                digitsLeft /= 10;
            } while (digitsLeft != 0);
        }
        return sizeCount;
    }

    /// `3, 4, 5, 6, 8` for those sizes, in static storage, so that a trace
    /// step can hold a view of it for as long as the program runs -- the
    /// guarantee `Step::lookupKeyName` relies on for an enumerator's name.
    template <SampleSizeTable Sizes>
    struct SampleSizeList
    {
        /// The characters, without a terminator.
        static constexpr std::array<char, sample_size_list_length<Sizes>()> characters = [] {
            std::array<char, sample_size_list_length<Sizes>()> spelled {};
            std::size_t at = 0;
            for (std::size_t rowIndex = 0; rowIndex < Sizes.size(); ++rowIndex)
            {
                if (rowIndex > 0)
                {
                    spelled[at++] = ',';
                    spelled[at++] = ' ';
                }
                std::array<char, 20> reversed {};
                std::size_t reversedLength = 0;
                std::size_t digitsLeft = Sizes[rowIndex];
                do
                {
                    reversed[reversedLength++] = static_cast<char>('0' + digitsLeft % 10);
                    digitsLeft /= 10;
                } while (digitsLeft != 0);
                while (reversedLength > 0)
                    spelled[at++] = reversed[--reversedLength];
            }
            return spelled;
        }();

        /// The list as a view into `characters`.
        [[nodiscard]] static constexpr std::string_view view() noexcept
        {
            return { characters.data(), characters.size() };
        }
    };
} // namespace detail

/// A critical value read from an author's table by sample size -- see the
/// file comment for what is structure, what is contents, and how a miss is
/// reported.
///
/// All three `static_assert`s sit in the class body, so that a node declared
/// without the factory -- it is a public aggregate -- is refused as well.
template <SampleSizeTable Sizes, Unit ResultUnit, Node Count>
struct SampleSizeLookupNode: NodeBase
{
    static_assert(RequireValidSampleSizeTable<Sizes>::value);
    static_assert(detail::RequireSampleCountScalar<Count>::value);
    static_assert(detail::RequireSampleCountInOne<Count, Count::dimension == dim::Scalar>::value);

    /// One value per declared size, in `unit`, in the table's order -- the
    /// table's contents, and `Corrections<K>` for that type's reason: a short
    /// list is refused rather than zero-filled. (For a table that is itself
    /// refused, a stand-in that accepts anything, so that one mistake draws
    /// one message; see `detail::UncheckedCorrections`.) No `{}` default
    /// member initialiser, for the reason `Corrections` gives.
    detail::SampleSizeCorrections<Sizes> corrections;

    /// The expression whose value is the sample size.
    ///
    /// Deliberately no `{}` default member initialiser -- see `Corrections`
    /// (`lookup.hpp`).
    Count count;

    /// The declared sizes, already validated above.
    static constexpr SampleSizeTable<Sizes.size()> sizes = Sizes;
    /// The unit every value in `corrections` is stated in, and this node's
    /// own declared unit.
    static constexpr Unit unit = ResultUnit;
    /// The dimension of `unit`.
    static constexpr Dimension dimension = ResultUnit.dimension;
};

/// Declares a critical-value lookup:
/// `critical_value<Sizes, unit::One>(var<Specimens>, { 10, 30, ... })`.
///
/// `Sizes` and `ResultUnit` are not deduced, for the reason `banded_lookup`
/// gives: a table's structure is the author's declared intent. `corrections`
/// is `Corrections<K>` for any table that is valid.
template <SampleSizeTable Sizes, Unit ResultUnit, Node Count>
[[nodiscard]] constexpr auto critical_value(Count sampleCount, detail::SampleSizeCorrections<Sizes> corrections) noexcept
{
    return SampleSizeLookupNode<Sizes, ResultUnit, Count> { {}, corrections, sampleCount };
}

/// Evaluates the count and reads the row whose size it is. Absence
/// propagates; a count that is no declared size, or no whole non-negative
/// number, is `ArithmeticError::DomainError`.
template <typename Rep = Rational,
          SampleSizeTable Sizes,
          Unit ResultUnit,
          Node Count,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(SampleSizeLookupNode<Sizes, ResultUnit, Count> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const evaluatedCount = detail::dispatch<Rep>(node.count, environment, sink);
    if (!evaluatedCount.has_value())
    {
        return detail::report_failure<Rep>(node, sink, evaluatedCount.error());
    }
    if (!evaluatedCount->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    if constexpr (!std::is_same_v<Rep, Rational>)
    {
        // The lookup family's rule and message shape (`lookup.hpp`): every
        // table answers in one representation. Dependent on `Rep`, so that
        // it fires only when instantiated with another one.
        static_assert(sizeof(Rep) == 0,
                      "formula: a critical-value lookup node can only be evaluated with Rep = Rational -- a "
                      "sample size is compared exactly, and every lookup table in this library answers in one "
                      "representation; evaluate this formula with Rep = Rational instead "
                      "(checked_evaluate<Result> always does)");
        return std::unexpected { ArithmeticError::DomainError };
    }
    else
    {
        std::optional<detail::UInt128> const sampleSize = detail::as_sample_size(**evaluatedCount);
        std::optional<std::size_t> const matchedRow =
            sampleSize.has_value() ? detail::find_sample_size<Sizes>(*sampleSize) : std::nullopt;
        if (!matchedRow.has_value())
        {
            // A miss is not a value: no nearest row, no interpolation, no
            // clamping to either end. See the file comment.
            return detail::report_failure<Rep>(node, sink, ArithmeticError::DomainError);
        }

        Evaluated<Rep> const evaluated = detail::in_si<Rep>(node.corrections[*matchedRow], ResultUnit);
        sink.produced(node, evaluated);
        return evaluated;
    }
}

} // namespace formula
