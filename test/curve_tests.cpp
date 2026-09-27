// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/curve.hpp>
#include <formula-cpp/document.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/snap.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace
{
namespace unit = formula::unit;
using formula::breakpoint;
using formula::Monotone;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// Invented, and plainly so: screen openings of 103, 127, 163, 197 and 241 m
// -- three significant digits, none a preferred number, and not a sieve size
// in any unit -- and the percentage passing
// each. Their gaps differ, so the point read is not the midpoint of its
// segment by accident.
struct Opening: formula::Quantity<Opening, "d", "screen opening", unit::Metre>
{
};
struct Passing: formula::Quantity<Passing, "p", "percentage passing a screen", unit::Percent>
{
};
// A share as a plain fraction, for the LaTeX renderings: `%` is emitted bare
// there, a tracked follow-up this phase does not fix.
struct Share: formula::Quantity<Share, "s", "share passing a screen", unit::One>
{
};

constexpr formula::BreakpointTable<5> screens { breakpoint(103),
                                                breakpoint(127),
                                                breakpoint(163),
                                                breakpoint(197),
                                                breakpoint(241) };

template <typename Q>
constexpr formula::Measured<Q> m(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Measured<Q> { rat(numerator, denominator) };
}

// Passing at each screen: 35.76, 46.16, 62.96, 70.56 and 97.76 %.
constexpr auto passingMeasured = formula::measured_series<Passing>(
    m<Passing>(894, 25), m<Passing>(1154, 25), m<Passing>(1574, 25), m<Passing>(1764, 25), m<Passing>(2444, 25));
constexpr auto screened = formula::environment(passingMeasured);

constexpr auto passing = formula::series<Passing, 5>;
constexpr auto grading = formula::curve(formula::domain<unit::Metre, screens>(), passing);

// The value along @p along at @p at, in `Result`'s unit, or nothing on any
// failure or absence.
template <typename Result, typename Along, typename At, typename Env>
constexpr std::optional<formula::Rational> along(Along const& curveExpression, At const& at, Env const& inputs)
{
    auto const out = formula::checked_evaluate<Result>(formula::interpolate_at(curveExpression, at), inputs);
    if (!out.has_value() || out->measurement().is_absent())
        return std::nullopt;
    return out->measurement().value();
}

template <typename Result, typename Along, typename At, typename Env>
constexpr std::optional<formula::ArithmeticError> failure_along(Along const& curveExpression, At const& at, Env const& inputs)
{
    auto const out = formula::checked_evaluate<Result>(formula::interpolate_at(curveExpression, at), inputs);
    if (out.has_value())
        return std::nullopt;
    return out.error();
}

constexpr auto metres(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::constant<unit::Metre>(rat(numerator, denominator));
}

template <typename Q, std::size_t N>
constexpr bool reads(std::array<formula::Measured<Q>, N> const& measurements, std::array<formula::Rational, N> const& expected)
{
    for (std::size_t at = 0; at < N; ++at)
        if (measurements[at].stored() != std::optional<formula::Rational> { expected[at] })
            return false;
    return true;
}
} // namespace

// ---- Interpolation along a curve ----

TEST_CASE("a curve interpolates by domain, not by position", "[curve]")
{
    // 173 m is 5/17 of the 34 m from 163 to 197 m: 62.96 + 7.6 * 5/17 =
    // 27708/425 %. Interpolating by position -- halfway from element 3 to
    // element 4 -- would give 66.76 %.
    STATIC_REQUIRE(along<Passing>(grading, metres(173), screened) == rat(27708, 425));
    // On a point: that point's value exactly, the last point included.
    STATIC_REQUIRE(along<Passing>(grading, metres(163), screened) == rat(1574, 25));
    STATIC_REQUIRE(along<Passing>(grading, metres(241), screened) == rat(2444, 25));
    STATIC_REQUIRE(along<Passing>(grading, metres(103), screened) == rat(894, 25));
}

TEST_CASE("the inverse curve answers exactly, off its segment's midpoint", "[curve]")
{
    // 50 % is 3.84 of the 16.8 points from 46.16 to 62.96 %: 127 + 3.84/16.8 *
    // 36 = 4733/35 m, exactly.
    constexpr auto inverse = formula::curve(passing, formula::domain<unit::Metre, screens>());
    STATIC_REQUIRE(along<Opening>(inverse, formula::constant<unit::Percent>(rat(50)), screened) == rat(4733, 35));
}

TEST_CASE("outside its domain a curve misses, at both ends", "[curve]")
{
    STATIC_REQUIRE(failure_along<Passing>(grading, metres(101), screened) == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(failure_along<Passing>(grading, metres(251), screened) == formula::ArithmeticError::DomainError);
}

TEST_CASE("a computed domain that does not strictly ascend fails at its first offending element", "[curve]")
{
    // 113 m after 127 m, and 163 m after 197 m: the first is reported.
    constexpr auto disordered = formula::environment(
        formula::measured_series<Opening>(m<Opening>(103), m<Opening>(127), m<Opening>(113), m<Opening>(197),
                                          m<Opening>(163)),
        passingMeasured);
    constexpr auto computed = formula::curve(formula::series<Opening, 5>, passing);
    constexpr auto evaluated = formula::checked_evaluate_curve<Opening, Passing>(computed, disordered);
    STATIC_REQUIRE(!evaluated.has_value());
    STATIC_REQUIRE(evaluated.error() == formula::SeriesFailure { formula::ArithmeticError::DomainError, 2 });
    // Interpolating along it relays the error.
    STATIC_REQUIRE(failure_along<Passing>(computed, metres(179), disordered) == formula::ArithmeticError::DomainError);

    // A point stated twice is not ascending either.
    constexpr auto repeated = formula::environment(
        formula::measured_series<Opening>(m<Opening>(103), m<Opening>(127), m<Opening>(127), m<Opening>(197),
                                          m<Opening>(241)),
        passingMeasured);
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(computed, repeated).error()
                   == formula::SeriesFailure { formula::ArithmeticError::DomainError, 2 });

    // A gap does not excuse the points around it (final review, M2): 163 m
    // comes after 197 m with an absent point between them, and the curve
    // fails there, at zero-based 3, rather than being returned with a domain
    // that does not ascend. A point equal to one before the gap is a
    // duplicate likewise.
    constexpr auto gapped = formula::environment(
        formula::measured_series<Opening>(m<Opening>(103), formula::Measured<Opening>::absent(), m<Opening>(197),
                                          m<Opening>(163), m<Opening>(241)),
        passingMeasured);
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(computed, gapped).error()
                   == formula::SeriesFailure { formula::ArithmeticError::DomainError, 3 });
    constexpr auto gappedTwice = formula::environment(
        formula::measured_series<Opening>(m<Opening>(103), m<Opening>(127), formula::Measured<Opening>::absent(),
                                          m<Opening>(127), m<Opening>(241)),
        passingMeasured);
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(computed, gappedTwice).error()
                   == formula::SeriesFailure { formula::ArithmeticError::DomainError, 3 });
    // A gapped domain that ascends is a curve, its point absent.
    constexpr auto gappedAscending = formula::environment(
        formula::measured_series<Opening>(m<Opening>(103), formula::Measured<Opening>::absent(), m<Opening>(163),
                                          m<Opening>(197), m<Opening>(241)),
        passingMeasured);
    constexpr auto withGap = formula::checked_evaluate_curve<Opening, Passing>(computed, gappedAscending);
    STATIC_REQUIRE(withGap.has_value());
    STATIC_REQUIRE(withGap->domain()[1].is_absent());
}

TEST_CASE("an absent element anywhere makes the interpolation absent", "[curve]")
{
    // Element 5, far from 173 m, is absent: still no answer (S7, strict).
    constexpr auto lastAbsent = formula::environment(formula::measured_series<Passing>(
        m<Passing>(894, 25), m<Passing>(1154, 25), m<Passing>(1574, 25), m<Passing>(1764, 25), formula::Measured<Passing>::absent()));
    constexpr auto out = formula::checked_evaluate<Passing>(formula::interpolate_at(grading, metres(173)), lastAbsent);
    STATIC_REQUIRE(out.has_value());
    STATIC_REQUIRE(out->measurement().is_absent());

    // An absent point in a computed domain, likewise.
    constexpr auto domainAbsent = formula::environment(
        formula::measured_series<Opening>(m<Opening>(103), formula::Measured<Opening>::absent(), m<Opening>(163),
                                          m<Opening>(197), m<Opening>(241)),
        passingMeasured);
    constexpr auto fromDomain = formula::checked_evaluate<Passing>(
        formula::interpolate_at(formula::curve(formula::series<Opening, 5>, passing), metres(173)), domainAbsent);
    STATIC_REQUIRE(fromDomain.has_value());
    STATIC_REQUIRE(fromDomain->measurement().is_absent());
}

TEST_CASE("an interpolation along an absent curve states no segment and no range", "[curve][trace]")
{
    // Read inside the curve's extent: a range clause would say it was read
    // outside, which is false; a segment clause would say it was located.
    constexpr auto noneMeasured = formula::environment(formula::measured_series<Passing>(
        formula::Measured<Passing>::absent(), formula::Measured<Passing>::absent(), formula::Measured<Passing>::absent(),
        formula::Measured<Passing>::absent(), formula::Measured<Passing>::absent()));
    formula::Trace<> none {};
    (void) formula::checked_evaluate<Passing>(formula::interpolate_at(grading, metres(173)), noneMeasured,
                                              formula::RecordingSink<> { none });
    CHECK(formula::render_trace(none, { .maxSteps = 30 }).ends_with("5. interpolate(#3, at #4) = (not measured)\n"));
    CHECK(!none.steps.back().coveredRange.has_value());

    constexpr auto firstAbsent = formula::environment(formula::measured_series<Passing>(
        formula::Measured<Passing>::absent(), m<Passing>(1154, 25), m<Passing>(1574, 25), m<Passing>(1764, 25),
        m<Passing>(2444, 25)));
    formula::Trace<> first {};
    (void) formula::checked_evaluate<Passing>(formula::interpolate_at(grading, metres(173)), firstAbsent,
                                              formula::RecordingSink<> { first });
    CHECK(formula::render_trace(first, { .maxSteps = 30 }).ends_with("5. interpolate(#3, at #4) = (not measured)\n"));
    CHECK(!first.steps.back().selectedSegment.has_value());
}

TEST_CASE("a one-point curve answers at its point and misses everywhere else", "[curve]")
{
    constexpr formula::BreakpointTable<1> onePoint { breakpoint(163) };
    constexpr auto single = formula::curve(formula::domain<unit::Metre, onePoint>(),
                                           formula::series_constant<unit::Percent>(rat(1574, 25)));
    constexpr auto none = formula::environment();
    STATIC_REQUIRE(along<Passing>(single, metres(163), none) == rat(1574, 25));
    STATIC_REQUIRE(failure_along<Passing>(single, metres(162), none) == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(failure_along<Passing>(single, metres(164), none) == formula::ArithmeticError::DomainError);
}

TEST_CASE("a curve evaluates to its domain and values, each in its own quantity's unit", "[curve]")
{
    constexpr auto evaluated = formula::checked_evaluate_curve<Opening, Passing>(grading, screened);
    STATIC_REQUIRE(evaluated.has_value());
    STATIC_REQUIRE(reads(evaluated->domain(), { rat(103), rat(127), rat(163), rat(197), rat(241) }));
    STATIC_REQUIRE(reads(evaluated->values(), { rat(894, 25), rat(1154, 25), rat(1574, 25), rat(1764, 25), rat(2444, 25) }));
    STATIC_REQUIRE(decltype(grading)::length == 5);
    STATIC_REQUIRE(decltype(grading)::domainDimension == unit::Metre.dimension);
    STATIC_REQUIRE(decltype(grading)::dimension == unit::Percent.dimension);
    STATIC_REQUIRE(formula::SeriesNode<decltype(formula::domain<unit::Metre, screens>())>);
    STATIC_REQUIRE(formula::CurveExpression<decltype(grading)>);
    STATIC_REQUIRE(formula::Node<decltype(formula::interpolate_at(grading, metres(179)))>);
}

// ---- Splicing two curves ----

namespace
{
constexpr formula::BreakpointTable<3> coarse { breakpoint(103), breakpoint(127), breakpoint(163) };
constexpr formula::BreakpointTable<3> fine { breakpoint(103, 10), breakpoint(137, 10), breakpoint(163, 10) };

// A: 103, 127 and 163 m at 35.76, 46.16 and 62.96 %. B: 10.3, 13.7 and 16.3 m at
// 3.1, 8.4 and 14.2 %. Invented, as the screens are.
constexpr auto curveA =
    formula::curve(formula::domain<unit::Metre, coarse>(),
                   formula::series_constant<unit::Percent>(rat(894, 25), rat(1154, 25), rat(1574, 25)));
constexpr auto curveB = formula::curve(formula::domain<unit::Metre, fine>(),
                                       formula::series_constant<unit::Percent>(rat(31, 10), rat(84, 10), rat(142, 10)));
constexpr auto noInputs = formula::environment();

constexpr std::array<formula::Rational, 6> splicedDomain { rat(103, 10), rat(137, 10), rat(163, 10), rat(103), rat(127), rat(163) };
constexpr std::array<formula::Rational, 6> splicedValues { rat(31, 10),  rat(84, 10),   rat(142, 10),
                                                           rat(894, 25), rat(1154, 25), rat(1574, 25) };
} // namespace

TEST_CASE("a splice is the sorted union by domain, whichever curve comes first", "[curve][splice]")
{
    constexpr auto ab = formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveA, curveB),
                                                                          noInputs);
    constexpr auto ba = formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveB, curveA),
                                                                          noInputs);
    STATIC_REQUIRE(ab.has_value());
    STATIC_REQUIRE(ba.has_value());
    // Kept in argument order, (A, B) would start at 103 m.
    STATIC_REQUIRE(reads(ab->domain(), splicedDomain));
    STATIC_REQUIRE(reads(ab->values(), splicedValues));
    STATIC_REQUIRE(reads(ba->domain(), splicedDomain));
    STATIC_REQUIRE(reads(ba->values(), splicedValues));
    STATIC_REQUIRE(decltype(formula::splice<Monotone::NonDecreasing>(curveA, curveB))::length == 6);
}

TEST_CASE("the direction is judged on the spliced curve, not on each operand", "[curve][splice]")
{
    // B's last value raised to 40 %: B alone still ascends, and so does A, but
    // the union falls from 40 % at 16.3 m to 35.76 % at 103 m -- element 3,
    // zero-based, the first that breaks the direction.
    constexpr auto raised = formula::curve(formula::domain<unit::Metre, fine>(),
                                           formula::series_constant<unit::Percent>(rat(31, 10), rat(84, 10), rat(40)));
    constexpr formula::SeriesFailure breaks { formula::ArithmeticError::DomainError, 3 };
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveA, raised),
                                                                     noInputs)
                       .error()
                   == breaks);
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(raised, curveA),
                                                                     noInputs)
                       .error()
                   == breaks);

    // The direction is the author's: the same two curves do not fall.
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonIncreasing>(curveA, curveB),
                                                                     noInputs)
                       .error()
                   == formula::SeriesFailure { formula::ArithmeticError::DomainError, 1 });
}

TEST_CASE("a falling curve splices under NonIncreasing, and not under NonDecreasing", "[curve][splice]")
{
    // Retained above each screen, the complement of A and B: falling.
    constexpr auto fallingA = formula::curve(
        formula::domain<unit::Metre, coarse>(),
        formula::series_constant<unit::Percent>(rat(1606, 25), rat(1346, 25), rat(926, 25)));
    constexpr auto fallingB = formula::curve(formula::domain<unit::Metre, fine>(),
                                             formula::series_constant<unit::Percent>(rat(969, 10), rat(916, 10), rat(858, 10)));
    constexpr auto falling =
        formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonIncreasing>(fallingA, fallingB), noInputs);
    STATIC_REQUIRE(falling.has_value());
    STATIC_REQUIRE(reads(falling->values(),
                         { rat(969, 10), rat(916, 10), rat(858, 10), rat(1606, 25), rat(1346, 25), rat(926, 25) }));
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(fallingB, fallingA),
                                                                     noInputs)
                       .error()
                   == formula::SeriesFailure { formula::ArithmeticError::DomainError, 1 });
}

TEST_CASE("two curves sharing a domain point miss where they meet", "[curve][splice]")
{
    // B ends at 103 m, where A begins: sorted, the second 103 m is element 3.
    constexpr formula::BreakpointTable<3> meeting { breakpoint(103, 10), breakpoint(137, 10), breakpoint(103) };
    constexpr auto meets = formula::curve(formula::domain<unit::Metre, meeting>(),
                                          formula::series_constant<unit::Percent>(rat(31, 10), rat(84, 10), rat(142, 10)));
    constexpr formula::SeriesFailure where { formula::ArithmeticError::DomainError, 3 };
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveA, meets),
                                                                     noInputs)
                       .error()
                   == where);
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(meets, curveA),
                                                                     noInputs)
                       .error()
                   == where);
}

TEST_CASE("two one-point curves splice to a two-point curve", "[curve][splice]")
{
    constexpr formula::BreakpointTable<1> low { breakpoint(163, 10) };
    constexpr formula::BreakpointTable<1> high { breakpoint(127) };
    constexpr auto lowPoint =
        formula::curve(formula::domain<unit::Metre, low>(), formula::series_constant<unit::Percent>(rat(142, 10)));
    constexpr auto highPoint =
        formula::curve(formula::domain<unit::Metre, high>(), formula::series_constant<unit::Percent>(rat(1154, 25)));
    constexpr auto spliced =
        formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(highPoint, lowPoint), noInputs);
    STATIC_REQUIRE(spliced.has_value());
    STATIC_REQUIRE(reads(spliced->domain(), { rat(163, 10), rat(127) }));
    STATIC_REQUIRE(reads(spliced->values(), { rat(142, 10), rat(1154, 25) }));
}

namespace
{
// x falls from 20 % at 103 m to 10 % at 163 m, and y states 163 m again: a
// duplicate beside a direction break. Invented: three significant digits, not a
// preferred number.
constexpr formula::BreakpointTable<2> xPoints { breakpoint(103), breakpoint(163) };
constexpr formula::BreakpointTable<2> yPoints { breakpoint(163), breakpoint(241) };
constexpr auto curveX =
    formula::curve(formula::domain<unit::Metre, xPoints>(), formula::series_constant<unit::Percent>(rat(20), rat(10)));
constexpr auto curveY =
    formula::curve(formula::domain<unit::Metre, yPoints>(), formula::series_constant<unit::Percent>(rat(30), rat(40)));
} // namespace

TEST_CASE("a duplicate point beside a direction break fails at the duplicate, in either order", "[curve][splice]")
{
    // Sorted, (x, y) reads 103 m: 20; 163 m: 10; 163 m: 30; 241 m: 40 and (y, x)
    // reads 103 m: 20; 163 m: 30; 163 m: 10; 241 m: 40. Judged in one pass, the
    // first breaks the direction at zero-based 1 and the second repeats 163 m
    // at 2; duplicates are judged over the whole union first, so both fail at
    // the second 163 m.
    constexpr formula::SeriesFailure duplicate { formula::ArithmeticError::DomainError, 2 };
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveX, curveY),
                                                                     noInputs)
                       .error()
                   == duplicate);
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveY, curveX),
                                                                     noInputs)
                       .error()
                   == duplicate);
}

TEST_CASE("a plateau splices in either direction: equal values do not break it", "[curve][splice]")
{
    // A reaching 100 % on its two largest screens, as a passing curve does.
    constexpr auto levelling = formula::curve(formula::domain<unit::Metre, coarse>(),
                                              formula::series_constant<unit::Percent>(rat(894, 25), rat(100), rat(100)));
    constexpr auto risingAB = formula::checked_evaluate_curve<Opening, Passing>(
        formula::splice<Monotone::NonDecreasing>(levelling, curveB), noInputs);
    constexpr auto risingBA = formula::checked_evaluate_curve<Opening, Passing>(
        formula::splice<Monotone::NonDecreasing>(curveB, levelling), noInputs);
    STATIC_REQUIRE(risingAB.has_value());
    STATIC_REQUIRE(risingBA.has_value());
    STATIC_REQUIRE(reads(risingBA->values(), { rat(31, 10), rat(84, 10), rat(142, 10), rat(894, 25), rat(100), rat(100) }));

    // Falling, with the plateau across the join: B ends at 1606/25 %, where
    // A begins.
    constexpr auto fallingA = formula::curve(
        formula::domain<unit::Metre, coarse>(),
        formula::series_constant<unit::Percent>(rat(1606, 25), rat(1346, 25), rat(926, 25)));
    constexpr auto fallingToA = formula::curve(formula::domain<unit::Metre, fine>(),
                                               formula::series_constant<unit::Percent>(rat(100), rat(916, 10), rat(1606, 25)));
    constexpr auto fallingAB = formula::checked_evaluate_curve<Opening, Passing>(
        formula::splice<Monotone::NonIncreasing>(fallingA, fallingToA), noInputs);
    constexpr auto fallingBA = formula::checked_evaluate_curve<Opening, Passing>(
        formula::splice<Monotone::NonIncreasing>(fallingToA, fallingA), noInputs);
    STATIC_REQUIRE(fallingAB.has_value());
    STATIC_REQUIRE(fallingBA.has_value());
    STATIC_REQUIRE(
        reads(fallingAB->values(), { rat(100), rat(916, 10), rat(1606, 25), rat(1606, 25), rat(1346, 25), rat(926, 25) }));
}

namespace
{
// A computed curve whose domain falls from 127 to 103 m: it fails at its own
// element 1, zero-based.
constexpr auto fallingDomain = formula::environment(
    formula::measured_series<Opening>(m<Opening>(127), m<Opening>(103), m<Opening>(163)),
    formula::measured_series<Passing>(m<Passing>(894, 25), m<Passing>(1154, 25), m<Passing>(1574, 25)));
constexpr auto computedCurve = formula::curve(formula::series<Opening, 3>, formula::series<Passing, 3>);
} // namespace

TEST_CASE("a splice relays an operand's failure with no element of its own", "[curve][splice]")
{
    // Element 1 is the operand's, not the splice's: in the union it would name
    // one of B's points.
    constexpr formula::SeriesFailure relayed { formula::ArithmeticError::DomainError, std::nullopt };
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(computedCurve, fallingDomain).error()
                   == formula::SeriesFailure { formula::ArithmeticError::DomainError, 1 });
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(
                       formula::splice<Monotone::NonDecreasing>(curveB, computedCurve), fallingDomain)
                       .error()
                   == relayed);
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(
                       formula::splice<Monotone::NonDecreasing>(computedCurve, curveB), fallingDomain)
                       .error()
                   == relayed);
}

TEST_CASE("a splice interpolates across the join, with nothing rescaled", "[curve][splice]")
{
    // 45.2 m is a third of the way from 16.3 m (14.2 %) to 103 m (35.76 %):
    // 14.2 + 21.56 / 3 = 1604/75 %.
    STATIC_REQUIRE(along<Passing>(formula::splice<Monotone::NonDecreasing>(curveA, curveB), metres(452, 10), noInputs)
                   == rat(1604, 75));
}

TEST_CASE("an absent element in either curve makes the whole splice absent", "[curve][splice]")
{
    constexpr auto withAbsent = formula::environment(
        formula::measured_series<Passing>(m<Passing>(31, 10), formula::Measured<Passing>::absent(), m<Passing>(142, 10)));
    constexpr auto measuredB = formula::curve(formula::domain<unit::Metre, fine>(), formula::series<Passing, 3>);
    constexpr auto spliced =
        formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveA, measuredB), withAbsent);
    STATIC_REQUIRE(spliced.has_value());
    for (std::size_t at = 0; at < 6; ++at)
    {
        CHECK(spliced->domain()[at].is_absent());
        CHECK(spliced->values()[at].is_absent());
    }
}

// ---- Renderings ----

TEST_CASE("an interpolation along a curve renders with every series marked", "[curve][render]")
{
    constexpr auto computed = formula::curve(formula::series<Opening, 5>, passing);
    CHECK(formula::render(formula::interpolate_at(computed, metres(173))) == "interpolate(curve(d(i), p(i)), at 173 m)");
    CHECK(formula::render<formula::Dialect::Markdown>(formula::interpolate_at(computed, metres(173)))
          == "interpolate(curve(`d(i)`, `p(i)`), at 173 m)");
    CHECK(formula::render<formula::Dialect::LaTeX>(
              formula::interpolate_at(formula::curve(formula::series<Opening, 5>, formula::series<Share, 5>), metres(173)))
          == "\\operatorname{interpolate}(\\operatorname{curve}({d}_{i},\\allowbreak {s}_{i}),\\allowbreak \\mathrm{at\\ }173\\,\\mathrm{m})");
    // A declared domain renders as its points, one unit for them all.
    CHECK(formula::render(formula::interpolate_at(grading, metres(173)))
          == "interpolate(curve(domain(103, 127, 163, 197, 241 m), p(i)), at 173 m)");
    // In a vocabulary.
    constexpr auto everyone = formula::vocabulary(formula::renames<Opening>("w"));
    CHECK(formula::render(formula::interpolate_at(computed, metres(173)), everyone)
          == "interpolate(curve(w(i), p(i)), at 173 m)");
}

TEST_CASE("a splice renders both curves and always states its direction", "[curve][render]")
{
    CHECK(formula::render(formula::splice<Monotone::NonDecreasing>(curveA, curveB))
          == "splice(curve(domain(103, 127, 163 m), values(894/25 %, 1154/25 %, 1574/25 %)), "
             "curve(domain(103/10, 137/10, 163/10 m), values(31/10 %, 42/5 %, 71/5 %)), non-decreasing)");
    constexpr auto shares = formula::curve(formula::series<Opening, 5>, formula::series<Share, 5>);
    CHECK(formula::render(formula::splice<Monotone::NonIncreasing>(shares, shares)) == "splice(curve(d(i), s(i)), curve(d(i), s(i)), non-increasing)");
    CHECK(formula::render<formula::Dialect::Markdown>(formula::splice<Monotone::NonIncreasing>(shares, shares))
          == "splice(curve(`d(i)`, `s(i)`), curve(`d(i)`, `s(i)`), non-increasing)");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::splice<Monotone::NonIncreasing>(shares, shares))
          == "\\operatorname{splice}(\\operatorname{curve}({d}_{i},\\allowbreak {s}_{i}),\\allowbreak "
             "\\operatorname{curve}({d}_{i},\\allowbreak {s}_{i}),\\allowbreak \\mathrm{non-increasing})");
    STATIC_REQUIRE(formula::describe(Monotone::NonDecreasing) == "non-decreasing");
    STATIC_REQUIRE(formula::describe(Monotone::NonIncreasing) == "non-increasing");
}

TEST_CASE("an interpolation along a curve documents its series, once each", "[curve][document]")
{
    formula::Documentation const page = formula::document(
        formula::interpolate_at(formula::curve(formula::series<Opening, 5>, passing), metres(173)));
    CHECK(page.formula == "interpolate(curve(d(i), p(i)), at 173 m)");
    REQUIRE(page.symbols.size() == 2);
    CHECK(page.symbols[0].symbol == "d");
    CHECK(page.symbols[0].shape == formula::ValueShape::Series);
    CHECK(page.symbols[1].symbol == "p");

    formula::Documentation const splicePage = formula::document(formula::splice<Monotone::NonDecreasing>(
        formula::curve(formula::series<Opening, 5>, passing), formula::curve(formula::series<Opening, 5>, passing)));
    CHECK(splicePage.formula == "splice(curve(d(i), p(i)), curve(d(i), p(i)), non-decreasing)");
    CHECK(splicePage.symbols.size() == 2);
}

// ---- Trace ----

TEST_CASE("a curve step shows its pairs, and an interpolation names its segment", "[curve][trace]")
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Passing>(formula::interpolate_at(grading, metres(173)), screened,
                                              formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
          == "1. 103 m; 127 m; 163 m; 197 m; 241 m\n"
             "2. p = 894/25 %; 1154/25 %; 1574/25 %; 1764/25 %; 2444/25 %\n"
             "3. curve(#1, #2) = 103 m: 894/25 %; 127 m: 1154/25 %; 163 m: 1574/25 %; 197 m: 1764/25 %; "
             "241 m: 2444/25 %\n"
             "4. 173 m\n"
             "5. interpolate(#3, at #4) = 27708/425 % [between 163 and 197 m]\n");
    REQUIRE(trace.steps.size() == 5);
    CHECK(trace.steps[0].kind == formula::StepKind::SeriesDomain);
    CHECK(trace.steps[2].kind == formula::StepKind::CurvePairing);
    CHECK(trace.steps[4].kind == formula::StepKind::CurveInterpolation);
    CHECK(trace.steps[4].selectedSegment == formula::Segment { breakpoint(163), breakpoint(197) });

    // The pairs share the element budget, one unit each: 15 units leave the
    // curve line two pairs and say how many more there are.
    CHECK(formula::render_trace(trace, { .maxSteps = 15 })
          == "1. 103 m; 127 m; 163 m; 197 m; 241 m\n"
             "2. p = 894/25 %; 1154/25 %; 1574/25 %; 1764/25 %; 2444/25 %\n"
             "3. curve(#1, #2) = 103 m: 894/25 %; 127 m: 1154/25 %; ... 3 more\n"
             "... 2 further steps not shown\n");
}

TEST_CASE("an interpolation on a point, and a miss, say so in the trace", "[curve][trace]")
{
    formula::Trace<> onPoint {};
    (void) formula::checked_evaluate<Passing>(formula::interpolate_at(grading, metres(163)), screened,
                                              formula::RecordingSink<> { onPoint });
    CHECK(formula::render_trace(onPoint, { .maxSteps = 30 }).ends_with("5. interpolate(#3, at #4) = 1574/25 % [on the row at 163 m]\n"));

    formula::Trace<> missed {};
    (void) formula::checked_evaluate<Passing>(formula::interpolate_at(grading, metres(251)), screened,
                                              formula::RecordingSink<> { missed });
    CHECK(formula::render_trace(missed, { .maxSteps = 30 })
              .ends_with("5. interpolate(#3, at #4) = argument outside the domain of the operation "
                         "[outside the curve, which runs 103 to 241 m]\n"));
    CHECK(!missed.steps[4].selectedSegment.has_value());
}

TEST_CASE("a splice step shows the union, and a failure names its element counted from one", "[curve][trace]")
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveA, curveB), noInputs,
                                                             formula::RecordingSink<> { trace });
    std::string const rendered = formula::render_trace(trace, { .maxSteps = 40 });
    CHECK(rendered.ends_with("7. splice(#3, #6, non-decreasing) = 103/10 m: 31/10 %; 137/10 m: 42/5 %; 163/10 m: 71/5 %; "
                             "103 m: 894/25 %; 127 m: 1154/25 %; 163 m: 1574/25 %\n"));
    CHECK(trace.steps.back().kind == formula::StepKind::CurveSplice);
    CHECK(trace.steps.back().monotone == Monotone::NonDecreasing);

    constexpr auto raised = formula::curve(formula::domain<unit::Metre, fine>(),
                                           formula::series_constant<unit::Percent>(rat(31, 10), rat(84, 10), rat(40)));
    formula::Trace<> failed {};
    (void) formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveA, raised), noInputs,
                                                             formula::RecordingSink<> { failed });
    CHECK(formula::render_trace(failed, { .maxSteps = 40 })
              .ends_with("7. splice(#3, #6, non-decreasing) = argument outside the domain of the operation at element 4 "
                         "[breaks non-decreasing at 103 m]\n"));
    CHECK(failed.steps.back().failedElement == std::optional<std::size_t> { 3 });
    CHECK(failed.steps.back().curveBreak == formula::CurveBreak::AgainstDirection);
}

TEST_CASE("a splice's failure line names the point and the rule, in either order", "[curve][trace]")
{
    for (bool const xFirst: { true, false })
    {
        formula::Trace<> trace {};
        if (xFirst)
            (void) formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveX, curveY),
                                                                     noInputs, formula::RecordingSink<> { trace });
        else
            (void) formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveY, curveX),
                                                                     noInputs, formula::RecordingSink<> { trace });
        CHECK(formula::render_trace(trace, { .maxSteps = 40 })
                  .ends_with("7. splice(#3, #6, non-decreasing) = argument outside the domain of the operation at element 3 "
                             "[duplicate domain point 163 m]\n"));
        CHECK(trace.steps.back().curveBreak == formula::CurveBreak::DuplicatePoint);
    }

    // Falling under NonIncreasing: A and B rise, and the union breaks at
    // 13.7 m, zero-based 1.
    formula::Trace<> rising {};
    (void) formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonIncreasing>(curveA, curveB), noInputs,
                                                             formula::RecordingSink<> { rising });
    CHECK(formula::render_trace(rising, { .maxSteps = 40 })
              .ends_with("7. splice(#3, #6, non-increasing) = argument outside the domain of the operation at element 2 "
                         "[breaks non-increasing at 137/10 m]\n"));
}

TEST_CASE("a splice's step relays an operand's failure without an element", "[curve][trace]")
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveB, computedCurve),
                                                             fallingDomain, formula::RecordingSink<> { trace });
    std::string const rendered = formula::render_trace(trace, { .maxSteps = 40 });
    CHECK(rendered.find("6. curve(#4, #5) = argument outside the domain of the operation at element 2 "
                        "[domain does not ascend at 103 m]\n")
          != std::string::npos);
    CHECK(rendered.ends_with("7. splice(#3, #6, non-decreasing) = argument outside the domain of the operation\n"));
    CHECK(!trace.steps.back().failedElement.has_value());
    CHECK(trace.steps.back().curveBreak == formula::CurveBreak::None);
}

TEST_CASE("a curve whose domain does not ascend names the element in the trace", "[curve][trace]")
{
    constexpr auto disordered = formula::environment(
        formula::measured_series<Opening>(m<Opening>(103), m<Opening>(127), m<Opening>(113), m<Opening>(197),
                                          m<Opening>(163)),
        passingMeasured);
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_curve<Opening, Passing>(formula::curve(formula::series<Opening, 5>, passing), disordered,
                                                             formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
              .ends_with("3. curve(#1, #2) = argument outside the domain of the operation at element 3 "
                         "[domain does not ascend at 113 m]\n"));
    CHECK(trace.steps.back().curveBreak == formula::CurveBreak::NotAscending);

    constexpr auto repeated = formula::environment(
        formula::measured_series<Opening>(m<Opening>(103), m<Opening>(127), m<Opening>(127), m<Opening>(197),
                                          m<Opening>(241)),
        passingMeasured);
    formula::Trace<> twice {};
    (void) formula::checked_evaluate_curve<Opening, Passing>(formula::curve(formula::series<Opening, 5>, passing), repeated,
                                                             formula::RecordingSink<> { twice });
    CHECK(formula::render_trace(twice, { .maxSteps = 30 })
              .ends_with("3. curve(#1, #2) = argument outside the domain of the operation at element 3 "
                         "[duplicate domain point 127 m]\n"));

    // Past a gap, the rule the evaluation judged is named too: 163 m after
    // 197 m, one-based 4.
    constexpr auto gapped = formula::environment(
        formula::measured_series<Opening>(m<Opening>(103), formula::Measured<Opening>::absent(), m<Opening>(197),
                                          m<Opening>(163), m<Opening>(241)),
        passingMeasured);
    formula::Trace<> pastGap {};
    (void) formula::checked_evaluate_curve<Opening, Passing>(formula::curve(formula::series<Opening, 5>, passing), gapped,
                                                             formula::RecordingSink<> { pastGap });
    CHECK(formula::render_trace(pastGap, { .maxSteps = 30 })
              .ends_with("3. curve(#1, #2) = argument outside the domain of the operation at element 4 "
                         "[domain does not ascend at 163 m]\n"));
    CHECK(pastGap.steps.back().curveBreak == formula::CurveBreak::NotAscending);

    // The values fail at element 3, where the domain also falls: the failure
    // is the values', and names no rule of the domain's.
    constexpr auto dividedValues =
        passing / formula::series_constant<unit::One>(rat(1), rat(1), rat(0), rat(1), rat(1));
    formula::Trace<> valuesFailed {};
    (void) formula::checked_evaluate_curve<Opening, Passing>(formula::curve(formula::series<Opening, 5>, dividedValues),
                                                             disordered, formula::RecordingSink<> { valuesFailed });
    CHECK(formula::render_trace(valuesFailed, { .maxSteps = 30 }).ends_with("5. curve(#1, #4) = division by zero at element 3\n"));
    CHECK(valuesFailed.steps.back().curveBreak == formula::CurveBreak::None);
}

// ---- The join with snapping (S17) ----

namespace
{
// A share passing, read off the inverse curve at 50 %, snapped to the nearest
// declared opening: 4733/35 m lies between 127 and 163 m, nearer 127.
constexpr auto snappedOpening = formula::snapped<unit::Metre, screens, formula::SnapTie::TowardLower>(
    formula::interpolate_at(formula::curve(passing, formula::domain<unit::Metre, screens>()),
                            formula::constant<unit::Percent>(rat(50))));
} // namespace

TEST_CASE("a value read off a curve and snapped to a declared domain point", "[curve][snap]")
{
    constexpr auto out = formula::checked_evaluate<Opening>(snappedOpening, screened);
    STATIC_REQUIRE(out.has_value());
    STATIC_REQUIRE(out->measurement().value() == rat(127));

    CHECK(formula::render(snappedOpening)
          == "snap(interpolate(curve(p(i), domain(103, 127, 163, 197, 241 m)), at 50 %), "
             "to 103, 127, 163, 197, 241 m)");
    formula::Documentation const page = formula::document(snappedOpening);
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "p");
    CHECK(page.symbols[0].shape == formula::ValueShape::Series);

    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Opening>(snappedOpening, screened, formula::RecordingSink<> { trace });
    std::string const rendered = formula::render_trace(trace, { .maxSteps = 30 });
    CHECK(rendered.find("5. interpolate(#3, at #4) = 4733/35 m [between 1154/25 and 1574/25 %]\n") != std::string::npos);
    CHECK(rendered.ends_with("6. snap(#5) = 127 m [127 m to 163 m; nearer 127 m]\n"));
}
