// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Where a formula's inputs come from.
///
/// An environment is keyed by quantity *type*, not by name: asking for a
/// quantity it does not hold is a compile error naming that quantity, so a
/// formula can never silently evaluate against a missing input.
///
/// Entries come in two kinds. A `Measured<Q>` is an observation that feeds the
/// formula. An `Entered<Q>` is a number a person typed in; when the evaluator
/// finds one for the quantity it was asked to produce, it returns that instead
/// of computing, and says so in the result's source. A result that was typed in
/// must never be presented as though the library had derived it.
///
/// Each kind has a series form, `MeasuredSeries<Q, N>` and `EnteredSeries<Q, N>`:
/// one quantity at each of the `N` points of a method's domain, any of which
/// may be absent (`series.hpp`). A quantity is held either as a single value or
/// as a series, never both, and each is read its own way -- `get<Q>()` and
/// `get_series<Q, N>()` -- so that reading one as the other is a compile error
/// in this library's words rather than a value of the wrong shape.
///
/// A third shape is **raw observations**, `MeasuredObservations<Q, Capacity>`:
/// as many values of one quantity as were observed, up to a stated capacity,
/// each at no point of any domain -- the particles measured one by one that a
/// method bins into classes (`binning.hpp`, `observations.hpp`). Read with
/// `get_observations<Q, Capacity>()`, and refused, in this library's words,
/// wherever a single value or a series is read.

#include <formula-cpp/error.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <span>
#include <tuple>
#include <type_traits>

namespace formula
{

/// A value a person supplied, as opposed to one the apparatus reported.
template <Described Q>
struct Entered
{
    /// The value as the person stated it.
    Measured<Q> measurement {};

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(Entered const&) const noexcept = default;
};

/// Marks a measurement as manually entered: `entered(Measured<Ratio> { … })`.
template <Described Q>
[[nodiscard]] constexpr Entered<Q> entered(Measured<Q> measurement) noexcept
{
    return Entered<Q> { measurement };
}

namespace detail
{
    /// Fails to compile when a series is built from a braced list of the wrong
    /// length. Named so both counts print.
    template <std::size_t Given, std::size_t Length>
    struct RequireSeriesElementCountMatches
    {
        static_assert(Given == Length,
                      "formula: this series was given a different number of elements than its length; "
                      "the two counts appear in this diagnostic as the template arguments Given and Length "
                      "of RequireSeriesElementCountMatches -- list one element per point of the series, "
                      "and write an element that was not measured as Measured<Q>::absent()");

        static constexpr bool value = true;
    };
} // namespace detail

/// One quantity observed at each of the `N` points of a method's domain -- a
/// screen analysis's retained masses, say, one per screen.
///
/// `N` is part of the type because a method's domain is part of the method,
/// not data (§16.1): a point the laboratory did not use is an **absent
/// element**, not a shorter series, so the length never comes from runtime
/// data. Each element keeps `Measured<Q>`'s invariant -- in `Q`'s declared
/// unit, no unit of its own -- so a series is `N` measurements and nothing
/// more; converting to the coherent unit is the evaluator's business.
///
/// Holds values, never expression nodes.
template <Described Q, std::size_t N>
class MeasuredSeries
{
  public:
    /// The series from an array the caller already holds, element by element,
    /// in the order of the method's domain.
    ///
    /// **Deduced, so that no braced list can reach it.** A braced list that
    /// could initialise a `std::array<Measured<Q>, N>` parameter is one
    /// `std::array` would pad: its own aggregate initialisation makes every
    /// element nobody typed absent, silently, for `({ { a, b, c } })`,
    /// `{ { { a, b, c } } }` and `({})` alike, and through `entered(...)` and
    /// `EnteredSeries` too. A template parameter cannot be deduced from a
    /// braced list, so every such spelling fails to compile, in the
    /// compiler's words, since no correct reading of them exists to point at.
    /// (For `({ { a, b, c } })` g++ 13.3 and 14.2 also reach the constructor
    /// below through the copy constructor, and add its count message, which
    /// is true; measured with cl 19.51, clang-cl and clang++ giving the
    /// compiler's words alone.)
    /// A braced list of the elements themselves goes to the constructor below.
    ///
    /// **The one route left open is an array the caller has already built
    /// short**, `std::array<Measured<Q>, 5> { a, b, c }`: `std::array` has
    /// padded it before this type sees it, so it is taken as it is. Build a
    /// series with `measured_series<Q>(...)`, which counts, or with one braced
    /// list of its elements.
    template <typename A>
        requires std::same_as<A, std::array<Measured<Q>, N>>
    constexpr explicit MeasuredSeries(A const& measurements) noexcept:
        _elements { measurements }
    {
    }

    /// The series from one braced list of its elements,
    /// `MeasuredSeries<Q, 5>({ a, b, c, d, e })` or `{ { a, b, c, d, e } }`.
    /// A list of any other length fails to compile, naming both counts through
    /// `detail::RequireSeriesElementCountMatches`: an element that was not
    /// measured is written `Measured<Q>::absent()`, never left out.
    ///
    /// `_elements` is default-initialised and then copied, not written `{}`:
    /// see `SeriesValue::elements` (`series.hpp`) for what value-initialising
    /// an array of `Measured` costs a consumer on cl.
    template <std::size_t Given>
    constexpr explicit MeasuredSeries(Measured<Q> const (&measurements)[Given]) noexcept
    {
        static_assert(detail::RequireSeriesElementCountMatches<Given, N>::value);
        if constexpr (Given == N)
            for (std::size_t at = 0; at < N; ++at)
                _elements[at] = measurements[at];
    }

    /// The element at zero-based position @p at. Past the end it is absent:
    /// never a neighbour's value, and never a zero -- but not reported
    /// either, so a caller indexes only inside `for (at = 0; at < N; ++at)`,
    /// never with a position it computed.
    [[nodiscard]] constexpr Measured<Q> element(std::size_t at) const noexcept
    {
        return at < N ? _elements[at] : Measured<Q>::absent();
    }

    /// How many points the series has -- `N`, present or not.
    [[nodiscard]] static constexpr std::size_t size() noexcept
    {
        return N;
    }

    /// Element-wise equality.
    [[nodiscard]] constexpr bool operator==(MeasuredSeries const&) const noexcept = default;

  private:
    std::array<Measured<Q>, N> _elements;
};

/// An element of `measured_series` that was not measured:
/// `measured_series<Retained>(127, formula::not_measured, 139)`.
struct NotMeasured
{
    /// Every `NotMeasured` is the same.
    [[nodiscard]] constexpr bool operator==(NotMeasured const&) const noexcept = default;
};

/// The spelling of an absent element -- see `NotMeasured`.
inline constexpr NotMeasured not_measured {};

namespace detail
{
    /// Fails to compile when an element of `measured_series<Q>` is neither a
    /// `Measured<Q>`, `not_measured`, nor something `Rational` is built from
    /// (a `Measured` of another quantity, a string, ...). A `double` is refused
    /// by `Rational` itself, in its own words. A built-in integer up to 64 bits
    /// converts exactly; `bool`, and an integer wider than that, draw this one.
    template <Described Q, typename Given>
    struct RequireSeriesElementOf
    {
        static_assert(std::is_same_v<Given, Measured<Q>> || std::is_same_v<Given, NotMeasured> || std::is_constructible_v<Rational, Given>,
                      "formula: an element of measured_series<Q> is a Measured<Q> of that one quantity, an exact number, or "
                      "formula::not_measured; the quantity and the element's type appear in this diagnostic as the template "
                      "arguments of RequireSeriesElementOf");

        static constexpr bool value = true;
    };

    /// @p given as the series element it stands for. After a refusal by
    /// `RequireSeriesElementOf` an element yields absent, so the mistake is
    /// reported once and not again by the conversion below.
    template <Described Q, typename Given>
    [[nodiscard]] constexpr Measured<Q> series_element(Given given) noexcept
    {
        if constexpr (std::is_same_v<Given, Measured<Q>>)
            return given;
        else if constexpr (std::is_same_v<Given, NotMeasured>)
            return Measured<Q>::absent();
        else if constexpr (std::is_constructible_v<Rational, Given>)
            return Measured<Q> { Rational { given } };
        else
            return Measured<Q>::absent();
    }
} // namespace detail

/// Builds a series from its elements, in order, and counts them. Each element
/// is a `Measured<Q>`, an exact number (`127`, `10.3_r`, a `Rational`), or
/// `not_measured`: `measured_series<Retained>(127, 10.3_r, formula::not_measured)`.
/// `Q` is stated rather than deduced, so a `Measured` of another quantity is
/// refused.
template <Described Q, typename... Given>
[[nodiscard]] constexpr auto measured_series(Given... given) noexcept
{
    static_assert((detail::RequireSeriesElementOf<Q, Given>::value && ...));
    return MeasuredSeries<Q, sizeof...(Given)> { std::array<Measured<Q>, sizeof...(Given)> { detail::series_element<Q>(given)... } };
}

/// A series a person supplied, as opposed to one the apparatus reported: the
/// series form of `Entered<Q>`.
template <Described Q, std::size_t N>
struct EnteredSeries
{
    /// The series as the person stated it. No default: a series of length `N`
    /// has no empty value a person could have meant.
    MeasuredSeries<Q, N> measurement;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(EnteredSeries const&) const noexcept = default;
};

/// Marks a series as manually entered: `entered(measured_series<Retained>(...))`.
template <Described Q, std::size_t N>
[[nodiscard]] constexpr EnteredSeries<Q, N> entered(MeasuredSeries<Q, N> measurement) noexcept
{
    return EnteredSeries<Q, N> { measurement };
}

namespace detail
{
    /// Fails to compile when more observations are listed than the stated
    /// capacity holds. Named so both counts print.
    template <std::size_t Given, std::size_t Capacity>
    struct RequireObservationsWithinCapacity
    {
        static_assert(Given <= Capacity,
                      "formula: more observations were listed than this set's capacity holds; the two counts "
                      "appear in this diagnostic as the template arguments Given and Capacity of "
                      "RequireObservationsWithinCapacity -- state a capacity at least as large as the most "
                      "observations the method takes");

        static constexpr bool value = true;
    };
} // namespace detail

/// Why `MeasuredObservations::from` refused: the observations given, and the
/// most the set holds. A count, not arithmetic, and so not an
/// `ArithmeticError` -- the shape `envelope_from`'s refusal has too
/// (`conformity.hpp`).
struct ObservationsOverCapacity
{
    /// How many observations the loader had.
    std::size_t given;
    /// The most the set holds: its `Capacity`.
    std::size_t capacity;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(ObservationsOverCapacity const&) const noexcept = default;
};

/// Raw observations of one quantity -- each particle's size, say, measured one
/// by one: as many as were observed, up to `Capacity`, in the order observed.
///
/// Unlike a series, an observation sits at no point of a method's domain, so
/// how many there are is data, not part of the type; only the most there can
/// be is. The capacity is static because evaluation allocates nothing (see
/// `series.hpp`): more observations than it holds are refused, at compile time
/// for a list written out and at run time by `from`, never truncated. Each
/// observation is present -- one not made is simply not listed -- and in
/// `Q`'s declared unit, as a `Measured<Q>` is.
///
/// Holds values, never expression nodes.
template <Described Q, std::size_t Capacity>
class MeasuredObservations
{
  public:
    /// The observations listed, in order, each in `Q`'s declared unit:
    /// `MeasuredObservations<Size, 50>(Rational { 163 }, Rational { 241 })`. More
    /// than `Capacity` fails to compile, naming both counts; none at all is an
    /// empty set.
    template <typename... Rs>
        requires(std::same_as<Rs, Rational> && ...)
    constexpr explicit MeasuredObservations(Rs... observed) noexcept
    {
        static_assert(detail::RequireObservationsWithinCapacity<sizeof...(Rs), Capacity>::value);
        if constexpr (sizeof...(Rs) <= Capacity)
        {
            std::size_t placed = 0;
            ((_observations[placed++] = observed), ...);
            _count = sizeof...(Rs);
        }
    }

    /// The observations in @p observed, in order, each in `Q`'s declared
    /// unit -- a set only known at run time. More than `Capacity` is refused
    /// with both counts, and nothing is dropped to make them fit.
    [[nodiscard]] static constexpr std::expected<MeasuredObservations, ObservationsOverCapacity> from(
        std::span<Rational const> observed) noexcept
    {
        if (observed.size() > Capacity)
            return std::unexpected { ObservationsOverCapacity { .given = observed.size(), .capacity = Capacity } };
        MeasuredObservations filled;
        for (std::size_t at = 0; at < observed.size(); ++at)
            filled._observations[at] = observed[at];
        filled._count = observed.size();
        return filled;
    }

    /// The observation at zero-based position @p at, or absent at and past
    /// `size()`: never a zero, and never another observation.
    [[nodiscard]] constexpr Measured<Q> observation(std::size_t at) const noexcept
    {
        return at < _count ? Measured<Q> { _observations[at] } : Measured<Q>::absent();
    }

    /// How many observations were made.
    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return _count;
    }

    /// The most observations this set holds.
    [[nodiscard]] static constexpr std::size_t capacity() noexcept
    {
        return Capacity;
    }

    /// Equality of the observations made, in order. Places past `size()`
    /// hold zero, so they compare equal.
    [[nodiscard]] constexpr bool operator==(MeasuredObservations const&) const noexcept = default;

  private:
    std::array<Rational, Capacity> _observations;
    std::size_t _count = 0;
};

namespace detail
{
    /// Reads the quantity and the provenance out of an environment entry. The
    /// primary template is deliberately empty, so a type that is neither a
    /// `Measured` nor an `Entered` fails at the point it is used rather than
    /// silently satisfying something.
    template <typename Entry>
    struct EntryTraits
    {
    };

    template <Described Q>
    struct EntryTraits<Measured<Q>>
    {
        using quantity = Q;
        static constexpr ValueSource source = ValueSource::Measured;
        static constexpr bool isEntered = false;
        static constexpr bool isSeries = false;
        static constexpr bool isObservations = false;

        [[nodiscard]] static constexpr Measured<Q> measurement(Measured<Q> measuredEntry) noexcept
        {
            return measuredEntry;
        }
    };

    template <Described Q>
    struct EntryTraits<Entered<Q>>
    {
        using quantity = Q;
        static constexpr ValueSource source = ValueSource::ManuallyEntered;
        static constexpr bool isEntered = true;
        static constexpr bool isSeries = false;
        static constexpr bool isObservations = false;

        [[nodiscard]] static constexpr Measured<Q> measurement(Entered<Q> enteredEntry) noexcept
        {
            return enteredEntry.measurement;
        }
    };

    template <Described Q, std::size_t N>
    struct EntryTraits<MeasuredSeries<Q, N>>
    {
        using quantity = Q;
        static constexpr ValueSource source = ValueSource::Measured;
        static constexpr bool isEntered = false;
        static constexpr bool isSeries = true;
        static constexpr bool isObservations = false;
        static constexpr std::size_t length = N;

        [[nodiscard]] static constexpr MeasuredSeries<Q, N> measurement(MeasuredSeries<Q, N> measuredEntry) noexcept
        {
            return measuredEntry;
        }
    };

    template <Described Q, std::size_t N>
    struct EntryTraits<EnteredSeries<Q, N>>
    {
        using quantity = Q;
        static constexpr ValueSource source = ValueSource::ManuallyEntered;
        static constexpr bool isEntered = true;
        static constexpr bool isSeries = true;
        static constexpr bool isObservations = false;
        static constexpr std::size_t length = N;

        [[nodiscard]] static constexpr MeasuredSeries<Q, N> measurement(EnteredSeries<Q, N> enteredEntry) noexcept
        {
            return enteredEntry.measurement;
        }
    };

    template <Described Q, std::size_t Capacity>
    struct EntryTraits<MeasuredObservations<Q, Capacity>>
    {
        using quantity = Q;
        static constexpr ValueSource source = ValueSource::Measured;
        static constexpr bool isEntered = false;
        static constexpr bool isSeries = false;
        static constexpr bool isObservations = true;
        static constexpr std::size_t capacity = Capacity;

        [[nodiscard]] static constexpr MeasuredObservations<Q, Capacity> measurement(
            MeasuredObservations<Q, Capacity> measuredEntry) noexcept
        {
            return measuredEntry;
        }
    };

    /// An entry this environment understands.
    template <typename Entry>
    concept EnvironmentEntry = requires { typename EntryTraits<Entry>::quantity; };

    /// Is @p Entry the one that carries @p Q?
    ///
    /// `Environment::provides`, `Environment::is_entered` and `Environment::index_of`
    /// each need exactly this test against every entry. Three independently
    /// written copies of one predicate are three chances for them to drift
    /// apart, and a drift would be invisible to tests: it would only widen
    /// `index_of`'s out-of-range sentinel into a reachable path, which no test
    /// can reach either, by construction. Naming the predicate once makes the
    /// three agree structurally instead of by care.
    template <typename Q, typename Entry>
    inline constexpr bool entry_is_for = std::is_same_v<Q, typename EntryTraits<Entry>::quantity>;

    /// Fails to compile when one quantity is supplied twice. First-wins and
    /// last-wins are equally arbitrary, and the caller meant exactly one of
    /// them, so neither is guessed.
    template <typename... Entries>
    struct RequireDistinctQuantities
    {
        static constexpr std::size_t count = sizeof...(Entries);

        template <typename Entry>
        static constexpr std::size_t occurrences =
            (std::size_t { 0 } + ...
             + std::size_t {
                 std::is_same_v<typename EntryTraits<Entry>::quantity, typename EntryTraits<Entries>::quantity> });

        static_assert((... && (occurrences<Entries> == 1)),
                      "formula: this environment supplies the same quantity more than once; the "
                      "entries appear in this diagnostic as the template arguments of "
                      "RequireDistinctQuantities");

        static constexpr bool value = true;
    };

    /// Fails to compile when an environment is asked for a quantity it does not
    /// hold. Named so the quantity and the environment both print.
    template <typename Q, typename Environment>
    struct RequireProvided
    {
        static_assert(Environment::template provides<Q>,
                      "formula: this environment provides no value for this quantity; the quantity "
                      "and the environment appear in this diagnostic as the template arguments of "
                      "RequireProvided");

        static constexpr bool value = true;
    };

    /// Fails to compile when a quantity the environment holds as a series is
    /// read as a single value. The environment does hold the quantity, so this
    /// is not `RequireProvided`'s refusal, and saying "no value" would be
    /// false. Named so the quantity and the entry, with its length, both print.
    template <typename Q, typename Entry>
    struct RequireSingleValueEntry
    {
        static_assert(!EntryTraits<Entry>::isSeries,
                      "formula: this environment holds a series for this quantity, not a single value; "
                      "read it with series<Q, N>, or reduce it to one value first (sum, interpolate_at); "
                      "the quantity and the entry appear in this diagnostic as the template arguments of "
                      "RequireSingleValueEntry");

        static constexpr bool value = true;
    };

    /// Fails to compile when a quantity the environment holds as a single
    /// value is read as a series.
    template <typename Q, typename Entry>
    struct RequireSeriesEntry
    {
        static_assert(EntryTraits<Entry>::isSeries || EntryTraits<Entry>::isObservations,
                      "formula: this environment holds a single value for this quantity, not a series; "
                      "supply it with measured_series, or read it with var<Q>; the quantity and the entry "
                      "appear in this diagnostic as the template arguments of RequireSeriesEntry");

        static constexpr bool value = true;
    };

    /// Fails to compile when a quantity the environment holds as raw
    /// observations is read as a single value or as a series. Neither
    /// `RequireSingleValueEntry`'s words nor `RequireSeriesEntry`'s would be
    /// true of it.
    template <typename Q, typename Entry>
    struct RequireNotObservationsEntry
    {
        static_assert(!EntryTraits<Entry>::isObservations,
                      "formula: this environment holds raw observations for this quantity, which are neither a "
                      "single value nor a series; read them with observations<Q, Capacity> and bin them into "
                      "classes with binned<KeyUnit, Classes>; the quantity and the entry appear in this "
                      "diagnostic as the template arguments of RequireNotObservationsEntry");

        static constexpr bool value = true;
    };

    /// Fails to compile when a quantity the environment holds as a single
    /// value or a series is read as raw observations.
    template <typename Q, typename Entry>
    struct RequireObservationsEntry
    {
        static_assert(EntryTraits<Entry>::isObservations,
                      "formula: this environment holds a single value or a series for this quantity, not raw "
                      "observations; supply them with MeasuredObservations<Q, Capacity>, or read the quantity "
                      "with var<Q> or series<Q, N>; the quantity and the entry appear in this diagnostic as "
                      "the template arguments of RequireObservationsEntry");

        static constexpr bool value = true;
    };

    /// Fails to compile when raw observations are read with a capacity other
    /// than the one they were supplied with. Named so both capacities print.
    template <typename Q, std::size_t Supplied, std::size_t Read>
    struct RequireObservationsCapacity
    {
        static_assert(Supplied == Read,
                      "formula: this environment holds observations of a different capacity for this quantity "
                      "than the one read; the quantity, the capacity supplied and the capacity read appear in "
                      "this diagnostic as the template arguments Q, Supplied and Read of "
                      "RequireObservationsCapacity -- a method states its capacity once");

        static constexpr bool value = true;
    };

    /// Fails to compile when a series is read with a length other than the
    /// one it was supplied with. Named so both lengths print.
    template <typename Q, std::size_t Supplied, std::size_t Read>
    struct RequireSeriesLength
    {
        static_assert(Supplied == Read,
                      "formula: this environment holds a series of a different length for this quantity "
                      "than the one read; the quantity, the length supplied and the length read appear in "
                      "this diagnostic as the template arguments Q, Supplied and Read of "
                      "RequireSeriesLength -- a method declares its length once, and a point it did not "
                      "measure is an absent element, not a shorter series");

        static constexpr bool value = true;
    };
} // namespace detail

/// A set of inputs, keyed by quantity type.
template <detail::EnvironmentEntry... Entries>
class Environment
{
  public:
    static_assert(detail::RequireDistinctQuantities<Entries...>::value);

    /// The only constructor. With no entries it is also the default one, which
    /// is why there is no separate `= default` declaration: for `Environment<>`
    /// the two would be the same signature declared twice.
    constexpr explicit Environment(Entries... entries) noexcept:
        _entries { entries... }
    {
    }

    /// Does this environment hold a value for @p Q?
    template <Described Q>
    static constexpr bool provides = (detail::entry_is_for<Q, Entries> || ...);

    /// Was the value for @p Q typed in by a person rather than measured?
    ///
    /// Written as one fold rather than as a call to a private helper: the
    /// initialiser of a static data member template is **not** a complete-class
    /// context, so a helper declared further down the class would not be
    /// visible here. Member function bodies are, which is why `index_of` may
    /// stay private and below.
    ///
    /// True for a single value and for a series alike: whichever shape the
    /// person typed in, the evaluator must not compute over it, and reading it
    /// in the wrong shape is refused where it is read.
    template <Described Q>
    static constexpr bool is_entered =
        (... || (detail::entry_is_for<Q, Entries> && detail::EntryTraits<Entries>::isEntered));

    /// Was the value for @p Q a series typed in by a person? False for a
    /// single entered value and for a measured series.
    template <Described Q>
    static constexpr bool is_entered_series =
        (...
         || (detail::entry_is_for<Q, Entries> && detail::EntryTraits<Entries>::isEntered
             && detail::EntryTraits<Entries>::isSeries));

    /// The value held for @p Q. Asking for a quantity this environment does not
    /// hold is a compile error naming it -- never a zero, and never a silent
    /// default.
    template <Described Q>
    [[nodiscard]] constexpr Measured<Q> get() const noexcept
    {
        static_assert(detail::RequireProvided<Q, Environment>::value);
        // The `else` below -- and its counterpart in `source_of()` further down --
        // is unreachable by construction: `RequireProvided`'s static_assert above
        // has already failed whenever `provides<Q>` is false. It stays rather than
        // being deleted because a failed static_assert does not stop compilation,
        // so the body below still gets instantiated against the same `Q`. Without
        // this guard that instantiation runs into code written for a provided
        // quantity, and the reader gets our one clean diagnostic followed by a
        // cascade of consequential errors from the mismatch: eight from cl where
        // one would do, three from clang-cl where one would do.
        if constexpr (provides<Q>)
        {
            constexpr std::size_t entryIndex = index_of<Q>();
            using Entry = std::tuple_element_t<entryIndex, std::tuple<Entries...>>;
            // Gated the same way, for the same reason: without the `if
            // constexpr` a series entry would reach the `return` below and add
            // the compiler's own conversion error to this library's message.
            static_assert(detail::RequireSingleValueEntry<Q, Entry>::value);
            static_assert(detail::RequireNotObservationsEntry<Q, Entry>::value);
            if constexpr (!detail::EntryTraits<Entry>::isSeries && !detail::EntryTraits<Entry>::isObservations)
                return detail::EntryTraits<Entry>::measurement(std::get<entryIndex>(_entries));
            else
                return Measured<Q>::absent();
        }
        else
            return Measured<Q>::absent();
    }

    /// The series held for @p Q, which must have length @p N. Asking for a
    /// quantity this environment does not hold, for one it holds as a single
    /// value, or for a series of another length, is a compile error with one
    /// message of this library's for each.
    template <Described Q, std::size_t N>
    [[nodiscard]] constexpr MeasuredSeries<Q, N> get_series() const noexcept
    {
        static_assert(detail::RequireProvided<Q, Environment>::value);
        // Every refusal below is followed by an `else` that returns an
        // all-absent series nobody will see, for `get()`'s reason above: a
        // failed static_assert does not stop compilation, and the code past it
        // was written for the case it refuses.
        if constexpr (provides<Q>)
        {
            constexpr std::size_t entryIndex = index_of<Q>();
            using Entry = std::tuple_element_t<entryIndex, std::tuple<Entries...>>;
            static_assert(detail::RequireSeriesEntry<Q, Entry>::value);
            static_assert(detail::RequireNotObservationsEntry<Q, Entry>::value);
            if constexpr (detail::EntryTraits<Entry>::isSeries)
            {
                static_assert(detail::RequireSeriesLength<Q, detail::EntryTraits<Entry>::length, N>::value);
                if constexpr (detail::EntryTraits<Entry>::length == N)
                    return detail::EntryTraits<Entry>::measurement(std::get<entryIndex>(_entries));
                else
                    return MeasuredSeries<Q, N> { std::array<Measured<Q>, N> {} };
            }
            else
                return MeasuredSeries<Q, N> { std::array<Measured<Q>, N> {} };
        }
        else
            return MeasuredSeries<Q, N> { std::array<Measured<Q>, N> {} };
    }

    /// The raw observations held for @p Q, which must have capacity
    /// @p Capacity. Asking for a quantity this environment does not hold, for
    /// one it holds as a single value or a series, or for observations of
    /// another capacity, is a compile error with one message of this
    /// library's for each.
    template <Described Q, std::size_t Capacity>
    [[nodiscard]] constexpr MeasuredObservations<Q, Capacity> get_observations() const noexcept
    {
        static_assert(detail::RequireProvided<Q, Environment>::value);
        // Each refusal is followed by an empty set nobody will see, for
        // `get()`'s reason above.
        if constexpr (provides<Q>)
        {
            constexpr std::size_t entryIndex = index_of<Q>();
            using Entry = std::tuple_element_t<entryIndex, std::tuple<Entries...>>;
            static_assert(detail::RequireObservationsEntry<Q, Entry>::value);
            if constexpr (detail::EntryTraits<Entry>::isObservations)
            {
                static_assert(detail::RequireObservationsCapacity<Q, detail::EntryTraits<Entry>::capacity, Capacity>::value);
                if constexpr (detail::EntryTraits<Entry>::capacity == Capacity)
                    return detail::EntryTraits<Entry>::measurement(std::get<entryIndex>(_entries));
                else
                    return MeasuredObservations<Q, Capacity> {};
            }
            else
                return MeasuredObservations<Q, Capacity> {};
        }
        else
            return MeasuredObservations<Q, Capacity> {};
    }

    /// Where the value for @p Q came from: `ManuallyEntered` for a value or a
    /// series a person typed in (an `Entered` or an `EnteredSeries` entry),
    /// `Measured` for any other entry.
    ///
    /// Asked, through `detail::known_source` (`evaluate.hpp`), for a sink
    /// that records where a value came from: by the evaluator of a `var<Q>`,
    /// of the value it reads (`detail::report_input_source`), and by the
    /// evaluators of an overlay's fixed constant and derived quantity, of the
    /// entry each replaced (`detail::report_replaced_entry`, `overlay.hpp`).
    /// Both prefer it to `is_entered<Q>` because an environment of another
    /// type can answer it at run time, and say `Derived` of a value it
    /// calculated; for this type the two always agree. `checked_evaluate`
    /// does not call it for the result: it already knows, from
    /// `Env::is_entered<Result>`, whether the result it is about to return
    /// was typed in or derived. So `ValueSource::Measured` -- correct as it
    /// is here -- appears on a trace's steps, and never as the source of an
    /// `Outcome` that `checked_evaluate` returns; only `Derived` and
    /// `ManuallyEntered` do.
    template <Described Q>
    [[nodiscard]] constexpr ValueSource source_of() const noexcept
    {
        static_assert(detail::RequireProvided<Q, Environment>::value);
        if constexpr (provides<Q>)
        {
            constexpr std::size_t entryIndex = index_of<Q>();
            return detail::EntryTraits<std::tuple_element_t<entryIndex, std::tuple<Entries...>>>::source;
        }
        else
            return ValueSource::Derived;
    }

  private:
    template <Described Q>
    [[nodiscard]] static constexpr std::size_t index_of() noexcept
    {
        std::size_t foundAt = sizeof...(Entries);
        std::size_t scanned = 0;
        (((detail::entry_is_for<Q, Entries> ? (foundAt = scanned) : foundAt), ++scanned), ...);
        return foundAt;
    }

    std::tuple<Entries...> _entries {};
};

/// Builds an environment: `environment(Measured<A> { … }, entered(Measured<B> { … }))`.
template <detail::EnvironmentEntry... Entries>
[[nodiscard]] constexpr Environment<Entries...> environment(Entries... entries) noexcept
{
    return Environment<Entries...> { entries... };
}

} // namespace formula
