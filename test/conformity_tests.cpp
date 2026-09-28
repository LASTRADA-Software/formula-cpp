// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/conformity.hpp>
#include <formula-cpp/document.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
namespace unit = formula::unit;
using formula::CumulativeDirection;
using formula::limit;
using formula::LimitRow;
using formula::unbounded;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// The shared fixture's quantities (see the phase 12 plan): an invented screen
// analysis, and the percentage passing each screen.
struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", unit::Gram>
{
};
struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", unit::Gram>
{
};
struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", unit::Percent>
{
};
struct Opening: formula::Quantity<Opening, "w", "screen opening", unit::Millimetre>
{
};

template <typename Q>
constexpr formula::Measured<Q> m(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Measured<Q> { rat(numerator, denominator) };
}

// 130, 210, 95, 340 and 28 g retained of 1250 g: passing 35.76, 46.16, 62.96,
// 70.56 and 97.76 %.
constexpr auto screens =
    formula::environment(formula::measured_series<Retained>(
                             m<Retained>(130), m<Retained>(210), m<Retained>(95), m<Retained>(340), m<Retained>(28)),
                         m<TotalMass>(1250));

constexpr auto passingOf(auto retained)
{
    return formula::constant<unit::Percent>(rat(100))
           - formula::cumulative<CumulativeDirection::FromLast>(retained) / formula::var<TotalMass>;
}

// The same percentages measured directly, and with the middle one unmeasured.
constexpr auto measuredPassing = formula::environment(formula::measured_series<Passing>(
    m<Passing>(894, 25), m<Passing>(1154, 25), m<Passing>(1574, 25), m<Passing>(1764, 25), m<Passing>(2444, 25)));
constexpr auto middleUnmeasured =
    formula::environment(formula::measured_series<Passing>(m<Passing>(894, 25),
                                                           m<Passing>(1154, 25),
                                                           formula::Measured<Passing>::absent(),
                                                           m<Passing>(1764, 25),
                                                           m<Passing>(2444, 25)));

constexpr formula::Verdict reject { "reject the specimen" };

// Invented numbers, one row per screen, in percent. Row 1 is violated below
// its lower limit and row 4 above its upper one -- one violation in the
// middle and one at the end, so neither an upper-only nor a lower-only check
// passes. Element 2 sits exactly on row 2's lower limit, 62.96 %, and row 2
// has no upper limit: a half-open lower, or `unbounded` read as 0, violates
// it. Row 3 is a point, 70.56 % to 70.56 %, with element 3 on it: a half-open
// bound on either side violates it.
constexpr formula::Envelope<5> envelope { LimitRow { limit(rat(30)), limit(rat(40)) },
                                          LimitRow { limit(rat(50)), limit(rat(60)) },
                                          LimitRow { limit(rat(1574, 25)), unbounded },
                                          LimitRow { limit(rat(1764, 25)), limit(rat(1764, 25)) },
                                          LimitRow { limit(rat(0)), limit(rat(95)) } };

constexpr auto passingCheck = formula::conformity<unit::Percent>(passingOf(formula::series<Retained, 5>), envelope, reject);
constexpr auto measuredCheck = formula::conformity<unit::Percent>(formula::series<Passing, 5>, envelope, reject);

using formula::ConstraintOutcome;
} // namespace

TEST_CASE("each element is judged against its own row of the envelope, bounds closed", "[conformity]")
{
    constexpr auto outcomes = formula::check_conformity(passingCheck, screens);
    STATIC_REQUIRE(outcomes.size() == 5);
    STATIC_REQUIRE(outcomes[0] == ConstraintOutcome::satisfied());
    STATIC_REQUIRE(outcomes[1] == ConstraintOutcome::violated(reject)); // 46.16 below 50
    STATIC_REQUIRE(outcomes[2] == ConstraintOutcome::satisfied());      // on the closed lower bound, no upper
    STATIC_REQUIRE(outcomes[3] == ConstraintOutcome::satisfied());      // on a point row, both bounds
    STATIC_REQUIRE(outcomes[4] == ConstraintOutcome::violated(reject)); // 97.76 above 95

    // The limits are stated in percent and the subject arrives in SI, as
    // fractions: compared without the unit, 0.3576 would be below 30.
    constexpr auto measured = formula::check_conformity(measuredCheck, measuredPassing);
    STATIC_REQUIRE(measured == outcomes);
}

TEST_CASE("an unmeasured element is not checked, and only that element", "[conformity]")
{
    constexpr auto outcomes = formula::check_conformity(measuredCheck, middleUnmeasured);
    STATIC_REQUIRE(outcomes[0] == ConstraintOutcome::satisfied());
    STATIC_REQUIRE(outcomes[1] == ConstraintOutcome::violated(reject));
    STATIC_REQUIRE(outcomes[2] == ConstraintOutcome::not_checked()); // never satisfied
    STATIC_REQUIRE(outcomes[3] == ConstraintOutcome::satisfied());
    STATIC_REQUIRE(outcomes[4] == ConstraintOutcome::violated(reject));
}

TEST_CASE("an inverted row makes its element invalid, and only that element", "[conformity]")
{
    constexpr formula::Envelope<5> inverted { LimitRow { limit(rat(30)), limit(rat(40)) },
                                              LimitRow { limit(rat(50)), limit(rat(60)) },
                                              LimitRow { limit(rat(30)), limit(rat(20)) },
                                              LimitRow { limit(rat(65)), limit(rat(1764, 25)) },
                                              LimitRow { limit(rat(0)), limit(rat(95)) } };
    constexpr auto outcomes = formula::check_conformity(
        formula::conformity<unit::Percent>(formula::series<Passing, 5>, inverted, reject), measuredPassing);
    STATIC_REQUIRE(outcomes[0] == ConstraintOutcome::satisfied());
    STATIC_REQUIRE(outcomes[1] == ConstraintOutcome::violated(reject));
    STATIC_REQUIRE(outcomes[2] == ConstraintOutcome::invalid(formula::ArithmeticError::DomainError));
    STATIC_REQUIRE(outcomes[3] == ConstraintOutcome::satisfied());
    STATIC_REQUIRE(outcomes[4] == ConstraintOutcome::violated(reject));

    // An inverted row is wrong whether or not its element was measured: the
    // row is the error, and it is reported as one.
    constexpr auto withAbsence = formula::check_conformity(
        formula::conformity<unit::Percent>(formula::series<Passing, 5>, inverted, reject), middleUnmeasured);
    STATIC_REQUIRE(withAbsence[2] == ConstraintOutcome::invalid(formula::ArithmeticError::DomainError));
}

TEST_CASE("one predicate says whether a row is well formed, for the check and for any loader", "[conformity]")
{
    STATIC_REQUIRE(formula::envelope_row_is_well_formed(LimitRow { limit(rat(30)), limit(rat(40)) }));
    STATIC_REQUIRE(formula::envelope_row_is_well_formed(LimitRow { limit(rat(20)), limit(rat(20)) })); // one value
    STATIC_REQUIRE_FALSE(formula::envelope_row_is_well_formed(LimitRow { limit(rat(30)), limit(rat(20)) }));
    STATIC_REQUIRE(formula::envelope_row_is_well_formed(LimitRow { unbounded, limit(rat(-5)) }));
    STATIC_REQUIRE(formula::envelope_row_is_well_formed(LimitRow { limit(rat(5)), unbounded }));
    STATIC_REQUIRE(formula::envelope_row_is_well_formed(LimitRow { unbounded, unbounded }));
}

TEST_CASE("a limit is a value or explicitly unbounded, never a value nobody filled in", "[conformity]")
{
    STATIC_REQUIRE_FALSE(std::is_default_constructible_v<formula::Limit>);
    STATIC_REQUIRE_FALSE(std::is_default_constructible_v<LimitRow>);
    // An `Envelope` declares a default constructor only to refuse it in this
    // library's words, so `{}` gets one message rather than the compiler's
    // (`conformity_empty_envelope`); the trait therefore answers true. An
    // `Elements`, which an expression holds, leaves `{}` undeclared and the
    // trait answers false. Both pinned, since the one rule that decides each
    // is stated on the two types.
    STATIC_REQUIRE(std::is_default_constructible_v<formula::Envelope<2>>);
    STATIC_REQUIRE_FALSE(std::is_default_constructible_v<formula::Elements<2>>);
    STATIC_REQUIRE(limit(rat(3)).value() == rat(3));
    STATIC_REQUIRE_FALSE(limit(rat(3)).is_unbounded());
    STATIC_REQUIRE(unbounded.is_unbounded());
    STATIC_REQUIRE_FALSE(unbounded.value().has_value());
    STATIC_REQUIRE_FALSE(limit(rat(0)) == unbounded); // zero is a limit, not its absence
}

TEST_CASE("a row with one side unbounded checks only the other side", "[conformity]")
{
    // At most 40 %, and at least 40 %, and neither: 35.76 % satisfies the
    // first and the third, and violates the second.
    constexpr auto single = formula::environment(formula::measured_series<Passing>(m<Passing>(894, 25)));
    constexpr auto atMost = formula::check_conformity(
        formula::conformity<unit::Percent>(
            formula::series<Passing, 1>, formula::Envelope<1> { LimitRow { unbounded, limit(rat(40)) } }, reject),
        single);
    constexpr auto atLeast = formula::check_conformity(
        formula::conformity<unit::Percent>(
            formula::series<Passing, 1>, formula::Envelope<1> { LimitRow { limit(rat(40)), unbounded } }, reject),
        single);
    constexpr auto either = formula::check_conformity(
        formula::conformity<unit::Percent>(
            formula::series<Passing, 1>, formula::Envelope<1> { LimitRow { unbounded, unbounded } }, reject),
        single);
    STATIC_REQUIRE(atMost[0] == ConstraintOutcome::satisfied());
    STATIC_REQUIRE(atLeast[0] == ConstraintOutcome::violated(reject));
    STATIC_REQUIRE(either[0] == ConstraintOutcome::satisfied());
}

TEST_CASE("a subject that fails makes every element invalid, with its error", "[conformity]")
{
    constexpr auto s = formula::series<Passing, 5>;
    constexpr auto broken = formula::conformity<unit::Percent>(s / (s - s), envelope, reject);
    constexpr auto outcomes = formula::check_conformity(broken, measuredPassing);
    for (ConstraintOutcome const& outcome: outcomes)
        CHECK(outcome == ConstraintOutcome::invalid(formula::ArithmeticError::DivisionByZero));
}

TEST_CASE("a conformity check renders each row as the range it permits, and no verdict", "[conformity][render]")
{
    CHECK(formula::render(measuredCheck)
          == "conform(p(i), from 30 to 40 %, from 50 to 60 %, at least 1574/25 %, from 1764/25 to 1764/25 %, "
             "from 0 to 95 %)");
    CHECK(formula::render<formula::Dialect::Markdown>(measuredCheck)
          == "conform(`p(i)`, from 30 to 40 %, from 50 to 60 %, at least 1574/25 %, from 1764/25 to 1764/25 %, "
             "from 0 to 95 %)");
    // The subject in the page's words.
    constexpr auto everyone = formula::vocabulary(formula::renames<Passing>("P"));
    CHECK(formula::render(measuredCheck, everyone).starts_with("conform(P(i), from 30 to 40 %"));

    // LaTeX, in millimetres -- percent's `%` is a LaTeX comment character, a
    // limit every unit symbol shares (`unit.hpp`) -- with an upper-only row.
    constexpr auto openings = formula::conformity<unit::Millimetre>(
        formula::series<Opening, 2>,
        formula::Envelope<2> { LimitRow { unbounded, limit(rat(5)) }, LimitRow { limit(rat(1)), limit(rat(2)) } },
        reject);
    CHECK(formula::render(openings) == "conform(w(i), at most 5 mm, from 1 to 2 mm)");
    CHECK(
        formula::render<formula::Dialect::LaTeX>(openings)
        == "\\operatorname{conform}({w}_{i},\\allowbreak \\mathrm{at\\ most\\ 5\\ mm},\\allowbreak \\mathrm{from\\ 1\\ to\\ 2\\ mm})");
    constexpr auto anything = formula::conformity<unit::Millimetre>(
        formula::series<Opening, 1>, formula::Envelope<1> { LimitRow { unbounded, unbounded } }, reject);
    CHECK(formula::render(anything) == "conform(w(i), any value)");
}

TEST_CASE("a conformity check documents its citation and its subject's rows", "[conformity][document]")
{
    constexpr formula::Citation cited { .reference = "Example Standard 1:2020", .section = "5.3" };
    constexpr auto citedCheck = formula::conformity<unit::Percent>(formula::series<Passing, 5>, envelope, reject, cited);
    formula::Documentation const page = formula::document(citedCheck);
    CHECK(page.formula == formula::render(citedCheck));
    REQUIRE(page.citations.size() == 1);
    CHECK(page.citations[0] == cited);
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "p");
    CHECK(page.symbols[0].shape == formula::ValueShape::Series);
    CHECK(page.symbols[0].length == 5);
    // Uncited: no blank citation on the page.
    CHECK(formula::document(measuredCheck).citations.empty());
}

TEST_CASE("a conformity check is one step with one outcome per element", "[conformity][trace]")
{
    formula::Trace<> trace {};
    auto const outcomes = formula::check_conformity(measuredCheck, measuredPassing, formula::RecordingSink<> { trace });
    CHECK(outcomes[1].is_violated());
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
          == "1. p = 894/25 %; 1154/25 %; 1574/25 %; 1764/25 %; 2444/25 %\n"
             "2. conform(#1) [1 satisfied, 894/25 % (from 30 to 40 %); 2 violated, 1154/25 % (from 50 to 60 %): "
             "reject the specimen; "
             "3 satisfied, 1574/25 % (at least 1574/25 %); 4 satisfied, 1764/25 % (from 1764/25 to 1764/25 %); "
             "5 violated, 2444/25 % (from 0 to 95 %): reject the specimen]\n");
    REQUIRE(trace.steps.size() == 2);
    CHECK(trace.steps[1].kind == formula::StepKind::ConformityChecked);
    CHECK(trace.steps[1].operands == std::vector<std::size_t> { 0 });
    REQUIRE(trace.steps[1].elementOutcomes.size() == 5);
    CHECK(trace.steps[1].elementOutcomes[4] == ConstraintOutcome::violated(reject));

    // The outcomes -- each with the row it was judged against, which is
    // master data read at run time and so part of the derivation -- share the
    // element budget: 12 units show every element of both lines, 9 leave the
    // conformity line two outcomes and say how many more there are, and 7
    // leave it none.
    CHECK(formula::render_trace(trace, { .maxSteps = 12 }) == formula::render_trace(trace, { .maxSteps = 20 }));
    CHECK(formula::render_trace(trace, { .maxSteps = 9 })
          == "1. p = 894/25 %; 1154/25 %; 1574/25 %; 1764/25 %; 2444/25 %\n"
             "2. conform(#1) [1 satisfied, 894/25 % (from 30 to 40 %); 2 violated, 1154/25 % (from 50 to 60 %): "
             "reject the specimen; "
             "... 3 more]\n");
    CHECK(formula::render_trace(trace, { .maxSteps = 7 })
          == "1. p = 894/25 %; 1154/25 %; 1574/25 %; 1764/25 %; 2444/25 %\n"
             "2. conform(#1) [... 5 more]\n");
}

TEST_CASE("an unmeasured element and an inverted row read as what they are in the trace", "[conformity][trace]")
{
    constexpr formula::Envelope<5> inverted { LimitRow { limit(rat(30)), limit(rat(40)) },
                                              LimitRow { limit(rat(50)), limit(rat(60)) },
                                              LimitRow { limit(rat(60)), unbounded },
                                              LimitRow { limit(rat(80)), limit(rat(70)) },
                                              LimitRow { limit(rat(0)), limit(rat(95)) } };
    formula::Trace<> trace {};
    (void) formula::check_conformity(formula::conformity<unit::Percent>(formula::series<Passing, 5>, inverted, reject),
                                     middleUnmeasured,
                                     formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 20 })
              .ends_with(
                  "2. conform(#1) [1 satisfied, 894/25 % (from 30 to 40 %); 2 violated, 1154/25 % (from 50 to 60 %): "
                  "reject the specimen; 3 not checked (at least 60 %); 4 invalid, 1764/25 % (from 80 to 70 %): argument "
                  "outside the domain of the operation; 5 violated, 2444/25 % (from 0 to 95 %): reject the specimen]\n"));
}

TEST_CASE("the trace keeps the rows as judged, even if the check is changed afterwards", "[conformity][trace]")
{
    // The envelope is runtime master data on a public member: a row changed
    // after the check must not change what the derivation says was judged.
    auto check = measuredCheck;
    formula::Trace<> trace {};
    (void) formula::check_conformity(check, measuredPassing, formula::RecordingSink<> { trace });
    check.envelope.rows[1] = LimitRow { limit(rat(40)), limit(rat(50)) };
    CHECK(formula::render_trace(trace, { .maxSteps = 20 }).find("2 violated, 1154/25 % (from 50 to 60 %)")
          != std::string::npos);
    REQUIRE(trace.conformityLimits.size() == 1);
    CHECK(trace.conformityLimits[0].step == 1);
    REQUIRE(trace.conformityLimits[0].rows.size() == 5);
    CHECK(trace.conformityLimits[0].rows[1] == (LimitRow { limit(rat(50)), limit(rat(60)) }));
}

TEST_CASE("a limit whose conversion fails makes its element invalid, on either side", "[conformity]")
{
    // A kilometre limit near Rational's limit overflows reading into metres.
    constexpr auto huge = limit(rat(std::numeric_limits<std::int64_t>::max()));
    constexpr auto one = formula::environment(formula::measured_series<Opening>(formula::Measured<Opening> { rat(127) }));
    constexpr auto lowerSide = formula::check_conformity(
        formula::conformity<unit::Kilometre>(
            formula::series<Opening, 1>, formula::Envelope<1> { LimitRow { huge, unbounded } }, reject),
        one);
    constexpr auto upperSide = formula::check_conformity(
        formula::conformity<unit::Kilometre>(
            formula::series<Opening, 1>, formula::Envelope<1> { LimitRow { unbounded, huge } }, reject),
        one);
    STATIC_REQUIRE(lowerSide[0] == ConstraintOutcome::invalid(formula::ArithmeticError::Overflow));
    STATIC_REQUIRE(upperSide[0] == ConstraintOutcome::invalid(formula::ArithmeticError::Overflow));
}

TEST_CASE("a loader builds an envelope from rows read at run time, its count checked", "[conformity]")
{
    // From an array of the right length, at compile time.
    constexpr std::array<LimitRow, 2> loadedArray { LimitRow { limit(rat(1)), limit(rat(2)) },
                                                    LimitRow { unbounded, limit(rat(3)) } };
    constexpr formula::Envelope<2> fromArray { loadedArray };
    STATIC_REQUIRE(fromArray[1] == (LimitRow { unbounded, limit(rat(3)) }));

    // From a span of rows whose count is only known at run time.
    std::vector<LimitRow> const loaded(envelope.rows.begin(), envelope.rows.end());
    auto const fromSpan = formula::envelope_from<5>(std::span<LimitRow const> { loaded });
    REQUIRE(fromSpan.has_value());
    CHECK(fromSpan->rows == envelope.rows);
    CHECK(formula::check_conformity(formula::conformity<unit::Percent>(formula::series<Passing, 5>, *fromSpan, reject),
                                    measuredPassing)
          == formula::check_conformity(measuredCheck, measuredPassing));

    // Four rows for five elements: refused at run time, naming both counts.
    auto const tooFew = formula::envelope_from<5>(std::span<LimitRow const> { loaded }.first(4));
    REQUIRE_FALSE(tooFew.has_value());
    CHECK(tooFew.error() == (formula::EnvelopeRowCountMismatch { 4, 5 }));
}
