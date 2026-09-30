// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Bounded retry: one attempt expression, evaluated again and again until an
/// acceptance condition holds, a fixed number of times at most, and the
/// method's verdict when it never holds.
///
///     retry<Estimate, 4, FirstJudged::AtFirstAttempt>(
///         starting_from(constant<unit::Gram>(0)),  // attempt 0's value, optional
///         constant<unit::Gram>(6.08) + previous_attempt<Estimate> / 2,
///         previous_attempt<Estimate> - this_attempt<Estimate> >= constant<unit::Gram>(-0.76),
///         Verdict { "repeat the determination" },
///         { .reference = "Example Standard 12", .section = "6" });
///
/// Inside the attempt and the acceptance, three nodes say where the retry is:
/// `attempt_number` (dimensionless, the method's own 1-based k),
/// `previous_attempt<R>` (the attempt before, or the starting value at the
/// first) and, in the acceptance only, `this_attempt<R>` (the value just
/// produced). A fourth, `attempt_input<Q>`, reads the determination of `Q`
/// recorded for the attempt that is running, from a series of one per attempt
/// allowed. Outside a retry each is refused where it is evaluated.
///
/// **Termination is structural.** The number of attempts is a template
/// argument, capped at 64 where the retry is written, and the loop is a `for`
/// over it. No path runs more attempts than that, and none depends on the
/// data to stop: an attempt either is accepted, or the next runs, or the last
/// one ran.
///
/// **It ends in exactly one of six ways** (`RetryEnd`): accepted at an
/// attempt, with that attempt's value; exhausted, with the method's verdict
/// and no value; not judgeable, empty, when an attempt or its judgement was
/// absent -- an absent comparison is "cannot tell", never "try again"; not
/// recorded, empty, when a determination an attempt read was never recorded;
/// failed, as a `RetryFailure` naming the attempt, when an attempt or its
/// judgement failed arithmetically; and manually entered, when a person typed
/// the result in, which no attempt then replaces.
///
/// A retry is not a `Node`: running out of attempts ends in a verdict, which
/// no node's value can carry. It is evaluated at the top, by
/// `checked_evaluate_retry`, into a `RetryOutcome`.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/record.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/yields.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>
#include <type_traits>

namespace formula
{

/// From which attempt on the acceptance is judged. Required, with no default:
/// "until two successive results agree" cannot be judged at the first
/// attempt, and "until the result reaches a threshold" can, so either default
/// would be silently wrong for the other.
enum class FirstJudged : std::uint8_t
{
    /// Judged after every attempt, the first included.
    AtFirstAttempt,
    /// Judged from the second attempt on; the first is not judged, which is
    /// not the same as rejected. A first attempt whose value is absent still
    /// ends the retry, `NotJudgeable`, though no judgement was due: the
    /// second attempt's judgement would compare against that absent value,
    /// and an absent comparison is "cannot tell", not "try again". One
    /// whose recorded determination is missing ends it `NotRecorded`.
    AtSecondAttempt,
};

/// How a retry ended: exactly one of these, always. `RetryOutcome::end()`
/// reports every one but `Failed`: a failed retry has no outcome, and
/// `checked_evaluate_retry` returns its `RetryFailure` instead. `Failed` is
/// how a trace records that end (`RetryStepData::end`, `trace.hpp`).
enum class RetryEnd : std::uint8_t
{
    /// The acceptance held after an attempt; the outcome is that attempt's
    /// value.
    Accepted,
    /// The acceptance never held in the attempts allowed; the outcome is the
    /// retry's verdict, never the last value.
    Exhausted,
    /// An attempt's value, or its judgement, was absent; the outcome is
    /// empty. The retry stops: an absent comparison is not "not yet".
    NotJudgeable,
    /// An attempt, or its acceptance, read a recorded determination
    /// (`attempt_input`) that is absent; the outcome is empty. Not
    /// `NotJudgeable`: nothing was compared -- the method needed a
    /// determination nobody recorded.
    NotRecorded,
    /// An attempt, or its judgement, failed arithmetically; there is no
    /// outcome, only the `RetryFailure` -- never `RetryOutcome::end()`.
    Failed,
    /// A person entered the result; no attempt ran.
    ManuallyEntered,
};

/// A lowercase phrase with no trailing punctuation, so callers can embed it in a longer sentence.
[[nodiscard]] constexpr std::string_view describe(RetryEnd ended) noexcept
{
    switch (ended)
    {
        case RetryEnd::Accepted:
            return "accepted";
        case RetryEnd::Exhausted:
            return "exhausted";
        case RetryEnd::NotJudgeable:
            return "not judgeable";
        case RetryEnd::NotRecorded:
            return "not recorded";
        case RetryEnd::Failed:
            return "failed";
        case RetryEnd::ManuallyEntered:
            return "manually entered";
    }
    return "unknown retry end";
}

namespace detail
{
template <>
inline constexpr bool formats_by_describe<RetryEnd> = true;
} // namespace detail

/// The most attempts a retry may allow. The methods this shape exists for
/// repeat a step a few times; a larger count is almost always a typo, and 64
/// attempts of a five-node attempt with a four-node judgement fit one
/// constant evaluation on cl 19.51, clang-cl and clang++ 22.1.3, g++ 13.3 and
/// g++ 14.2 (measured in phase 15's spike, step 5).
///
/// The cap bounds the count, not the numbers: an exact fixpoint as simple as
/// `6.08 g + w(k-1) / 2` doubles its denominator every attempt and passes
/// `Rational`'s range at attempt 53 of 64. That is reported, `Failed` with
/// `Overflow`, never a wrapped value.
inline constexpr std::size_t retryAttemptCap = 64;

/// Attempt 0's value: what `previous_attempt` reads at the first attempt.
/// No `{}` initialiser on the expression, deliberately (defect class 4): see
/// `Corrections` (`lookup.hpp`).
template <Node E>
struct StartingValue
{
    /// The starting value's expression, evaluated once, before the first
    /// attempt, in the caller's environment.
    E expression;
};

/// A retry with no starting value: `previous_attempt` read at the first
/// attempt is then the author's mistake, and fails with `DomainError`.
struct NoStartingValue
{
};

/// @p expression as a retry's starting value.
template <Node E>
[[nodiscard]] constexpr StartingValue<E> starting_from(E expression) noexcept
{
    return StartingValue<E> { expression };
}

/// Which part of a retry is being evaluated: its starting value, before any
/// attempt; an attempt expression; or the acceptance over what it produced.
enum class AttemptPhase : std::uint8_t
{
    Starting,
    Attempting,
    Judging,
};

/// The attempt's number, k, counted from 1: a dimensionless value in the
/// method's own algebra, not a position. Meaningful only inside a retry.
struct AttemptNumberNode: NodeBase
{
    /// A pure number.
    static constexpr Dimension dimension = dim::Scalar;
};

/// The attempt's number, k, from 1.
inline constexpr AttemptNumberNode attempt_number {};

/// The previous attempt's value of @p R, or the starting value at the first
/// attempt. Meaningful only inside a retry.
template <Described R>
struct PreviousAttemptNode: NodeBase
{
    /// The retry's result quantity.
    using quantity = R;
    /// Its dimension.
    static constexpr Dimension dimension = Describe<R>::dimension;
};

/// The previous attempt's value of @p R.
template <Described R>
inline constexpr PreviousAttemptNode<R> previous_attempt {};

/// The value of @p R this attempt produced. Meaningful only in a retry's
/// acceptance: in the attempt expression it would be circular.
template <Described R>
struct ThisAttemptNode: NodeBase
{
    /// The retry's result quantity.
    using quantity = R;
    /// Its dimension.
    static constexpr Dimension dimension = Describe<R>::dimension;
};

/// The value of @p R this attempt produced.
template <Described R>
inline constexpr ThisAttemptNode<R> this_attempt {};

/// The determination of @p Q recorded for the attempt that is running:
/// element k - 1 of the series of @p Q the environment holds, one per attempt
/// the retry allows -- so a retry of at most 4 attempts reads a
/// `series<Q, 4>`, and its third attempt reads the third element.
/// Meaningful only in a retry's attempt and its acceptance.
///
/// An absent element ends the retry `NotRecorded` at that attempt, not
/// `NotJudgeable`; one after the attempt that ends it is never read.
template <Described Q>
struct AttemptInputNode: NodeBase
{
    /// The recorded quantity.
    using quantity = Q;
    /// Its dimension.
    static constexpr Dimension dimension = Describe<Q>::dimension;
};

/// The determination of @p Q recorded for the attempt that is running.
template <Described Q>
inline constexpr AttemptInputNode<Q> attempt_input {};

namespace detail
{
    /// The context nodes, seen by a precision limit's checks
    /// (`precision.hpp`, which cannot include this header): each is a value
    /// read whole, with nothing inside it, so an acceptance may take a
    /// limit's level from `this_attempt`, or from any of them.
    template <>
    struct LevelChildren<AttemptNumberNode>: LevelLeaf
    {
    };

    template <Described R>
    struct LevelChildren<PreviousAttemptNode<R>>: LevelLeaf
    {
    };

    template <Described R>
    struct LevelChildren<ThisAttemptNode<R>>: LevelLeaf
    {
    };

    template <Described Q>
    struct LevelChildren<AttemptInputNode<Q>>: LevelLeaf
    {
    };
} // namespace detail

namespace detail
{
    /// The environment an attempt is evaluated in: the caller's, and where
    /// the retry is. Everything a node asks an environment is forwarded
    /// unchanged -- the seven public members of `Environment`, and a read
    /// that can fail (`checked_get`) when the wrapped environment has one --
    /// so a specimen's data reads the same inside a retry as outside it, and
    /// its refusals (`RequireProvided`, the series/single cross refusals) fire
    /// once, in their own words. `source_of` and `checked_get` are there only
    /// when the wrapped environment has them, so that the variable evaluator,
    /// which asks whether an environment has each (`evaluate.hpp`), gets the
    /// same answer inside a retry as outside it.
    ///
    /// **If a change adds a member of `Environment` that a node calls, it
    /// must be forwarded here**; `retry_tests.cpp` lists the members by hand
    /// and fails for one that is not.
    ///
    /// Values are in the coherent unit, in @p Rep, as every evaluator's.
    /// @p R is the retry's result quantity, the only one `previous_attempt`
    /// and `this_attempt` may name (`RequireRetriedQuantity`); @p Max the
    /// attempts it allows, the length of the series `attempt_input` reads.
    template <typename Env, typename Rep, typename R, AttemptPhase P, std::size_t Max>
    class AttemptEnvironment
    {
      public:
        /// The retry's result quantity.
        using retried = R;

        /// The starting value's environment: @p inner alone, before any
        /// attempt, where no context node has anything to read.
        constexpr explicit AttemptEnvironment(Env const& inner) noexcept
            requires(P == AttemptPhase::Starting)
            :
            _inner { &inner },
            _attemptAt { 0 },
            _before { std::unexpected { ArithmeticError::DomainError } },
            _produced {}
        {
        }

        /// Attempt @p attemptAt (from 1) of a retry over @p inner, after
        /// @p before: the previous attempt's value, the starting value, or
        /// the error reading it is -- `DomainError` with no starting value.
        /// An absent determination `attempt_input` reads is marked in
        /// @p unrecorded, which the retry's loop reads.
        constexpr AttemptEnvironment(Env const& inner,
                                     std::size_t attemptAt,
                                     Evaluated<Rep> before,
                                     bool* unrecorded = nullptr) noexcept
            requires(P == AttemptPhase::Attempting)
            :
            _inner { &inner },
            _attemptAt { attemptAt },
            _before { before },
            _produced {},
            _unrecorded { unrecorded }
        {
        }

        /// The same, judging @p produced, the value the attempt produced.
        constexpr AttemptEnvironment(Env const& inner,
                                     std::size_t attemptAt,
                                     Evaluated<Rep> before,
                                     Rep produced,
                                     bool* unrecorded = nullptr) noexcept
            requires(P == AttemptPhase::Judging)
            :
            _inner { &inner },
            _attemptAt { attemptAt },
            _before { before },
            _produced { produced },
            _unrecorded { unrecorded }
        {
        }

        /// Forwarded: `Environment::provides`.
        template <Described Q>
        static constexpr bool provides = Env::template provides<Q>;
        /// Forwarded: `Environment::is_entered`.
        template <Described Q>
        static constexpr bool is_entered = Env::template is_entered<Q>;
        /// Forwarded: `Environment::is_entered_series`.
        template <Described Q>
        static constexpr bool is_entered_series = Env::template is_entered_series<Q>;

        /// Forwarded: `Environment::get`.
        template <Described Q>
        [[nodiscard]] constexpr Measured<Q> get() const noexcept
        {
            return _inner->template get<Q>();
        }

        /// Forwarded: `Environment::get_series`.
        template <Described Q, std::size_t N>
        [[nodiscard]] constexpr MeasuredSeries<Q, N> get_series() const noexcept
        {
            return _inner->template get_series<Q, N>();
        }

        /// Forwarded: `Environment::get_observations`.
        template <Described Q, std::size_t Capacity>
        [[nodiscard]] constexpr MeasuredObservations<Q, Capacity> get_observations() const noexcept
        {
            return _inner->template get_observations<Q, Capacity>();
        }

        /// Forwarded: `Environment::source_of`, when the wrapped environment
        /// has it (`RunTimeSource`).
        template <Described Q>
            requires RunTimeSource<Env, Q>
        [[nodiscard]] constexpr ValueSource source_of() const noexcept
        {
            return _inner->template source_of<Q>();
        }

        /// Forwarded: a read that can fail, when the wrapped environment has
        /// one (`ReportsReadFailure`, `evaluate.hpp`). `Environment` has
        /// none.
        template <Described Q>
            requires ReportsReadFailure<Env, Q>
        [[nodiscard]] constexpr std::expected<Measured<Q>, ArithmeticError> checked_get() const noexcept
        {
            return _inner->template checked_get<Q>();
        }

        /// Which attempt this is, from 1.
        [[nodiscard]] constexpr std::size_t attempt() const noexcept
        {
            return _attemptAt;
        }

        /// The previous attempt's value, the starting value, or the error
        /// reading it is.
        [[nodiscard]] constexpr Evaluated<Rep> previous() const noexcept
        {
            return _before;
        }

        /// The value this attempt produced; asked only when judging it.
        [[nodiscard]] constexpr Rep current() const noexcept
            requires(P == AttemptPhase::Judging)
        {
            return _produced;
        }

        /// Says that a determination this attempt needed is absent: the
        /// retry ends `NotRecorded` once the attempt, or its judgement, is
        /// done.
        constexpr void mark_not_recorded() const noexcept
        {
            if (_unrecorded != nullptr)
                *_unrecorded = true;
        }

      private:
        template <typename>
        friend struct RecordContextOf;

        Env const* _inner;
        std::size_t _attemptAt;
        Evaluated<Rep> _before;
        Rep _produced;
        bool* _unrecorded = nullptr;
    };

    /// Whether @p E is an attempt's environment, and in which phase.
    template <typename E>
    struct AttemptEnvironmentTraits
    {
        static constexpr bool isAttempt = false;
        static constexpr bool isStarting = false;
        static constexpr bool isJudging = false;
        using retried = void;
        static constexpr std::size_t attemptLimit = 0;
    };

    template <typename Env, typename Rep, typename R, AttemptPhase P, std::size_t Max>
    struct AttemptEnvironmentTraits<AttemptEnvironment<Env, Rep, R, P, Max>>
    {
        static constexpr bool isAttempt = true;
        static constexpr bool isStarting = P == AttemptPhase::Starting;
        static constexpr bool isJudging = P == AttemptPhase::Judging;
        using retried = R;
        static constexpr std::size_t attemptLimit = Max;
    };

    /// An attempt's environment reaches the `RecordContext` its caller's
    /// does, and only that one: a retry evaluated against a
    /// `record_context(...)` reads from another record inside an attempt, its
    /// starting value or its acceptance as it would outside the retry. Over an
    /// environment that reaches none, nothing, so a scope there is refused in
    /// `RequireRecordContext`'s words, as it is outside a retry.
    template <typename Env, typename Rep, typename R, AttemptPhase P, std::size_t Max>
        requires reachesRecordContext<Env>
    struct RecordContextOf<AttemptEnvironment<Env, Rep, R, P, Max>>
    {
        /// The context the caller's environment reaches.
        using type = typename RecordContextOf<Env>::type;

        /// The context @p attemptEnvironment's inner environment reaches.
        [[nodiscard]] static constexpr type const& of(
            AttemptEnvironment<Env, Rep, R, P, Max> const& attemptEnvironment) noexcept
        {
            return RecordContextOf<Env>::of(*attemptEnvironment._inner);
        }
    };
    /// Fails to compile when a retry's context node is evaluated outside any
    /// retry. Named so the environment prints.
    template <typename Env>
    struct RequireInsideRetry
    {
        static_assert(AttemptEnvironmentTraits<Env>::isAttempt,
                      "formula: previous_attempt, this_attempt and attempt_number are only meaningful inside a retry; "
                      "inside from_record<Role>(...) the other record's data is read, where no attempt runs -- read "
                      "the context node outside the scope");

        static constexpr bool value = true;
    };

    /// Fails to compile when a context node is read in a retry's starting
    /// value, which is evaluated before the first attempt.
    template <typename Env>
    struct RequireNotInStartingValue
    {
        static_assert(!AttemptEnvironmentTraits<Env>::isStarting,
                      "formula: a retry's starting value is evaluated before its first attempt, so no attempt exists "
                      "yet; previous_attempt, this_attempt and attempt_number cannot be read in it");

        static constexpr bool value = true;
    };

    /// Fails to compile when `previous_attempt<Q>` or `this_attempt<Q>` names
    /// a quantity that is not the retry's own result: it would read the
    /// retry's value under another quantity's name -- and, with another
    /// dimension, as another kind of value. Named so both print.
    template <typename Q, typename Env>
    struct RequireRetriedQuantity
    {
        static_assert(std::is_same_v<std::remove_cv_t<Q>, std::remove_cv_t<typename AttemptEnvironmentTraits<Env>::retried>>,
                      "formula: previous_attempt and this_attempt name the retry's own result quantity; this one "
                      "names another -- both appear in this diagnostic as the template arguments of "
                      "RequireRetriedQuantity");

        static constexpr bool value = true;
    };

    /// Fails to compile when `this_attempt` is read in the attempt
    /// expression, where the value it names is still being computed.
    template <typename Env>
    struct RequireThisAttemptInJudgement
    {
        static_assert(AttemptEnvironmentTraits<Env>::isJudging,
                      "formula: this_attempt is the value an attempt produced, known only when judging it; read in "
                      "the attempt expression itself it would be circular -- use previous_attempt there");

        static constexpr bool value = true;
    };

    /// Fails to compile when `attempt_input` is evaluated outside any retry.
    template <typename Env>
    struct RequireAttemptInputInsideRetry
    {
        static_assert(AttemptEnvironmentTraits<Env>::isAttempt,
                      "formula: attempt_input reads the determination recorded for the attempt that is running, so "
                      "it is only meaningful inside a retry's attempt or its acceptance; inside "
                      "from_record<Role>(...) the other record's data is read, where no attempt runs -- read it "
                      "outside the scope");

        static constexpr bool value = true;
    };

    /// Fails to compile when `attempt_input` is read in a retry's starting
    /// value, before any attempt has a determination.
    template <typename Env>
    struct RequireAttemptInputAfterStart
    {
        static_assert(!AttemptEnvironmentTraits<Env>::isStarting,
                      "formula: a retry's starting value is evaluated before its first attempt, so there is no "
                      "recorded determination for attempt_input to read in it");

        static constexpr bool value = true;
    };

    /// Both checks of `attempt_input`, staged so that each mistake draws one
    /// message; `value` says whether the node has anything to read. The
    /// refusals sit in their own structs, as `RequireContextReadable`'s do,
    /// so that this one stays a valid class whose `value` is false.
    template <typename Env>
    struct RequireAttemptInputReadable
    {
        using Traits = AttemptEnvironmentTraits<Env>;

        static_assert(RequireAttemptInputInsideRetry<Env>::value);
        static_assert(std::conditional_t<Traits::isAttempt, RequireAttemptInputAfterStart<Env>, std::true_type>::value);

        static constexpr bool value = Traits::isAttempt && !Traits::isStarting;
    };

    /// Fails to compile when a retry allows no attempts, more than the cap,
    /// or one attempt judged from the second. One struct, its refusals in
    /// order, so that one mistake draws one message: zero attempts judged
    /// from the second says "zero attempts" and nothing else.
    template <std::size_t Max, FirstJudged J>
    struct RequireRetryBound
    {
        [[nodiscard]] static consteval bool check() noexcept
        {
            if constexpr (Max == 0)
                static_assert(Max != 0, "formula: a retry allows no attempts at all; MaxAttempts must be at least one");
            else if constexpr (Max > retryAttemptCap)
                static_assert(Max <= retryAttemptCap,
                              "formula: a retry allows at most 64 attempts; the methods this shape exists for repeat a "
                              "step a few times, and a larger count is almost always a typo");
            else if constexpr (J == FirstJudged::AtSecondAttempt && Max == 1)
                static_assert(!(J == FirstJudged::AtSecondAttempt && Max == 1),
                              "formula: a retry judged from its second attempt allows only one attempt, so its "
                              "acceptance could never be judged; allow at least two attempts, or judge from the first");
            return Max != 0 && Max <= retryAttemptCap && !(J == FirstJudged::AtSecondAttempt && Max == 1);
        }

        static constexpr bool value = check();
    };

    /// Fails to compile when a retry's attempt is not one expression.
    template <typename A>
    struct RequireAttemptExpression
    {
        static_assert(Node<A>,
                      "formula: a retry's attempt is one expression that produces one value, such as "
                      "constant<unit::Gram>(c) + previous_attempt<R> / 2; a series or a comparison is not one");

        static constexpr bool value = true;
    };

    /// Fails to compile when a retry's attempt does not compute its result
    /// quantity's dimension. Named so the quantity and the expression print.
    template <typename R, typename A>
    struct RequireAttemptDimension
    {
        static_assert(refused_already<A>() || Describe<R>::dimension == A::dimension,
                      "formula: this retry's attempt expression does not compute the dimension of its result "
                      "quantity; each attempt's value is the next previous_attempt, so the two must agree -- the "
                      "quantity and the expression appear in this diagnostic as the template arguments of "
                      "RequireAttemptDimension");

        static constexpr bool value = true;
    };

    /// Fails to compile when a retry's starting value does not compute its
    /// result quantity's dimension.
    template <typename R, typename E>
    struct RequireStartingValueDimension
    {
        static_assert(refused_already<E>() || Describe<R>::dimension == E::dimension,
                      "formula: this retry's starting value does not compute the dimension of its result quantity; "
                      "it is what previous_attempt reads at the first attempt, so the two must agree -- the "
                      "quantity and the expression appear in this diagnostic as the template arguments of "
                      "RequireStartingValueDimension");

        static constexpr bool value = true;
    };

    /// Fails to compile when a retry's acceptance is not a comparison.
    template <typename P>
    struct RequireAcceptancePredicate
    {
        static_assert(Predicate<P>,
                      "formula: a retry's acceptance is a comparison that holds or does not, such as "
                      "this_attempt<R> <= constant<unit::Gram>(t); an expression that computes a value is not one");

        static constexpr bool value = true;
    };

    /// Whether @p Start states a starting value.
    template <typename Start>
    struct StartTraits
    {
        static constexpr bool states = false;
    };

    template <Node E>
    struct StartTraits<StartingValue<E>>
    {
        static constexpr bool states = true;
        using expression = E;
    };

    /// `RequireStartingValueDimension`, asked of a starting value only once
    /// @p Ask says the checks before it passed; nothing for no starting value.
    template <typename R, typename Start, bool Ask>
    struct RequireStartChecked
    {
        static constexpr bool value = true;
    };

    template <typename R, Node E>
    struct RequireStartChecked<R, StartingValue<E>, true>
    {
        static constexpr bool value = RequireStartingValueDimension<R, E>::value;
    };

    /// Whether @p Start is a starting value or none: the two things a retry's
    /// first part may be.
    template <typename Start>
    inline constexpr bool isStartShape = StartTraits<Start>::states || std::is_same_v<Start, NoStartingValue>;

    /// Fails to compile when a retry's starting value is neither
    /// `starting_from(...)` nor `NoStartingValue` -- reachable only by
    /// building the retry as an aggregate. Named so the type prints.
    template <typename Start>
    struct RequireStartShape
    {
        static_assert(isStartShape<Start>,
                      "formula: a retry starts from starting_from(expression), or from nothing; this starting value "
                      "is neither -- it appears in this diagnostic as the template argument of RequireStartShape");

        static constexpr bool value = true;
    };

    /// Whether the type @p T -- a node, or any type a node is built from --
    /// holds a type @p Probe matches (`Probe::matches<U>`). A walk over types,
    /// not values, so that a mistake is refused where the formula is written
    /// or documented, and not only where it is evaluated.
    ///
    /// Structural: it looks into every type argument of a class template
    /// whose parameters are types, or one to three values followed by types,
    /// or a type followed by values -- every shape a node of this library has
    /// -- and stops at anything else. It knows no node kind; @p Probe does, so
    /// a node added later is walked without an entry here. One whose
    /// parameters take another shape is not looked into, and what it holds is
    /// still refused where it is evaluated.
    template <typename Probe, typename T>
    struct HoldsNodeType: std::bool_constant<Probe::template matches<T>>
    {
    };

    template <typename Probe, template <typename...> class T, typename... Args>
    struct HoldsNodeType<Probe, T<Args...>>:
        std::bool_constant<Probe::template matches<T<Args...>>
                           || (HoldsNodeType<Probe, std::remove_cv_t<Args>>::value || ...)>
    {
    };

    template <typename Probe, template <auto, typename...> class T, auto V, typename... Args>
    struct HoldsNodeType<Probe, T<V, Args...>>:
        std::bool_constant<(HoldsNodeType<Probe, std::remove_cv_t<Args>>::value || ...)>
    {
    };

    template <typename Probe, template <auto, auto, typename...> class T, auto V, auto W, typename... Args>
    struct HoldsNodeType<Probe, T<V, W, Args...>>:
        std::bool_constant<(HoldsNodeType<Probe, std::remove_cv_t<Args>>::value || ...)>
    {
    };

    template <typename Probe, template <auto, auto, auto, typename...> class T, auto V, auto W, auto X, typename... Args>
    struct HoldsNodeType<Probe, T<V, W, X, Args...>>:
        std::bool_constant<(HoldsNodeType<Probe, std::remove_cv_t<Args>>::value || ...)>
    {
    };

    // At least one value: with none, `T<Arg>` would also match `T<Args...>`
    // above, and clang finds the two ambiguous.
    template <typename Probe, template <typename, auto, auto...> class T, typename Arg, auto Value, auto... Values>
    struct HoldsNodeType<Probe, T<Arg, Value, Values...>>: HoldsNodeType<Probe, std::remove_cv_t<Arg>>
    {
    };

    /// Whether @p T is a context node naming a quantity other than @p R,
    /// cv-qualification aside. A probe's test rather than a specialisation of
    /// the walk, so that exactly one specialisation of the walk matches any
    /// type: `PreviousAttemptNode<Q>` is also a `T<Args...>`.
    template <typename R, typename T>
    inline constexpr bool isMisnamedContextNode = false;

    template <typename R, typename Q>
    inline constexpr bool isMisnamedContextNode<R, PreviousAttemptNode<Q>> =
        !std::is_same_v<std::remove_cv_t<Q>, std::remove_cv_t<R>>;

    template <typename R, typename Q>
    inline constexpr bool isMisnamedContextNode<R, ThisAttemptNode<Q>> =
        !std::is_same_v<std::remove_cv_t<Q>, std::remove_cv_t<R>>;

    /// `HoldsNodeType`'s probe for a context node misnamed under @p R.
    template <typename R>
    struct MisnamedContextProbe
    {
        template <typename T>
        static constexpr bool matches = isMisnamedContextNode<R, T>;
    };

    /// Whether @p T -- a retry's attempt or acceptance -- reads
    /// `previous_attempt<Q>` or `this_attempt<Q>` for a `Q` that is not @p R,
    /// so that `render` and `document` never print another quantity's label
    /// for the retry's own value.
    template <typename R, typename T>
    struct NamesAnotherQuantity: HoldsNodeType<MisnamedContextProbe<R>, T>
    {
    };

    /// Whether @p T is an `attempt_input` node.
    template <typename T>
    inline constexpr bool isAttemptInputNode = false;

    template <typename Q>
    inline constexpr bool isAttemptInputNode<AttemptInputNode<Q>> = true;

    /// `HoldsNodeType`'s probe for `attempt_input`.
    struct AttemptInputProbe
    {
        template <typename T>
        static constexpr bool matches = isAttemptInputNode<T>;
    };

    /// Whether @p T reads `attempt_input` anywhere.
    template <typename T>
    inline constexpr bool readsAttemptInput = HoldsNodeType<AttemptInputProbe, std::remove_cv_t<T>>::value;

    /// Fails to compile when a formula documented on its own, outside any
    /// retry, reads `attempt_input`: its page would list a series of no
    /// determinations, and nothing could ever evaluate it. Evaluating one is
    /// refused in the same words (`RequireAttemptInputInsideRetry`). Named so
    /// the formula prints.
    template <typename Formula>
    struct RequireAttemptInputOnlyInRetry
    {
        static_assert(!readsAttemptInput<Formula>,
                      "formula: attempt_input reads the determination recorded for the attempt that is running, so "
                      "it is only meaningful inside a retry's attempt or its acceptance -- the formula appears in "
                      "this diagnostic as the template argument of RequireAttemptInputOnlyInRetry");

        static constexpr bool value = true;
    };

    /// Whether @p T is `attempt_number`, `previous_attempt<Q>` or
    /// `this_attempt<Q>`, for any `Q`.
    template <typename T>
    inline constexpr bool isAttemptContextNode = false;

    template <>
    inline constexpr bool isAttemptContextNode<AttemptNumberNode> = true;

    template <typename Q>
    inline constexpr bool isAttemptContextNode<PreviousAttemptNode<Q>> = true;

    template <typename Q>
    inline constexpr bool isAttemptContextNode<ThisAttemptNode<Q>> = true;

    /// `HoldsNodeType`'s probe for a context node.
    struct AttemptContextProbe
    {
        template <typename T>
        static constexpr bool matches = isAttemptContextNode<T>;
    };

    /// Whether @p T reads `attempt_number`, `previous_attempt` or
    /// `this_attempt` anywhere, whatever quantity it names.
    template <typename T>
    inline constexpr bool readsAttemptContext = HoldsNodeType<AttemptContextProbe, std::remove_cv_t<T>>::value;

    /// Whether @p T is `this_attempt<Q>`, for any `Q`.
    template <typename T>
    inline constexpr bool isThisAttemptNode = false;

    template <typename Q>
    inline constexpr bool isThisAttemptNode<ThisAttemptNode<Q>> = true;

    /// `HoldsNodeType`'s probe for `this_attempt`.
    struct ThisAttemptProbe
    {
        template <typename T>
        static constexpr bool matches = isThisAttemptNode<T>;
    };

    /// Fails to compile when a retry's attempt expression reads
    /// `this_attempt`, where the retry is built -- so that `render` and
    /// `document`, which never evaluate it, do not print a circular
    /// definition -- in the words its evaluation uses
    /// (`RequireThisAttemptInJudgement`).
    template <typename A>
    struct RequireNoThisAttemptInAttempt
    {
        static_assert(!HoldsNodeType<ThisAttemptProbe, std::remove_cv_t<A>>::value,
                      "formula: this_attempt is the value an attempt produced, known only when judging it; read in "
                      "the attempt expression itself it would be circular -- use previous_attempt there; the attempt "
                      "appears in this diagnostic as the template argument of RequireNoThisAttemptInAttempt");

        static constexpr bool value = true;
    };

    /// Fails to compile when a retry's starting value reads a context node,
    /// where the retry is built -- so that `render` and `document`, which
    /// never evaluate it, are refused too, and a misnamed
    /// `previous_attempt<Q>` there is refused as the misplaced read it is --
    /// in the words its evaluation uses (`RequireNotInStartingValue`).
    template <typename Start>
    struct RequireNoContextInStart
    {
        static_assert(!readsAttemptContext<Start>,
                      "formula: a retry's starting value is evaluated before its first attempt, so no attempt exists "
                      "yet; previous_attempt, this_attempt and attempt_number cannot be read in it -- the starting "
                      "value appears in this diagnostic as the template argument of RequireNoContextInStart");

        static constexpr bool value = true;
    };

    /// Fails to compile when a retry's starting value reads `attempt_input`,
    /// where the retry is built, in the words its evaluation uses
    /// (`RequireAttemptInputAfterStart`).
    template <typename Start>
    struct RequireNoAttemptInputInStart
    {
        static_assert(!readsAttemptInput<Start>,
                      "formula: a retry's starting value is evaluated before its first attempt, so there is no "
                      "recorded determination for attempt_input to read in it -- the starting value appears in this "
                      "diagnostic as the template argument of RequireNoAttemptInputInStart");

        static constexpr bool value = true;
    };
    /// Fails to compile when a retry's attempt or acceptance reads
    /// `previous_attempt<Q>` or `this_attempt<Q>` for a `Q` that is not its
    /// result quantity, where the retry is built -- so that `render` and
    /// `document`, which never evaluate it, are refused too. Named so the
    /// quantity and the part that names another print.
    template <typename R, typename Part>
    struct RequireOnlyRetriedQuantity
    {
        static_assert(!NamesAnotherQuantity<R, std::remove_cv_t<Part>>::value,
                      "formula: previous_attempt and this_attempt name the retry's own result quantity; this one "
                      "names another -- the retry's result quantity and the attempt or acceptance that names "
                      "another appear in this diagnostic as the template arguments of RequireOnlyRetriedQuantity");

        static constexpr bool value = true;
    };

    /// Every check of a retry, staged so that each mistake draws one message.
    template <typename R, std::size_t Max, FirstJudged J, typename Start, typename A, typename P>
    struct RequireRetryValid
    {
        static constexpr bool boundsOk = RequireRetryBound<Max, J>::value;
        static_assert(std::conditional_t<boundsOk, RequireStartShape<Start>, std::true_type>::value);
        static constexpr bool boundOk = boundsOk && isStartShape<Start>;

        static constexpr bool attemptIsNode = Node<A>;
        static_assert(std::conditional_t<boundOk, RequireAttemptExpression<A>, std::true_type>::value);

        [[nodiscard]] static consteval bool attempt_measures_result() noexcept
        {
            if constexpr (boundOk && attemptIsNode)
                return refused_already<A>() || Describe<R>::dimension == A::dimension;
            else
                return false;
        }

        static constexpr bool attemptOk = attempt_measures_result();
        static_assert(std::conditional_t<boundOk && attemptIsNode, RequireAttemptDimension<R, A>, std::true_type>::value);

        [[nodiscard]] static consteval bool start_measures_result() noexcept
        {
            if constexpr (StartTraits<Start>::states)
                return refused_already<typename StartTraits<Start>::expression>()
                       || Describe<R>::dimension == StartTraits<Start>::expression::dimension;
            else
                return true;
        }

        static constexpr bool startOk = start_measures_result();

        static_assert(RequireStartChecked<R, Start, attemptOk>::value);

        static constexpr bool acceptOk = Predicate<P>;
        static_assert(std::conditional_t<attemptOk && startOk, RequireAcceptancePredicate<P>, std::true_type>::value);

        // Last: the shape of every part is known to be right, so the walk
        // only asks which quantities the context nodes name -- and is not
        // taken at all over a part already refused.
        static constexpr bool shapesOk = attemptOk && startOk && acceptOk;

        template <typename Part, bool Ask>
        [[nodiscard]] static consteval bool names_only_result() noexcept
        {
            if constexpr (Ask)
                return !NamesAnotherQuantity<R, std::remove_cv_t<Part>>::value;
            else
                return true;
        }

        static constexpr bool attemptNamesOk = names_only_result<A, shapesOk>();
        static_assert(std::conditional_t<shapesOk, RequireOnlyRetriedQuantity<R, A>, std::true_type>::value);
        static constexpr bool acceptNamesOk = names_only_result<P, shapesOk && attemptNamesOk>();
        static_assert(
            std::conditional_t<shapesOk && attemptNamesOk, RequireOnlyRetriedQuantity<R, P>, std::true_type>::value);

        // And a starting value that reads an attempt's context or a recorded
        // determination, which it runs before any attempt has either.
        static constexpr bool quantitiesOk = shapesOk && attemptNamesOk && acceptNamesOk;

        // An attempt that reads the value it is producing.
        template <bool Ask>
        [[nodiscard]] static consteval bool attempt_reads_no_own_value() noexcept
        {
            if constexpr (Ask)
                return !HoldsNodeType<ThisAttemptProbe, std::remove_cv_t<A>>::value;
            else
                return true;
        }

        static constexpr bool attemptSelfOk = attempt_reads_no_own_value<quantitiesOk>();
        static_assert(std::conditional_t<quantitiesOk, RequireNoThisAttemptInAttempt<A>, std::true_type>::value);

        static constexpr bool namesOk = quantitiesOk && attemptSelfOk;

        template <bool Ask>
        [[nodiscard]] static consteval bool start_reads_no_context() noexcept
        {
            if constexpr (Ask)
                return !readsAttemptContext<Start>;
            else
                return true;
        }

        static constexpr bool startContextOk = start_reads_no_context<namesOk>();
        static_assert(std::conditional_t<namesOk, RequireNoContextInStart<Start>, std::true_type>::value);

        template <bool Ask>
        [[nodiscard]] static consteval bool start_reads_no_input() noexcept
        {
            if constexpr (Ask)
                return !readsAttemptInput<Start>;
            else
                return true;
        }

        static constexpr bool startInputOk = start_reads_no_input<namesOk && startContextOk>();
        static_assert(
            std::conditional_t<namesOk && startContextOk, RequireNoAttemptInputInStart<Start>, std::true_type>::value);

        /// Whether every check passed.
        static constexpr bool value = namesOk && startContextOk && startInputOk;
    };

    struct RetryOutcomeFactory;

    /// Whether @p decision holds anything but blanks: a retry that runs out of
    /// attempts ends in its words, and a blank one would end in no decision
    /// at all. Blank is every character `std::isspace` counts in the "C"
    /// locale -- space, `\t`, `\n`, `\v`, `\f` and `\r` -- and the no-break
    /// space U+00A0, spelt in UTF-8 as the two bytes C2 A0. Spelt out,
    /// because `std::isspace` is neither `constexpr` nor locale-free.
    [[nodiscard]] constexpr bool verdict_says_something(Verdict const& decision) noexcept
    {
        std::string_view const verdictWords = decision.label;
        for (std::size_t at = 0; at < verdictWords.size(); ++at)
        {
            char const spelt = verdictWords[at];
            if (spelt == ' ' || spelt == '\t' || spelt == '\n' || spelt == '\v' || spelt == '\f' || spelt == '\r')
                continue;
            if (spelt == '\xC2' && at + 1 < verdictWords.size() && verdictWords[at + 1] == '\xA0')
            {
                ++at;
                continue;
            }
            return true;
        }
        return false;
    }

    /// Deliberately NOT `constexpr`, and deliberately harmless at run time.
    /// Called while a retry is built in a constant expression, it makes that
    /// expression non-constant, so a blank verdict is a compile error whose
    /// diagnostic names this function -- which is why the name is a sentence,
    /// as `formula_exponent_denominator_must_not_be_zero`'s is. At run time it
    /// does nothing: `checked_evaluate_retry` then refuses the retry instead.
    inline void formula_retry_verdict_must_say_something() noexcept {}

    /// Refuses a blank verdict where the retry is built, when that is a
    /// constant expression.
    constexpr void require_verdict(Verdict const& decision) noexcept
    {
        if consteval
        {
            if (!verdict_says_something(decision))
                formula_retry_verdict_must_say_something();
        }
    }
} // namespace detail

/// A retry of @p A, at most @p Max times, for result quantity @p R, judged by
/// @p P from the attempt @p J says; @p Start is `StartingValue<E>` or
/// `NoStartingValue`. Built by `retry<R, Max, J>(...)`, and checked in its
/// class body, so one built as an aggregate is checked too. Not a `Node`.
///
/// No `{}` initialiser on the start, the attempt or the acceptance,
/// deliberately (defect class 4): see `Corrections` (`lookup.hpp`).
template <Described R, std::size_t Max, FirstJudged J, typename Start, typename A, typename P>
struct Retry
{
    // `sizeof` makes the checks a complete type, which fires them in the
    // class body -- see `OpaqueCall` (`opaque.hpp`) for why a static data
    // member's initialiser alone is not enough.
    static_assert(sizeof(detail::RequireRetryValid<R, Max, J, Start, A, P>) > 0);

    /// The retry's result quantity.
    using quantity = R;
    /// The most attempts it runs.
    static constexpr std::size_t maxAttempts = Max;
    /// From which attempt on it is judged.
    static constexpr FirstJudged firstJudged = J;
    /// Whether it was refused; then it is never evaluated.
    static constexpr detail::RefusedFlag refused = !detail::RequireRetryValid<R, Max, J, Start, A, P>::value;

    /// The starting value, or `NoStartingValue`.
    Start start;
    /// The attempt expression, evaluated once per attempt.
    A attempt;
    /// The acceptance, judged after each attempt from `J` on.
    P accept;
    /// What the retry ends in when no attempt is accepted.
    Verdict onExhausted;
    /// Why the method repeats here.
    Citation citation;
};

/// A retry of @p attemptExpression starting from @p start: at most @p Max
/// attempts for @p R, each judged by @p accept from the attempt @p J says;
/// @p onExhausted when none is accepted.
///
/// The verdict must say something (`detail::verdict_says_something`): a
/// retry built in a constant expression with a blank one fails to compile,
/// naming `formula_retry_verdict_must_say_something`, and one built at run
/// time fails to evaluate (`checked_evaluate_retry`), with
/// `RetryFailure { DomainError, RetryFailure::refusedBeforeStart }` -- even
/// when the environment holds an entered result.
template <Described R, std::size_t Max, FirstJudged J, Node E, typename A, typename P>
[[nodiscard]] constexpr auto retry(
    StartingValue<E> start, A attemptExpression, P accept, Verdict onExhausted, Citation citation) noexcept
{
    detail::require_verdict(onExhausted);
    return Retry<R, Max, J, StartingValue<E>, A, P> { start, attemptExpression, accept, onExhausted, citation };
}

/// A retry with no starting value: `previous_attempt` at the first attempt
/// is then the author's mistake, and fails with `DomainError`. The verdict
/// must say something, as above.
template <Described R, std::size_t Max, FirstJudged J, typename A, typename P>
[[nodiscard]] constexpr auto retry(A attemptExpression, P accept, Verdict onExhausted, Citation citation) noexcept
{
    detail::require_verdict(onExhausted);
    return Retry<R, Max, J, NoStartingValue, A, P> { NoStartingValue {}, attemptExpression, accept, onExhausted, citation };
}

/// Why a retry failed: the arithmetic error, and the attempt it arose at --
/// a **zero-based position**, as `SeriesFailure::element` is, so attempt 2 is
/// `1`. Every text the library writes says attempts one-based.
struct RetryFailure
{
    /// The `attempt` of a retry refused before anything of it ran: built at
    /// run time with a blank verdict (`checked_evaluate_retry`). No attempt
    /// has this position, so it is never a starting value's failure or an
    /// attempt's.
    static constexpr std::size_t refusedBeforeStart = static_cast<std::size_t>(-1);

    /// The `attempt` of a retry whose starting value failed, before its
    /// first attempt ran. No attempt has this position, so a starting
    /// value's failure is never taken for the first attempt's, which is at 0.
    static constexpr std::size_t atStartingValue = static_cast<std::size_t>(-2);

    /// What went wrong.
    ArithmeticError error;
    /// At which attempt, from 0; `atStartingValue` when the starting value
    /// failed, and `refusedBeforeStart` when nothing ran.
    std::size_t attempt;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(RetryFailure const&) const noexcept = default;
};

/// How one attempt was judged, as a trace records it.
enum class AttemptJudgement : std::uint8_t
{
    /// Not judged: the first attempt of a retry judged from the second, or
    /// an attempt whose value failed.
    NotJudged,
    /// The acceptance held: the retry ends here with this attempt's value.
    Accepted,
    /// The acceptance did not hold: the next attempt runs, if one is allowed.
    Rejected,
    /// The attempt's value, or its judgement, was absent: the retry ends here.
    NotJudgeable,
    /// The judgement failed arithmetically: the retry ends here, failed. The
    /// error is the failing side's, which the attempt step's last operand
    /// holds; the attempt step itself keeps only its value.
    JudgementFailed,
    /// The attempt, or its judgement, read a recorded determination that is
    /// absent (`attempt_input`): the retry ends here, `NotRecorded`.
    NotRecorded,
};

template <Described R>
class RetryOutcome;

/// What a sink that hears retries is told of one: plain data, so that a
/// sink need know nothing of the retry's types.
struct RetryInfo
{
    /// The most attempts it runs.
    std::size_t attemptLimit;
    /// From which attempt on it is judged.
    FirstJudged firstJudged;
    /// Why the method repeats here.
    Citation citation;
    /// The verdict it ends in when no attempt is accepted: author text.
    std::string_view verdictLabel;
};

/// What a sink that hears attempts is told of one.
struct AttemptInfo
{
    /// The attempt's number, from 1.
    std::size_t attemptNumber;
    /// The comparison the acceptance makes.
    Comparison comparison;
};

namespace detail
{
    /// Whether @p Sink hears a retry: `retry_entered(info)` before the
    /// starting value is evaluated, and `retry_produced(info, ended)` after
    /// the last attempt, with what `checked_evaluate_retry` returns. Asked
    /// for together, as a series' hooks are (`sink.hpp`).
    template <typename Sink, typename R>
    concept HearsRetry =
        requires(Sink sink, RetryInfo const& info, std::expected<RetryOutcome<R>, RetryFailure> const& ended) {
            sink.retry_entered(info);
            sink.retry_produced(info, ended);
        };

    /// Whether @p Sink hears each attempt: `attempt_entered(info)` before it
    /// runs, and `attempt_produced(info, produced, judgement)` once it has
    /// been judged -- or not.
    template <typename Sink, typename Rep>
    concept HearsAttempts =
        requires(Sink sink, AttemptInfo const& info, Evaluated<Rep> const& produced, AttemptJudgement judgement) {
            sink.attempt_entered(info);
            sink.attempt_produced(info, produced, judgement);
        };
} // namespace detail
/// How a retry ended, and in what. Built only by `checked_evaluate_retry`: no
/// public constructor and no setters, so how it ended and where it was
/// accepted are the library's to state (defect class 3).
template <Described R>
class RetryOutcome
{
  public:
    /// The outcome: the accepted value, the verdict, empty, or the entered
    /// value -- as `end()` says.
    [[nodiscard]] constexpr Outcome<R> outcome() const noexcept
    {
        return _outcome;
    }

    /// How the retry ended.
    [[nodiscard]] constexpr RetryEnd end() const noexcept
    {
        return _end;
    }

    /// How many attempts ran: a count, 0 when the result was entered.
    [[nodiscard]] constexpr std::size_t attempts_made() const noexcept
    {
        return _attemptsMade;
    }

    /// Where it was accepted: a **zero-based position**, so the fourth
    /// attempt is `3`; empty unless it was accepted.
    [[nodiscard]] constexpr std::optional<std::size_t> accepted_at() const noexcept
    {
        return _acceptedAt;
    }

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(RetryOutcome const&) const noexcept = default;

  private:
    friend struct detail::RetryOutcomeFactory;

    constexpr RetryOutcome(Outcome<R> made,
                           RetryEnd ended,
                           std::size_t attemptsMade,
                           std::optional<std::size_t> acceptedAt) noexcept:
        _outcome { made },
        _end { ended },
        _attemptsMade { attemptsMade },
        _acceptedAt { acceptedAt }
    {
    }

    Outcome<R> _outcome;
    RetryEnd _end;
    std::size_t _attemptsMade;
    std::optional<std::size_t> _acceptedAt;
};

/// The number the outcome a retry ended with holds -- see `number_of(Outcome)`.
template <Described R>
[[nodiscard]] constexpr std::optional<Rational> number_of(RetryOutcome<R> const& ended) noexcept
{
    return number_of(ended.outcome());
}

namespace detail
{
    /// The one place a `RetryOutcome` is made.
    struct RetryOutcomeFactory
    {
        template <Described R>
        [[nodiscard]] static constexpr RetryOutcome<R> make(Outcome<R> made,
                                                            RetryEnd ended,
                                                            std::size_t attemptsMade,
                                                            std::optional<std::size_t> acceptedAt) noexcept
        {
            return RetryOutcome<R> { made, ended, attemptsMade, acceptedAt };
        }
    };

    /// Fails to compile when a retry is evaluated in anything but `Rational`:
    /// its result is an `Outcome`, which holds an exact value.
    template <typename Rep>
    struct RequireExactRetry
    {
        static_assert(std::is_same_v<Rep, Rational>,
                      "formula: a retry can only be evaluated with Rep = Rational -- its result is an Outcome, "
                      "which holds an exact value, and its acceptance compares");

        static constexpr bool value = true;
    };
} // namespace detail

namespace detail
{
    /// Every check of a context node read in @p Env, staged so that one
    /// mistake draws one message: outside any retry; in a starting value; a
    /// quantity that is not the retry's (@p Q, `void` for `attempt_number`);
    /// and, for `this_attempt` (@p ReadsThisAttempt), anywhere but the
    /// acceptance. `value` says whether the node has anything to read.
    template <typename Env, typename Q, bool ReadsThisAttempt>
    struct RequireContextReadable
    {
        using Traits = AttemptEnvironmentTraits<Env>;

        static_assert(RequireInsideRetry<Env>::value);
        static constexpr bool attemptStarted = Traits::isAttempt && !Traits::isStarting;
        static_assert(std::conditional_t<Traits::isAttempt, RequireNotInStartingValue<Env>, std::true_type>::value);

        // `Estimate const` and `Estimate` are one quantity, one dimension and
        // one label: the retry's own, however either is spelt.
        static constexpr bool quantityOk =
            attemptStarted
            && (std::is_void_v<Q> || std::is_same_v<std::remove_cv_t<Q>, std::remove_cv_t<typename Traits::retried>>);
        static_assert(
            std::conditional_t<attemptStarted && !std::is_void_v<Q>, RequireRetriedQuantity<Q, Env>, std::true_type>::value);
        static_assert(
            std::conditional_t<ReadsThisAttempt && quantityOk, RequireThisAttemptInJudgement<Env>, std::true_type>::value);

        /// Whether the node reads anything: every check above passed.
        static constexpr bool value = quantityOk && (!ReadsThisAttempt || Traits::isJudging);
    };
} // namespace detail

/// The attempt number, from 1, inside a retry.
template <typename Rep = Rational, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(AttemptNumberNode const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    constexpr bool readable = detail::RequireContextReadable<Env, void, false>::value;
    sink.entered(node);
    Evaluated<Rep> const numbered = [&]() -> Evaluated<Rep> {
        if constexpr (readable)
        {
            std::expected<Rep, ArithmeticError> const counted =
                RepTraits<Rep>::from(Rational { static_cast<std::int64_t>(environment.attempt()) });
            if (!counted.has_value())
                return std::unexpected { counted.error() };
            return detail::present<Rep>(*counted);
        }
        else
        {
            (void) environment;
            return detail::nothing<Rep>();
        }
    }();
    sink.produced(node, numbered);
    return numbered;
}

/// The previous attempt's value, or the starting value, inside a retry; the
/// author's `DomainError` at the first attempt of a retry with no starting
/// value -- never absence, which would read as "not measured". Refused for a
/// quantity that is not the retry's own (`detail::RequireRetriedQuantity`).
template <typename Rep = Rational, Described R, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(PreviousAttemptNode<R> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    constexpr bool readable = detail::RequireContextReadable<Env, R, false>::value;
    sink.entered(node);
    Evaluated<Rep> const before = [&]() -> Evaluated<Rep> {
        if constexpr (readable)
            return environment.previous();
        else
        {
            (void) environment;
            return detail::nothing<Rep>();
        }
    }();
    sink.produced(node, before);
    return before;
}

/// The value this attempt produced, in a retry's acceptance. Refused for a
/// quantity that is not the retry's own, as `previous_attempt` is.
template <typename Rep = Rational, Described R, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(ThisAttemptNode<R> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    constexpr bool readable = detail::RequireContextReadable<Env, R, true>::value;
    sink.entered(node);
    Evaluated<Rep> const produced = [&]() -> Evaluated<Rep> {
        if constexpr (readable)
            return detail::present<Rep>(environment.current());
        else
        {
            (void) environment;
            return detail::nothing<Rep>();
        }
    }();
    sink.produced(node, produced);
    return produced;
}

namespace detail
{
    /// What `attempt_input<Q>` reads in @p environment, an attempt's: see its
    /// evaluator below, which calls this only once the node's checks passed.
    template <typename Rep, Described Q, typename Env>
    [[nodiscard]] constexpr Evaluated<Rep> read_attempt_input(Env const& environment) noexcept
    {
        constexpr std::size_t attemptLimit = AttemptEnvironmentTraits<Env>::attemptLimit;
        MeasuredSeries<Q, attemptLimit> const recorded = environment.template get_series<Q, attemptLimit>();
        Measured<Q> const recordedDetermination = recorded.element(environment.attempt() - 1);
        if (recordedDetermination.is_absent())
        {
            environment.mark_not_recorded();
            return nothing<Rep>();
        }
        return in_si<Rep>(*recordedDetermination.stored(), Describe<Q>::unit);
    }
} // namespace detail

/// The determination of @p Q recorded for the attempt that is running:
/// element `attempt() - 1` of the environment's series of @p Q, whose length
/// is the retry's attempt limit, in the coherent unit. An absent element
/// is absent here, and marked on the attempt's environment, so that the
/// retry ends `NotRecorded` rather than `NotJudgeable`. A series of another
/// length draws the environment's own message, naming both lengths, once.
template <typename Rep = Rational, Described Q, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(AttemptInputNode<Q> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    constexpr bool readable = detail::RequireAttemptInputReadable<Env>::value;
    sink.entered(node);
    Evaluated<Rep> const read = [&]() -> Evaluated<Rep> {
        if constexpr (readable)
            return detail::read_attempt_input<Rep, Q>(environment);
        else
        {
            (void) environment;
            return detail::nothing<Rep>();
        }
    }();
    // Whether the series the determination came from was measured or typed
    // in, as a series variable reports it (`report_series_input_source`):
    // through `series_input_source`, when the sink asks and the environment
    // can answer. A person's entry reads as one in the trace, an empty one
    // included.
    if constexpr (
        readable && requires { sink.series_input_source(node, ValueSource::Measured); }
        && requires { Env::template is_entered_series<Q>; })
        sink.series_input_source(node,
                                 Env::template is_entered_series<Q> ? ValueSource::ManuallyEntered : ValueSource::Measured);
    sink.produced(node, read);
    return read;
}

namespace detail
{
    /// The attempts themselves, for `checked_evaluate_retry`, which tells a
    /// sink that hears retries (`HearsRetry`) before and after. A sink that
    /// hears attempts (`HearsAttempts`) is told of each attempt that runs --
    /// and of none that does not.
    template <typename Rep,
              typename R,
              std::size_t Max,
              FirstJudged J,
              typename Start,
              typename A,
              typename P,
              typename Env,
              typename Sink>
    [[nodiscard]] constexpr std::expected<RetryOutcome<R>, RetryFailure> run_retry(
        Retry<R, Max, J, Start, A, P> const& retrying, Env const& environment, Sink& sink) noexcept
    {
        using Factory = RetryOutcomeFactory;

        // No starting value: reading one at the first attempt is the
        // author's mistake, and says so.
        Evaluated<Rep> before = std::unexpected { ArithmeticError::DomainError };
        if constexpr (StartTraits<Start>::states)
        {
            AttemptEnvironment<Env, Rep, R, AttemptPhase::Starting, Max> const starting { environment };
            before = dispatch<Rep>(retrying.start.expression, starting, sink);
            if (!before.has_value())
                return std::unexpected { RetryFailure { before.error(), RetryFailure::atStartingValue } };
        }

        // The one loop: at most Max attempts, and no other bound.
        for (std::size_t attemptAt = 1; attemptAt <= Max; ++attemptAt)
        {
            AttemptInfo const attemptInfo { .attemptNumber = attemptAt, .comparison = P::comparison };
            if constexpr (HearsAttempts<Sink, Rep>)
                sink.attempt_entered(attemptInfo);
            // Tells the sink how the attempt was judged, once, whichever way
            // the attempt ends.
            auto const told = [&](Evaluated<Rep> const& produced, AttemptJudgement judged) {
                if constexpr (HearsAttempts<Sink, Rep>)
                    sink.attempt_produced(attemptInfo, produced, judged);
                else
                {
                    (void) produced;
                    (void) judged;
                }
            };

            // Set by `attempt_input` when the determination it reads is
            // absent, in the attempt or in its judgement.
            bool unrecorded = false;
            AttemptEnvironment<Env, Rep, R, AttemptPhase::Attempting, Max> const attempting {
                environment, attemptAt, before, &unrecorded
            };
            Evaluated<Rep> const produced = dispatch<Rep>(retrying.attempt, attempting, sink);
            if (!produced.has_value())
            {
                told(produced, AttemptJudgement::NotJudged);
                return std::unexpected { RetryFailure { produced.error(), attemptAt - 1 } };
            }
            // Before "not judgeable": the determination is missing, not the
            // comparison -- whatever the attempt made of its absence.
            if (unrecorded)
            {
                told(produced, AttemptJudgement::NotRecorded);
                return Factory::make<R>(Outcome<R>::empty(), RetryEnd::NotRecorded, attemptAt, std::nullopt);
            }
            if (!produced->has_value())
            {
                told(produced, AttemptJudgement::NotJudgeable);
                return Factory::make<R>(Outcome<R>::empty(), RetryEnd::NotJudgeable, attemptAt, std::nullopt);
            }

            // Judged on the value the attempt produced, never on an
            // intermediate; the first attempt of a retry judged from the
            // second is not judged, which is not a rejection.
            if (J == FirstJudged::AtSecondAttempt && attemptAt == 1)
                told(produced, AttemptJudgement::NotJudged);
            else
            {
                AttemptEnvironment<Env, Rep, R, AttemptPhase::Judging, Max> const judging {
                    environment, attemptAt, before, **produced, &unrecorded
                };
                std::expected<std::optional<bool>, ArithmeticError> const held =
                    checked_evaluate_predicate<Rep>(retrying.accept, judging, sink);
                if (!held.has_value())
                {
                    told(produced, AttemptJudgement::JudgementFailed);
                    return std::unexpected { RetryFailure { held.error(), attemptAt - 1 } };
                }
                if (unrecorded)
                {
                    told(produced, AttemptJudgement::NotRecorded);
                    return Factory::make<R>(Outcome<R>::empty(), RetryEnd::NotRecorded, attemptAt, std::nullopt);
                }
                if (!held->has_value())
                {
                    told(produced, AttemptJudgement::NotJudgeable);
                    return Factory::make<R>(Outcome<R>::empty(), RetryEnd::NotJudgeable, attemptAt, std::nullopt);
                }
                told(produced, **held ? AttemptJudgement::Accepted : AttemptJudgement::Rejected);
                if (**held)
                {
                    std::expected<Rational, ArithmeticError> const inDeclaredUnit =
                        checked_convert(**produced, coherent(A::dimension), Describe<R>::unit);
                    if (!inDeclaredUnit.has_value())
                        return std::unexpected { RetryFailure { inDeclaredUnit.error(), attemptAt - 1 } };
                    return Factory::make<R>(Outcome<R>::value(Measured<R> { *inDeclaredUnit }, ValueSource::Derived),
                                            RetryEnd::Accepted,
                                            attemptAt,
                                            attemptAt - 1);
                }
            }
            before = produced;
        }
        return Factory::make<R>(Outcome<R>::verdict(retrying.onExhausted), RetryEnd::Exhausted, Max, std::nullopt);
    }
} // namespace detail
/// Runs @p retrying over @p environment: the starting value once, then
/// attempt after attempt -- each judged from the attempt `J` says -- until
/// one is accepted or the last allowed has run. See `RetryEnd` for the six
/// ways it ends.
///
/// When the environment holds an `entered` value for `R`, that value is
/// returned, `ManuallyEntered`, and no attempt runs: what a person typed in
/// is never replaced by a computation, as in `checked_evaluate`.
///
/// A retry built at run time with a blank verdict is refused first, before
/// the entered value is looked at: `RetryFailure { DomainError,
/// RetryFailure::refusedBeforeStart }`, a position no attempt has, so that it
/// is never read as a starting value that failed. Nothing is traced for it.
template <typename Rep = Rational,
          Described R,
          std::size_t Max,
          FirstJudged J,
          typename Start,
          typename A,
          typename P,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<RetryOutcome<R>, RetryFailure> checked_evaluate_retry(
    Retry<R, Max, J, Start, A, P> const& retrying, Env const& environment, Sink sink = {}) noexcept
{
    static_assert(detail::RequireExactRetry<Rep>::value);
    using Factory = detail::RetryOutcomeFactory;

    if constexpr (Retry<R, Max, J, Start, A, P>::refused || !std::is_same_v<Rep, Rational>)
        return std::unexpected { RetryFailure { ArithmeticError::DomainError, 0 } };
    // A retry built at run time with a blank verdict: `retry()` could not
    // refuse it where it was built, so it is refused here, before any
    // attempt, rather than ending exhausted in no decision -- at
    // `refusedBeforeStart`, so that it is never taken for a starting value
    // that failed. Before an entered result, too: the retry as written is
    // refused, whatever the environment holds.
    else if (!detail::verdict_says_something(retrying.onExhausted))
        return std::unexpected { RetryFailure { ArithmeticError::DomainError, RetryFailure::refusedBeforeStart } };
    else if constexpr (Env::template is_entered<R>)
        return Factory::make<R>(Outcome<R>::value(environment.template get<R>(), ValueSource::ManuallyEntered),
                                RetryEnd::ManuallyEntered,
                                0,
                                std::nullopt);
    else
    {
        RetryInfo const retryInfo {
            .attemptLimit = Max, .firstJudged = J, .citation = retrying.citation, .verdictLabel = retrying.onExhausted.label
        };
        if constexpr (detail::HearsRetry<Sink, R>)
            sink.retry_entered(retryInfo);
        std::expected<RetryOutcome<R>, RetryFailure> const ended =
            detail::run_retry<Rep, R, Max, J, Start, A, P>(retrying, environment, sink);
        if constexpr (detail::HearsRetry<Sink, R>)
            sink.retry_produced(retryInfo, ended);
        return ended;
    }
}
namespace detail
{
    /// Whether @p T is a retry.
    template <typename T>
    inline constexpr bool isRetry = false;

    template <Described R, std::size_t Max, FirstJudged J, typename Start, typename A, typename P>
    inline constexpr bool isRetry<Retry<R, Max, J, Start, A, P>> = true;

    /// The dimension of whichever of @p L and @p Rt is a retry: its result's.
    template <typename L, typename Rt>
    [[nodiscard]] consteval Dimension retried_dimension() noexcept
    {
        if constexpr (isRetry<L>)
            return Describe<typename L::quantity>::dimension;
        else
            return Describe<typename Rt::quantity>::dimension;
    }

    /// Fails to compile when a retry is handed where a formula belongs: to
    /// `checked_evaluate`, `evaluate`, `variant<Tag>`, arithmetic or a
    /// comparison. Named so
    /// the retry prints.
    template <typename Misplaced>
    struct RequireRetryAtTop
    {
        static_assert(sizeof(Misplaced) == 0,
                      "formula: a retry is evaluated at the top, by checked_evaluate_retry; it cannot stand in a "
                      "formula or be a method's variant -- running out of attempts ends in a verdict, which no "
                      "value can carry");

        static constexpr bool value = true;
    };

    /// A bound retry handed to a verb that answers with one value
    /// (`yields.hpp`): refused as `checked_evaluate` refuses a retry.
    template <Described R, std::size_t Max, FirstJudged J, typename Start, typename A, typename P>
    struct RequireSingleValueBound<Retry<R, Max, J, Start, A, P>>: RequireRetryAtTop<Retry<R, Max, J, Start, A, P>>
    {
    };

    /// What arithmetic over a retry gives, once refused: a node of the
    /// retry's result's dimension that is refused already
    /// (`refused_already`), so nothing over it asks again, and that is never
    /// evaluated but to a `DomainError`.
    template <Dimension D>
    struct RefusedRetryValue: NodeBase
    {
        static constexpr Dimension dimension = D;
        static constexpr detail::RefusedFlag refused = true;
    };

    /// The refused value arithmetic over @p L and @p Rt gives, one of them a
    /// retry.
    template <typename L, typename Rt>
    [[nodiscard]] constexpr auto refused_retry_value() noexcept
    {
        return RefusedRetryValue<retried_dimension<L, Rt>()> {};
    }

    /// The type an arithmetic operator over @p L and @p Rt returns when one
    /// of them is a retry; none otherwise. The operators name it, so that
    /// asking whether a retry can be added, as a concept does, is answered
    /// without instantiating their bodies -- the refusal. A class, so that
    /// over two operands neither of which is a retry it has no `type`, a
    /// substitution failure: clang substitutes into the return type before
    /// it checks the operators' constraints.
    template <typename L, typename Rt, bool = isRetry<L> || isRetry<Rt>>
    struct RefusedRetryResult
    {
    };

    template <typename L, typename Rt>
    struct RefusedRetryResult<L, Rt, true>
    {
        using type = decltype(refused_retry_value<L, Rt>());
    };
} // namespace detail

/// A refused retry value evaluates to nothing but `DomainError`; a program
/// holding one never compiles, so this is never seen.
template <typename Rep = Rational, Dimension D, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(detail::RefusedRetryValue<D> const&,
                                                           Env const&,
                                                           Sink = {}) noexcept
{
    return Evaluated<Rep> { std::unexpected { ArithmeticError::DomainError } };
}

/// A retry handed to `checked_evaluate`: refused in this library's words,
/// pointing at `checked_evaluate_retry`.
template <Described Result,
          Described R,
          std::size_t Max,
          FirstJudged J,
          typename Start,
          typename A,
          typename P,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<Outcome<Result>, ArithmeticError> checked_evaluate(
    Retry<R, Max, J, Start, A, P> const&, Env const&, Sink = {}) noexcept
{
    static_assert(detail::RequireRetryAtTop<Retry<R, Max, J, Start, A, P>>::value);
    return Outcome<Result>::empty();
}

/// A retry handed to `evaluate`: refused as `checked_evaluate` refuses it.
template <Described Result,
          Described R,
          std::size_t Max,
          FirstJudged J,
          typename Start,
          typename A,
          typename P,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Outcome<Result> evaluate(Retry<R, Max, J, Start, A, P> const&, Env const&, Sink = {}) noexcept
{
    static_assert(detail::RequireRetryAtTop<Retry<R, Max, J, Start, A, P>>::value);
    return Outcome<Result>::empty();
}

/// A retry handed to `variant<Tag>`: refused, and a placeholder constant of
/// its result's dimension returned, so that `variants(...)` and `method(...)`
/// around the call find nothing further to refuse -- as a series handed there
/// is (`method.hpp`).
template <typename Tag, Described R, std::size_t Max, FirstJudged J, typename Start, typename A, typename P>
[[nodiscard]] constexpr VariantCase<Tag, ConstantNode<coherent(Describe<R>::dimension)>> variant(
    Retry<R, Max, J, Start, A, P>) noexcept
{
    static_assert(detail::RequireRetryAtTop<Retry<R, Max, J, Start, A, P>>::value);
    return VariantCase<Tag, ConstantNode<coherent(Describe<R>::dimension)>> {
        ConstantNode<coherent(Describe<R>::dimension)> {}
    };
}

/// A retry in arithmetic, on either side of `+`, `-`, `*` or `/`, or negated:
/// refused in this library's words, giving a node refused already.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator+(L, Rt) noexcept -> typename detail::RefusedRetryResult<L, Rt>::type
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::refused_retry_value<L, Rt>();
}

/// See `operator+` over a retry.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator-(L, Rt) noexcept -> typename detail::RefusedRetryResult<L, Rt>::type
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::refused_retry_value<L, Rt>();
}

/// See `operator+` over a retry.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator*(L, Rt) noexcept -> typename detail::RefusedRetryResult<L, Rt>::type
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::refused_retry_value<L, Rt>();
}

/// See `operator+` over a retry.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator/(L, Rt) noexcept -> typename detail::RefusedRetryResult<L, Rt>::type
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::refused_retry_value<L, Rt>();
}

/// See `operator+` over a retry.
template <typename Operand>
    requires(detail::isRetry<Operand>)
[[nodiscard]] constexpr auto operator-(Operand) noexcept -> typename detail::RefusedRetryResult<Operand, Operand>::type
{
    static_assert(detail::RequireRetryAtTop<Operand>::value);
    return detail::refused_retry_value<Operand, Operand>();
}

namespace detail
{
    /// What comparing a retry gives, once refused: a comparison of two
    /// refused values of the retry's result's dimension, so that nothing it
    /// is used in -- an acceptance, a constraint -- asks again.
    ///
    /// A function, not an alias template: g++ 13.3 crashes (a segmentation
    /// fault in `coerce_template_parms`) substituting a `Dimension` into such
    /// an alias from these operators.
    template <Comparison Op, typename L, typename Rt>
    [[nodiscard]] constexpr auto refused_retry_comparison() noexcept
    {
        constexpr Dimension retriedDimension = retried_dimension<L, Rt>();
        return PredicateNode<Op, RefusedRetryValue<retriedDimension>, RefusedRetryValue<retriedDimension>> { {}, {} };
    }

    /// The type a comparison operator over @p L and @p Rt returns when one of
    /// them is a retry; none otherwise. The operators name it, so that asking
    /// whether a retry can be compared -- as `std::equality_comparable`
    /// does -- answers no, and is not the refusal; a class for the reason
    /// `RefusedRetryResult` is one.
    template <Comparison Op, typename L, typename Rt, bool = isRetry<L> || isRetry<Rt>>
    struct RefusedRetryComparison
    {
    };

    template <Comparison Op, typename L, typename Rt>
    struct RefusedRetryComparison<Op, L, Rt, true>
    {
        using type = decltype(refused_retry_comparison<Op, L, Rt>());
    };
} // namespace detail

/// A retry compared, on either side of `<`, `<=`, `>`, `>=`, `==` or `!=`:
/// refused in this library's words, as arithmetic over a retry is -- an
/// acceptance is a comparison, so this is the likeliest place to write one.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator<(L, Rt) noexcept ->
    typename detail::RefusedRetryComparison<Comparison::Less, L, Rt>::type
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::refused_retry_comparison<Comparison::Less, L, Rt>();
}

/// See `operator<` over a retry.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator<=(L, Rt) noexcept ->
    typename detail::RefusedRetryComparison<Comparison::LessOrEqual, L, Rt>::type
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::refused_retry_comparison<Comparison::LessOrEqual, L, Rt>();
}

/// See `operator<` over a retry.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator>(L, Rt) noexcept ->
    typename detail::RefusedRetryComparison<Comparison::Greater, L, Rt>::type
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::refused_retry_comparison<Comparison::Greater, L, Rt>();
}

/// See `operator<` over a retry.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator>=(L, Rt) noexcept ->
    typename detail::RefusedRetryComparison<Comparison::GreaterOrEqual, L, Rt>::type
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::refused_retry_comparison<Comparison::GreaterOrEqual, L, Rt>();
}

/// See `operator<` over a retry.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator==(L, Rt) noexcept ->
    typename detail::RefusedRetryComparison<Comparison::Equal, L, Rt>::type
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::refused_retry_comparison<Comparison::Equal, L, Rt>();
}

/// See `operator<` over a retry.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator!=(L, Rt) noexcept ->
    typename detail::RefusedRetryComparison<Comparison::NotEqual, L, Rt>::type
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::refused_retry_comparison<Comparison::NotEqual, L, Rt>();
}

} // namespace formula
