// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/least_squares.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>

namespace
{
namespace unit = formula::unit;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// The shared fixture's quantities (see the plan): invented times and lengths.
struct Elapsed: formula::Quantity<Elapsed, "t", "an invented elapsed time", unit::Second>
{
};
struct Length: formula::Quantity<Length, "L", "an invented length", unit::Millimetre>
{
};
struct Rate: formula::Quantity<Rate, "v", "an invented rate of change", unit::MillimetrePerMinute>
{
};
struct Offset: formula::Quantity<Offset, "L_0", "an invented starting length", unit::Millimetre>
{
};

// t = 1, 2, 4, 7 s and L = 10.2, 10.9, 12.1, 14.3 mm: slope 19/28 mm/s and
// intercept 9.5 mm, computed by hand in the plan. The secant from first to
// last is 41/60 mm/s, x-on-y about 0.6798, through the origin about 2.5786,
// and inputs swapped about 1.4710 -- all different.
constexpr auto fitPoints =
    formula::environment(formula::measured_series<Elapsed>(formula::Measured<Elapsed> { rat(1) },
                                                           formula::Measured<Elapsed> { rat(2) },
                                                           formula::Measured<Elapsed> { rat(4) },
                                                           formula::Measured<Elapsed> { rat(7) }),
                         formula::measured_series<Length>(formula::Measured<Length> { rat(102, 10) },
                                                          formula::Measured<Length> { rat(109, 10) },
                                                          formula::Measured<Length> { rat(121, 10) },
                                                          formula::Measured<Length> { rat(143, 10) }));

constexpr auto fit =
    formula::linear_least_squares(formula::curve(formula::series<Elapsed, 4>, formula::series<Length, 4>),
                                  { .title = "Rate of change", .reference = "Example Standard 12", .section = "5.1" });

// The same pairs in coherent SI, seconds and metres, in the order written.
constexpr std::array<formula::Rational, 4> inOrderTimes { rat(1), rat(2), rat(4), rat(7) };
constexpr std::array<formula::Rational, 4> inOrderLengths {
    rat(102, 10'000), rat(109, 10'000), rat(121, 10'000), rat(143, 10'000)
};
} // namespace

TEST_CASE("least squares gives the exact slope and intercept", "[least-squares]")
{
    // 19/28 mm/s is 19/28 * 60 = 285/7 mm/min in Rate's declared unit. Not the
    // secant (41/60 mm/s), not x-on-y (~0.6798), not through the origin
    // (~2.5786), not swapped (~1.4710).
    constexpr auto slope = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(fit), fitPoints);
    STATIC_REQUIRE(slope->measurement().value() == rat(285, 7));
    constexpr auto intercept = formula::checked_evaluate<Offset>(formula::opaque_output<"intercept">(fit), fitPoints);
    STATIC_REQUIRE(intercept->measurement().value() == rat(19, 2));
    STATIC_REQUIRE(decltype(formula::opaque_output<"slope">(fit))::dimension == formula::dim::Velocity);
    STATIC_REQUIRE(decltype(formula::opaque_output<"intercept">(fit))::dimension == formula::dim::Length);
}

TEST_CASE("the order the points are listed in does not change the fit", "[least-squares]")
{
    // Through compute directly: a curve refuses a domain listed out of order
    // as its own NotAscending failure (plan, C4). Same four pairs, listed
    // 4, 1, 7, 2 s: identical coefficients (defect class 6).
    constexpr std::array<formula::Rational, 4> shuffledTimes { rat(4), rat(1), rat(7), rat(2) };
    constexpr std::array<formula::Rational, 4> shuffledLengths {
        rat(121, 10'000), rat(102, 10'000), rat(143, 10'000), rat(109, 10'000)
    };
    constexpr auto inOrder = formula::LinearLeastSquares::compute<formula::Rational>(
        std::span<formula::Rational const> { inOrderTimes }, std::span<formula::Rational const> { inOrderLengths });
    constexpr auto shuffled = formula::LinearLeastSquares::compute<formula::Rational>(
        std::span<formula::Rational const> { shuffledTimes }, std::span<formula::Rational const> { shuffledLengths });
    STATIC_REQUIRE(inOrder.has_value());
    STATIC_REQUIRE((*inOrder)[0] == rat(19, 2'000));  // 9.5 mm in metres
    STATIC_REQUIRE((*inOrder)[1] == rat(19, 28'000)); // 19/28 mm/s in m/s
    STATIC_REQUIRE(shuffled == inOrder);
}

TEST_CASE("a fit whose domain values are all equal, or which has one point, is the fit's own domain error",
          "[least-squares]")
{
    // One point: no line through it. The fit's own DomainError, never slope 0.
    constexpr auto onePoint =
        formula::environment(formula::measured_series<Elapsed>(formula::Measured<Elapsed> { rat(3) }),
                             formula::measured_series<Length>(formula::Measured<Length> { rat(103, 10) }));
    constexpr auto single = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 1>, formula::series<Length, 1>), { .reference = "Example Standard 12" });
    constexpr auto outcome = formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(single), onePoint);
    STATIC_REQUIRE(!outcome.has_value());
    STATIC_REQUIRE(outcome.error() == formula::ArithmeticError::DomainError);
    formula::Trace<> own {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::opaque_output<"slope">(single), onePoint, formula::RecordingSink { own });
    REQUIRE(formula::opaque_data(own, 3) != nullptr);
    CHECK(formula::opaque_data(own, 3)->failure == formula::OpaqueFailure::Own);

    // All equal, through compute: the same DomainError.
    constexpr std::array<formula::Rational, 4> sameTime { rat(3), rat(3), rat(3), rat(3) };
    constexpr auto flat = formula::LinearLeastSquares::compute<formula::Rational>(
        std::span<formula::Rational const> { sameTime }, std::span<formula::Rational const> { inOrderLengths });
    STATIC_REQUIRE(!flat.has_value());
    STATIC_REQUIRE(flat.error() == formula::ArithmeticError::DomainError);

    // All equal, through a curve: the curve refuses its repeated point
    // itself, so the call relays that failure -- Propagated, not Own (C4).
    constexpr auto repeated =
        formula::environment(formula::measured_series<Elapsed>(formula::Measured<Elapsed> { rat(3) },
                                                               formula::Measured<Elapsed> { rat(3) },
                                                               formula::Measured<Elapsed> { rat(3) },
                                                               formula::Measured<Elapsed> { rat(3) }),
                             formula::measured_series<Length>(formula::Measured<Length> { rat(102, 10) },
                                                              formula::Measured<Length> { rat(109, 10) },
                                                              formula::Measured<Length> { rat(121, 10) },
                                                              formula::Measured<Length> { rat(143, 10) }));
    formula::Trace<> relayed {};
    auto const throughCurve = formula::detail::dispatch<formula::Rational>(
        formula::opaque_output<"slope">(fit), repeated, formula::RecordingSink { relayed });
    REQUIRE(!throughCurve.has_value());
    CHECK(throughCurve.error() == formula::ArithmeticError::DomainError);
    REQUIRE(formula::opaque_data(relayed, 3) != nullptr);
    CHECK(formula::opaque_data(relayed, 3)->failure == formula::OpaqueFailure::Propagated);
}

TEST_CASE("two points give the exact line through them", "[least-squares]")
{
    // (1 s, 10.3 mm), (3 s, 13.9 mm): slope 9/5 mm/s = 108 mm/min, intercept
    // 17/2 mm. A fit that dropped either point would have no line at all.
    constexpr auto twoPoints = formula::environment(
        formula::measured_series<Elapsed>(formula::Measured<Elapsed> { rat(1) }, formula::Measured<Elapsed> { rat(3) }),
        formula::measured_series<Length>(formula::Measured<Length> { rat(103, 10) },
                                         formula::Measured<Length> { rat(139, 10) }));
    constexpr auto line = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 2>, formula::series<Length, 2>), { .reference = "Example Standard 12" });
    STATIC_REQUIRE(formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(line), twoPoints)->measurement().value()
                   == rat(108));
    STATIC_REQUIRE(
        formula::checked_evaluate<Offset>(formula::opaque_output<"intercept">(line), twoPoints)->measurement().value()
        == rat(17, 2));
}

namespace
{
// Point k at ((k + 1)/(k + 2) s, (2k + 3)/(k + 3) mm): a different
// denominator on every point, the spike's shape that overflows from 15
// points (step 3). Invented, and ascending, as a curve's points must be.
template <std::size_t N>
[[nodiscard]] auto distinct_denominators()
{
    std::array<formula::Measured<Elapsed>, N> times;
    std::array<formula::Measured<Length>, N> lengths;
    for (std::size_t k = 0; k < N; ++k)
    {
        auto const position = static_cast<std::int64_t>(k);
        times[k] = formula::Measured<Elapsed> { rat(position + 1, position + 2) };
        lengths[k] = formula::Measured<Length> { rat(2 * position + 3, position + 3) };
    }
    return formula::environment(formula::MeasuredSeries<Elapsed, N> { times },
                                formula::MeasuredSeries<Length, N> { lengths });
}
} // namespace

TEST_CASE("a fit that exceeds Rational's range says Overflow, never a wrong number", "[least-squares]")
{
    constexpr auto fifteen = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 15>, formula::series<Length, 15>), { .reference = "Example Standard 12" });
    auto const overflowing =
        formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(fifteen), distinct_denominators<15>());
    REQUIRE(!overflowing.has_value());
    CHECK(overflowing.error() == formula::ArithmeticError::Overflow);
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::opaque_output<"slope">(fifteen), distinct_denominators<15>(), formula::RecordingSink { recorded });
    REQUIRE(formula::opaque_data(recorded, 3) != nullptr);
    CHECK(formula::opaque_data(recorded, 3)->failure == formula::OpaqueFailure::Own);

    // The control: the same shape at five points fits.
    constexpr auto five = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 5>, formula::series<Length, 5>), { .reference = "Example Standard 12" });
    CHECK(formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(five), distinct_denominators<5>()).has_value());
}

TEST_CASE("least squares works in double, to within the representation", "[least-squares]")
{
    // Through compute directly: a curve is evaluated only in Rational (C3).
    std::array<double, 4> const times { 1.0, 2.0, 4.0, 7.0 };
    std::array<double, 4> const lengths { 0.0102, 0.0109, 0.0121, 0.0143 };
    auto const inDouble =
        formula::LinearLeastSquares::compute<double>(std::span<double const> { times }, std::span<double const> { lengths });
    REQUIRE(inDouble.has_value());
    CHECK(std::abs((*inDouble)[1] - 19.0 / 28'000.0) < 1e-12);
    CHECK(std::abs((*inDouble)[0] - 0.0095) < 1e-12);
}

TEST_CASE("a fit is traced as an opaque call over its curve, and renders as one", "[least-squares][trace][render]")
{
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::opaque_output<"slope">(fit), fitPoints, formula::RecordingSink { recorded });
    CHECK(formula::render_trace(recorded, { .maxSteps = 30 })
          == "1. t = 1 s; 2 s; 4 s; 7 s\n"
             "2. L = 51/5 mm; 109/10 mm; 121/10 mm; 143/10 mm\n"
             "3. curve(#1, #2) = 1 s: 51/5 mm; 2 s: 109/10 mm; 4 s: 121/10 mm; 7 s: 143/10 mm\n"
             "4. linear least squares(#3) = intercept = 19/2 mm; slope = 19/28000 [inside not shown] "
             "[Rate of change, Example Standard 12, 5.1]\n"
             "5. slope of #4 = 19/28000\n");
    CHECK(formula::render(formula::opaque_output<"slope">(fit)) == "linear least squares(t(i), L(i)).slope");
    CHECK(formula::render<formula::Dialect::Markdown>(formula::opaque_output<"slope">(fit))
          == "linear least squares(`t(i)`, `L(i)`).slope");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::opaque_output<"slope">(fit))
          == "\\text{linear least squares}({t}_{i}, {L}_{i})_{\\text{slope}}");
}
