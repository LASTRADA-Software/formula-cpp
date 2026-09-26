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

// Invented, and plainly so: screen openings measured in metres -- no sieve
// has one -- and the percentage passing each. The openings are not any
// published aperture series, and their gaps differ, so no point is the
// midpoint of its segment by accident.
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

constexpr formula::BreakpointTable<5> screens { breakpoint(7, 10),
                                                breakpoint(19, 10),
                                                breakpoint(33, 10),
                                                breakpoint(71, 10),
                                                breakpoint(137, 10) };

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
    // 4.2 m is 0.9 of the 3.8 m from 3.3 to 7.1 m: 62.96 + 0.9/3.8 * 7.6 =
    // 64.76 %. Interpolating by position -- halfway from element 3 to element
    // 4 -- would give 66.76 %.
    STATIC_REQUIRE(along<Passing>(grading, metres(42, 10), screened) == rat(1619, 25));
    // On a point: that point's value exactly, the last point included.
    STATIC_REQUIRE(along<Passing>(grading, metres(33, 10), screened) == rat(1574, 25));
    STATIC_REQUIRE(along<Passing>(grading, metres(137, 10), screened) == rat(2444, 25));
    STATIC_REQUIRE(along<Passing>(grading, metres(7, 10), screened) == rat(894, 25));
}

TEST_CASE("the inverse curve answers exactly, off its segment's midpoint", "[curve]")
{
    // 50 % is 3.84 of the 16.8 points from 46.16 to 62.96 %: 1.9 + 3.84/16.8 *
    // 1.4 = 2.22 m, exactly.
    constexpr auto inverse = formula::curve(passing, formula::domain<unit::Metre, screens>());
    STATIC_REQUIRE(along<Opening>(inverse, formula::constant<unit::Percent>(rat(50)), screened) == rat(111, 50));
}

TEST_CASE("outside its domain a curve misses, at both ends", "[curve]")
{
    STATIC_REQUIRE(failure_along<Passing>(grading, metres(6, 10), screened) == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(failure_along<Passing>(grading, metres(14), screened) == formula::ArithmeticError::DomainError);
}

TEST_CASE("a computed domain that does not strictly ascend fails at its first offending element", "[curve]")
{
    // 1.5 m after 1.9 m, and 3.3 m after 7.1 m: the first is reported.
    constexpr auto disordered = formula::environment(
        formula::measured_series<Opening>(m<Opening>(7, 10), m<Opening>(19, 10), m<Opening>(15, 10), m<Opening>(71, 10),
                                          m<Opening>(33, 10)),
        passingMeasured);
    constexpr auto computed = formula::curve(formula::series<Opening, 5>, passing);
    constexpr auto evaluated = formula::checked_evaluate_curve<Opening, Passing>(computed, disordered);
    STATIC_REQUIRE(!evaluated.has_value());
    STATIC_REQUIRE(evaluated.error() == formula::SeriesFailure { formula::ArithmeticError::DomainError, 2 });
    // Interpolating along it relays the error.
    STATIC_REQUIRE(failure_along<Passing>(computed, metres(4), disordered) == formula::ArithmeticError::DomainError);

    // A point stated twice is not ascending either.
    constexpr auto repeated = formula::environment(
        formula::measured_series<Opening>(m<Opening>(7, 10), m<Opening>(19, 10), m<Opening>(19, 10), m<Opening>(71, 10),
                                          m<Opening>(137, 10)),
        passingMeasured);
    STATIC_REQUIRE(formula::checked_evaluate_curve<Opening, Passing>(computed, repeated).error()
                   == formula::SeriesFailure { formula::ArithmeticError::DomainError, 2 });
}

TEST_CASE("an absent element anywhere makes the interpolation absent", "[curve]")
{
    // Element 5, far from 4.2 m, is absent: still no answer (S7, strict).
    constexpr auto lastAbsent = formula::environment(formula::measured_series<Passing>(
        m<Passing>(894, 25), m<Passing>(1154, 25), m<Passing>(1574, 25), m<Passing>(1764, 25), formula::Measured<Passing>::absent()));
    constexpr auto out = formula::checked_evaluate<Passing>(formula::interpolate_at(grading, metres(42, 10)), lastAbsent);
    STATIC_REQUIRE(out.has_value());
    STATIC_REQUIRE(out->measurement().is_absent());

    // An absent point in a computed domain, likewise.
    constexpr auto domainAbsent = formula::environment(
        formula::measured_series<Opening>(m<Opening>(7, 10), formula::Measured<Opening>::absent(), m<Opening>(33, 10),
                                          m<Opening>(71, 10), m<Opening>(137, 10)),
        passingMeasured);
    constexpr auto fromDomain = formula::checked_evaluate<Passing>(
        formula::interpolate_at(formula::curve(formula::series<Opening, 5>, passing), metres(42, 10)), domainAbsent);
    STATIC_REQUIRE(fromDomain.has_value());
    STATIC_REQUIRE(fromDomain->measurement().is_absent());
}

TEST_CASE("a one-point curve answers at its point and misses everywhere else", "[curve]")
{
    constexpr formula::BreakpointTable<1> onePoint { breakpoint(33, 10) };
    constexpr auto single = formula::curve(formula::domain<unit::Metre, onePoint>(),
                                           formula::series_constant<unit::Percent>(rat(1574, 25)));
    constexpr auto none = formula::environment();
    STATIC_REQUIRE(along<Passing>(single, metres(33, 10), none) == rat(1574, 25));
    STATIC_REQUIRE(failure_along<Passing>(single, metres(32, 10), none) == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(failure_along<Passing>(single, metres(34, 10), none) == formula::ArithmeticError::DomainError);
}

TEST_CASE("a curve evaluates to its domain and values, each in its own quantity's unit", "[curve]")
{
    constexpr auto evaluated = formula::checked_evaluate_curve<Opening, Passing>(grading, screened);
    STATIC_REQUIRE(evaluated.has_value());
    STATIC_REQUIRE(reads(evaluated->domain(), { rat(7, 10), rat(19, 10), rat(33, 10), rat(71, 10), rat(137, 10) }));
    STATIC_REQUIRE(reads(evaluated->values(), { rat(894, 25), rat(1154, 25), rat(1574, 25), rat(1764, 25), rat(2444, 25) }));
    STATIC_REQUIRE(decltype(grading)::length == 5);
    STATIC_REQUIRE(decltype(grading)::domainDimension == unit::Metre.dimension);
    STATIC_REQUIRE(decltype(grading)::dimension == unit::Percent.dimension);
    STATIC_REQUIRE(formula::SeriesNode<decltype(formula::domain<unit::Metre, screens>())>);
    STATIC_REQUIRE(formula::CurveExpression<decltype(grading)>);
    STATIC_REQUIRE(formula::Node<decltype(formula::interpolate_at(grading, metres(4)))>);
}

// ---- Splicing two curves ----

namespace
{
constexpr formula::BreakpointTable<3> coarse { breakpoint(7, 10), breakpoint(19, 10), breakpoint(33, 10) };
constexpr formula::BreakpointTable<3> fine { breakpoint(2, 100), breakpoint(9, 100), breakpoint(3, 10) };

// A: 0.7, 1.9 and 3.3 m at 35.76, 46.16 and 62.96 %. B: 0.02, 0.09 and 0.3 m
// at 3.1, 8.4 and 14.2 %.
constexpr auto curveA =
    formula::curve(formula::domain<unit::Metre, coarse>(),
                   formula::series_constant<unit::Percent>(rat(894, 25), rat(1154, 25), rat(1574, 25)));
constexpr auto curveB = formula::curve(formula::domain<unit::Metre, fine>(),
                                       formula::series_constant<unit::Percent>(rat(31, 10), rat(84, 10), rat(142, 10)));
constexpr auto noInputs = formula::environment();

constexpr std::array<formula::Rational, 6> splicedDomain { rat(2, 100), rat(9, 100), rat(3, 10),
                                                           rat(7, 10),  rat(19, 10), rat(33, 10) };
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
    // Kept in argument order, (A, B) would start at 0.7 m.
    STATIC_REQUIRE(reads(ab->domain(), splicedDomain));
    STATIC_REQUIRE(reads(ab->values(), splicedValues));
    STATIC_REQUIRE(reads(ba->domain(), splicedDomain));
    STATIC_REQUIRE(reads(ba->values(), splicedValues));
    STATIC_REQUIRE(decltype(formula::splice<Monotone::NonDecreasing>(curveA, curveB))::length == 6);
}

TEST_CASE("the direction is judged on the spliced curve, not on each operand", "[curve][splice]")
{
    // B's last value raised to 40 %: B alone still ascends, and so does A, but
    // the union falls from 40 % at 0.3 m to 35.76 % at 0.7 m -- element 3,
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
    // B ends at 0.7 m, where A begins: sorted, the second 0.7 m is element 3.
    constexpr formula::BreakpointTable<3> meeting { breakpoint(2, 100), breakpoint(9, 100), breakpoint(7, 10) };
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
    constexpr formula::BreakpointTable<1> low { breakpoint(3, 10) };
    constexpr formula::BreakpointTable<1> high { breakpoint(19, 10) };
    constexpr auto lowPoint =
        formula::curve(formula::domain<unit::Metre, low>(), formula::series_constant<unit::Percent>(rat(142, 10)));
    constexpr auto highPoint =
        formula::curve(formula::domain<unit::Metre, high>(), formula::series_constant<unit::Percent>(rat(1154, 25)));
    constexpr auto spliced =
        formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(highPoint, lowPoint), noInputs);
    STATIC_REQUIRE(spliced.has_value());
    STATIC_REQUIRE(reads(spliced->domain(), { rat(3, 10), rat(19, 10) }));
    STATIC_REQUIRE(reads(spliced->values(), { rat(142, 10), rat(1154, 25) }));
}

TEST_CASE("a splice interpolates across the join, with nothing rescaled", "[curve][splice]")
{
    // 0.4 m is a quarter of the way from 0.3 m (14.2 %) to 0.7 m (35.76 %):
    // 14.2 + 21.56 / 4 = 19.59 %.
    STATIC_REQUIRE(along<Passing>(formula::splice<Monotone::NonDecreasing>(curveA, curveB), metres(4, 10), noInputs)
                   == rat(1959, 100));
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
    CHECK(formula::render(formula::interpolate_at(computed, metres(42, 10))) == "interpolate(curve(d(i), p(i)), at 21/5 m)");
    CHECK(formula::render<formula::Dialect::Markdown>(formula::interpolate_at(computed, metres(42, 10)))
          == "interpolate(curve(`d(i)`, `p(i)`), at 21/5 m)");
    CHECK(formula::render<formula::Dialect::LaTeX>(
              formula::interpolate_at(formula::curve(formula::series<Opening, 5>, formula::series<Share, 5>), metres(42, 10)))
          == "\\operatorname{interpolate}(\\operatorname{curve}({d}_{i},\\allowbreak {s}_{i}),\\allowbreak \\mathrm{at\\ }21/5\\,\\mathrm{m})");
    // A declared domain renders as its points, one unit for them all.
    CHECK(formula::render(formula::interpolate_at(grading, metres(42, 10)))
          == "interpolate(curve(domain(7/10, 19/10, 33/10, 71/10, 137/10 m), p(i)), at 21/5 m)");
    // In a vocabulary.
    constexpr auto everyone = formula::vocabulary(formula::renames<Opening>("w"));
    CHECK(formula::render(formula::interpolate_at(computed, metres(42, 10)), everyone)
          == "interpolate(curve(w(i), p(i)), at 21/5 m)");
}

TEST_CASE("a splice renders both curves and always states its direction", "[curve][render]")
{
    CHECK(formula::render(formula::splice<Monotone::NonDecreasing>(curveA, curveB))
          == "splice(curve(domain(7/10, 19/10, 33/10 m), values(894/25 %, 1154/25 %, 1574/25 %)), "
             "curve(domain(1/50, 9/100, 3/10 m), values(31/10 %, 42/5 %, 71/5 %)), non-decreasing)");
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
        formula::interpolate_at(formula::curve(formula::series<Opening, 5>, passing), metres(42, 10)));
    CHECK(page.formula == "interpolate(curve(d(i), p(i)), at 21/5 m)");
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
    (void) formula::checked_evaluate<Passing>(formula::interpolate_at(grading, metres(42, 10)), screened,
                                              formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
          == "1. 7/10 m; 19/10 m; 33/10 m; 71/10 m; 137/10 m\n"
             "2. p = 894/25 %; 1154/25 %; 1574/25 %; 1764/25 %; 2444/25 %\n"
             "3. curve(#1, #2) = 7/10 m: 894/25 %; 19/10 m: 1154/25 %; 33/10 m: 1574/25 %; 71/10 m: 1764/25 %; "
             "137/10 m: 2444/25 %\n"
             "4. 21/5 m\n"
             "5. interpolate(#3, at #4) = 1619/25 % [between 33/10 and 71/10 m]\n");
    REQUIRE(trace.steps.size() == 5);
    CHECK(trace.steps[0].kind == formula::StepKind::SeriesDomain);
    CHECK(trace.steps[2].kind == formula::StepKind::CurvePairing);
    CHECK(trace.steps[4].kind == formula::StepKind::CurveInterpolation);
    CHECK(trace.steps[4].selectedSegment == formula::Segment { breakpoint(33, 10), breakpoint(71, 10) });

    // The pairs share the element budget, one unit each: 15 units leave the
    // curve line two pairs and say how many more there are.
    CHECK(formula::render_trace(trace, { .maxSteps = 15 })
          == "1. 7/10 m; 19/10 m; 33/10 m; 71/10 m; 137/10 m\n"
             "2. p = 894/25 %; 1154/25 %; 1574/25 %; 1764/25 %; 2444/25 %\n"
             "3. curve(#1, #2) = 7/10 m: 894/25 %; 19/10 m: 1154/25 %; ... 3 more\n"
             "... 2 further steps not shown\n");
}

TEST_CASE("an interpolation on a point, and a miss, say so in the trace", "[curve][trace]")
{
    formula::Trace<> onPoint {};
    (void) formula::checked_evaluate<Passing>(formula::interpolate_at(grading, metres(33, 10)), screened,
                                              formula::RecordingSink<> { onPoint });
    CHECK(formula::render_trace(onPoint, { .maxSteps = 30 }).ends_with("5. interpolate(#3, at #4) = 1574/25 % [on the row at 33/10 m]\n"));

    formula::Trace<> missed {};
    (void) formula::checked_evaluate<Passing>(formula::interpolate_at(grading, metres(14)), screened,
                                              formula::RecordingSink<> { missed });
    CHECK(formula::render_trace(missed, { .maxSteps = 30 })
              .ends_with("5. interpolate(#3, at #4) = argument outside the domain of the operation "
                         "[outside the curve, which runs 7/10 to 137/10 m]\n"));
    CHECK(!missed.steps[4].selectedSegment.has_value());
}

TEST_CASE("a splice step shows the union, and a failure names its element counted from one", "[curve][trace]")
{
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveA, curveB), noInputs,
                                                             formula::RecordingSink<> { trace });
    std::string const rendered = formula::render_trace(trace, { .maxSteps = 40 });
    CHECK(rendered.ends_with("7. splice(#3, #6, non-decreasing) = 1/50 m: 31/10 %; 9/100 m: 42/5 %; 3/10 m: 71/5 %; "
                             "7/10 m: 894/25 %; 19/10 m: 1154/25 %; 33/10 m: 1574/25 %\n"));
    CHECK(trace.steps.back().kind == formula::StepKind::CurveSplice);
    CHECK(trace.steps.back().monotone == Monotone::NonDecreasing);

    constexpr auto raised = formula::curve(formula::domain<unit::Metre, fine>(),
                                           formula::series_constant<unit::Percent>(rat(31, 10), rat(84, 10), rat(40)));
    formula::Trace<> failed {};
    (void) formula::checked_evaluate_curve<Opening, Passing>(formula::splice<Monotone::NonDecreasing>(curveA, raised), noInputs,
                                                             formula::RecordingSink<> { failed });
    CHECK(formula::render_trace(failed, { .maxSteps = 40 })
              .ends_with("7. splice(#3, #6, non-decreasing) = argument outside the domain of the operation at element 4\n"));
    CHECK(failed.steps.back().failedElement == std::optional<std::size_t> { 3 });
}

TEST_CASE("a curve whose domain does not ascend names the element in the trace", "[curve][trace]")
{
    constexpr auto disordered = formula::environment(
        formula::measured_series<Opening>(m<Opening>(7, 10), m<Opening>(19, 10), m<Opening>(15, 10), m<Opening>(71, 10),
                                          m<Opening>(33, 10)),
        passingMeasured);
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_curve<Opening, Passing>(formula::curve(formula::series<Opening, 5>, passing), disordered,
                                                             formula::RecordingSink<> { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 30 })
              .ends_with("3. curve(#1, #2) = argument outside the domain of the operation at element 3\n"));
}

// ---- The join with snapping (S17) ----

namespace
{
// A share passing, read off the inverse curve at 50 %, snapped to the nearest
// declared opening: 111/50 m lies between 1.9 and 3.3 m, nearer 1.9.
constexpr auto snappedOpening = formula::snapped<unit::Metre, screens, formula::SnapTie::TowardLower>(
    formula::interpolate_at(formula::curve(passing, formula::domain<unit::Metre, screens>()),
                            formula::constant<unit::Percent>(rat(50))));
} // namespace

TEST_CASE("a value read off a curve and snapped to a declared domain point", "[curve][snap]")
{
    constexpr auto out = formula::checked_evaluate<Opening>(snappedOpening, screened);
    STATIC_REQUIRE(out.has_value());
    STATIC_REQUIRE(out->measurement().value() == rat(19, 10));

    CHECK(formula::render(snappedOpening)
          == "snap(interpolate(curve(p(i), domain(7/10, 19/10, 33/10, 71/10, 137/10 m)), at 50 %), "
             "to 7/10, 19/10, 33/10, 71/10, 137/10 m)");
    formula::Documentation const page = formula::document(snappedOpening);
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "p");
    CHECK(page.symbols[0].shape == formula::ValueShape::Series);

    formula::Trace<> trace {};
    (void) formula::checked_evaluate<Opening>(snappedOpening, screened, formula::RecordingSink<> { trace });
    std::string const rendered = formula::render_trace(trace, { .maxSteps = 30 });
    CHECK(rendered.find("5. interpolate(#3, at #4) = 111/50 m [between 1154/25 and 1574/25 %]\n") != std::string::npos);
    CHECK(rendered.ends_with("6. snap(#5) = 19/10 m [19/10 m to 33/10 m; nearer 19/10 m]\n"));
}
