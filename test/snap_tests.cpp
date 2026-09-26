// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/snap.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::breakpoint;
using formula::SnapTie;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// Invented, and plainly so: a computed screen opening measured in metres --
// no sieve has one -- and the same opening stated in centimetres.
struct Opening: formula::Quantity<Opening, "d", "screen opening", unit::Metre>
{
};
struct OpeningInCm: formula::Quantity<OpeningInCm, "d_c", "screen opening", unit::Centimetre>
{
};

// The permitted openings, in metres: 11, 29, 41, 59 and 83. Invented: primes,
// and not a sieve size in any unit. The midpoints of their gaps -- 20, 35,
// 50 and 71 -- are all different, so a tie in one gap is no tie in another.
constexpr formula::BreakpointTable<5> permitted { breakpoint(11),
                                                  breakpoint(29),
                                                  breakpoint(41),
                                                  breakpoint(59),
                                                  breakpoint(83) };

template <SnapTie Tie>
constexpr auto snappedOpening(formula::Rational metres)
{
    return formula::checked_evaluate<Opening>(
        formula::snapped<unit::Metre, permitted, Tie>(formula::var<Opening>),
        formula::environment(formula::Measured<Opening> { metres }));
}

// The snapped value in metres, or nothing when the snap missed.
template <SnapTie Tie>
constexpr std::optional<formula::Rational> snapOf(formula::Rational metres)
{
    auto const out = snappedOpening<Tie>(metres);
    if (!out.has_value())
        return std::nullopt;
    return out->measurement().value();
}

template <SnapTie Tie>
constexpr bool misses(formula::Rational metres)
{
    auto const out = snappedOpening<Tie>(metres);
    return !out.has_value() && out.error() == formula::ArithmeticError::DomainError;
}

constexpr auto lower = SnapTie::TowardLower;
constexpr auto higher = SnapTie::TowardHigher;
} // namespace

TEST_CASE("a value snaps to the nearest permitted one by plain distance, and a tie by the stated rule", "[snap]")
{
    // 50 m is exactly midway between 41 and 59: the rule decides.
    STATIC_REQUIRE(snapOf<lower>(rat(50)) == rat(41));
    STATIC_REQUIRE(snapOf<higher>(rat(50)) == rat(59));
    // Nearer below and nearer above: the rule does not enter.
    STATIC_REQUIRE(snapOf<lower>(rat(48)) == rat(41));
    STATIC_REQUIRE(snapOf<higher>(rat(48)) == rat(41));
    STATIC_REQUIRE(snapOf<lower>(rat(52)) == rat(59));
    STATIC_REQUIRE(snapOf<higher>(rat(52)) == rat(59));
    // A billionth of a metre either side of that midpoint is no tie: only an
    // exact midpoint is, never one within a tolerance.
    STATIC_REQUIRE(snapOf<lower>(rat(49'999'999'999, 1'000'000'000)) == rat(41));
    STATIC_REQUIRE(snapOf<higher>(rat(49'999'999'999, 1'000'000'000)) == rat(41));
    STATIC_REQUIRE(snapOf<lower>(rat(50'000'000'001, 1'000'000'000)) == rat(59));
    STATIC_REQUIRE(snapOf<higher>(rat(50'000'000'001, 1'000'000'000)) == rat(59));
    // On a permitted value: an exact hit, not a tie.
    STATIC_REQUIRE(snapOf<lower>(rat(29)) == rat(29));
    STATIC_REQUIRE(snapOf<higher>(rat(29)) == rat(29));
    // A tie in the first gap (20 m) and in the last (71 m), against a
    // check of one gap only.
    STATIC_REQUIRE(snapOf<lower>(rat(20)) == rat(11));
    STATIC_REQUIRE(snapOf<higher>(rat(20)) == rat(29));
    STATIC_REQUIRE(snapOf<lower>(rat(71)) == rat(59));
    STATIC_REQUIRE(snapOf<higher>(rat(71)) == rat(83));
}

TEST_CASE("both ends of the permitted set are inside it, and anything beyond them misses", "[snap]")
{
    STATIC_REQUIRE(snapOf<lower>(rat(11)) == rat(11));
    STATIC_REQUIRE(snapOf<higher>(rat(11)) == rat(11));
    STATIC_REQUIRE(snapOf<lower>(rat(83)) == rat(83));
    STATIC_REQUIRE(snapOf<higher>(rat(83)) == rat(83));
    // Below the first and above the last: a miss, never the nearest end.
    STATIC_REQUIRE(misses<lower>(rat(10)));
    STATIC_REQUIRE(misses<higher>(rat(10)));
    STATIC_REQUIRE(misses<lower>(rat(84)));
    STATIC_REQUIRE(misses<higher>(rat(84)));
}

TEST_CASE("a value in another unit is converted into the key unit before it is compared", "[snap]")
{
    // 5000 cm is 50 m, the midpoint of 41 and 59 m.
    constexpr auto inCm = formula::environment(formula::Measured<OpeningInCm> { rat(5000) });
    constexpr auto towardLower = formula::checked_evaluate<Opening>(
        formula::snapped<unit::Metre, permitted, lower>(formula::var<OpeningInCm>), inCm);
    constexpr auto towardHigher = formula::checked_evaluate<Opening>(
        formula::snapped<unit::Metre, permitted, higher>(formula::var<OpeningInCm>), inCm);
    STATIC_REQUIRE(towardLower->measurement().value() == rat(41));
    STATIC_REQUIRE(towardHigher->measurement().value() == rat(59));
}

TEST_CASE("distances are taken in the key unit, where a form in SI would overflow", "[snap]")
{
    // Invented at Rational's limit, in millimetres: the first permitted value
    // is 1/10^16 mm, which is 1/10^19 m -- a denominator no int64 holds. In
    // the key unit every distance is exact, and 11.9 mm (given in metres)
    // snaps to 11 mm; a snap that compared in SI would have to convert that
    // value and could only fail. Equivalent wherever both forms can be
    // represented; told apart here.
    constexpr formula::BreakpointTable<3> atTheLimit { breakpoint(1, 10'000'000'000'000'000), breakpoint(11), breakpoint(13) };
    constexpr auto snapped = formula::checked_evaluate<Opening>(
        formula::snapped<unit::Millimetre, atTheLimit, lower>(formula::var<Opening>),
        formula::environment(formula::Measured<Opening> { rat(119, 10'000) }));
    STATIC_REQUIRE(snapped.has_value());
    STATIC_REQUIRE(snapped->measurement().value() == rat(11, 1000));
}

namespace
{
// Invented: a temperature, snapped among permitted values either side of
// zero in an affine unit.
struct Setpoint: formula::Quantity<Setpoint, "T", "setpoint", unit::Celsius>
{
};

constexpr formula::BreakpointTable<3> setpoints { breakpoint(-73, 10), breakpoint(-11, 10), breakpoint(53, 10) };

template <SnapTie Tie>
constexpr auto snappedSetpoint(formula::Rational celsius)
{
    return formula::checked_evaluate<Setpoint>(formula::snapped<unit::Celsius, setpoints, Tie>(formula::var<Setpoint>),
                                               formula::environment(formula::Measured<Setpoint> { celsius }));
}
} // namespace

TEST_CASE("negative keys in an affine unit snap by signed distance", "[snap]")
{
    // -4.2 degrees is midway between -7.3 and -1.1: a tie, decided by the
    // rule -- which distances taken as magnitudes (|-4.2| - |-1.1|, say)
    // would not see.
    STATIC_REQUIRE(snappedSetpoint<lower>(rat(-42, 10))->measurement().value() == rat(-73, 10));
    STATIC_REQUIRE(snappedSetpoint<higher>(rat(-42, 10))->measurement().value() == rat(-11, 10));
    // Below the first, in the negative range: a miss.
    STATIC_REQUIRE(snappedSetpoint<lower>(rat(-74, 10)).error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("absence and a failed operand pass through a snap", "[snap]")
{
    constexpr auto absent = formula::checked_evaluate<Opening>(
        formula::snapped<unit::Metre, permitted, lower>(formula::var<Opening>),
        formula::environment(formula::Measured<Opening>::absent()));
    STATIC_REQUIRE(absent.has_value());
    STATIC_REQUIRE(absent->measurement().is_absent());

    constexpr auto d = formula::var<Opening>;
    constexpr auto failed = formula::checked_evaluate<Opening>(
        formula::snapped<unit::Metre, permitted, lower>(d * (d / (d - d))),
        formula::environment(formula::Measured<Opening> { rat(103) }));
    STATIC_REQUIRE(!failed.has_value());
    STATIC_REQUIRE(failed.error() == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("a snap is a scalar node carrying its key unit, set and tie rule", "[snap]")
{
    using Snap = decltype(formula::snapped<unit::Metre, permitted, higher>(formula::var<Opening>));
    STATIC_REQUIRE(formula::Node<Snap>);
    STATIC_REQUIRE(Snap::unit == unit::Metre);
    STATIC_REQUIRE(Snap::dimension == unit::Metre.dimension);
    STATIC_REQUIRE(Snap::tie == SnapTie::TowardHigher);
    STATIC_REQUIRE(Snap::permitted == permitted);
    STATIC_REQUIRE(formula::describe(SnapTie::TowardLower) == "toward lower");
    STATIC_REQUIRE(formula::describe(SnapTie::TowardHigher) == "toward higher");
}

TEST_CASE("a snap renders its permitted set and no tie rule", "[snap][render]")
{
    constexpr auto snap = formula::snapped<unit::Metre, permitted, higher>(formula::var<Opening>);
    CHECK(formula::render(snap) == "snap(d, to 11, 29, 41, 59, 83 m)");
    CHECK(formula::render<formula::Dialect::Markdown>(snap) == "snap(`d`, to 11, 29, 41, 59, 83 m)");
    CHECK(formula::render<formula::Dialect::LaTeX>(snap)
          == "\\operatorname{snap}(d,\\allowbreak \\mathrm{to\\ 11,\\ 29,\\ 41,\\ 59,\\ 83\\ m})");
    // A call groups itself: no brackets in a product, and the operand in the
    // page's words.
    constexpr auto everyone = formula::vocabulary(formula::renames<Opening>("w"));
    CHECK(formula::render(snap * rat(2), everyone) == "snap(w, to 11, 29, 41, 59, 83 m) * 2");
}

TEST_CASE("a snap documents its operand's row", "[snap][document]")
{
    formula::Documentation const page =
        formula::document(formula::snapped<unit::Metre, permitted, lower>(formula::var<Opening>));
    CHECK(page.formula == "snap(d, to 11, 29, 41, 59, 83 m)");
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "d");
}

namespace
{
template <SnapTie Tie>
std::string traceOf(formula::Rational metres, formula::Trace<>& trace)
{
    (void) formula::checked_evaluate<Opening>(formula::snapped<unit::Metre, permitted, Tie>(formula::var<Opening>),
                                              formula::environment(formula::Measured<Opening> { metres }),
                                              formula::RecordingSink<> { trace });
    return formula::render_trace(trace, { .maxSteps = 10 });
}
} // namespace

TEST_CASE("a snap step names the two neighbours, and the tie rule where it decided", "[snap][trace]")
{
    formula::Trace<> tie {};
    CHECK(traceOf<higher>(rat(50), tie)
          == "1. d = 50 m\n"
             "2. snap(#1) = 59 m [41 m to 59 m; tie, toward higher]\n");
    REQUIRE(tie.steps.size() == 2);
    CHECK(tie.steps[1].kind == formula::StepKind::SnappedToPermitted);
    CHECK(tie.steps[1].selectedSegment == formula::Segment { breakpoint(41), breakpoint(59) });
    CHECK(tie.steps[1].snapTie == SnapTie::TowardHigher);
    CHECK(tie.steps[1].tieBroken);

    formula::Trace<> nearer {};
    CHECK(traceOf<higher>(rat(48), nearer).ends_with("2. snap(#1) = 41 m [41 m to 59 m; nearer 41 m]\n"));
    CHECK(!nearer.steps[1].tieBroken);

    formula::Trace<> exact {};
    CHECK(traceOf<lower>(rat(29), exact).ends_with("2. snap(#1) = 29 m [on 29 m]\n"));
    CHECK(exact.steps[1].selectedSegment == formula::Segment { breakpoint(29), breakpoint(29) });
    CHECK(exact.steps[1].snapTie == SnapTie::TowardLower);

    formula::Trace<> missed {};
    CHECK(traceOf<lower>(rat(84), missed)
              .ends_with("2. snap(#1) = argument outside the domain of the operation "
                         "[outside the permitted set, 11 m to 83 m]\n"));
    CHECK(!missed.steps[1].selectedSegment.has_value());
    REQUIRE(missed.steps[1].coveredRange.has_value());
}
