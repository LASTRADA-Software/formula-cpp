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

// Invented: a computed screen opening, and the same opening stated in
// centimetres.
struct Opening: formula::Quantity<Opening, "d", "screen opening", unit::Millimetre>
{
};
struct OpeningInCm: formula::Quantity<OpeningInCm, "d_c", "screen opening", unit::Centimetre>
{
};

// The permitted openings, in millimetres: 0.7, 1.9, 3.3, 7.1 and 13.7. Their
// gaps differ, so a midpoint in one gap is not a midpoint in another.
constexpr formula::BreakpointTable<5> permitted { breakpoint(7, 10),
                                                  breakpoint(19, 10),
                                                  breakpoint(33, 10),
                                                  breakpoint(71, 10),
                                                  breakpoint(137, 10) };

template <SnapTie Tie>
constexpr auto snappedOpening(formula::Rational millimetres)
{
    return formula::checked_evaluate<Opening>(
        formula::snapped<unit::Millimetre, permitted, Tie>(formula::var<Opening>),
        formula::environment(formula::Measured<Opening> { millimetres }));
}

// The snapped value in millimetres, or nothing when the snap missed.
template <SnapTie Tie>
constexpr std::optional<formula::Rational> snapOf(formula::Rational millimetres)
{
    auto const out = snappedOpening<Tie>(millimetres);
    if (!out.has_value())
        return std::nullopt;
    return out->measurement().value();
}

template <SnapTie Tie>
constexpr bool misses(formula::Rational millimetres)
{
    auto const out = snappedOpening<Tie>(millimetres);
    return !out.has_value() && out.error() == formula::ArithmeticError::DomainError;
}

constexpr auto lower = SnapTie::TowardLower;
constexpr auto higher = SnapTie::TowardHigher;
} // namespace

TEST_CASE("a value snaps to the nearest permitted one by plain distance, and a tie by the stated rule", "[snap]")
{
    // 5.2 mm is exactly midway between 3.3 and 7.1: the rule decides.
    STATIC_REQUIRE(snapOf<lower>(rat(52, 10)) == rat(33, 10));
    STATIC_REQUIRE(snapOf<higher>(rat(52, 10)) == rat(71, 10));
    // Nearer below and nearer above: the rule does not enter.
    STATIC_REQUIRE(snapOf<lower>(rat(49, 10)) == rat(33, 10));
    STATIC_REQUIRE(snapOf<higher>(rat(49, 10)) == rat(33, 10));
    STATIC_REQUIRE(snapOf<lower>(rat(55, 10)) == rat(71, 10));
    STATIC_REQUIRE(snapOf<higher>(rat(55, 10)) == rat(71, 10));
    // On a permitted value: an exact hit, not a tie.
    STATIC_REQUIRE(snapOf<lower>(rat(19, 10)) == rat(19, 10));
    STATIC_REQUIRE(snapOf<higher>(rat(19, 10)) == rat(19, 10));
    // A tie in the first gap (1.3 mm) and in the last (10.4 mm), against a
    // check of one gap only.
    STATIC_REQUIRE(snapOf<lower>(rat(13, 10)) == rat(7, 10));
    STATIC_REQUIRE(snapOf<higher>(rat(13, 10)) == rat(19, 10));
    STATIC_REQUIRE(snapOf<lower>(rat(104, 10)) == rat(71, 10));
    STATIC_REQUIRE(snapOf<higher>(rat(104, 10)) == rat(137, 10));
}

TEST_CASE("both ends of the permitted set are inside it, and anything beyond them misses", "[snap]")
{
    STATIC_REQUIRE(snapOf<lower>(rat(7, 10)) == rat(7, 10));
    STATIC_REQUIRE(snapOf<higher>(rat(7, 10)) == rat(7, 10));
    STATIC_REQUIRE(snapOf<lower>(rat(137, 10)) == rat(137, 10));
    STATIC_REQUIRE(snapOf<higher>(rat(137, 10)) == rat(137, 10));
    // Below the first and above the last: a miss, never the nearest end.
    STATIC_REQUIRE(misses<lower>(rat(6, 10)));
    STATIC_REQUIRE(misses<higher>(rat(6, 10)));
    STATIC_REQUIRE(misses<lower>(rat(14)));
    STATIC_REQUIRE(misses<higher>(rat(14)));
}

TEST_CASE("a value in another unit is converted into the key unit before it is compared", "[snap]")
{
    // 0.52 cm is 5.2 mm, the midpoint of 3.3 and 7.1 mm.
    constexpr auto inCm = formula::environment(formula::Measured<OpeningInCm> { rat(52, 100) });
    constexpr auto towardLower = formula::checked_evaluate<Opening>(
        formula::snapped<unit::Millimetre, permitted, lower>(formula::var<OpeningInCm>), inCm);
    constexpr auto towardHigher = formula::checked_evaluate<Opening>(
        formula::snapped<unit::Millimetre, permitted, higher>(formula::var<OpeningInCm>), inCm);
    STATIC_REQUIRE(towardLower->measurement().value() == rat(33, 10));
    STATIC_REQUIRE(towardHigher->measurement().value() == rat(71, 10));
}

TEST_CASE("absence and a failed operand pass through a snap", "[snap]")
{
    constexpr auto absent = formula::checked_evaluate<Opening>(
        formula::snapped<unit::Millimetre, permitted, lower>(formula::var<Opening>),
        formula::environment(formula::Measured<Opening>::absent()));
    STATIC_REQUIRE(absent.has_value());
    STATIC_REQUIRE(absent->measurement().is_absent());

    constexpr auto d = formula::var<Opening>;
    constexpr auto failed = formula::checked_evaluate<Opening>(
        formula::snapped<unit::Millimetre, permitted, lower>(d * (d / (d - d))),
        formula::environment(formula::Measured<Opening> { rat(103) }));
    STATIC_REQUIRE(!failed.has_value());
    STATIC_REQUIRE(failed.error() == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("a snap is a scalar node carrying its key unit, set and tie rule", "[snap]")
{
    using Snap = decltype(formula::snapped<unit::Millimetre, permitted, higher>(formula::var<Opening>));
    STATIC_REQUIRE(formula::Node<Snap>);
    STATIC_REQUIRE(Snap::unit == unit::Millimetre);
    STATIC_REQUIRE(Snap::dimension == unit::Millimetre.dimension);
    STATIC_REQUIRE(Snap::tie == SnapTie::TowardHigher);
    STATIC_REQUIRE(Snap::permitted == permitted);
    STATIC_REQUIRE(formula::describe(SnapTie::TowardLower) == "toward lower");
    STATIC_REQUIRE(formula::describe(SnapTie::TowardHigher) == "toward higher");
}

TEST_CASE("a snap renders its permitted set and no tie rule", "[snap][render]")
{
    constexpr auto snap = formula::snapped<unit::Millimetre, permitted, higher>(formula::var<Opening>);
    CHECK(formula::render(snap) == "snap(d, to 7/10, 19/10, 33/10, 71/10, 137/10 mm)");
    CHECK(formula::render<formula::Dialect::Markdown>(snap) == "snap(`d`, to 7/10, 19/10, 33/10, 71/10, 137/10 mm)");
    CHECK(formula::render<formula::Dialect::LaTeX>(snap)
          == "\\operatorname{snap}(d,\\allowbreak \\mathrm{to\\ 7/10,\\ 19/10,\\ 33/10,\\ 71/10,\\ 137/10\\ mm})");
    // A call groups itself: no brackets in a product, and the operand in the
    // page's words.
    constexpr auto everyone = formula::vocabulary(formula::renames<Opening>("w"));
    CHECK(formula::render(snap * rat(2), everyone) == "snap(w, to 7/10, 19/10, 33/10, 71/10, 137/10 mm) * 2");
}

TEST_CASE("a snap documents its operand's row", "[snap][document]")
{
    formula::Documentation const page =
        formula::document(formula::snapped<unit::Millimetre, permitted, lower>(formula::var<Opening>));
    CHECK(page.formula == "snap(d, to 7/10, 19/10, 33/10, 71/10, 137/10 mm)");
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "d");
}

namespace
{
template <SnapTie Tie>
std::string traceOf(formula::Rational millimetres, formula::Trace<>& trace)
{
    (void) formula::checked_evaluate<Opening>(formula::snapped<unit::Millimetre, permitted, Tie>(formula::var<Opening>),
                                              formula::environment(formula::Measured<Opening> { millimetres }),
                                              formula::RecordingSink<> { trace });
    return formula::render_trace(trace, { .maxSteps = 10 });
}
} // namespace

TEST_CASE("a snap step names the two neighbours, and the tie rule where it decided", "[snap][trace]")
{
    formula::Trace<> tie {};
    CHECK(traceOf<higher>(rat(52, 10), tie)
          == "1. d = 26/5 mm\n"
             "2. snap(#1) = 71/10 mm [33/10 mm to 71/10 mm; tie, toward higher]\n");
    REQUIRE(tie.steps.size() == 2);
    CHECK(tie.steps[1].kind == formula::StepKind::SnappedToPermitted);
    CHECK(tie.steps[1].selectedSegment == formula::Segment { breakpoint(33, 10), breakpoint(71, 10) });
    CHECK(tie.steps[1].snapTie == SnapTie::TowardHigher);
    CHECK(tie.steps[1].tieBroken);

    formula::Trace<> nearer {};
    CHECK(traceOf<higher>(rat(49, 10), nearer).ends_with("2. snap(#1) = 33/10 mm [33/10 mm to 71/10 mm; nearer 33/10 mm]\n"));
    CHECK(!nearer.steps[1].tieBroken);

    formula::Trace<> exact {};
    CHECK(traceOf<lower>(rat(19, 10), exact).ends_with("2. snap(#1) = 19/10 mm [on 19/10 mm]\n"));
    CHECK(exact.steps[1].selectedSegment == formula::Segment { breakpoint(19, 10), breakpoint(19, 10) });
    CHECK(exact.steps[1].snapTie == SnapTie::TowardLower);

    formula::Trace<> missed {};
    CHECK(traceOf<lower>(rat(14), missed)
              .ends_with("2. snap(#1) = argument outside the domain of the operation "
                         "[outside the permitted set, 7/10 mm to 137/10 mm]\n"));
    CHECK(!missed.steps[1].selectedSegment.has_value());
    REQUIRE(missed.steps[1].coveredRange.has_value());
}
