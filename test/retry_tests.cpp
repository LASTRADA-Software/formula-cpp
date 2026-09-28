// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/retry.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

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
    constexpr auto never = formula::this_attempt<Estimate> > formula::constant<unit::Gram>(rat(103'000));
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
    // Accepted when this attempt is at least 5.93 g: the first attempt, 6.08 g,
    // would already pass. Judged from the second, it is not judged at all, and
    // the second, 9.12 g, is accepted -- with no starting value, which the
    // attempt then never reads.
    constexpr auto doubling = formula::constant<unit::Gram>(rat(152, 25)) * formula::attempt_number;

    constexpr auto large = formula::this_attempt<Estimate> >= formula::constant<unit::Gram>(rat(593, 100));
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
        formula::this_attempt<Estimate> / (formula::attempt_number - rat(3)) > formula::constant<unit::Gram>(rat(1030));
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

TEST_CASE("a retry of exactly the cap runs every attempt, and one of one or two of two is allowed", "[retry]")
{
    // 6.08 g times k stays exact and never reaches 103 kg: all 64 attempts
    // run. A cap of 63, or one refusing 64, fails here.
    constexpr auto stepping = formula::constant<unit::Gram>(rat(152, 25)) * formula::attempt_number;
    constexpr auto never = formula::this_attempt<Estimate> > formula::constant<unit::Gram>(rat(103'000));
    constexpr auto sixtyFour =
        formula::retry<Estimate, 64, formula::FirstJudged::AtFirstAttempt>(stepping, never, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(sixtyFour, nothing);
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Exhausted);
    STATIC_REQUIRE(ran->attempts_made() == 64);

    // The allowed neighbours of the refused one-attempt retry judged from
    // the second.
    constexpr auto one = formula::retry<Estimate, 1, formula::FirstJudged::AtFirstAttempt>(stepping, never, repeat, cite);
    STATIC_REQUIRE(formula::checked_evaluate_retry(one, nothing)->attempts_made() == 1);
    constexpr auto two = formula::retry<Estimate, 2, formula::FirstJudged::AtSecondAttempt>(stepping, never, repeat, cite);
    STATIC_REQUIRE(formula::checked_evaluate_retry(two, nothing)->attempts_made() == 2);
}

TEST_CASE("a retry whose exact values outgrow Rational says Overflow at that attempt", "[retry]")
{
    // The fixture's own fixpoint doubles its denominator every attempt: never
    // settling by a strict margin, it passes 2^63 at attempt 53 (zero-based
    // 52), before the cap. Reported, never wrapped.
    constexpr auto flat =
        formula::previous_attempt<Estimate> - formula::this_attempt<Estimate> >= formula::constant<unit::Gram>(rat(0));
    auto const sixtyFour =
        formula::retry<Estimate, 64, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, flat, repeat, cite);
    auto const ran = formula::checked_evaluate_retry(sixtyFour, nothing);
    REQUIRE(!ran.has_value());
    CHECK(ran.error() == formula::RetryFailure { formula::ArithmeticError::Overflow, 52 });
}

TEST_CASE("a result that is only measured is recomputed, not taken as entered", "[retry]")
{
    // 11.3 g measured, not entered: the attempts run as ever, and the
    // retry's value is its own. Were a measured result short-circuited as an
    // entered one, this would read 11.3 g, ManuallyEntered, with no attempt.
    constexpr auto measured = formula::environment(formula::Measured<Estimate> { rat(113, 10) });
    constexpr auto four =
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, settled, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(four, measured);
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Accepted);
    STATIC_REQUIRE(ran->attempts_made() == 4);
    STATIC_REQUIRE(ran->outcome().source() == formula::ValueSource::Derived);
    STATIC_REQUIRE(ran->outcome().measurement().value() == rat(57, 5));
}

TEST_CASE("an absent first attempt ends a retry judged from the second as not judgeable", "[retry]")
{
    // Attempt 1 is not due a judgement, but its value is absent -- the
    // starting value is not measured -- and attempt 2 would compare against
    // it: the retry stops, not judgeable, after one attempt.
    constexpr auto fromUnmeasured = formula::starting_from(formula::var<Tolerance>);
    constexpr auto unmeasured = formula::environment(formula::Measured<Tolerance>::absent());
    constexpr auto fromSecond =
        formula::retry<Estimate, 4, formula::FirstJudged::AtSecondAttempt>(fromUnmeasured, halving, settled, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(fromSecond, unmeasured);
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::NotJudgeable);
    STATIC_REQUIRE(ran->attempts_made() == 1);
}

TEST_CASE("a retry built at run time with a blank verdict is refused before any attempt", "[retry]")
{
    // Built outside a constant expression, so retry() cannot refuse it where
    // it is written: evaluating it is the refusal, never an exhausted retry
    // that ends in no decision.
    formula::Verdict const blank { "  " };
    auto const unsaid =
        formula::retry<Estimate, 3, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, settled, blank, cite);
    auto const ran = formula::checked_evaluate_retry(unsaid, nothing);
    REQUIRE(!ran.has_value());
    CHECK(ran.error() == formula::RetryFailure { formula::ArithmeticError::DomainError, 0 });
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

namespace
{
struct Readings: formula::Quantity<Readings, "r_s", "invented readings in a series", unit::Gram>
{
};
struct Observed: formula::Quantity<Observed, "r_o", "invented raw observations", unit::Gram>
{
};

// One entry of each kind: a single measured value, a series a person entered
// and raw observations -- so that every forwarded member has something of its
// own to answer.
static constexpr auto everyKind =
    formula::environment(formula::Measured<Reading> { rat(127) },
                         formula::entered(formula::measured_series<Readings>(formula::Measured<Readings> { rat(103) },
                                                                             formula::Measured<Readings> { rat(191) },
                                                                             formula::Measured<Readings> { rat(139) })),
                         formula::MeasuredObservations<Observed, 3>(rat(163), rat(197)));
using Specimen = std::remove_cv_t<decltype(everyKind)>;

template <typename AE>
constexpr bool answers_as_the_environment(AE const& wrapped)
{
    return AE::template provides<Reading> == Specimen::provides<Reading>
           && AE::template provides<Observed> == Specimen::provides<Observed>
           && AE::template is_entered<Reading> == Specimen::is_entered<Reading>
           && AE::template is_entered_series<Readings> == Specimen::is_entered_series<Readings>
           && AE::template is_entered_series<Reading> == Specimen::is_entered_series<Reading>
           && wrapped.template get<Reading>() == everyKind.get<Reading>()
           && wrapped.template get_series<Readings, 3>() == everyKind.get_series<Readings, 3>()
           && wrapped.template get_observations<Observed, 3>() == everyKind.get_observations<Observed, 3>()
           && wrapped.template source_of<Readings>() == everyKind.source_of<Readings>()
           && wrapped.template source_of<Reading>() == everyKind.source_of<Reading>();
}
} // namespace

TEST_CASE("the attempt's environment answers every member of Environment", "[retry]")
{
    using Starting =
        formula::detail::AttemptEnvironment<Specimen, formula::Rational, Estimate, formula::AttemptPhase::Starting>;
    using Attempting =
        formula::detail::AttemptEnvironment<Specimen, formula::Rational, Estimate, formula::AttemptPhase::Attempting>;
    using Judging =
        formula::detail::AttemptEnvironment<Specimen, formula::Rational, Estimate, formula::AttemptPhase::Judging>;
    STATIC_REQUIRE(answers_every_environment_member<Specimen, Reading>()); // the list itself is right
    STATIC_REQUIRE(answers_every_environment_member<Starting, Reading>());
    STATIC_REQUIRE(answers_every_environment_member<Attempting, Reading>());
    STATIC_REQUIRE(answers_every_environment_member<Judging, Reading>());

    // And each answers as the environment does, value for value: the entered
    // series is entered, and the series and the observations are theirs.
    STATIC_REQUIRE(Specimen::is_entered_series<Readings>); // the control has something to compare
    STATIC_REQUIRE(answers_as_the_environment(Starting { everyKind }));
    STATIC_REQUIRE(answers_as_the_environment(Attempting { everyKind, 1, formula::detail::nothing<formula::Rational>() }));
    STATIC_REQUIRE(
        answers_as_the_environment(Judging { everyKind, 1, formula::detail::nothing<formula::Rational>(), rat(1) }));
}
namespace
{
constexpr auto fourAttempts =
    formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, settled, repeat, cite);
constexpr auto threeAttempts =
    formula::retry<Estimate, 3, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, settled, repeat, cite);

std::size_t count_kind(formula::Trace<> const& recorded, formula::StepKind wanted)
{
    std::size_t found = 0;
    for (formula::Step<> const& each: recorded.steps)
        if (each.kind == wanted)
            ++found;
    return found;
}

std::vector<formula::AttemptJudgement> judgements(formula::Trace<> const& recorded)
{
    std::vector<formula::AttemptJudgement> judged;
    for (formula::AttemptStepData const& row: recorded.attemptSteps)
        judged.push_back(row.judgement);
    return judged;
}

// The first three attempts of the fixpoint, as the trace shows them: the same
// for the four-attempt retry and the three-attempt one. Computed steps read in
// the coherent unit, as everywhere in a trace.
constexpr std::string_view firstThreeAttempts = "1. 0 g\n"
                                                "2. 152/25 g\n"
                                                "3. w(k-1) = 0 g\n"
                                                "4. 2\n"
                                                "5. #3 / #4 = 0\n"
                                                "6. #2 + #5 = 19/3125\n"
                                                "7. w(k-1) = 0 g\n"
                                                "8. w(k) = 152/25 g\n"
                                                "9. #7 - #8 = -19/3125\n"
                                                "10. -19/25 g\n"
                                                "11. attempt 1: w(k) = #6 = 152/25 g; judged #9 >= #10: rejected\n"
                                                "12. 152/25 g\n"
                                                "13. w(k-1) = 152/25 g\n"
                                                "14. 2\n"
                                                "15. #13 / #14 = 19/6250\n"
                                                "16. #12 + #15 = 57/6250\n"
                                                "17. w(k-1) = 152/25 g\n"
                                                "18. w(k) = 228/25 g\n"
                                                "19. #17 - #18 = -19/6250\n"
                                                "20. -19/25 g\n"
                                                "21. attempt 2: w(k) = #16 = 228/25 g; judged #19 >= #20: rejected\n"
                                                "22. 152/25 g\n"
                                                "23. w(k-1) = 228/25 g\n"
                                                "24. 2\n"
                                                "25. #23 / #24 = 57/12500\n"
                                                "26. #22 + #25 = 133/12500\n"
                                                "27. w(k-1) = 228/25 g\n"
                                                "28. w(k) = 266/25 g\n"
                                                "29. #27 - #28 = -19/12500\n"
                                                "30. -19/25 g\n"
                                                "31. attempt 3: w(k) = #26 = 266/25 g; judged #29 >= #30: rejected\n";
} // namespace

TEST_CASE("every attempt of an accepted retry is in the trace, and how it ended", "[retry][trace]")
{
    auto const accepted = formula::explain_retry(fourAttempts, nothing);
    REQUIRE(accepted.outcome.has_value());
    CHECK(accepted.outcome->end() == formula::RetryEnd::Accepted);
    // Exactly four attempts: three rejected, the fourth accepted -- accepted
    // because it was, as the exhausted retry below, whose last attempt is
    // rejected, shows.
    CHECK(count_kind(accepted.trace, formula::StepKind::RetryAttempt) == 4);
    CHECK(count_kind(accepted.trace, formula::StepKind::RetryConcluded) == 1);
    CHECK(judgements(accepted.trace)
          == std::vector<formula::AttemptJudgement> { formula::AttemptJudgement::Rejected,
                                                      formula::AttemptJudgement::Rejected,
                                                      formula::AttemptJudgement::Rejected,
                                                      formula::AttemptJudgement::Accepted });
    CHECK(formula::render_trace(accepted.trace, { .maxSteps = 100 })
          == std::string { firstThreeAttempts }
                 + "32. 152/25 g\n"
                   "33. w(k-1) = 266/25 g\n"
                   "34. 2\n"
                   "35. #33 / #34 = 133/25000\n"
                   "36. #32 + #35 = 57/5000\n"
                   "37. w(k-1) = 266/25 g\n"
                   "38. w(k) = 57/5 g\n"
                   "39. #37 - #38 = -19/25000\n"
                   "40. -19/25 g\n"
                   "41. attempt 4: w(k) = #36 = 57/5 g; judged #39 >= #40: accepted\n"
                   "42. w = retry: accepted at attempt 4 of 4 = 57/5 g [Settled estimate, Example Standard 12, 6]\n");
    // The retry's step claims the starting value and the four attempts.
    formula::RetryStepData const* const concluded = formula::retry_data(accepted.trace, accepted.trace.root());
    REQUIRE(concluded != nullptr);
    CHECK(concluded->end == formula::RetryEnd::Accepted);
    CHECK(concluded->attemptLimit == 4);
    CHECK(accepted.trace.steps[accepted.trace.root()].operands == std::vector<std::size_t> { 0, 10, 20, 30, 40 });
}

TEST_CASE("an exhausted retry shows every attempt rejected, and its verdict", "[retry][trace]")
{
    auto const exhausted = formula::explain_retry(threeAttempts, nothing);
    CHECK(count_kind(exhausted.trace, formula::StepKind::RetryAttempt) == 3);
    CHECK(judgements(exhausted.trace)
          == std::vector<formula::AttemptJudgement> { formula::AttemptJudgement::Rejected,
                                                      formula::AttemptJudgement::Rejected,
                                                      formula::AttemptJudgement::Rejected });
    CHECK(formula::render_trace(exhausted.trace, { .maxSteps = 100 })
          == std::string { firstThreeAttempts }
                 + "32. w = retry: exhausted after 3 of 3: repeat the determination [Settled estimate, Example "
                   "Standard 12, 6]\n");
}

TEST_CASE("a failed attempt ends the trace: no step for an attempt that did not run", "[retry][trace]")
{
    constexpr auto dividing = formula::constant<unit::Gram>(rat(152, 25)) / (formula::attempt_number - rat(2));
    constexpr auto never = formula::this_attempt<Estimate> > formula::constant<unit::Gram>(rat(103'000));
    auto const failed = formula::explain_retry(
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(dividing, never, repeat, cite), nothing);
    REQUIRE(!failed.outcome.has_value());
    CHECK(count_kind(failed.trace, formula::StepKind::RetryAttempt) == 2);
    for (formula::AttemptStepData const& row: failed.trace.attemptSteps)
        CHECK(row.attemptNumber <= 2);
    CHECK(formula::render_trace(failed.trace, { .maxSteps = 100 })
          == "1. 152/25 g\n"
             "2. k = 1\n"
             "3. 2\n"
             "4. #2 - #3 = -1\n"
             "5. #1 / #4 = -19/3125\n"
             "6. w(k) = -152/25 g\n"
             "7. 103000 g\n"
             "8. attempt 1: w(k) = #5 = -152/25 g; judged #6 > #7: rejected\n"
             "9. 152/25 g\n"
             "10. k = 2\n"
             "11. 2\n"
             "12. #10 - #11 = 0\n"
             "13. #9 / #12 = division by zero\n"
             "14. attempt 2: w(k) = #13 = division by zero\n"
             "15. w = retry: failed at attempt 2: division by zero [Settled estimate, Example Standard 12, 6]\n");
}

TEST_CASE("a previous attempt read with nothing before it says so in the trace", "[retry][trace]")
{
    auto const noStart = formula::explain_retry(
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(halving, settled, repeat, cite), nothing);
    std::string const text = formula::render_trace(noStart.trace, { .maxSteps = 100 });
    CHECK(text.find("2. w(k-1) = previous attempt: none before attempt 1\n") != std::string::npos);
    CHECK(text.ends_with("w = retry: failed at attempt 1: argument outside the domain of the operation "
                         "[Settled estimate, Example Standard 12, 6]\n"));
}

TEST_CASE("a retry longer than the render budget says how much was cut, and the trace keeps every attempt", "[retry][trace]")
{
    // Rising by less each time, but always rising: never accepted.
    constexpr auto flat =
        formula::previous_attempt<Estimate> - formula::this_attempt<Estimate> >= formula::constant<unit::Gram>(rat(0));
    constexpr auto sixteen =
        formula::retry<Estimate, 16, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, flat, repeat, cite);
    auto const longRun = formula::explain_retry(sixteen, nothing);
    REQUIRE(longRun.outcome.has_value());
    CHECK(longRun.outcome->end() == formula::RetryEnd::Exhausted);
    CHECK(count_kind(longRun.trace, formula::StepKind::RetryAttempt) == 16);
    std::string const text = formula::render_trace(longRun.trace, { .maxSteps = 10 });
    std::size_t lineCount = 0;
    for (char const each: text)
        if (each == '\n')
            ++lineCount;
    CHECK(lineCount == 11); // ten steps and the footer
    CHECK(text.ends_with("... " + std::to_string(longRun.trace.steps.size() - 10) + " further steps not shown\n"));
}

TEST_CASE("a result entered by a person leaves the retry's trace empty", "[retry][trace]")
{
    constexpr auto typedIn = formula::environment(formula::entered(formula::Measured<Estimate> { rat(113, 10) }));
    auto const entered = formula::explain_retry(fourAttempts, typedIn);
    REQUIRE(entered.outcome.has_value());
    CHECK(entered.outcome->end() == formula::RetryEnd::ManuallyEntered);
    CHECK(entered.trace.empty());
}

TEST_CASE("a retry renders its attempt, starting value, acceptance and verdict in every dialect", "[retry][render]")
{
    CHECK(formula::render(fourAttempts)
          == "up to 4 attempts: w(k) = 152/25 g + w(k-1) / 2, starting from w(0) = 0 g; accept when w(k-1) - w(k) "
             ">= -19/25 g; otherwise: repeat the determination");
    CHECK(formula::render<formula::Dialect::Markdown>(fourAttempts)
          == "up to 4 attempts: `w(k)` = 152/25 g + `w(k-1)` / 2, starting from `w(0)` = 0 g; accept when `w(k-1)` - "
             "`w(k)` >= -19/25 g; otherwise: repeat the determination");
    CHECK(formula::render<formula::Dialect::LaTeX>(fourAttempts)
          == "\\mathrm{up\\ to\\ 4\\ attempts:}\\ {w}_{k} = 152/25\\,\\mathrm{g} + \\frac{{w}_{k-1}}{2},\\ "
             "\\mathrm{starting\\ from}\\ {w}_{0} = 0\\,\\mathrm{g};\\ \\mathrm{accept\\ when}\\ {w}_{k-1} - {w}_{k} "
             "\\geq -19/25\\,\\mathrm{g};\\ \\mathrm{otherwise:}\\ \\mathrm{repeat\\ the\\ determination}");
    // Judged from the second attempt, with no starting value, it says so.
    constexpr auto fromSecond =
        formula::retry<Estimate, 3, formula::FirstJudged::AtSecondAttempt>(halving, settled, repeat, cite);
    CHECK(formula::render(fromSecond)
          == "up to 3 attempts: w(k) = 152/25 g + w(k-1) / 2; accept from attempt 2 when w(k-1) - w(k) >= -19/25 g; "
             "otherwise: repeat the determination");
    CHECK(formula::render(formula::attempt_number * rat(2)) == "k * 2");
}

TEST_CASE("a retry's page lists its citation and its result as iterated", "[retry][document]")
{
    constexpr auto measuredStep = formula::var<StepSize> + formula::previous_attempt<Estimate> / rat(2);
    constexpr auto stepped =
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, measuredStep, settled, repeat, cite);
    formula::Documentation const page = formula::document(stepped);
    REQUIRE(page.citations.size() == 1);
    CHECK(page.citations[0].reference == "Example Standard 12");
    REQUIRE(page.symbols.size() == 2);
    CHECK(page.symbols[0].symbol == "w");
    CHECK(page.symbols[0].description
          == "an invented iterated estimate (iterated: the value of the attempt a retry accepted)");
    CHECK(page.symbols[0].shape == formula::ValueShape::Single);
    CHECK(page.symbols[1].symbol == "s_w");
    CHECK(page.symbols[1].description == "an invented step");
    CHECK(page.formula == formula::render(stepped));
}

TEST_CASE("a scoped vocabulary renames a retry's result in the trace, the render and the page", "[retry][vocabulary]")
{
    constexpr auto north = formula::vocabulary(formula::renames<Estimate>("m"));
    auto const renamed = formula::explain_retry(fourAttempts, nothing, north);
    std::string const text = formula::render_trace(renamed.trace, { .maxSteps = 100 });
    CHECK(text.find("w(") == std::string::npos);
    CHECK(text.find("w =") == std::string::npos);
    CHECK(text.find("41. attempt 4: m(k) = #36 = 57/5 g; judged #39 >= #40: accepted\n") != std::string::npos);
    CHECK(text.find("42. m = retry: accepted at attempt 4 of 4") != std::string::npos);
    CHECK(formula::render(fourAttempts, north).find("m(k) = 152/25 g + m(k-1) / 2, starting from m(0) = 0 g")
          != std::string::npos);
    formula::Documentation const page = formula::document(fourAttempts, north);
    CHECK(page.symbols[0].symbol == "m");
    CHECK(page.formula.find("w(") == std::string::npos);
}
