// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Conformity: each element of a series judged against its own row of a limit
/// envelope -- the passing percentage at each screen against the least and
/// the greatest the product specification permits there.
///
/// **The numbers are master data.** An envelope is the product
/// specification, registered per customer and read at run time, not a
/// formula: the library supplies the mechanism -- a row per element, each
/// side a limit or explicitly unbounded, and the judgement -- and never the
/// limits. That is why an `Envelope` is runtime state, like a series
/// constant's values, while its length is the method's and lives in the type.
///
/// **A limit is closed.** An element satisfies its row when `lower <= x <=
/// upper`. This differs from a `Band`'s half-open rule on purpose: a limit
/// states the least and the greatest *permitted* value, where a band
/// partitions a line and must give each point to exactly one band.
///
/// A conformity check is deliberately **not** a `Node`, for `Constraint`'s
/// reason (`constraint.hpp`): it produces verdicts, which have no dimension.
/// It reuses `ConstraintOutcome`'s four states, one per element, so that an
/// element nobody measured is `NotChecked` and never `Satisfied`, and a row
/// that cannot be judged -- its lower limit above its upper one -- is
/// `Invalid`, never `Satisfied` or `Violated`.
///
/// Its citation is the author's own declaration of where the check comes
/// from, as a `Constraint`'s is. Nothing here claims a jurisdiction's
/// provenance.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace formula
{

class Limit;

/// A limit at @p limitValue, in the unit the conformity check states its limits
/// in.
[[nodiscard]] constexpr Limit limit(Rational limitValue) noexcept;

namespace detail
{
    /// Builds the one `Limit` that holds no value: `unbounded`.
    struct LimitAccess;
} // namespace detail

/// One side of an envelope row: a value, or explicitly unbounded.
///
/// **There is no default.** A `Limit` is built by `limit(value)` or is
/// `unbounded`, and nothing else: a limit nobody filled in -- a zero where a
/// loader skipped a column -- cannot be written, because a zero is a real
/// limit and "no limit" must be said.
class Limit
{
  public:
    /// True when this side has no limit.
    [[nodiscard]] constexpr bool is_unbounded() const noexcept
    {
        return !_bounded;
    }

    /// The limit, or nothing when this side is unbounded.
    [[nodiscard]] constexpr std::optional<Rational> value() const noexcept
    {
        if (_bounded)
            return _value;
        return std::nullopt;
    }

    /// Memberwise equality: a limit of 0 is not `unbounded`.
    [[nodiscard]] constexpr bool operator==(Limit const&) const noexcept = default;

  private:
    constexpr Limit(bool bounded, Rational limitValue) noexcept:
        _bounded { bounded },
        _value { limitValue }
    {
    }

    friend constexpr Limit limit(Rational limitValue) noexcept;
    friend struct detail::LimitAccess;

    bool _bounded;
    Rational _value;
};

[[nodiscard]] constexpr Limit limit(Rational limitValue) noexcept
{
    return Limit { true, limitValue };
}

namespace detail
{
    struct LimitAccess
    {
        /// The side of a row with no limit.
        [[nodiscard]] static constexpr Limit none() noexcept
        {
            return Limit { false, Rational {} };
        }
    };
} // namespace detail

/// No limit on this side of the row: `LimitRow { limit(60), unbounded }`
/// is "at least 60".
inline constexpr Limit unbounded = detail::LimitAccess::none();

/// The least and the greatest value one element of the subject may take,
/// each closed, either unbounded. Not default-constructible: `Limit` is not.
struct LimitRow
{
    /// The least permitted value, or `unbounded`.
    Limit lower;
    /// The greatest permitted value, or `unbounded`.
    Limit upper;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(LimitRow const&) const noexcept = default;
};

/// Whether @p limitRow can be judged: false exactly when both sides are limits and
/// the lower is above the upper. A row whose two limits are equal permits one
/// value, and is well formed.
///
/// **The one predicate.** `check_conformity` uses it for every row, and a
/// loader reading an envelope from master data should use it too, so that
/// the rule is written once and applied the same everywhere.
[[nodiscard]] constexpr bool envelope_row_is_well_formed(LimitRow limitRow) noexcept
{
    std::optional<Rational> const lowerValue = limitRow.lower.value();
    std::optional<Rational> const upperValue = limitRow.upper.value();
    return !lowerValue.has_value() || !upperValue.has_value() || *lowerValue <= *upperValue;
}

namespace detail
{
    /// Fails to compile when an envelope is given a different number of rows
    /// than its length. Named so both counts print.
    template <std::size_t Given, std::size_t Expected>
    struct RequireEnvelopeRowCountMatches
    {
        static_assert(Given == Expected,
                      "formula: this envelope was given a different number of rows than its series has elements; "
                      "the two counts appear in this diagnostic as the template arguments Given and Expected of "
                      "RequireEnvelopeRowCountMatches -- give one limit row per point of the series");

        static constexpr bool value = true;
    };

    /// @p N rows, each unbounded on both sides: what a refused envelope holds,
    /// since a `LimitRow` has no default. Never judged -- the program does not
    /// compile.
    template <std::size_t N>
    [[nodiscard]] constexpr std::array<LimitRow, N> unbounded_rows() noexcept
    {
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return std::array<LimitRow, N> { ((void) Is, LimitRow { unbounded, unbounded })... };
        }(std::make_index_sequence<N> {});
    }
} // namespace detail

/// One `LimitRow` per point of a series, in the series' own order.
///
/// `Elements`' idiom (`series.hpp`): two arity-disjoint constructors, the
/// matching one building `rows`, and every other non-zero count reaching a
/// body whose `static_assert` names both counts -- rather than letting
/// `std::array` pad the rows nobody typed. It is the conformity check's own
/// member type, so aggregate initialisation of the check with no factory is
/// refused too.
///
/// A loader holding rows read from master data builds one from a
/// `std::array` of `N` rows, or, when the count is only known at run time,
/// with `envelope_from`, which checks it.
///
/// No `{}` default member initialiser, deliberately (defect class 4): an
/// envelope must state its contents.
///
/// **`{}` is refused in this library's words**, naming both counts as every
/// other wrong count is, by the one rule `Elements` (`series.hpp`) follows
/// too: a type an expression holds leaves `{}` undeclared, and a type no
/// expression holds may refuse `{}` in words. An envelope belongs to a
/// conformity check, which no expression and no method's variant holds, and
/// `.envelope = {}` is a spelling an author filling one from master data
/// writes. The price is stated: `std::is_default_constructible_v` answers
/// `true` for an `Envelope<N>` and a `Conformity` that holds one, though
/// constructing either with `{}` does not compile.
template <std::size_t N>
struct Envelope
{
    /// The `N`-row case: the one path that builds `rows` from what was given.
    template <typename... Rs>
        requires(sizeof...(Rs) == N) && (std::convertible_to<Rs, LimitRow> && ...)
    constexpr Envelope(Rs... givenRows) noexcept:
        rows { givenRows... }
    {
    }

    /// Every other non-zero count: fails to compile, naming both counts.
    template <typename... Rs>
        requires(sizeof...(Rs) != N) && (sizeof...(Rs) != 0) && (std::convertible_to<Rs, LimitRow> && ...)
    constexpr Envelope(Rs...) noexcept:
        rows { detail::unbounded_rows<N>() }
    {
        static_assert(detail::RequireEnvelopeRowCountMatches<sizeof...(Rs), N>::value);
    }

    /// No rows for a series with some: fails to compile, naming both counts.
    /// Declared rather than left out, so that `{}` reaches this library's
    /// words instead of the compiler's -- see the rule above.
    constexpr Envelope() noexcept
        requires(N != 0)
        :
        rows { detail::unbounded_rows<N>() }
    {
        static_assert(detail::RequireEnvelopeRowCountMatches<0, N>::value);
    }

    /// Rows a loader has already gathered, one per point: the count is the
    /// array's, so it is checked by the type.
    constexpr explicit Envelope(std::array<LimitRow, N> const& loadedRows) noexcept:
        rows { loadedRows }
    {
    }

    /// One row per point, in the unit the check states its limits in.
    std::array<LimitRow, N> rows;

    /// The row for zero-based position @p at, which must be below `N`.
    [[nodiscard]] constexpr LimitRow operator[](std::size_t at) const noexcept
    {
        return rows[at];
    }

    /// How many rows there are -- `N`.
    [[nodiscard]] static constexpr std::size_t size() noexcept
    {
        return N;
    }
};

/// Why `envelope_from` refused: the rows given, and the rows the series
/// needs.
struct EnvelopeRowCountMismatch
{
    /// How many rows the loader had.
    std::size_t given;
    /// How many the envelope needs: its series' length.
    std::size_t expected;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(EnvelopeRowCountMismatch const&) const noexcept = default;
};

/// An `Envelope<N>` from rows whose count is only known at run time -- read
/// from master data -- or, when there are not exactly `N`, the two counts.
/// Never pads a short list nor drops the rows of a long one.
template <std::size_t N>
[[nodiscard]] constexpr std::expected<Envelope<N>, EnvelopeRowCountMismatch> envelope_from(
    std::span<LimitRow const> loadedRows) noexcept
{
    if (loadedRows.size() != N)
        return std::unexpected(EnvelopeRowCountMismatch { .given = loadedRows.size(), .expected = N });
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return Envelope<N> { std::array<LimitRow, N> { loadedRows[Is]... } };
    }(std::make_index_sequence<N> {});
}

namespace detail
{
    /// Fails to compile when a conformity check's unit does not measure its
    /// subject's dimension. Named so the unit's dimension and the subject
    /// print.
    template <Unit U, typename Subject>
    struct RequireConformityUnitMatches
    {
        static_assert(U.dimension == Subject::dimension,
                      "formula: this conformity check states its limits in a unit that does not measure the "
                      "dimension of its subject; the unit's dimension and the subject appear in this diagnostic as "
                      "the template arguments of RequireConformityUnitMatches");

        static constexpr bool value = true;
    };

    /// Fails to compile when a conformity check is given a single value to
    /// judge. Named so the subject prints.
    template <typename Subject>
    struct RequireConformityOfSeries
    {
        static_assert(SeriesNode<Subject>,
                      "formula: a conformity check judges each element of a series against its own row, and this "
                      "subject is a single value, not a series; the subject appears in this diagnostic as the "
                      "template argument of RequireConformityOfSeries -- read a quantity measured at every point "
                      "with series<Q, N>, or check a single value with a Constraint");

        static constexpr bool value = true;
    };

    /// Whatever a caller wrote for the envelope of a single-value subject --
    /// limit rows, an `Envelope` of any length, or `{}` -- so that the
    /// refusing `conformity` overload is the one chosen and the subject is
    /// the one thing reported. Holds nothing: the program does not compile.
    struct AnyEnvelope
    {
        constexpr AnyEnvelope() noexcept = default;

        template <typename... Rs>
            requires(sizeof...(Rs) != 0) && (std::convertible_to<Rs, LimitRow> && ...)
        constexpr AnyEnvelope(Rs...) noexcept
        {
        }

        template <std::size_t M>
        constexpr AnyEnvelope(Envelope<M> const&) noexcept
        {
        }
    };
} // namespace detail

/// Each element of the series @p S judged against its own row of `envelope`,
/// every limit stated in `U`; `verdict` is what applies to an element
/// outside its row.
///
/// `subject` and `envelope` have no `{}` initialiser, deliberately: see
/// `Corrections` (`lookup.hpp`) and `Envelope`.
template <Unit U, SeriesNode S>
struct Conformity
{
    static_assert(std::conditional_t<!detail::refused_already<S>(),
                                     detail::RequireConformityUnitMatches<U, S>,
                                     std::true_type>::value);
    static_assert(detail::RequireNamedScaledScalar<U>::value);

    /// The series judged.
    S subject;
    /// One row per element, in `U`.
    Envelope<S::length> envelope;
    /// What applies to an element outside its row.
    Verdict verdict {};
    /// Where this check comes from: the author's own declaration.
    Citation citation {};

    /// The unit every limit is stated in.
    static constexpr Unit unit = U;
    /// How many elements are judged.
    static constexpr std::size_t length = S::length;
};

/// `conformity<U>(subject, envelope, verdict, citation = {})`: each element of
/// @p subject against its row of @p envelope, limits in `U`. The envelope
/// takes the subject's length, so a braced list of the wrong count is refused
/// naming both counts (`Envelope`).
template <Unit U, SeriesNode S>
[[nodiscard]] constexpr Conformity<U, S> conformity(S subject,
                                                    Envelope<S::length> envelope,
                                                    Verdict verdict,
                                                    Citation citation = {}) noexcept
{
    return Conformity<U, S> { subject, envelope, verdict, citation };
}

/// A single value given a conformity check: refused in this library's words,
/// returning a check of a refused series (`detail::RefusedSeries`) that
/// silences every check downstream. The return type is deduced, as
/// `cumulative`'s refusing overload's is, and for its reason.
template <Unit U, Node N>
[[nodiscard]] constexpr auto conformity(N, detail::AnyEnvelope, Verdict verdict, Citation citation = {}) noexcept
{
    static_assert(detail::RequireConformityOfSeries<N>::value);
    return Conformity<U, detail::RefusedSeries<N::dimension>> {
        detail::RefusedSeries<N::dimension> {}, Envelope<1> { LimitRow { unbounded, unbounded } }, verdict, citation
    };
}

namespace detail
{
    /// Whether @p Sink wants to hear about the conformity check @p C: true
    /// when it defines **both** `conformity_entered(check)` and
    /// `conformity_produced(check, elementOutcomes)` -- `HearsSeries`' rule, for its
    /// reason.
    template <typename Sink, typename C>
    concept HearsConformity =
        requires(Sink sink, C const& conformityCheck, std::array<ConstraintOutcome, C::length> const& elementOutcomes) {
            sink.conformity_entered(conformityCheck);
            sink.conformity_produced(conformityCheck, elementOutcomes);
        };

    /// One element against its row: `Invalid` for a row that cannot be judged
    /// (whether or not the element was measured -- the row is the error),
    /// `NotChecked` for an element nobody measured, and otherwise `Satisfied`
    /// or `Violated` against the closed bounds, each limit read from @p unit
    /// into the coherent unit the element is in.
    template <typename Rep>
    [[nodiscard]] constexpr ConstraintOutcome judge_element(std::optional<Rep> const& element,
                                                            LimitRow limitRow,
                                                            Unit unit,
                                                            Verdict verdict) noexcept
    {
        if (!envelope_row_is_well_formed(limitRow))
            return ConstraintOutcome::invalid(ArithmeticError::DomainError);
        if (!element.has_value())
            return ConstraintOutcome::not_checked();

        if (std::optional<Rational> const lowerValue = limitRow.lower.value(); lowerValue.has_value())
        {
            Evaluated<Rep> const lowerInSi = in_si<Rep>(*lowerValue, unit);
            if (!lowerInSi.has_value())
                return ConstraintOutcome::invalid(lowerInSi.error());
            if (*element < **lowerInSi)
                return ConstraintOutcome::violated(verdict);
        }
        if (std::optional<Rational> const upperValue = limitRow.upper.value(); upperValue.has_value())
        {
            Evaluated<Rep> const upperInSi = in_si<Rep>(*upperValue, unit);
            if (!upperInSi.has_value())
                return ConstraintOutcome::invalid(upperInSi.error());
            if (**upperInSi < *element)
                return ConstraintOutcome::violated(verdict);
        }
        return ConstraintOutcome::satisfied();
    }
} // namespace detail

/// Judges each element of @p conformityCheck's subject against its row, and
/// returns one `ConstraintOutcome` per element, at the element's position.
///
/// A subject that fails to evaluate has no elements to judge -- a series
/// with one wrong element is not a series of right ones (`series.hpp`) -- so
/// every element is `Invalid`, carrying the failure's error. Otherwise each
/// element is judged alone (`detail::judge_element`): every element is
/// judged, with no short-circuit, for `check_all`'s reason.
///
/// A sink that defines both `conformity_entered` and `conformity_produced`
/// -- `RecordingSink` does -- records one step for the whole check, whose
/// operand is the subject's step.
///
/// `Rep = Rational` only, refused otherwise in this library's words: a limit
/// is closed, and a value exactly on it judged a few units in the last place
/// off is violated where it is satisfied.
template <typename Rep = Rational, Unit U, SeriesNode S, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::array<ConstraintOutcome, S::length> check_conformity(Conformity<U, S> const& conformityCheck,
                                                                                  Env const& environment,
                                                                                  Sink sink = {}) noexcept
{
    if constexpr (!std::is_same_v<Rep, Rational>)
    {
        static_assert(sizeof(Rep) == 0,
                      "formula: a conformity check can only be evaluated with Rep = Rational -- a limit is closed, "
                      "and judging a value that sits exactly on it needs exact comparison; evaluate it with "
                      "Rep = Rational instead (check_conformity's default)");
        // Default-initialised, not `{}`: see `SeriesValue` for cl's reason.
        std::array<ConstraintOutcome, S::length> refusedOutcomes;
        for (ConstraintOutcome& refusedOutcome: refusedOutcomes)
            refusedOutcome = ConstraintOutcome::invalid(ArithmeticError::DomainError);
        return refusedOutcomes;
    }
    else
    {
        if constexpr (detail::HearsConformity<Sink, Conformity<U, S>>)
            sink.conformity_entered(conformityCheck);

        EvaluatedSeries<Rep, S::length> const evaluated =
            detail::dispatch_series<Rep>(conformityCheck.subject, environment, sink);
        // Default-initialised, not `{}`: see `SeriesValue` for cl's reason.
        std::array<ConstraintOutcome, S::length> elementOutcomes;
        for (std::size_t at = 0; at < S::length; ++at)
        {
            if (!evaluated.has_value())
                elementOutcomes[at] = ConstraintOutcome::invalid(evaluated.error().error);
            else
                elementOutcomes[at] = detail::judge_element<Rep>(
                    evaluated->elements[at], conformityCheck.envelope[at], U, conformityCheck.verdict);
        }

        if constexpr (detail::HearsConformity<Sink, Conformity<U, S>>)
            sink.conformity_produced(conformityCheck, elementOutcomes);
        return elementOutcomes;
    }
}

} // namespace formula
