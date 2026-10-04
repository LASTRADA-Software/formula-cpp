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
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
namespace unit = formula::unit;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// The retry fixpoint: an invented iterated estimate in grams.
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
    // At its own position, not at 0, which is the first attempt's: a consumer
    // holding only the failure can tell the two apart.
    STATIC_REQUIRE(
        ran.error()
        == formula::RetryFailure { formula::ArithmeticError::DivisionByZero, formula::RetryFailure::atStartingValue });
    STATIC_REQUIRE(ran.error().attempt != 0);
    STATIC_REQUIRE(formula::RetryFailure::atStartingValue != formula::RetryFailure::refusedBeforeStart);
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
    // A step that squares its value: from 3/2 g, w(k) = w(k-1)^2 / 1 g, never
    // settling by a strict margin, outgrows 128 bits at attempt 7 (zero-based
    // 6), before the cap. Reported, never wrapped.
    constexpr auto flat =
        formula::previous_attempt<Estimate> - formula::this_attempt<Estimate> >= formula::constant<unit::Gram>(rat(0));
    constexpr auto fromThreeHalves = formula::starting_from(formula::constant<unit::Gram>(rat(3, 2)));
    constexpr auto squaring =
        formula::previous_attempt<Estimate> * formula::previous_attempt<Estimate> / formula::constant<unit::Gram>(rat(1));
    auto const sixtyFour =
        formula::retry<Estimate, 64, formula::FirstJudged::AtFirstAttempt>(fromThreeHalves, squaring, flat, repeat, cite);
    auto const ran = formula::checked_evaluate_retry(sixtyFour, nothing);
    REQUIRE(!ran.has_value());
    CHECK(ran.error() == formula::RetryFailure { formula::ArithmeticError::Overflow, 6 });

    // The fixture's own fixpoint, which doubles its denominator every
    // attempt and passed 2^63 at attempt 53, runs all 64 attempts: 2^64 is a
    // denominator 128 bits hold.
    auto const halvingRun = formula::checked_evaluate_retry(
        formula::retry<Estimate, 64, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, flat, repeat, cite), nothing);
    REQUIRE(halvingRun.has_value());
    CHECK(halvingRun->end() == formula::RetryEnd::Exhausted);
    CHECK(halvingRun->attempts_made() == 64);
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
    // Not { DomainError, 0 }, which a starting value that failed gives: no
    // attempt has this position.
    CHECK(ran.error()
          == formula::RetryFailure { formula::ArithmeticError::DomainError, formula::RetryFailure::refusedBeforeStart });
    CHECK(ran.error().attempt != 0);

    // Every character std::isspace counts, and the no-break space in UTF-8,
    // is blank; one printable character is not.
    for (std::string_view const blankLabel: { std::string_view { "\v\f" },
                                              std::string_view { "\t\n\r " },
                                              std::string_view { "\xC2\xA0" },
                                              std::string_view { " \xC2\xA0\v" } })
    {
        INFO("label of " << blankLabel.size() << " bytes");
        auto const blankRetry = formula::retry<Estimate, 3, formula::FirstJudged::AtFirstAttempt>(
            fromZero, halving, settled, formula::Verdict { blankLabel }, cite);
        auto const refused = formula::checked_evaluate_retry(blankRetry, nothing);
        REQUIRE(!refused.has_value());
        CHECK(refused.error().attempt == formula::RetryFailure::refusedBeforeStart);
    }
    auto const said = formula::retry<Estimate, 3, formula::FirstJudged::AtFirstAttempt>(
        fromZero, halving, settled, formula::Verdict { "\xC2\xA0x" }, cite);
    CHECK(formula::checked_evaluate_retry(said, nothing).has_value());

    // An entered result does not rescue it: the retry as written is refused,
    // whatever the environment holds.
    constexpr auto typedIn = formula::environment(formula::entered(formula::Measured<Estimate> { rat(163) }));
    auto const enteredButBlank = formula::checked_evaluate_retry(unsaid, typedIn);
    REQUIRE(!enteredButBlank.has_value());
    CHECK(enteredButBlank.error().attempt == formula::RetryFailure::refusedBeforeStart);
}

TEST_CASE("a retry of a cv-qualified result quantity reads its own value under either spelling", "[retry]")
{
    // `Estimate const` is Estimate: the same quantity, dimension and label.
    constexpr auto constFour =
        formula::retry<Estimate const, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, settled, repeat, cite);
    constexpr auto ran = formula::checked_evaluate_retry(constFour, nothing);
    STATIC_REQUIRE(ran.has_value());
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Accepted);
    STATIC_REQUIRE(ran->outcome().measurement().value() == rat(57, 5));
    CHECK(formula::render(constFour).find("w(k) = 152/25 g + w(k-1) / 2") != std::string::npos);
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
        formula::detail::AttemptEnvironment<Specimen, formula::Rational, Estimate, formula::AttemptPhase::Starting, 4>;
    using Attempting =
        formula::detail::AttemptEnvironment<Specimen, formula::Rational, Estimate, formula::AttemptPhase::Attempting, 4>;
    using Judging =
        formula::detail::AttemptEnvironment<Specimen, formula::Rational, Estimate, formula::AttemptPhase::Judging, 4>;
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
// for the four-attempt retry and the three-attempt one. Each computed step is a
// mass halved, or a sum or difference of masses in grams, so it reads in grams,
// as the values it is computed from do.
constexpr std::string_view firstThreeAttempts = "1. 0 g\n"
                                                "2. 152/25 g\n"
                                                "3. w(k-1) = 0 g\n"
                                                "4. 2\n"
                                                "5. #3 / #4 = 0 g\n"
                                                "6. #2 + #5 = 152/25 g\n"
                                                "7. w(k-1) = 0 g\n"
                                                "8. w(k) = 152/25 g\n"
                                                "9. #7 - #8 = -152/25 g\n"
                                                "10. -19/25 g\n"
                                                "11. attempt 1: w(k) = #6 = 152/25 g; judged #9 >= #10: rejected\n"
                                                "12. 152/25 g\n"
                                                "13. w(k-1) = 152/25 g\n"
                                                "14. 2\n"
                                                "15. #13 / #14 = 76/25 g\n"
                                                "16. #12 + #15 = 228/25 g\n"
                                                "17. w(k-1) = 152/25 g\n"
                                                "18. w(k) = 228/25 g\n"
                                                "19. #17 - #18 = -76/25 g\n"
                                                "20. -19/25 g\n"
                                                "21. attempt 2: w(k) = #16 = 228/25 g; judged #19 >= #20: rejected\n"
                                                "22. 152/25 g\n"
                                                "23. w(k-1) = 228/25 g\n"
                                                "24. 2\n"
                                                "25. #23 / #24 = 114/25 g\n"
                                                "26. #22 + #25 = 266/25 g\n"
                                                "27. w(k-1) = 228/25 g\n"
                                                "28. w(k) = 266/25 g\n"
                                                "29. #27 - #28 = -38/25 g\n"
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
                   "35. #33 / #34 = 133/25 g\n"
                   "36. #32 + #35 = 57/5 g\n"
                   "37. w(k-1) = 266/25 g\n"
                   "38. w(k) = 57/5 g\n"
                   "39. #37 - #38 = -19/25 g\n"
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
             "5. #1 / #4 = -152/25 g\n"
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
    CHECK(
        formula::render<formula::Dialect::LaTeX>(fourAttempts)
        == "\\mathrm{up}\\ \\allowbreak \\mathrm{to}\\ \\allowbreak \\mathrm{4}\\ \\allowbreak "
           "\\mathrm{attempts:}\\ {w}_{k} = 152/25\\,\\mathrm{g} + \\frac{{w}_{k-1}}{2},\\ \\allowbreak "
           "\\mathrm{starting}\\ \\allowbreak \\mathrm{from}\\ {w}_{0} = 0\\,\\mathrm{g};\\ \\allowbreak "
           "\\mathrm{accept}\\ \\allowbreak \\mathrm{when}\\ {w}_{k-1} - {w}_{k} \\geq -19/25\\,\\mathrm{g};\\ "
           "\\allowbreak \\mathrm{otherwise:}\\ \\mathrm{repeat}\\ \\allowbreak \\mathrm{the}\\ \\allowbreak "
           "\\mathrm{determination}");
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

TEST_CASE("an absent attempt ends the trace not judgeable, and never accepted", "[retry][trace]")
{
    constexpr auto unmeasured = formula::environment(formula::Measured<Tolerance>::absent());
    constexpr auto absentStep = formula::var<Tolerance> + formula::previous_attempt<Estimate> / rat(2);
    auto const ran = formula::explain_retry(
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, absentStep, settled, repeat, cite),
        unmeasured);
    CHECK(judgements(ran.trace) == std::vector<formula::AttemptJudgement> { formula::AttemptJudgement::NotJudgeable });
    CHECK(formula::render_trace(ran.trace, { .maxSteps = 100 })
          == "1. 0 g\n"
             "2. t_w = (not measured)\n"
             "3. w(k-1) = 0 g\n"
             "4. 2\n"
             "5. #3 / #4 = 0 g\n"
             "6. #2 + #5 = (not measured)\n"
             "7. attempt 1: w(k) = #6 = (not measured); cannot be judged\n"
             "8. w = retry: not judgeable at attempt 1 [Settled estimate, Example Standard 12, 6]\n");
}

TEST_CASE("an absent judgement ends the trace not judgeable, naming the sides it could not compare", "[retry][trace]")
{
    constexpr auto unmeasured = formula::environment(formula::Measured<Tolerance>::absent());
    constexpr auto againstAbsent = formula::this_attempt<Estimate> >= formula::var<Tolerance>;
    auto const ran = formula::explain_retry(
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, againstAbsent, repeat, cite),
        unmeasured);
    CHECK(judgements(ran.trace) == std::vector<formula::AttemptJudgement> { formula::AttemptJudgement::NotJudgeable });
    CHECK(formula::render_trace(ran.trace, { .maxSteps = 100 })
          == "1. 0 g\n"
             "2. 152/25 g\n"
             "3. w(k-1) = 0 g\n"
             "4. 2\n"
             "5. #3 / #4 = 0 g\n"
             "6. #2 + #5 = 152/25 g\n"
             "7. w(k) = 152/25 g\n"
             "8. t_w = (not measured)\n"
             "9. attempt 1: w(k) = #6 = 152/25 g; judged #7 >= #8: cannot be judged\n"
             "10. w = retry: not judgeable at attempt 1 [Settled estimate, Example Standard 12, 6]\n");
}

TEST_CASE("a judgement that fails names the failing side, and the attempt keeps only its value", "[retry][trace]")
{
    // The right side divides by (k - 1): zero at the first attempt.
    constexpr auto rightFails =
        formula::this_attempt<Estimate> >= formula::constant<unit::Gram>(rat(1)) / (formula::attempt_number - rat(1));
    auto const right = formula::explain_retry(
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, rightFails, repeat, cite),
        nothing);
    CHECK(judgements(right.trace) == std::vector<formula::AttemptJudgement> { formula::AttemptJudgement::JudgementFailed });
    CHECK(formula::render_trace(right.trace, { .maxSteps = 100 })
          == "1. 0 g\n"
             "2. 152/25 g\n"
             "3. w(k-1) = 0 g\n"
             "4. 2\n"
             "5. #3 / #4 = 0 g\n"
             "6. #2 + #5 = 152/25 g\n"
             "7. w(k) = 152/25 g\n"
             "8. 1 g\n"
             "9. k = 1\n"
             "10. 1\n"
             "11. #9 - #10 = 0\n"
             "12. #8 / #11 = division by zero\n"
             "13. attempt 1: w(k) = #6 = 152/25 g; judged #7 >= #12: division by zero\n"
             "14. w = retry: failed at attempt 1: division by zero [Settled estimate, Example Standard 12, 6]\n");
    // A step holds a value or an error, never both: the error is the side's.
    formula::Step<> const& attemptStep = right.trace.steps[12];
    CHECK(attemptStep.value.has_value());
    CHECK(!attemptStep.error.has_value());

    // The left side fails first: the right is never evaluated, and the line
    // names the side that failed.
    constexpr auto leftFails =
        formula::this_attempt<Estimate> / (formula::attempt_number - rat(1)) >= formula::constant<unit::Gram>(rat(1));
    auto const left = formula::explain_retry(
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, leftFails, repeat, cite),
        nothing);
    std::string const leftText = formula::render_trace(left.trace, { .maxSteps = 100 });
    CHECK(leftText.find("12. attempt 1: w(k) = #6 = 152/25 g; judged #11: division by zero\n") != std::string::npos);
    CHECK(leftText.ends_with("13. w = retry: failed at attempt 1: division by zero [Settled estimate, Example "
                             "Standard 12, 6]\n"));
}

TEST_CASE("the first attempt of a retry judged from the second reads not judged, not rejected", "[retry][trace]")
{
    auto const ran = formula::explain_retry(
        formula::retry<Estimate, 5, formula::FirstJudged::AtSecondAttempt>(fromZero, halving, settled, repeat, cite),
        nothing);
    CHECK(judgements(ran.trace)
          == std::vector<formula::AttemptJudgement> { formula::AttemptJudgement::NotJudged,
                                                      formula::AttemptJudgement::Rejected,
                                                      formula::AttemptJudgement::Rejected,
                                                      formula::AttemptJudgement::Accepted });
    std::string const text = formula::render_trace(ran.trace, { .maxSteps = 100 });
    CHECK(text.find("7. attempt 1: w(k) = #6 = 152/25 g; not judged\n") != std::string::npos);
    CHECK(text.find("17. attempt 2: w(k) = #12 = 228/25 g; judged #15 >= #16: rejected\n") != std::string::npos);
    CHECK(text.find("37. attempt 4: w(k) = #32 = 57/5 g; judged #35 >= #36: accepted\n") != std::string::npos);
    CHECK(text.ends_with("38. w = retry: accepted at attempt 4 of 5 = 57/5 g [Settled estimate, Example Standard "
                         "12, 6]\n"));
}

TEST_CASE("a starting value that fails ends the trace before any attempt", "[retry][trace]")
{
    auto const ran = formula::explain_retry(
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(
            formula::starting_from(formula::constant<unit::Gram>(rat(1)) / rat(0)), halving, settled, repeat, cite),
        nothing);
    CHECK(ran.trace.attemptSteps.empty());
    CHECK(formula::render_trace(ran.trace, { .maxSteps = 100 })
          == "1. 1 g\n"
             "2. 0\n"
             "3. #1 / #2 = division by zero\n"
             "4. w = retry: failed at its starting value: division by zero [Settled estimate, Example Standard 12, "
             "6]\n");
}

TEST_CASE("an exhausted retry counts the attempts it shows, not its limit", "[retry][trace]")
{
    // A trace that lost its last attempt -- edited by hand, or recorded by a
    // sink that dropped one -- says two ran, not three.
    auto exhausted = formula::explain_retry(threeAttempts, nothing);
    exhausted.trace.attemptSteps.pop_back();
    CHECK(formula::render_trace(exhausted.trace, { .maxSteps = 100 })
              .ends_with("w = retry: exhausted after 2 of 3: repeat the determination [Settled estimate, Example "
                         "Standard 12, 6]\n"));
}

namespace
{
struct ReferenceRecord
{
};
} // namespace

TEST_CASE("an attempt reads from another record, and only what it read says so", "[retry][trace][record]")
{
    // w_k = 6.08 g + w_{k-1} / 2 + (s_w there - s_w here): both records hold
    // 139 g, so the sequence is the fixture's, accepted at attempt 4 at
    // 11.4 g. The attempt is evaluated against an attempt's environment over
    // the record context; the scope must still find the context through it.
    constexpr auto stepHere = formula::environment(formula::Measured<StepSize> { rat(139) });
    constexpr auto stepThere = formula::environment(formula::Measured<StepSize> { rat(139) });
    auto const context = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), stepHere),
        formula::record<ReferenceRecord>(formula::record_key(formula::sample_id(23), formula::test_id(3)), stepThere));
    constexpr auto reading =
        halving + formula::from_record<ReferenceRecord>(formula::var<StepSize>) - formula::var<StepSize>;
    constexpr auto four =
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, reading, settled, repeat, cite);

    auto const ran = formula::checked_evaluate_retry(four, context);
    REQUIRE(ran.has_value());
    CHECK(ran->end() == formula::RetryEnd::Accepted);
    CHECK(ran->outcome().measurement().value() == rat(57, 5));

    formula::ExplainedRetry<Estimate> const explained = formula::explain_retry(four, context);
    REQUIRE(explained.outcome.has_value());
    std::size_t scopes = 0;
    for (formula::Step<> const& recorded: explained.trace.steps)
    {
        bool const readThere =
            recorded.kind == formula::StepKind::RecordScope
            || (recorded.kind == formula::StepKind::Variable && formula::origin_of(explained.trace, recorded).has_value());
        if (recorded.kind == formula::StepKind::RecordScope)
            ++scopes;
        if (readThere)
        {
            REQUIRE(formula::origin_of(explained.trace, recorded).has_value());
            CHECK(formula::origin_of(explained.trace, recorded)->role() == "ReferenceRecord");
        }
        // The attempts and the retry are this record's.
        if (recorded.kind == formula::StepKind::RetryAttempt || recorded.kind == formula::StepKind::RetryConcluded)
            CHECK(!formula::origin_of(explained.trace, recorded).has_value());
    }
    CHECK(scopes == 4); // one read per attempt
    CHECK(explained.trace.recordStack.empty());
}

TEST_CASE("an attempt and a retry recorded inside a record's scope say which record", "[retry][trace][record]")
{
    // No retry is a node, so none sits inside a from_record today; but
    // `sink.hpp` says every step recorded between `record_entered` and the
    // scope's own `produced` is stamped, and a sink composed of the
    // library's may be told the hooks in that order. Drive them by hand, with
    // an origin the library stated for a real read.
    constexpr auto stepHere = formula::environment(formula::Measured<StepSize> { rat(139) });
    auto const context = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), stepHere),
        formula::record<ReferenceRecord>(formula::record_key(formula::sample_id(23), formula::test_id(3)), stepHere));
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(
        formula::from_record<ReferenceRecord>(formula::var<StepSize>), context, sink);
    REQUIRE(trace.origins.size() == 1);
    REQUIRE(trace.recordStack.empty());

    sink.record_entered(trace.origins.front());
    formula::RetryInfo const retryInfo { .attemptLimit = 1,
                                         .firstJudged = formula::FirstJudged::AtFirstAttempt,
                                         .citation = cite,
                                         .verdictLabel = repeat.label };
    formula::AttemptInfo const attemptInfo { .attemptNumber = 1, .comparison = formula::Comparison::GreaterOrEqual };
    sink.retry_entered(retryInfo);
    sink.attempt_entered(attemptInfo);
    sink.attempt_produced(
        attemptInfo, formula::Evaluated<formula::Rational> { rat(139, 1000) }, formula::AttemptJudgement::Rejected);
    sink.retry_produced<Estimate>(
        retryInfo,
        std::expected<formula::RetryOutcome<Estimate>, formula::RetryFailure> {
            std::unexpected { formula::RetryFailure { .error = formula::ArithmeticError::Overflow, .attempt = 1 } } });

    REQUIRE(trace.steps.size() == 4);
    CHECK(trace.steps[2].kind == formula::StepKind::RetryAttempt);
    CHECK(trace.steps[3].kind == formula::StepKind::RetryConcluded);
    for (std::size_t const inside: { std::size_t { 2 }, std::size_t { 3 } })
    {
        INFO("step " << inside);
        REQUIRE(formula::origin_of(trace, trace.steps[inside]).has_value());
        CHECK(formula::origin_of(trace, trace.steps[inside])->role() == "ReferenceRecord");
    }
}

TEST_CASE("the walk behind a retry's quantity check sees a context node inside every kind of node", "[retry]")
{
    // The check that refuses a misnamed previous_attempt where the retry is
    // built (RequireOnlyRetriedQuantity) is only as good as this walk: it must
    // find the node however deep, and through every template shape a node has
    // -- types only (when, documented), a value then types (arithmetic, a
    // comparison), several values then types (a rounding).
    using formula::detail::NamesAnotherQuantity;
    constexpr auto misnamed = formula::previous_attempt<Tolerance>;
    constexpr auto own = formula::previous_attempt<Estimate>;
    constexpr auto gram = formula::constant<unit::Gram>(rat(1));
    STATIC_REQUIRE(NamesAnotherQuantity<Estimate, std::remove_cv_t<decltype(misnamed)>>::value);
    STATIC_REQUIRE(!NamesAnotherQuantity<Estimate, std::remove_cv_t<decltype(own)>>::value);
    STATIC_REQUIRE(NamesAnotherQuantity<Estimate, std::remove_cv_t<decltype(gram + misnamed / rat(2))>>::value);
    STATIC_REQUIRE(!NamesAnotherQuantity<Estimate, std::remove_cv_t<decltype(gram + own / rat(2))>>::value);
    constexpr auto nested = formula::documented(
        formula::when(own >= gram,
                      formula::rounded<unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
                          gram - misnamed),
                      gram),
        { .reference = "Example Standard 12" });
    STATIC_REQUIRE(NamesAnotherQuantity<Estimate, std::remove_cv_t<decltype(nested)>>::value);
    STATIC_REQUIRE(
        NamesAnotherQuantity<Estimate, std::remove_cv_t<decltype(formula::this_attempt<Tolerance> >= gram)>>::value);
    // cv-qualification aside, the retry's own quantity is its own.
    STATIC_REQUIRE(!NamesAnotherQuantity<Estimate const, std::remove_cv_t<decltype(own)>>::value);
    STATIC_REQUIRE(!NamesAnotherQuantity<Estimate, formula::PreviousAttemptNode<Estimate const>>::value);
}

namespace
{
// Recorded attempts: attempt k's result is determination k,
// accepted from the second attempt when two successive results agree within
// 1.27 g. 41.3, 43.9, 42.7, 45.7 g: |43.9 - 41.3| = 2.6, not accepted at 2;
// |42.7 - 43.9| = 1.2, accepted at 3 with 42.7 g. Never stopping would give
// 45.7, the earlier of the agreeing pair 43.9, and judging at attempt 1
// against an absent previous would fail.
struct Determination: formula::Quantity<Determination, "d", "an invented determination", unit::Gram>
{
};
struct Agreed: formula::Quantity<Agreed, "d_a", "an invented agreed determination", unit::Gram>
{
};

constexpr auto agree = formula::when(formula::this_attempt<Agreed> >= formula::previous_attempt<Agreed>,
                                     formula::this_attempt<Agreed> - formula::previous_attempt<Agreed>,
                                     formula::previous_attempt<Agreed> - formula::this_attempt<Agreed>)
                       <= formula::constant<unit::Gram>(rat(127, 100));

constexpr auto successive = formula::retry<Agreed, 4, formula::FirstJudged::AtSecondAttempt>(
    formula::attempt_input<Determination>,
    agree,
    formula::Verdict { "repeat the test" },
    { .title = "Agreed determination", .reference = "Example Standard 12", .section = "7" });

constexpr auto recorded_as(formula::Measured<Determination> first,
                           formula::Measured<Determination> second,
                           formula::Measured<Determination> third,
                           formula::Measured<Determination> fourth)
{
    return formula::environment(formula::measured_series<Determination>(first, second, third, fourth));
}

constexpr formula::Measured<Determination> grams(std::int64_t tenths)
{
    return formula::Measured<Determination> { rat(tenths, 10) };
}

constexpr auto allFour = recorded_as(grams(413), grams(439), grams(427), grams(457));
} // namespace

TEST_CASE("a retry over recorded determinations stops at the first pair that agrees", "[retry][recorded]")
{
    constexpr auto ran = formula::checked_evaluate_retry(successive, allFour);
    STATIC_REQUIRE(ran.has_value());
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Accepted);
    STATIC_REQUIRE(ran->accepted_at() == std::optional<std::size_t> { 2 }); // the 3rd attempt, zero-based
    STATIC_REQUIRE(ran->attempts_made() == 3);
    STATIC_REQUIRE(ran->outcome().measurement().value() == rat(427, 10)); // 42.7, not 45.7 nor 43.9
}

TEST_CASE("a determination the method needed but nobody recorded ends the retry as not recorded", "[retry][recorded]")
{
    // Element 3 absent: the third attempt needs it, and the retry ends there,
    // not recorded -- not "not judgeable", which says a comparison could not
    // tell, and not an empty value quietly accepted or rejected.
    constexpr auto thirdMissing =
        recorded_as(grams(413), grams(439), formula::Measured<Determination>::absent(), grams(457));
    constexpr auto missing = formula::checked_evaluate_retry(successive, thirdMissing);
    STATIC_REQUIRE(missing.has_value());
    STATIC_REQUIRE(missing->end() == formula::RetryEnd::NotRecorded);
    STATIC_REQUIRE(missing->attempts_made() == 3);
    STATIC_REQUIRE(!missing->accepted_at().has_value());
    STATIC_REQUIRE(missing->outcome().is_empty());

    // Element 4 absent: the retry is accepted at the third attempt and never
    // reads it.
    constexpr auto fourthMissing =
        recorded_as(grams(413), grams(439), grams(427), formula::Measured<Determination>::absent());
    constexpr auto accepted = formula::checked_evaluate_retry(successive, fourthMissing);
    STATIC_REQUIRE(accepted.has_value());
    STATIC_REQUIRE(accepted->end() == formula::RetryEnd::Accepted);
    STATIC_REQUIRE(accepted->accepted_at() == std::optional<std::size_t> { 2 });
    STATIC_REQUIRE(accepted->outcome().measurement().value() == rat(427, 10));

    // The first missing: not recorded at attempt 1, although attempt 1 is not
    // judged.
    constexpr auto firstMissing =
        recorded_as(formula::Measured<Determination>::absent(), grams(439), grams(427), grams(457));
    constexpr auto atFirst = formula::checked_evaluate_retry(successive, firstMissing);
    STATIC_REQUIRE(atFirst->end() == formula::RetryEnd::NotRecorded);
    STATIC_REQUIRE(atFirst->attempts_made() == 1);
}

TEST_CASE("a determination read only by the acceptance, and missing, is not recorded either", "[retry][recorded]")
{
    // The attempt reads the previous value; the acceptance compares it with
    // the recorded determination. A missing one is still "not recorded".
    constexpr auto closeTo = formula::attempt_input<Determination> - formula::this_attempt<Agreed>
                             <= formula::constant<unit::Gram>(rat(127, 100));
    constexpr auto follows = formula::retry<Agreed, 4, formula::FirstJudged::AtFirstAttempt>(
        formula::starting_from(formula::constant<unit::Gram>(rat(413, 10))),
        formula::previous_attempt<Agreed> + formula::constant<unit::Gram>(rat(0)),
        closeTo,
        formula::Verdict { "repeat the test" },
        { .reference = "Example Standard 12", .section = "7" });
    constexpr auto secondMissing =
        recorded_as(grams(457), formula::Measured<Determination>::absent(), grams(427), grams(413));
    constexpr auto ran = formula::checked_evaluate_retry(follows, secondMissing);
    // Attempt 1: 45.7 - 41.3 = 4.4 g, rejected; attempt 2 reads nothing.
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::NotRecorded);
    STATIC_REQUIRE(ran->attempts_made() == 2);
}

TEST_CASE("a recorded attempt reads as the determination in the trace, the render and the page",
          "[retry][recorded][trace][render][document]")
{
    auto const accepted = formula::explain_retry(successive, allFour);
    REQUIRE(accepted.outcome.has_value());
    std::string const text = formula::render_trace(accepted.trace, { .maxSteps = 100 });
    INFO(text);
    CHECK(text.find("1. d(k) = 413/10 g\n") != std::string::npos);
    CHECK(text.find("2. attempt 1: d_a(k) = #1 = 413/10 g; not judged\n") != std::string::npos);
    CHECK(text.find("d_a = retry: accepted at attempt 3 of 4 = 427/10 g [Agreed determination, Example Standard 12, 7]")
          != std::string::npos);
    std::vector<formula::AttemptJudgement> const judged = judgements(accepted.trace);
    CHECK(judged
          == std::vector<formula::AttemptJudgement> { formula::AttemptJudgement::NotJudged,
                                                      formula::AttemptJudgement::Rejected,
                                                      formula::AttemptJudgement::Accepted });
    CHECK(count_kind(accepted.trace, formula::StepKind::AttemptInput) == 3);

    constexpr auto thirdMissing =
        recorded_as(grams(413), grams(439), formula::Measured<Determination>::absent(), grams(457));
    auto const missing = formula::explain_retry(successive, thirdMissing);
    REQUIRE(missing.outcome.has_value());
    std::string const missingText = formula::render_trace(missing.trace, { .maxSteps = 100 });
    INFO(missingText);
    CHECK(missingText.find("d(k) = (not recorded)\n") != std::string::npos);
    // The attempt's line says "not recorded" once, as its value -- never
    // "(not measured)" for the same missing determination.
    std::size_t const attemptAt = missingText.find("attempt 3: d_a(k) = #");
    REQUIRE(attemptAt != std::string::npos);
    std::string const attemptLine = missingText.substr(attemptAt, missingText.find('\n', attemptAt) - attemptAt);
    CHECK(attemptLine.ends_with(" = (not recorded)"));
    CHECK(attemptLine.find("not recorded") == attemptLine.rfind("not recorded"));
    CHECK(missingText.find("(not measured)") == std::string::npos);
    CHECK(missingText.find("d_a = retry: attempt 3 not recorded [Agreed determination, Example Standard 12, 7]")
          != std::string::npos);
    CHECK(judgements(missing.trace).back() == formula::AttemptJudgement::NotRecorded);

    CHECK(formula::render(successive).find("up to 4 attempts: d_a(k) = d(k); accept from attempt 2 when")
          != std::string::npos);
    CHECK(formula::render<formula::Dialect::Markdown>(successive).find("`d_a(k)` = `d(k)`") != std::string::npos);
    CHECK(formula::render<formula::Dialect::LaTeX>(successive).find("{d_a}_{k} = {d}_{k}") != std::string::npos);

    // The page: the result, iterated, then the determinations, a series of
    // one per attempt allowed.
    formula::Documentation const page = formula::document(successive);
    REQUIRE(page.symbols.size() == 2);
    CHECK(page.symbols[0].symbol == "d_a");
    CHECK(page.symbols[1].symbol == "d");
    CHECK(page.symbols[1].description == "an invented determination");
    CHECK(page.symbols[1].shape == formula::ValueShape::Series);
    CHECK(page.symbols[1].length == 4);
}

TEST_CASE("a determination typed in by a person says so, and so does one typed in empty", "[retry][recorded][trace]")
{
    // The series of determinations was entered by hand: each attempt's read
    // says so, as a series variable over the same entry does, and the empty
    // third reads as a variable typed in empty does -- never "(not recorded)".
    constexpr auto typedIn = formula::environment(formula::entered(formula::measured_series<Determination>(
        grams(413), grams(439), formula::Measured<Determination>::absent(), grams(457))));
    auto const ran = formula::explain_retry(successive, typedIn);
    REQUIRE(ran.outcome.has_value());
    CHECK(ran.outcome->end() == formula::RetryEnd::NotRecorded);
    std::string const text = formula::render_trace(ran.trace, { .maxSteps = 100 });
    INFO(text);
    CHECK(text.find("1. d(k) = 413/10 g, entered by hand\n") != std::string::npos);
    CHECK(text.find("d(k) = 439/10 g, entered by hand\n") != std::string::npos);
    CHECK(text.find("d(k) = (entered by hand as empty)\n") != std::string::npos);
    CHECK(text.find("d(k) = (not recorded)") == std::string::npos);
    std::size_t typedSteps = 0;
    for (formula::Step<> const& recorded: ran.trace.steps)
        if (recorded.kind == formula::StepKind::AttemptInput)
        {
            CHECK(recorded.inputSource == formula::ValueSource::ManuallyEntered);
            ++typedSteps;
        }
    CHECK(typedSteps == 3);
    // Measured, it says nothing, and records that it was measured.
    auto const measured = formula::explain_retry(successive, allFour);
    for (formula::Step<> const& recorded: measured.trace.steps)
        if (recorded.kind == formula::StepKind::AttemptInput)
            CHECK(recorded.inputSource == formula::ValueSource::Measured);
    CHECK(formula::render_trace(measured.trace, { .maxSteps = 100 }).find("entered by hand") == std::string::npos);
}

TEST_CASE("an arithmetic failure outranks a missing determination, in the attempt and in the judgement", "[retry][recorded]")
{
    // 1 g / (k - k) divides by zero whatever is read beside it; the first
    // determination is missing. A binary node evaluates both sides, and the
    // failure is what the retry reports -- Failed, not NotRecorded.
    constexpr auto zero = formula::attempt_number - formula::attempt_number;
    constexpr auto firstMissing =
        recorded_as(formula::Measured<Determination>::absent(), grams(439), grams(427), grams(457));
    constexpr auto failingAttempt = formula::retry<Agreed, 4, formula::FirstJudged::AtSecondAttempt>(
        formula::attempt_input<Determination> + formula::constant<unit::Gram>(rat(1)) / zero,
        agree,
        formula::Verdict { "repeat the test" },
        { .reference = "Example Standard 12", .section = "7" });
    constexpr auto inAttempt = formula::checked_evaluate_retry(failingAttempt, firstMissing);
    STATIC_REQUIRE(!inAttempt.has_value());
    STATIC_REQUIRE(inAttempt.error() == formula::RetryFailure { formula::ArithmeticError::DivisionByZero, 0 });

    constexpr auto failingJudgement = formula::retry<Agreed, 4, formula::FirstJudged::AtFirstAttempt>(
        formula::constant<unit::Gram>(rat(413, 10)),
        formula::this_attempt<Agreed> >= formula::attempt_input<Determination>
                                             + formula::constant<unit::Gram>(rat(1)) / zero,
        formula::Verdict { "repeat the test" },
        { .reference = "Example Standard 12", .section = "7" });
    constexpr auto inJudgement = formula::checked_evaluate_retry(failingJudgement, firstMissing);
    STATIC_REQUIRE(!inJudgement.has_value());
    STATIC_REQUIRE(inJudgement.error() == formula::RetryFailure { formula::ArithmeticError::DivisionByZero, 0 });
}

TEST_CASE("a retry that reads its determinations both ways lists them once", "[retry][recorded][document]")
{
    // attempt_input<d> and series<d, 4> in a retry of 4 read one entry of the
    // environment: one row, whichever the page meets first.
    constexpr auto total = formula::sum(formula::series<Determination, 4>);
    constexpr auto inputFirst = formula::retry<Agreed, 4, formula::FirstJudged::AtSecondAttempt>(
        formula::attempt_input<Determination> + total - total,
        agree,
        formula::Verdict { "repeat the test" },
        { .reference = "Example Standard 12" });
    constexpr auto seriesFirst = formula::retry<Agreed, 4, formula::FirstJudged::AtSecondAttempt>(
        total - total + formula::attempt_input<Determination>,
        agree,
        formula::Verdict { "repeat the test" },
        { .reference = "Example Standard 12" });
    for (formula::Documentation const& page: { formula::document(inputFirst), formula::document(seriesFirst) })
    {
        REQUIRE(page.symbols.size() == 2);
        CHECK(page.symbols[1].symbol == "d");
        CHECK(page.symbols[1].shape == formula::ValueShape::Series);
        CHECK(page.symbols[1].length == 4);
    }
    // A series of another length is another row.
    constexpr auto otherLength = formula::sum(formula::series<Determination, 3>);
    constexpr auto both = formula::retry<Agreed, 4, formula::FirstJudged::AtSecondAttempt>(
        formula::attempt_input<Determination> + otherLength - otherLength,
        agree,
        formula::Verdict { "repeat the test" },
        { .reference = "Example Standard 12" });
    CHECK(formula::document(both).symbols.size() == 3);
}

TEST_CASE("the walk behind attempt_input's refusals finds it however deep", "[retry][recorded]")
{
    // The walk behind the refusal finds it however deep.
    STATIC_REQUIRE(formula::detail::readsAttemptInput<decltype(formula::attempt_input<Determination> * rat(2))>);
    STATIC_REQUIRE(!formula::detail::readsAttemptInput<decltype(formula::previous_attempt<Agreed> * rat(2))>);
    STATIC_REQUIRE(formula::detail::readsAttemptInput<decltype(formula::documented(
                       formula::attempt_input<Determination>, { .reference = "Example Standard 12" }))>);
}

TEST_CASE("an acceptance may take a precision limit's level from this attempt", "[retry][precision]")
{
    // |d_a(k) - d_a(k-1)| <= r, r = 0.347 g + level / 50, the level this
    // attempt's value. Attempt 2: |43.9 - 41.3| = 2.6 > 0.347 + 0.878 g;
    // attempt 3: |42.7 - 43.9| = 1.2 <= 0.347 + 0.854 = 1.201 g, accepted. An
    // unbound level, read as 0, would give 0.347 g and accept nothing.
    constexpr auto limitOfLevel =
        formula::constant<unit::Gram>(rat(347, 1000)) + rat(1, 50) * formula::precision_level<Agreed>;
    constexpr auto withinLimit =
        formula::abs(formula::previous_attempt<Agreed> - formula::this_attempt<Agreed>)
        <= formula::precision_limit<formula::PrecisionKind::Repeatability>(formula::this_attempt<Agreed>, limitOfLevel);
    constexpr auto limited = formula::retry<Agreed, 4, formula::FirstJudged::AtSecondAttempt>(
        formula::attempt_input<Determination>,
        withinLimit,
        formula::Verdict { "repeat the test" },
        { .reference = "Example Standard 12", .section = "7" });
    constexpr auto ran = formula::checked_evaluate_retry(limited, allFour);
    STATIC_REQUIRE(ran.has_value());
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Accepted);
    STATIC_REQUIRE(ran->accepted_at() == std::optional<std::size_t> { 2 });
    STATIC_REQUIRE(ran->outcome().measurement().value() == rat(427, 10));
    // The level is its own value's, and each context node a leaf the level
    // checks see whole.
    STATIC_REQUIRE(formula::detail::LevelChildren<formula::ThisAttemptNode<Agreed>>::seen);
    STATIC_REQUIRE(formula::detail::LevelChildren<formula::PreviousAttemptNode<Agreed>>::seen);
    STATIC_REQUIRE(formula::detail::LevelChildren<formula::AttemptNumberNode>::seen);
    STATIC_REQUIRE(formula::detail::LevelChildren<formula::AttemptInputNode<Determination>>::seen);
    CHECK(formula::render(limited).find("d_a(k)") != std::string::npos);
}

TEST_CASE("the walk behind the starting value's refusal finds every context node, whatever it names", "[retry]")
{
    using formula::detail::readsAttemptContext;
    STATIC_REQUIRE(readsAttemptContext<decltype(formula::previous_attempt<Determination> * rat(2))>);
    STATIC_REQUIRE(readsAttemptContext<decltype(formula::this_attempt<Agreed> + formula::constant<unit::Gram>(rat(1)))>);
    STATIC_REQUIRE(readsAttemptContext<decltype(formula::constant<unit::Gram>(rat(1)) * formula::attempt_number)>);
    STATIC_REQUIRE(readsAttemptContext<decltype(formula::starting_from(formula::previous_attempt<Determination>))>);
    STATIC_REQUIRE(!readsAttemptContext<decltype(formula::starting_from(formula::constant<unit::Gram>(rat(0))))>);
    STATIC_REQUIRE(!readsAttemptContext<decltype(formula::attempt_input<Determination> * rat(2))>);
}

TEST_CASE("asking whether a retry compares or adds is answered, not refused", "[retry]")
{
    // A concept over a retry is an answer: the refused operators name their
    // return type, so their bodies -- the refusal -- are never instantiated.
    using Retried = std::remove_cv_t<decltype(successive)>;
    STATIC_REQUIRE(!std::equality_comparable<Retried>);
    STATIC_REQUIRE(!std::totally_ordered<Retried>);
    STATIC_REQUIRE(!std::equality_comparable_with<Retried, formula::ConstantNode<unit::Gram>>);
    STATIC_REQUIRE(std::is_same_v<decltype(std::declval<Retried>() + std::declval<Retried>()),
                                  formula::detail::RefusedRetryValue<formula::dim::Mass>>);
    STATIC_REQUIRE(
        std::is_same_v<decltype(-std::declval<Retried>()), formula::detail::RefusedRetryValue<formula::dim::Mass>>);
}

namespace
{
/// The step of the fixpoint, 6.08 g, from an environment of a consumer's own
/// that works it out: `checked_get` answers it -- or, when @p Fails, fails
/// with `DomainError` -- and `source_of` says it was calculated. Its `get`
/// answers 1009 g and its `is_entered` says nothing was typed in, so an
/// attempt that read either instead of the hooks would show it.
template <bool Fails>
struct CalculatedStep
{
    template <formula::Described Q>
    static constexpr bool is_entered = false;

    template <formula::Described Q>
    [[nodiscard]] constexpr formula::Measured<Q> get() const noexcept
    {
        return formula::Measured<Q> { rat(1009) };
    }

    template <formula::Described Q>
    [[nodiscard]] constexpr std::expected<formula::Measured<Q>, formula::ArithmeticError> checked_get() const noexcept
    {
        if constexpr (Fails)
            return std::unexpected { formula::ArithmeticError::DomainError };
        else
            return formula::Measured<Q> { rat(152, 25) };
    }

    template <formula::Described Q>
    [[nodiscard]] constexpr formula::ValueSource source_of() const noexcept
    {
        return formula::ValueSource::Derived;
    }
};

/// The same step from an environment with neither hook: `get` and
/// `is_entered` only, which says the step was typed in and the estimate was
/// not.
struct PlainStep
{
    template <formula::Described Q>
    static constexpr bool is_entered = std::is_same_v<Q, StepSize>;

    template <formula::Described Q>
    [[nodiscard]] constexpr formula::Measured<Q> get() const noexcept
    {
        return formula::Measured<Q> { rat(152, 25) };
    }
};

template <typename Env>
concept FailsReads = requires(Env const& asked) { asked.template checked_get<StepSize>(); };

template <typename Env>
concept SaysSource = requires(Env const& asked) { asked.template source_of<StepSize>(); };

// w_k = s + w_{k-1} / 2 from 0 g, with s = 6.08 g read from the environment:
// accepted at 11.4 g, as the halving fixpoint is. s = 1009 g would never
// settle within four attempts.
constexpr auto calculatedStep = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(
    fromZero, formula::var<StepSize> + formula::previous_attempt<Estimate> / rat(2), settled, repeat, cite);
} // namespace

TEST_CASE("the attempt's environment forwards a read's source and its failure when the specimen's has them", "[retry]")
{
    using Forwarding = formula::detail::
        AttemptEnvironment<CalculatedStep<false>, formula::Rational, Estimate, formula::AttemptPhase::Attempting, 4>;
    using Plain =
        formula::detail::AttemptEnvironment<Specimen, formula::Rational, Estimate, formula::AttemptPhase::Attempting, 4>;
    STATIC_REQUIRE(FailsReads<Forwarding>);
    STATIC_REQUIRE(SaysSource<Forwarding>);
    STATIC_REQUIRE_FALSE(FailsReads<Plain>);
    STATIC_REQUIRE(SaysSource<Plain>);

    constexpr auto ran = formula::checked_evaluate_retry(calculatedStep, CalculatedStep<false> {});
    STATIC_REQUIRE(ran->end() == formula::RetryEnd::Accepted);
    STATIC_REQUIRE(ran->outcome().measurement().value() == rat(57, 5));
    // The first attempt's read fails, and so does the retry, there.
    STATIC_REQUIRE(formula::checked_evaluate_retry(calculatedStep, CalculatedStep<true> {}).error()
                   == formula::RetryFailure { formula::ArithmeticError::DomainError, 0 });

    // Every attempt's read of the step says it was calculated.
    auto const explained = formula::explain_retry(calculatedStep, CalculatedStep<false> {});
    std::size_t calculatedReads = 0;
    for (formula::Step<> const& each: explained.trace.steps)
        if (each.kind == formula::StepKind::Variable)
        {
            CHECK(each.inputSource == formula::ValueSource::Derived);
            ++calculatedReads;
        }
    CHECK(calculatedReads == 4);
    CHECK(formula::render_trace(explained.trace, { .maxSteps = 60 }).find("s_w = 152/25 g, calculated\n")
          != std::string::npos);
}

TEST_CASE("the attempt's environment has neither hook when the specimen's has neither", "[retry]")
{
    using Neither =
        formula::detail::AttemptEnvironment<PlainStep, formula::Rational, Estimate, formula::AttemptPhase::Attempting, 4>;
    STATIC_REQUIRE_FALSE(FailsReads<Neither>);
    STATIC_REQUIRE_FALSE(SaysSource<Neither>);

    // It still evaluates and traces, and is_entered decides every read's
    // source: typed in, as the specimen says.
    auto const explained = formula::explain_retry(calculatedStep, PlainStep {});
    REQUIRE(explained.outcome.has_value());
    CHECK(explained.outcome->end() == formula::RetryEnd::Accepted);
    std::size_t typedReads = 0;
    for (formula::Step<> const& each: explained.trace.steps)
        if (each.kind == formula::StepKind::Variable)
        {
            CHECK(each.inputSource == formula::ValueSource::ManuallyEntered);
            ++typedReads;
        }
    CHECK(typedReads == 4);
    CHECK(formula::render_trace(explained.trace, { .maxSteps = 60 }).find("s_w = 152/25 g, entered by hand\n")
          != std::string::npos);
}

TEST_CASE("number_of a retry is the accepted value, and nothing for its verdict", "[retry]")
{
    constexpr auto four =
        formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, settled, repeat, cite);
    constexpr auto three =
        formula::retry<Estimate, 3, formula::FirstJudged::AtFirstAttempt>(fromZero, halving, settled, repeat, cite);
    constexpr auto accepted = formula::checked_evaluate_retry(four, nothing);
    constexpr auto exhausted = formula::checked_evaluate_retry(three, nothing);
    // The accepted 11.4 g, not the exhausted run's last value, 10.64 g.
    STATIC_REQUIRE(formula::number_of(*accepted) == rat(57, 5));
    STATIC_REQUIRE(formula::number_of(accepted) == rat(57, 5));
    STATIC_REQUIRE(!formula::number_of(*exhausted).has_value());
}

TEST_CASE("retry ends describe themselves in lowercase words", "[retry]")
{
    STATIC_REQUIRE(formula::describe(formula::RetryEnd::Accepted) == "accepted");
    STATIC_REQUIRE(formula::describe(formula::RetryEnd::Exhausted) == "exhausted");
    STATIC_REQUIRE(formula::describe(formula::RetryEnd::NotJudgeable) == "not judgeable");
    STATIC_REQUIRE(formula::describe(formula::RetryEnd::NotRecorded) == "not recorded");
    STATIC_REQUIRE(formula::describe(formula::RetryEnd::Failed) == "failed");
    STATIC_REQUIRE(formula::describe(formula::RetryEnd::ManuallyEntered) == "manually entered");
}
