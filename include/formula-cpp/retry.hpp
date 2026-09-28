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
/// produced). Outside a retry each is refused where it is evaluated.
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
/// recorded (bounded retry over recorded determinations, not yet shipped);
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
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/record.hpp>
#include <formula-cpp/sink.hpp>

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
    /// and an absent comparison is "cannot tell", not "try again".
    AtSecondAttempt,
};

/// How a retry ended: exactly one of these, always. `RetryOutcome::end()`
/// reports every one but `Failed`: a failed retry has no outcome, and
/// `checked_evaluate_retry` returns its `RetryFailure` instead. `Failed` is
/// how a trace records that end (`RetryStepData::end`, `trace.hpp`).
/// `NotRecorded` is declared for recorded attempts, which no retry reads yet.
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
    /// An attempt needed a recorded determination that is absent; the outcome
    /// is empty. Declared for recorded attempts, which no retry here reads yet.
    NotRecorded,
    /// An attempt, or its judgement, failed arithmetically; there is no
    /// outcome, only the `RetryFailure` -- never `RetryOutcome::end()`.
    Failed,
    /// A person entered the result; no attempt ran.
    ManuallyEntered,
};

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

namespace detail
{
    /// The environment an attempt is evaluated in: the caller's, and where
    /// the retry is. Everything a node asks an environment is forwarded
    /// unchanged -- the seven public members of `Environment`, and nothing
    /// else -- so a specimen's data reads the same inside a retry as outside
    /// it, and its refusals (`RequireProvided`, the series/single cross
    /// refusals) fire once, in their own words.
    ///
    /// **If a change adds a member of `Environment` that a node calls, it
    /// must be forwarded here**; `retry_tests.cpp` lists the members by hand
    /// and fails for one that is not.
    ///
    /// Values are in the coherent SI unit, in @p Rep, as every evaluator's.
    /// @p R is the retry's result quantity, the only one `previous_attempt`
    /// and `this_attempt` may name (`RequireRetriedQuantity`).
    template <typename Env, typename Rep, typename R, AttemptPhase P>
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
        constexpr AttemptEnvironment(Env const& inner, std::size_t attemptAt, Evaluated<Rep> before) noexcept
            requires(P == AttemptPhase::Attempting)
            :
            _inner { &inner },
            _attemptAt { attemptAt },
            _before { before },
            _produced {}
        {
        }

        /// The same, judging @p produced, the value the attempt produced.
        constexpr AttemptEnvironment(Env const& inner, std::size_t attemptAt, Evaluated<Rep> before, Rep produced) noexcept
            requires(P == AttemptPhase::Judging)
            :
            _inner { &inner },
            _attemptAt { attemptAt },
            _before { before },
            _produced { produced }
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

        /// Forwarded: `Environment::source_of`.
        template <Described Q>
        [[nodiscard]] constexpr ValueSource source_of() const noexcept
        {
            return _inner->template source_of<Q>();
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

      private:
        template <typename>
        friend struct RecordContextOf;

        Env const* _inner;
        std::size_t _attemptAt;
        Evaluated<Rep> _before;
        Rep _produced;
    };

    /// Whether @p E is an attempt's environment, and in which phase.
    template <typename E>
    struct AttemptEnvironmentTraits
    {
        static constexpr bool isAttempt = false;
        static constexpr bool isStarting = false;
        static constexpr bool isJudging = false;
        using retried = void;
    };

    template <typename Env, typename Rep, typename R, AttemptPhase P>
    struct AttemptEnvironmentTraits<AttemptEnvironment<Env, Rep, R, P>>
    {
        static constexpr bool isAttempt = true;
        static constexpr bool isStarting = P == AttemptPhase::Starting;
        static constexpr bool isJudging = P == AttemptPhase::Judging;
        using retried = R;
    };

    /// An attempt's environment reaches the `RecordContext` its caller's
    /// does, and only that one: a retry evaluated against a
    /// `record_context(...)` reads from another record inside an attempt, its
    /// starting value or its acceptance as it would outside the retry. Over an
    /// environment that reaches none, nothing, so a scope there is refused in
    /// `RequireRecordContext`'s words, as it is outside a retry.
    template <typename Env, typename Rep, typename R, AttemptPhase P>
        requires reachesRecordContext<Env>
    struct RecordContextOf<AttemptEnvironment<Env, Rep, R, P>>
    {
        /// The context the caller's environment reaches.
        using type = typename RecordContextOf<Env>::type;

        /// The context @p attemptEnvironment's inner environment reaches.
        [[nodiscard]] static constexpr type const& of(AttemptEnvironment<Env, Rep, R, P> const& attemptEnvironment) noexcept
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
                      "formula: previous_attempt, this_attempt and attempt_number are only meaningful inside a retry");

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
        static_assert(std::is_same_v<Q, typename AttemptEnvironmentTraits<Env>::retried>,
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

        /// Whether every check passed.
        static constexpr bool value = boundOk && attemptOk && startOk && acceptOk;
    };

    struct RetryOutcomeFactory;

    /// Whether @p verdict holds anything but blanks: a retry that runs out of
    /// attempts ends in its words, and a blank one would end in no decision
    /// at all.
    [[nodiscard]] constexpr bool verdict_says_something(Verdict const& verdict) noexcept
    {
        for (char const spelt: verdict.label)
            if (spelt != ' ' && spelt != '\t' && spelt != '\n' && spelt != '\r')
                return true;
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
    constexpr void require_verdict(Verdict const& verdict) noexcept
    {
        if consteval
        {
            if (!verdict_says_something(verdict))
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
    static constexpr bool refused = !detail::RequireRetryValid<R, Max, J, Start, A, P>::value;

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
/// time fails to evaluate (`checked_evaluate_retry`).
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
    /// What went wrong.
    ArithmeticError error;
    /// At which attempt, from 0: the starting value's failure is at 0 too.
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

        static constexpr bool quantityOk =
            attemptStarted && (std::is_void_v<Q> || std::is_same_v<Q, typename Traits::retried>);
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
        // author's mistake (R4), and says so.
        Evaluated<Rep> before = std::unexpected { ArithmeticError::DomainError };
        if constexpr (StartTraits<Start>::states)
        {
            AttemptEnvironment<Env, Rep, R, AttemptPhase::Starting> const starting { environment };
            before = dispatch<Rep>(retrying.start.expression, starting, sink);
            if (!before.has_value())
                return std::unexpected { RetryFailure { before.error(), 0 } };
        }

        // The one loop: at most Max attempts, and no other bound.
        for (std::size_t k = 1; k <= Max; ++k)
        {
            AttemptInfo const attemptInfo { .attemptNumber = k, .comparison = P::comparison };
            if constexpr (HearsAttempts<Sink, Rep>)
                sink.attempt_entered(attemptInfo);
            // Tells the sink how the attempt was judged, once, whichever way
            // the attempt ends.
            auto const told = [&](Evaluated<Rep> const& produced, AttemptJudgement judgement) {
                if constexpr (HearsAttempts<Sink, Rep>)
                    sink.attempt_produced(attemptInfo, produced, judgement);
                else
                {
                    (void) produced;
                    (void) judgement;
                }
            };

            AttemptEnvironment<Env, Rep, R, AttemptPhase::Attempting> const attempting { environment, k, before };
            Evaluated<Rep> const produced = dispatch<Rep>(retrying.attempt, attempting, sink);
            if (!produced.has_value())
            {
                told(produced, AttemptJudgement::NotJudged);
                return std::unexpected { RetryFailure { produced.error(), k - 1 } };
            }
            if (!produced->has_value())
            {
                told(produced, AttemptJudgement::NotJudgeable);
                return Factory::make<R>(Outcome<R>::empty(), RetryEnd::NotJudgeable, k, std::nullopt);
            }

            // Judged on the value the attempt produced, never on an
            // intermediate; the first attempt of a retry judged from the
            // second is not judged, which is not a rejection.
            if (J == FirstJudged::AtSecondAttempt && k == 1)
                told(produced, AttemptJudgement::NotJudged);
            else
            {
                AttemptEnvironment<Env, Rep, R, AttemptPhase::Judging> const judging { environment, k, before, **produced };
                std::expected<std::optional<bool>, ArithmeticError> const held =
                    checked_evaluate_predicate<Rep>(retrying.accept, judging, sink);
                if (!held.has_value())
                {
                    told(produced, AttemptJudgement::JudgementFailed);
                    return std::unexpected { RetryFailure { held.error(), k - 1 } };
                }
                if (!held->has_value())
                {
                    told(produced, AttemptJudgement::NotJudgeable);
                    return Factory::make<R>(Outcome<R>::empty(), RetryEnd::NotJudgeable, k, std::nullopt);
                }
                told(produced, **held ? AttemptJudgement::Accepted : AttemptJudgement::Rejected);
                if (**held)
                {
                    std::expected<Rational, ArithmeticError> const inDeclaredUnit =
                        checked_convert(**produced, coherent(A::dimension), Describe<R>::unit);
                    if (!inDeclaredUnit.has_value())
                        return std::unexpected { RetryFailure { inDeclaredUnit.error(), k - 1 } };
                    return Factory::make<R>(Outcome<R>::value(Measured<R> { *inDeclaredUnit }, ValueSource::Derived),
                                            RetryEnd::Accepted,
                                            k,
                                            k - 1);
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
    // attempt, rather than ending exhausted in no decision.
    else if (!detail::verdict_says_something(retrying.onExhausted))
        return std::unexpected { RetryFailure { ArithmeticError::DomainError, 0 } };
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
    /// `checked_evaluate`, `evaluate`, `variant<Tag>` or arithmetic. Named so
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

    /// What arithmetic over a retry gives, once refused: a node of the
    /// retry's result's dimension that is refused already
    /// (`refused_already`), so nothing over it asks again, and that is never
    /// evaluated but to a `DomainError`.
    template <Dimension D>
    struct RefusedRetryValue: NodeBase
    {
        static constexpr Dimension dimension = D;
        static constexpr bool refused = true;
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
[[nodiscard]] constexpr auto operator+(L, Rt) noexcept
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::RefusedRetryValue<detail::retried_dimension<L, Rt>()> {};
}

/// See `operator+` over a retry.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator-(L, Rt) noexcept
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::RefusedRetryValue<detail::retried_dimension<L, Rt>()> {};
}

/// See `operator+` over a retry.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator*(L, Rt) noexcept
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::RefusedRetryValue<detail::retried_dimension<L, Rt>()> {};
}

/// See `operator+` over a retry.
template <typename L, typename Rt>
    requires(detail::isRetry<L> || detail::isRetry<Rt>)
[[nodiscard]] constexpr auto operator/(L, Rt) noexcept
{
    static_assert(detail::RequireRetryAtTop<std::conditional_t<detail::isRetry<L>, L, Rt>>::value);
    return detail::RefusedRetryValue<detail::retried_dimension<L, Rt>()> {};
}

/// See `operator+` over a retry.
template <typename Operand>
    requires(detail::isRetry<Operand>)
[[nodiscard]] constexpr auto operator-(Operand) noexcept
{
    static_assert(detail::RequireRetryAtTop<Operand>::value);
    return detail::RefusedRetryValue<Describe<typename Operand::quantity>::dimension> {};
}
} // namespace formula
