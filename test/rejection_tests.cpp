// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::Rational;
using formula::var;

[[nodiscard]] constexpr Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return Rational { numerator, denominator };
}

/// A determination, in grams.
struct Mass: formula::Quantity<Mass, "m", "mass of a determination", unit::Gram>
{
};
/// How many determinations: a bare number.
struct Determinations: formula::Quantity<Determinations, "n", "number of determinations", unit::One>
{
};
/// A relative tolerance a jurisdiction may tighten.
struct Tolerance: formula::Quantity<Tolerance, "t", "relative tolerance", unit::One>
{
};

[[nodiscard]] constexpr formula::Measured<Mass> grams(Rational value)
{
    return formula::Measured<Mass> { value };
}

template <typename... Values>
[[nodiscard]] constexpr auto sampleOf(Values... values)
{
    return formula::environment(formula::measured_series<Mass>(grams(values)...));
}

// The shared fixtures (the plan's "The shared fixtures"), invented; every
// number below was checked with Python's fractions against a mirror of the
// loop.
// A, "re-running matters": 40.2, 39.8, 40.5, 44.0, 40.0, 43.3 g.
inline constexpr auto fixtureA = sampleOf(rat(402, 10), rat(398, 10), rat(405, 10), rat(44), rat(40), rat(433, 10));
// B, "policy matters": 40.2, 39.8, 40.5, 45.2, 40.0, 37.2 g.
inline constexpr auto fixtureB = sampleOf(rat(402, 10), rat(398, 10), rat(405, 10), rat(452, 10), rat(40), rat(372, 10));
// C, "tie": 40, 40, 44, 40, 36 g, and the same reversed.
inline constexpr auto fixtureC = sampleOf(rat(40), rat(40), rat(44), rat(40), rat(36));
inline constexpr auto fixtureCReversed = sampleOf(rat(36), rat(40), rat(44), rat(40), rat(40));
// D, "on the limit": 40, 40, 40, 40, 42.5 g.
inline constexpr auto fixtureD = sampleOf(rat(40), rat(40), rat(40), rat(40), rat(425, 10));
// E, "all equal": 40 g five times.
inline constexpr auto fixtureE = sampleOf(rat(40), rat(40), rat(40), rat(40), rat(40));

inline constexpr formula::Verdict repeatTest { "discard the determinations and repeat the test" };
inline constexpr formula::Citation exampleCited { .title = "Example Standard", .section = "7.4" };
/// Where a jurisdiction states its tighter tolerance.
inline constexpr formula::Citation jurisdictionCited { .reference = "Example Standard 1:2020 NA", .section = "NA.2" };

/// A 6 % relative deviation from the pass's mean.
inline constexpr auto sixPercent = formula::deviation_from_mean(rat(6, 100) * formula::pass_mean<Mass>);
/// 7/4 sample standard deviations.
inline constexpr auto sevenQuarters = formula::deviation_in_stddevs(formula::number(rat(7, 4)));

template <formula::PerPass P, formula::OnLimit L, std::size_t K, std::size_t M, std::size_t N, typename Criterion>
[[nodiscard]] constexpr auto rejectionOf(Criterion criterion)
{
    return formula::without_outliers<P, L, formula::AtMost<K>, formula::KeepAtLeast<M>>(
        formula::series<Mass, N>, criterion, repeatTest, exampleCited);
}

constexpr auto MostExtreme = formula::PerPass::MostExtreme;
constexpr auto EveryExceeding = formula::PerPass::EveryExceeding;
constexpr auto Keep = formula::OnLimit::Keep;
constexpr auto Reject = formula::OnLimit::Reject;

inline constexpr auto rejectionA = rejectionOf<MostExtreme, Keep, 2, 4, 6>(sixPercent);
inline constexpr auto rejectionA1 = rejectionOf<MostExtreme, Keep, 1, 4, 6>(sixPercent);
} // namespace

TEST_CASE("rejection re-runs the aggregate, and a value inside at first can be rejected later (fixture A)", "[rejection]")
{
    // Pass 1 (mean 41.3 g, limit 1239/500 g = 2.478 g) rejects element 3
    // (deviation 2.7 g); element 5 is inside (2.0 g). Pass 2 (mean 40.76 g,
    // limit 2.4456 g) rejects element 5 (2.54 g). Pass 3 (mean 321/8 g)
    // settles. A single-pass rejection settles at 40.76 g with element 3
    // alone removed. (A limit evaluated once, at the pass-1 mean, is 2.478 g
    // and still rejects element 5 here: fixture A' below is the one that
    // tells the two apart.)
    constexpr auto out = formula::checked_evaluate_rejection<Mass>(rejectionA, fixtureA);
    STATIC_REQUIRE(out->outcome().measurement().value() == rat(321, 8));
    STATIC_REQUIRE(out->outcome().source() == formula::ValueSource::Derived);
    STATIC_REQUIRE(out->rejected().size() == 2);
    STATIC_REQUIRE(out->rejected()[0] == formula::RejectedElement { 3, 1 });
    STATIC_REQUIRE(out->rejected()[1] == formula::RejectedElement { 5, 2 });
    STATIC_REQUIRE(out->passes() == 3);
    STATIC_REQUIRE(out->survivors().size() == 4);
    STATIC_REQUIRE(out->survivors()[0] == 0);
    STATIC_REQUIRE(out->survivors()[3] == 4);
    // The survivors feed every statistic: their mean is the outcome's, their
    // count 4.
    STATIC_REQUIRE(formula::checked_evaluate<Mass>(formula::sample_mean(rejectionA), fixtureA)->measurement().value()
                   == rat(321, 8));
    STATIC_REQUIRE(
        formula::checked_evaluate<Determinations>(formula::sample_count(rejectionA), fixtureA)->measurement().value()
        == rat(4));
}

TEST_CASE("the limit is evaluated again at each pass's mean (fixture A')", "[rejection]")
{
    // Fixture A with 43.2 g for 43.3 g. Pass 1 (mean 2477/60 g, limit
    // 2.477 g) rejects element 3 (2.7167 g). Pass 2's mean is 40.74 g and
    // element 5 deviates 2.46 g: past pass 2's own limit, 2.4444 g, so it
    // goes and pass 3 settles at 321/8 g. A limit evaluated once, at the
    // pass-1 mean, keeps it (2.46 g < 2.477 g) and settles at 2037/50 g.
    constexpr auto fixtureAPrime = sampleOf(rat(402, 10), rat(398, 10), rat(405, 10), rat(44), rat(40), rat(432, 10));
    constexpr auto out = formula::checked_evaluate_rejection<Mass>(rejectionA, fixtureAPrime);
    STATIC_REQUIRE(out->outcome().measurement().value() == rat(321, 8));
    STATIC_REQUIRE(out->rejected().size() == 2);
    STATIC_REQUIRE(out->rejected()[1] == formula::RejectedElement { 5, 2 });
}

TEST_CASE("the bound turns one rejection too many into the author's verdict, never a value", "[rejection]")
{
    // Fixture A under AtMost<1>: pass 2 would reject element 5 as the second
    // rejection. Aborted: the verdict, element 3 rejected, and anything
    // reduced from the rejection a domain error.
    constexpr auto out = formula::checked_evaluate_rejection<Mass>(rejectionA1, fixtureA);
    STATIC_REQUIRE(out->outcome().is_verdict());
    STATIC_REQUIRE(out->outcome().verdict_label() == repeatTest.label);
    STATIC_REQUIRE(out->rejected().size() == 1);
    STATIC_REQUIRE(out->rejected()[0] == formula::RejectedElement { 3, 1 });
    STATIC_REQUIRE(out->passes() == 2);
    STATIC_REQUIRE(formula::checked_evaluate<Mass>(formula::sample_mean(rejectionA1), fixtureA).error()
                   == formula::ArithmeticError::DomainError);
}

TEST_CASE("most-extreme and every-exceeding differ, and so does the bound (fixture B)", "[rejection]")
{
    // Pass 1: mean 2429/60 g, limit 2.429 g; elements 3 (4.7167 g) and 5
    // (3.2833 g) both exceed.
    // MostExtreme, AtMost<1>: rejects 3; pass 2 (mean 39.54 g, limit
    // 2.3724 g) finds element 5 inside (2.34 g) and settles at 1977/50 g.
    constexpr auto most =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 1, 4, 6>(sixPercent), fixtureB);
    STATIC_REQUIRE(most->outcome().measurement().value() == rat(1977, 50));
    STATIC_REQUIRE(most->rejected().size() == 1);
    STATIC_REQUIRE(most->rejected()[0] == formula::RejectedElement { 3, 1 });
    // EveryExceeding, AtMost<1>: two in pass 1 is one too many -- aborted,
    // nothing rejected.
    constexpr auto everyOne =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<EveryExceeding, Keep, 1, 4, 6>(sixPercent), fixtureB);
    STATIC_REQUIRE(everyOne->outcome().is_verdict());
    STATIC_REQUIRE(everyOne->rejected().empty());
    STATIC_REQUIRE(everyOne->passes() == 1);
    // EveryExceeding, AtMost<2>: both in pass 1, settling at 321/8 g.
    constexpr auto everyTwo =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<EveryExceeding, Keep, 2, 4, 6>(sixPercent), fixtureB);
    STATIC_REQUIRE(everyTwo->outcome().measurement().value() == rat(321, 8));
    STATIC_REQUIRE(everyTwo->rejected().size() == 2);
    STATIC_REQUIRE(everyTwo->rejected()[0] == formula::RejectedElement { 3, 1 });
    STATIC_REQUIRE(everyTwo->rejected()[1] == formula::RejectedElement { 5, 1 });
    STATIC_REQUIRE(everyTwo->passes() == 2);
}

TEST_CASE("deviation in standard deviations decides exactly, by squares (fixture B)", "[rejection]")
{
    // deviation_in_stddevs(7/4), decided as (x - mean)^2 against
    // (7/4)^2 * s^2 = 49/16 * s^2, the candidate inside s^2 (n - 1).
    //  - pass 1: element 3's z^2 = 80089/24342 = 3.290 > 3.0625: rejected.
    //  - pass 2 (mean 1977/50, s^2 = 889/500): element 5's z^2 =
    //    13689/4445 = 3.0796 > 3.0625, by a hair: rejected.
    //  - pass 3 (n = 4, s^2 = 107/1200): max z^2 = 675/428 = 1.577: settles.
    // AtMost<2> gives 321/8 g, {3, 5}; AtMost<1> aborts in pass 2 on 5.
    // The statistic compared unsquared (z = 1.7549 against 49/16) finds
    // element 5 inside in pass 2 and settles at 1977/50 g. (A population
    // variance, or one without the candidate, raises z^2 -- 3.85 and more
    // -- and does not flip this decision; the brief's claim that it would
    // is not so.)
    constexpr auto two =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 4, 6>(sevenQuarters), fixtureB);
    STATIC_REQUIRE(two->outcome().measurement().value() == rat(321, 8));
    STATIC_REQUIRE(two->rejected().size() == 2);
    STATIC_REQUIRE(two->rejected()[1] == formula::RejectedElement { 5, 2 });
    constexpr auto one =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 1, 4, 6>(sevenQuarters), fixtureB);
    STATIC_REQUIRE(one->outcome().is_verdict());
    STATIC_REQUIRE(one->passes() == 2);
}

TEST_CASE("two equally extreme values are rejected together, whatever order they were entered in (fixture C)", "[rejection]")
{
    // Mean 40 g, limit 2.4 g: 44 g and 36 g are both 4 g away. Both go in
    // pass 1 and it settles at 40 g -- in either order of entry, the same
    // values at mirrored positions. AtMost<1> aborts on both.
    constexpr auto both = rejectionOf<MostExtreme, Keep, 2, 3, 5>(sixPercent);
    constexpr auto forward = formula::checked_evaluate_rejection<Mass>(both, fixtureC);
    constexpr auto reversed = formula::checked_evaluate_rejection<Mass>(both, fixtureCReversed);
    STATIC_REQUIRE(forward->outcome().measurement().value() == rat(40));
    STATIC_REQUIRE(reversed->outcome().measurement().value() == rat(40));
    STATIC_REQUIRE(forward->rejected().size() == 2);
    STATIC_REQUIRE(forward->rejected()[0] == formula::RejectedElement { 2, 1 });
    STATIC_REQUIRE(forward->rejected()[1] == formula::RejectedElement { 4, 1 });
    STATIC_REQUIRE(reversed->rejected()[0] == formula::RejectedElement { 0, 1 });
    STATIC_REQUIRE(reversed->rejected()[1] == formula::RejectedElement { 2, 1 });

    constexpr auto oneOnly = rejectionOf<MostExtreme, Keep, 1, 3, 5>(sixPercent);
    STATIC_REQUIRE(formula::checked_evaluate_rejection<Mass>(oneOnly, fixtureC)->outcome().is_verdict());
    STATIC_REQUIRE(formula::checked_evaluate_rejection<Mass>(oneOnly, fixtureCReversed)->outcome().is_verdict());
}

TEST_CASE("an element exactly on the limit is kept or rejected as declared (fixture D)", "[rejection]")
{
    // Mean 40.5 g; element 4 is exactly 2 g away, on a 2 g limit.
    constexpr auto twoGrams = formula::deviation_from_mean(formula::constant<unit::Gram>(rat(2)));
    constexpr auto kept =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 3, 5>(twoGrams), fixtureD);
    STATIC_REQUIRE(kept->outcome().measurement().value() == rat(81, 2));
    STATIC_REQUIRE(kept->rejected().empty());
    constexpr auto rejected =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Reject, 2, 3, 5>(twoGrams), fixtureD);
    STATIC_REQUIRE(rejected->outcome().measurement().value() == rat(40));
    STATIC_REQUIRE(rejected->rejected().size() == 1);
    STATIC_REQUIRE(rejected->rejected()[0] == formula::RejectedElement { 4, 1 });
}

TEST_CASE("a sample of equal values has no outlier and settles in one pass (fixture E)", "[rejection]")
{
    // Never a division by zero, and never every element "on" a zero limit:
    // under deviation_in_stddevs with OnLimit::Reject, 0 >= 0 would reject
    // every determination.
    constexpr auto relative =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 3, 5>(sixPercent), fixtureE);
    STATIC_REQUIRE(relative->outcome().measurement().value() == rat(40));
    STATIC_REQUIRE(relative->passes() == 1);
    STATIC_REQUIRE(relative->rejected().empty());
    constexpr auto inStddevs =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Reject, 2, 3, 5>(sevenQuarters), fixtureE);
    STATIC_REQUIRE(inStddevs->outcome().measurement().value() == rat(40));
    STATIC_REQUIRE(inStddevs->rejected().empty());
}

TEST_CASE("a rejection that would leave fewer than KeepAtLeast aborts", "[rejection]")
{
    // Fixture A with KeepAtLeast<5>: pass 2 would leave 4 of at least 5.
    constexpr auto out =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 2, 5, 6>(sixPercent), fixtureA);
    STATIC_REQUIRE(out->outcome().is_verdict());
    STATIC_REQUIRE(out->rejected().size() == 1);
    STATIC_REQUIRE(out->passes() == 2);
}

TEST_CASE("deviation in standard deviations on a two-value pass is a domain error, not a tie that empties the sample",
          "[rejection]")
{
    constexpr auto pair = sampleOf(rat(40), rat(44));
    constexpr auto out =
        formula::checked_evaluate_rejection<Mass>(rejectionOf<MostExtreme, Keep, 1, 1, 2>(sevenQuarters), pair);
    STATIC_REQUIRE(!out.has_value());
    STATIC_REQUIRE(out.error().error == formula::ArithmeticError::DomainError);
}

TEST_CASE("an absent determination runs no pass and yields an empty outcome", "[rejection]")
{
    constexpr auto missing = formula::environment(formula::measured_series<Mass>(grams(rat(402, 10)),
                                                                                 grams(rat(398, 10)),
                                                                                 formula::Measured<Mass>::absent(),
                                                                                 grams(rat(44)),
                                                                                 grams(rat(40)),
                                                                                 grams(rat(433, 10))));
    constexpr auto out = formula::checked_evaluate_rejection<Mass>(rejectionA, missing);
    STATIC_REQUIRE(out->outcome().is_empty());
    STATIC_REQUIRE(out->passes() == 0);
    STATIC_REQUIRE(out->rejected().empty());
    STATIC_REQUIRE(formula::checked_evaluate<Mass>(formula::sample_mean(rejectionA), missing)->is_empty());

    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Mass>(formula::sample_mean(rejectionA), missing, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
          == "1. m = 201/5 g; 199/5 g; (not measured); 44 g; 40 g; 433/10 g\n"
             "2. sample_mean(#1) = (not measured)\n");
}

TEST_CASE("pass n reads the current pass's size", "[rejection]")
{
    // A limit of 7/24 * pass n standard deviations: 7/4 at n = 6, and the
    // trace names each pass's n as it was read.
    constexpr auto scaled = rejectionOf<MostExtreme, Keep, 1, 4, 6>(
        formula::deviation_in_stddevs(formula::number(rat(7, 24)) * formula::pass_count));
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_rejection<Mass>(scaled, fixtureB, formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
             "2. 7/24\n"
             "3. pass n = 6\n"
             "4. #2 * #3 = 7/4\n"
             "5. pass 1: 6 values, mean 2429/60 g\n"
             "6. rejected element 4 of 6 (226/5 g) in pass 1: (x - mean)^2 = 80089/3600 g2 > limit^2 * s^2 = 198793/9600 g2 "
             "(deviation in standard deviations)\n"
             "7. 7/24\n"
             "8. pass n = 5\n"
             "9. #7 * #8 = 35/24\n"
             "10. pass 2: 5 values, mean 1977/50 g\n"
             "11. element 6 of 6 would be rejection 2 of at most 1: discard the determinations and repeat the test [Example "
             "Standard, 7.4]\n");
}

TEST_CASE("only the library builds a RejectionOutcome", "[rejection]")
{
    using Built = formula::RejectionOutcome<Mass, 6>;
    STATIC_REQUIRE_FALSE(std::is_default_constructible_v<Built>);
    STATIC_REQUIRE_FALSE(std::is_constructible_v<Built, formula::Outcome<Mass>>);
    STATIC_REQUIRE(std::is_copy_constructible_v<Built>);
}

TEST_CASE("every rejection is a step naming the value, the statistic, the limit and the criterion (T10)",
          "[rejection][trace-render]")
{
    formula::Trace<> settled {};
    (void) formula::checked_evaluate_rejection<Mass>(rejectionA, fixtureA, formula::RecordingSink<> { settled });
    CHECK(
        formula::render_trace(settled, { .maxSteps = 40 })
        == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
           "2. 3/50\n"
           "3. pass mean = 413/10 g\n"
           "4. #2 * #3 = 1239/500000\n"
           "5. pass 1: 6 values, mean 413/10 g\n"
           "6. rejected element 4 of 6 (44 g) in pass 1: abs(x - mean) = 27/10 g > 1239/500 g (deviation from mean)\n"
           "7. 3/50\n"
           "8. pass mean = 1019/25 g\n"
           "9. #7 * #8 = 3057/1250000\n"
           "10. pass 2: 5 values, mean 1019/25 g\n"
           "11. rejected element 6 of 6 (433/10 g) in pass 2: abs(x - mean) = 127/50 g > 3057/1250 g (deviation from mean)\n"
           "12. 3/50\n"
           "13. pass mean = 321/8 g\n"
           "14. #12 * #13 = 963/400000\n"
           "15. pass 3: 4 values, mean 321/8 g\n"
           "16. settled: 2 rejected, 4 remain\n");

    formula::Trace<> aborted {};
    (void) formula::checked_evaluate_rejection<Mass>(rejectionA1, fixtureA, formula::RecordingSink<> { aborted });
    CHECK(formula::render_trace(aborted, { .maxSteps = 40 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
             "2. 3/50\n"
             "3. pass mean = 413/10 g\n"
             "4. #2 * #3 = 1239/500000\n"
             "5. pass 1: 6 values, mean 413/10 g\n"
             "6. rejected element 4 of 6 (44 g) in pass 1: abs(x - mean) = 27/10 g > 1239/500 g (deviation from mean)\n"
             "7. 3/50\n"
             "8. pass mean = 1019/25 g\n"
             "9. #7 * #8 = 3057/1250000\n"
             "10. pass 2: 5 values, mean 1019/25 g\n"
             "11. element 6 of 6 would be rejection 2 of at most 1: discard the determinations and repeat the test [Example "
             "Standard, 7.4]\n");

    // A budget of 4 truncates, and says how many steps it left out.
    CHECK(formula::render_trace(settled, { .maxSteps = 4 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; ... 3 more\n"
             "... 15 further steps not shown\n");

    // Every exceeding one at once, aborted: both named.
    formula::Trace<> both {};
    (void) formula::checked_evaluate_rejection<Mass>(
        rejectionOf<EveryExceeding, Keep, 1, 4, 6>(sixPercent), fixtureB, formula::RecordingSink<> { both });
    CHECK(formula::render_trace(both, { .maxSteps = 40 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
             "2. 3/50\n"
             "3. pass mean = 2429/60 g\n"
             "4. #2 * #3 = 2429/1000000\n"
             "5. pass 1: 6 values, mean 2429/60 g\n"
             "6. elements 4 and 6 of 6 would be rejections 1 and 2 of at most 1: discard the determinations and repeat the "
             "test [Example Standard, 7.4]\n");

    // In standard deviations, the exact comparison by squares.
    formula::Trace<> squares {};
    (void) formula::checked_evaluate_rejection<Mass>(
        rejectionOf<MostExtreme, Keep, 2, 4, 6>(sevenQuarters), fixtureB, formula::RecordingSink<> { squares });
    CHECK(formula::render_trace(squares, { .maxSteps = 40 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
             "2. 7/4\n"
             "3. pass 1: 6 values, mean 2429/60 g\n"
             "4. rejected element 4 of 6 (226/5 g) in pass 1: (x - mean)^2 = 80089/3600 g2 > limit^2 * s^2 = 198793/9600 g2 "
             "(deviation in standard deviations)\n"
             "5. 7/4\n"
             "6. pass 2: 5 values, mean 1977/50 g\n"
             "7. rejected element 6 of 6 (186/5 g) in pass 2: (x - mean)^2 = 13689/2500 g2 > limit^2 * s^2 = 43561/8000 g2 "
             "(deviation in standard deviations)\n"
             "8. 7/4\n"
             "9. pass 3: 4 values, mean 321/8 g\n"
             "10. settled: 2 rejected, 4 remain\n");

    // Below KeepAtLeast.
    formula::Trace<> kept {};
    (void) formula::checked_evaluate_rejection<Mass>(
        rejectionOf<MostExtreme, Keep, 2, 5, 6>(sixPercent), fixtureA, formula::RecordingSink<> { kept });
    CHECK(formula::render_trace(kept, { .maxSteps = 40 })
          == "1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g\n"
             "2. 3/50\n"
             "3. pass mean = 413/10 g\n"
             "4. #2 * #3 = 1239/500000\n"
             "5. pass 1: 6 values, mean 413/10 g\n"
             "6. rejected element 4 of 6 (44 g) in pass 1: abs(x - mean) = 27/10 g > 1239/500 g (deviation from mean)\n"
             "7. 3/50\n"
             "8. pass mean = 1019/25 g\n"
             "9. #7 * #8 = 3057/1250000\n"
             "10. pass 2: 5 values, mean 1019/25 g\n"
             "11. element 6 of 6 would leave 4 of at least 5: discard the determinations and repeat the test [Example "
             "Standard, 7.4]\n");
}

TEST_CASE("an aborted rejection's verdict and citation are escaped as a step's own author text", "[rejection][trace-render]")
{
    // A verdict and a citation holding the characters a trace line is built
    // from: each is escaped, so neither can close the line's clause early or
    // open a clause of its own.
    constexpr auto bracketed = formula::without_outliers<MostExtreme, Keep, formula::AtMost<1>, formula::KeepAtLeast<4>>(
        formula::series<Mass, 6>,
        sixPercent,
        formula::Verdict { "discard; repeat [now]" },
        formula::Citation { .title = "Example Standard [draft]", .section = "7.4" });
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_rejection<Mass>(bracketed, fixtureA, formula::RecordingSink<> { trace });
    std::string const rendered = formula::render_trace(trace, { .maxSteps = 40 });
    CHECK(rendered.ends_with("11. element 6 of 6 would be rejection 2 of at most 1: discard\\; repeat \\[now\\] "
                             "[Example Standard \\[draft\\], 7.4]\n"));
}

TEST_CASE("a rejection renders with every parameter stated, and never a bar outside LaTeX", "[rejection][render]")
{
    CHECK(formula::render(rejectionA)
          == "without outliers(m(i); abs(x - pass mean) > 3/50 * pass mean; most extreme per pass; keep on limit; at most "
             "2; keep at least 4)");
    std::string const markdown = formula::render<formula::Dialect::Markdown>(rejectionA);
    CHECK(markdown
          == "without outliers(`m(i)`; abs(x - pass mean) > 3/50 * pass mean; most extreme per pass; keep on limit; at most "
             "2; keep at least 4)");
    CHECK(markdown.find('|') == std::string::npos);
    CHECK(markdown.find('[') == std::string::npos);
    CHECK(formula::render<formula::Dialect::LaTeX>(rejectionA)
          == "\\operatorname{without\\ outliers}({m}_{i};\\allowbreak \\left|x - \\bar{x}_{\\text{pass}}\\right| > "
             "3/50 \\cdot \\bar{x}_{\\text{pass}};\\allowbreak \\text{most extreme per pass};\\allowbreak \\text{keep on "
             "limit};\\allowbreak \\text{at most }2;\\allowbreak \\text{keep at least }4)");
    CHECK(formula::render(rejectionOf<EveryExceeding, Reject, 1, 3, 6>(sevenQuarters))
          == "without outliers(m(i); abs(x - pass mean) / s >= 7/4; every exceeding per pass; reject on limit; at most 1; "
             "keep at least 3)");
    CHECK(formula::render<formula::Dialect::LaTeX>(rejectionOf<EveryExceeding, Reject, 1, 3, 6>(sevenQuarters))
          == "\\operatorname{without\\ outliers}({m}_{i};\\allowbreak \\frac{\\left|x - \\bar{x}_{\\text{pass}}\\right|}"
             "{s} \\geq 7/4;\\allowbreak \\text{every exceeding per pass};\\allowbreak \\text{reject on limit};"
             "\\allowbreak \\text{at most }1;\\allowbreak \\text{keep at least }3)");
    // The pass placeholders, as a limit expression reads them.
    CHECK(formula::render(formula::number(rat(7, 24)) * formula::pass_count) == "7/24 * pass n");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::number(rat(7, 24)) * formula::pass_count)
          == "7/24 \\cdot n_{\\text{pass}}");
    CHECK(formula::render(formula::sample_mean(rejectionA))
          == "sample_mean(without outliers(m(i); abs(x - pass mean) > 3/50 * pass mean; most extreme per pass; keep on "
             "limit; at most 2; keep at least 4))");
}

TEST_CASE("a rejection's page states its criterion, limit, bounds, verdict and citation", "[rejection][document]")
{
    formula::Documentation const page = formula::document(formula::sample_mean(rejectionA));
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "m");
    CHECK(page.symbols[0].shape == formula::ValueShape::Series);
    REQUIRE(page.rejections.size() == 1);
    formula::RejectionEntry const& entry = page.rejections[0];
    CHECK(entry.criterion == formula::CriterionKind::DeviationFromMean);
    CHECK(entry.limit == "3/50 * pass mean");
    CHECK(entry.perPass == formula::PerPass::MostExtreme);
    CHECK(entry.onLimit == formula::OnLimit::Keep);
    CHECK(entry.atMost == 2);
    CHECK(entry.keepAtLeast == 4);
    CHECK(entry.verdict == repeatTest);
    CHECK(entry.citation.section == "7.4");
    REQUIRE(page.citations.size() == 1);
    CHECK(page.citations[0].title == "Example Standard");
}

namespace
{
struct MeanOfSurvivors
{
};

/// A method reporting the mean of fixture-shaped determinations after a
/// rejection at a jurisdiction-set relative tolerance.
inline constexpr auto survivorsMethod = formula::method(
    formula::variants(formula::variant<MeanOfSurvivors>(formula::sample_mean(
        rejectionOf<MostExtreme, Keep, 2, 4, 6>(formula::deviation_from_mean(var<Tolerance> * formula::pass_mean<Mass>))))),
    formula::rounding_rule<unit::Gram, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());

inline constexpr auto abortingMethod = formula::method(
    formula::variants(formula::variant<MeanOfSurvivors>(formula::sample_mean(rejectionA1))),
    formula::rounding_rule<unit::Gram, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
} // namespace

TEST_CASE("a rejection joins a method: evaluated, overlaid, rendered, documented and traced", "[rejection][method]")
{
    // Fixture B at 6 %: pass 2 finds element 5 inside (2.34 g < 2.3724 g),
    // so 1977/50 g = 39.54 g. At a jurisdiction's tighter 3 %, pass 2's limit
    // is 1.1862 g, element 5 goes too, and pass 3 settles at 321/8 g =
    // 40.125 g.
    constexpr auto inputs = formula::environment(formula::measured_series<Mass>(grams(rat(402, 10)),
                                                                                grams(rat(398, 10)),
                                                                                grams(rat(405, 10)),
                                                                                grams(rat(452, 10)),
                                                                                grams(rat(40)),
                                                                                grams(rat(372, 10))),
                                                 formula::Measured<Tolerance> { rat(6, 100) });
    STATIC_REQUIRE(**formula::evaluate_method<MeanOfSurvivors>(survivorsMethod, inputs) == rat(3954, 100000));
    constexpr auto tighter =
        formula::apply(formula::overlay(formula::with_constant<Tolerance>(rat(3, 100), jurisdictionCited)), survivorsMethod);
    STATIC_REQUIRE(**formula::evaluate_method<MeanOfSurvivors>(tighter, inputs) == rat(40125, 1000000));

    constexpr auto jurisdiction = formula::vocabulary(formula::renames<Mass>("x_m"));
    constexpr auto variantExpression = std::get<0>(tighter.variantSet.cases).expression;
    CHECK(formula::render(variantExpression, jurisdiction)
          == "sample_mean(without outliers(x_m(i); abs(x - pass mean) > t * pass mean; most extreme per pass; keep on "
             "limit; at most 2; keep at least 4))");
    formula::Documentation const page = formula::document(variantExpression, jurisdiction);
    CHECK(page.symbols.size() == 2);
    REQUIRE(page.rejections.size() == 1);
    CHECK(page.rejections[0].limit == "t * pass mean");

    formula::Trace<> trace {};
    (void) formula::evaluate_method<MeanOfSurvivors>(tighter, inputs, formula::RecordingSink { trace, jurisdiction });
    CHECK(
        formula::render_trace(trace, { .maxSteps = 60 })
        == "1. x_m = 201/5 g; 199/5 g; 81/2 g; 226/5 g; 40 g; 186/5 g\n"
           "2. t = 3/100 [fixed by jurisdiction overlay: Example Standard 1:2020 NA, NA.2]\n"
           "3. pass mean = 2429/60 g\n"
           "4. #2 * #3 = 2429/2000000\n"
           "5. pass 1: 6 values, mean 2429/60 g\n"
           "6. rejected element 4 of 6 (226/5 g) in pass 1: abs(x - mean) = 283/60 g > 2429/2000 g (deviation from mean)\n"
           "7. t = 3/100 [fixed by jurisdiction overlay: Example Standard 1:2020 NA, NA.2]\n"
           "8. pass mean = 1977/50 g\n"
           "9. #7 * #8 = 5931/5000000\n"
           "10. pass 2: 5 values, mean 1977/50 g\n"
           "11. rejected element 6 of 6 (186/5 g) in pass 2: abs(x - mean) = 117/50 g > 5931/5000 g (deviation from mean)\n"
           "12. t = 3/100 [fixed by jurisdiction overlay: Example Standard 1:2020 NA, NA.2]\n"
           "13. pass mean = 321/8 g\n"
           "14. #12 * #13 = 963/800000\n"
           "15. pass 3: 4 values, mean 321/8 g\n"
           "16. settled: 2 rejected, 4 remain\n"
           "17. sample_mean(#16) = 321/8 g\n"
           "18. round(#17, in g) = 321/8 g [rounded to 3 dp (method default); nearest, ties away from zero]\n"
           "19. #18 = 321/8 g [variant MeanOfSurvivors (1st of 1), selected by tag]\n");

    // An aborted rejection inside a method: DomainError, and the trace names
    // the verdict.
    STATIC_REQUIRE(formula::evaluate_method<MeanOfSurvivors>(abortingMethod, fixtureA).error()
                   == formula::ArithmeticError::DomainError);
    formula::Trace<> abortedTrace {};
    (void) formula::evaluate_method<MeanOfSurvivors>(abortingMethod, fixtureA, formula::RecordingSink<> { abortedTrace });
    CHECK(formula::render_trace(abortedTrace, { .maxSteps = 60 })
              .find("would be rejection 2 of at most 1: discard the "
                    "determinations and repeat the test")
          != std::string::npos);
}
