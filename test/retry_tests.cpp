// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/retry.hpp>

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace
{
namespace unit = formula::unit;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// The retry fixpoint (plan, C13): an invented iterated estimate in grams.
struct Estimate: formula::Quantity<Estimate, "w", "an invented iterated estimate", unit::Gram>
{
};
struct Tolerance: formula::Quantity<Tolerance, "t_w", "an invented tolerance", unit::Gram>
{
};
struct StepSize: formula::Quantity<StepSize, "s_w", "an invented step", unit::Gram>
{
};

// w_k = 6.08 g + w_{k-1} / 2 from 0 g: 6.08, 9.12, 10.64, 11.40, 11.78 g,
// rising by 6.08, 3.04, 1.52, 0.76, 0.38 g.
constexpr auto halving = formula::constant<unit::Gram>(rat(152, 25)) + formula::previous_attempt<Estimate> / rat(2);
// Accepted when w_k - w_{k-1} <= 0.76 g, written w_{k-1} - w_k >= -0.76 g:
// the sequence rises, so the sign is known.
constexpr auto settled =
    formula::previous_attempt<Estimate> - formula::this_attempt<Estimate> >= formula::constant<unit::Gram>(rat(-19, 25));

constexpr formula::Citation cite { .title = "Settled estimate", .reference = "Example Standard 12", .section = "6" };
constexpr formula::Verdict repeat { "repeat the determination" };
constexpr auto nothing = formula::environment();
constexpr auto fromZero = formula::starting_from(formula::constant<unit::Gram>(rat(0)));
} // namespace

TEST_CASE("a retry accepts on exactly the last permitted attempt, at equality", "[retry]")
{
    constexpr auto four =
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, settled, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(four, nothing);
    STATIC_REQUIRE(ran.has_value());
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Accepted);
    // The 4th attempt, zero-based. A loop running Max - 1 attempts would be
    // exhausted at 3; a strict < would not accept at 0.76 g and exhaust at 4.
    STATIC_REQUIRE(ran->accepted_at() == std::optional<std::size_t> { 3 });
    STATIC_REQUIRE(ran->attempts_made() == 4);
    STATIC_REQUIRE(ran->outcome().measurement().value() == rat(57, 5)); // 11.4 g
    STATIC_REQUIRE(ran->outcome().source() == formula::ValueSource::Derived);
}

TEST_CASE("a strict acceptance takes one attempt more", "[retry]")
{
    // The control on the fixture: with <, 0.76 g does not settle it, and the
    // fifth attempt, 11.78 g, rising by 0.38 g, does.
    constexpr auto strictly =
        formula::previous_attempt<Estimate> - formula::this_attempt<Estimate> > formula::constant<unit::Gram>(rat(-19, 25));
    constexpr auto five =
        formula::retry<Estimate, 5, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, strictly, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(five, nothing);
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Accepted);
    STATIC_REQUIRE(ran->accepted_at() == std::optional<std::size_t> { 4 });
    STATIC_REQUIRE(ran->outcome().measurement().value() == rat(589, 50));
}

TEST_CASE("a retry that runs out of attempts returns the method's verdict, not a value", "[retry]")
{
    constexpr auto three =
        formula::retry<Estimate, 3, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, settled, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(three, nothing);
    STATIC_REQUIRE(ran.has_value());
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Exhausted);
    STATIC_REQUIRE(ran->attempts_made() == 3);
    STATIC_REQUIRE(!ran->accepted_at().has_value());
    // The verdict, never the last value (10.64 g).
    STATIC_REQUIRE(ran->outcome().is_verdict());
    STATIC_REQUIRE(ran->outcome().verdict_label() == "repeat the determination");
}

TEST_CASE("an attempt that fails stops the retry and names the attempt", "[retry]")
{
    // 6.08 g / (k - 2): attempt 2 divides by zero. Zero-based, that is 1;
    // attempts 3 and 4 never run (a later attempt would divide by 1 and 2).
    constexpr auto dividing = formula::constant<unit::Gram>(rat(152, 25)) / (formula::attempt_number - rat(2));
    constexpr auto never = formula::this_attempt<Estimate> > formula::constant<unit::Gram>(rat(100'000));
    constexpr auto four = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(dividing, never, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(four, nothing);
    STATIC_REQUIRE(!ran.has_value());
    STATIC_REQUIRE(ran.error() == formula::RetryFailure { formula::ArithmeticError::DivisionByZero, 1 });
}

TEST_CASE("previous_attempt with no starting value is the author's mistake at attempt 1", "[retry]")
{
    constexpr auto four = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(halving, settled, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(four, nothing);
    STATIC_REQUIRE(!ran.has_value());
    // Failed at the first attempt, zero-based 0 -- never empty, which would
    // read as "not measured".
    STATIC_REQUIRE(ran.error() == formula::RetryFailure { formula::ArithmeticError::DomainError, 0 });
}

TEST_CASE("an absent judgement stops the retry as not judgeable, not as another try", "[retry]")
{
    // The tolerance is not measured: attempt 1 produces 6.08 g, and whether
    // it is accepted cannot be told. Continuing would turn "cannot tell" into
    // "try again", and a fourth attempt would then be judged the same way.
    constexpr auto withinTolerance =
        formula::this_attempt<Estimate> - formula::previous_attempt<Estimate> <= formula::var<Tolerance>;
    constexpr auto unmeasured = formula::environment(formula::Measured<Tolerance>::absent());
    constexpr auto four =
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, withinTolerance, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(four, unmeasured);
    STATIC_REQUIRE(ran.has_value());
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::NotJudgeable);
    STATIC_REQUIRE(ran->attempts_made() == 1);
    STATIC_REQUIRE(ran->outcome().is_empty());

    // The control: measured at 0.76 g, the same retry is accepted at 4.
    constexpr auto measured = formula::environment(formula::Measured<Tolerance> { rat(19, 25) });
    STATIC_REQUIRE(formula::checked_evaluate_retry(four, measured)->end() == formula::RetryEnd::Accepted);
}

TEST_CASE("an absent attempt stops the retry as not judgeable", "[retry]")
{
    // The starting value is not measured, so the first attempt, which reads
    // it, is absent: not judgeable after one attempt.
    constexpr auto fromUnmeasured = formula::starting_from(formula::var<Tolerance>);
    constexpr auto unmeasured = formula::environment(formula::Measured<Tolerance>::absent());
    constexpr auto four =
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromUnmeasured, halving, settled, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(four, unmeasured);
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::NotJudgeable);
    STATIC_REQUIRE(ran->attempts_made() == 1);
    STATIC_REQUIRE(ran->outcome().is_empty());
}

TEST_CASE("a result entered by a person is returned as entered and no attempt runs", "[retry]")
{
    // 11.3 g typed in. Had any attempt run, the retry would end Accepted at
    // 11.4 g.
    constexpr auto typedIn = formula::environment(formula::entered(formula::Measured<Estimate> { rat(113, 10) }));
    constexpr auto four =
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, settled, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(four, typedIn);
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::ManuallyEntered);
    STATIC_REQUIRE(ran->attempts_made() == 0);
    STATIC_REQUIRE(ran->outcome().measurement().value() == rat(113, 10));
    STATIC_REQUIRE(ran->outcome().source() == formula::ValueSource::ManuallyEntered);
}

TEST_CASE("a retry judged from its second attempt does not judge the first", "[retry]")
{
    // Accepted when this attempt is at least 6 g: the first attempt, 6.08 g,
    // would already pass. Judged from the second, it is not judged at all, and
    // the second, 9.12 g, is accepted -- with no starting value, which the
    // attempt then never reads.
    constexpr auto doubling = formula::constant<unit::Gram>(rat(152, 25)) * formula::attempt_number;

    constexpr auto large = formula::this_attempt<Estimate> >= formula::constant<unit::Gram>(rat(6));
    constexpr auto fromSecond =
        formula::retry<Estimate, 3, formula::FirstJudged::AtSecondAttempt>(doubling, large, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(fromSecond, nothing);
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Accepted);
    STATIC_REQUIRE(ran->accepted_at() == std::optional<std::size_t> { 1 });
    STATIC_REQUIRE(ran->outcome().measurement().value() == rat(304, 25)); // 12.16 g

    // Judged from the first, the same retry accepts the first attempt.
    constexpr auto fromFirst =
        formula::retry<Estimate, 3, formula::FirstJudged::AtFirstAttempt>(doubling, large, repeat, cite);
    STATIC_REQUIRE(formula::checked_evaluate_retry(fromFirst, nothing)->accepted_at() == std::optional<std::size_t> { 0 });
}

TEST_CASE("an acceptance that fails arithmetically stops the retry at that attempt", "[retry]")
{
    // The judgement divides by (k - 3): the third attempt's judgement fails.
    constexpr auto failing =
        formula::this_attempt<Estimate> / (formula::attempt_number - rat(3)) > formula::constant<unit::Gram>(rat(1000));
    constexpr auto four =
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, failing, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(four, nothing);
    STATIC_REQUIRE(!ran.has_value());
    STATIC_REQUIRE(ran.error() == formula::RetryFailure { formula::ArithmeticError::DivisionByZero, 2 });
}

TEST_CASE("a starting value that fails fails the retry before any attempt", "[retry]")
{
    constexpr auto badStart = formula::starting_from(formula::constant<unit::Gram>(rat(1)) / rat(0));
    constexpr auto four =
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(badStart, halving, settled, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(four, nothing);
    STATIC_REQUIRE(!ran.has_value());
    STATIC_REQUIRE(ran.error() == formula::RetryFailure { formula::ArithmeticError::DivisionByZero, 0 });
}

TEST_CASE("the retry reads the specimen's data through the attempt's environment", "[retry]")
{
    // Accepted against a measured tolerance, and the attempt reading a
    // measured step: w_k = s + w_{k-1} / 2 with s = 6.08 g is the fixture.
    constexpr auto measuredStep = formula::var<StepSize> + formula::previous_attempt<Estimate> / rat(2);
    constexpr auto specimen = formula::environment(formula::Measured<StepSize> { rat(152, 25) });
    constexpr auto four =
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, measuredStep, settled, repeat, cite);
    STATIC_REQUIRE(formula::checked_evaluate_retry(four, specimen)->outcome().measurement().value() == rat(57, 5));
}

namespace
{
struct Reading: formula::Quantity<Reading, "r", "an invented reading", unit::Gram>
{
};

template <typename AE, typename Q>
constexpr bool answers_every_environment_member()
{
    // Environment's public members, listed by hand at the branch point
    // (d09657e): provides, is_entered, is_entered_series, get, get_series,
    // get_observations and source_of. If phase 14 or any later change adds a
    // member nodes call, add it here and forward it.
    return requires(AE const& wrapped) {
        { AE::template provides<Q> } -> std::convertible_to<bool>;
        { AE::template is_entered<Q> } -> std::convertible_to<bool>;
        { AE::template is_entered_series<Q> } -> std::convertible_to<bool>;
        wrapped.template get<Q>();
        wrapped.template get_series<Q, 3>();
        wrapped.template get_observations<Q, 3>();
        wrapped.template source_of<Q>();
    };
}
} // namespace

TEST_CASE("the attempt's environment answers every member of Environment", "[retry]")
{
    using Specimen = decltype(formula::environment(formula::Measured<Reading> { rat(127) }));
    using Attempting = formula::detail::AttemptEnvironment<Specimen, formula::Rational, formula::AttemptPhase::Attempting>;
    using Judging = formula::detail::AttemptEnvironment<Specimen, formula::Rational, formula::AttemptPhase::Judging>;
    STATIC_REQUIRE(answers_every_environment_member<Specimen, Reading>()); // the list itself is right
    STATIC_REQUIRE(answers_every_environment_member<Attempting, Reading>());
    STATIC_REQUIRE(answers_every_environment_member<Judging, Reading>());

    // And each answers as the environment does.
    static constexpr auto specimen = formula::environment(formula::Measured<Reading> { rat(127) });
    constexpr Attempting wrapped { specimen, 1, formula::detail::nothing<formula::Rational>() };
    STATIC_REQUIRE(Attempting::provides<Reading> && !Attempting::is_entered<Reading>);
    STATIC_REQUIRE(wrapped.get<Reading>().value() == rat(127));
    STATIC_REQUIRE(wrapped.source_of<Reading>() == specimen.source_of<Reading>());
}
