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

#include <formula-cpp/measured.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/quantity.hpp>

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace formula
{

/// A value a person supplied, as opposed to one the apparatus reported.
template <Described Q>
struct Entered
{
    Measured<Q> measurement {};

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

        [[nodiscard]] static constexpr Measured<Q> measurement(Measured<Q> entry) noexcept
        {
            return entry;
        }
    };

    template <Described Q>
    struct EntryTraits<Entered<Q>>
    {
        using quantity = Q;
        static constexpr ValueSource source = ValueSource::ManuallyEntered;
        static constexpr bool isEntered = true;

        [[nodiscard]] static constexpr Measured<Q> measurement(Entered<Q> entry) noexcept
        {
            return entry.measurement;
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
    template <Described Q>
    static constexpr bool is_entered =
        (... || (detail::entry_is_for<Q, Entries> && detail::EntryTraits<Entries>::isEntered));

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
            constexpr std::size_t index = index_of<Q>();
            using Entry = std::tuple_element_t<index, std::tuple<Entries...>>;
            return detail::EntryTraits<Entry>::measurement(std::get<index>(_entries));
        }
        else
            return Measured<Q>::absent();
    }

    /// Where the value for @p Q came from.
    ///
    /// `checked_evaluate` does not call this: it already knows, from
    /// `Env::is_entered<Result>`, whether the result it is about to return was
    /// typed in or derived, and decides `ValueSource` from that directly. This
    /// accessor is the one a future tracing layer (phase 7) will consult for an
    /// *input's* provenance instead, which is a question the evaluator never
    /// asks today. Consequently `ValueSource::Measured` -- correct as it is
    /// here -- cannot appear in any `Outcome` this phase produces; only
    /// `Derived` and `ManuallyEntered` do.
    template <Described Q>
    [[nodiscard]] constexpr ValueSource source_of() const noexcept
    {
        static_assert(detail::RequireProvided<Q, Environment>::value);
        if constexpr (provides<Q>)
        {
            constexpr std::size_t index = index_of<Q>();
            return detail::EntryTraits<std::tuple_element_t<index, std::tuple<Entries...>>>::source;
        }
        else
            return ValueSource::Derived;
    }

  private:
    template <Described Q>
    [[nodiscard]] static constexpr std::size_t index_of() noexcept
    {
        std::size_t found = sizeof...(Entries);
        std::size_t position = 0;
        (((detail::entry_is_for<Q, Entries> ? (found = position) : found), ++position), ...);
        return found;
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
