// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/least_squares.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>
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
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

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

// A unit the tests round the fitted rate in, declared here as a method would
// declare it: a millimetre a second, with four decimals, so that the padded
// style writes 0.6786 as it is.
constexpr formula::Unit MillimetrePerSecond { .dimension = formula::dim::Velocity,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 1000,
                                              .symbolText = formula::symbol("mm/s"),
                                              .decimals = 4 };

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
constexpr auto roundedSlope =
    formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(fit);

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
    // as its own NotAscending failure. Same four pairs, listed
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

    // All equal in double, where the mean of three 0.1 s is not 0.1 and the
    // spread is rounding noise, never zero: decided on the points themselves,
    // it is the same DomainError and no slope of noise.
    std::array<double, 3> const sameInDouble { 0.1, 0.1, 0.1 };
    std::array<double, 3> const valuesInDouble { 0.0103, 0.0139, 0.0191 };
    auto const flatInDouble = formula::LinearLeastSquares::compute<double>(std::span<double const> { sameInDouble },
                                                                           std::span<double const> { valuesInDouble });
    REQUIRE(!flatInDouble.has_value());
    CHECK(flatInDouble.error() == formula::ArithmeticError::DomainError);

    // No points at all: fewer than two, so the fit's DomainError, not a
    // division by a count of zero.
    auto const noPoints = formula::LinearLeastSquares::compute<formula::Rational>(std::span<formula::Rational const> {},
                                                                                  std::span<formula::Rational const> {});
    REQUIRE(!noPoints.has_value());
    CHECK(noPoints.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a fit handed spans of different lengths is a domain error, never a read past the shorter", "[least-squares]")
{
    // compute is public, and a consumer calling it with their own arrays can
    // mismatch them; a curve never does. Three points and two values.
    std::array<formula::Rational, 3> const threePoints { rat(1), rat(2), rat(4) };
    std::array<formula::Rational, 2> const twoValues { rat(103, 10), rat(139, 10) };
    auto const mismatched = formula::LinearLeastSquares::compute<formula::Rational>(
        std::span<formula::Rational const> { threePoints }, std::span<formula::Rational const> { twoValues });
    REQUIRE(!mismatched.has_value());
    CHECK(mismatched.error() == formula::ArithmeticError::DomainError);
    // And the other way round.
    auto const reversed = formula::LinearLeastSquares::compute<formula::Rational>(
        std::span<formula::Rational const> { threePoints }.first(2), std::span<formula::Rational const> { threePoints });
    REQUIRE(!reversed.has_value());
    CHECK(reversed.error() == formula::ArithmeticError::DomainError);
    // All equal, through a curve: the curve refuses its repeated point
    // itself, so the call relays that failure -- Propagated, not Own.
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
    // Through compute directly: a curve is evaluated only in Rational.
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
             "4. linear least squares(#3) = intercept = 19/2 mm; slope = 19/28 mm/s [inside not shown] "
             "[Rate of change, Example Standard 12, 5.1]\n"
             "5. slope of #4 = 19/28 mm/s\n");
    CHECK(formula::render(formula::opaque_output<"slope">(fit)) == "linear least squares(t(i), L(i)).slope");
    CHECK(formula::render<formula::Dialect::Markdown>(formula::opaque_output<"slope">(fit))
          == "linear least squares(`t(i)`, `L(i)`).slope");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::opaque_output<"slope">(fit))
          == "\\text{linear least squares}({t}_{i}, {L}_{i})_{\\text{slope}}");
}

namespace
{
struct Fitted
{
};
struct Nominal
{
};

// Invented dimensionless factors: a scale read inside the fit's input and
// outside it, and a correction read outside it only.
struct Scale: formula::Quantity<Scale, "s", "an invented scale", unit::One>
{
};
struct Correction: formula::Quantity<Correction, "c", "an invented correction", unit::One>
{
};

using TenthOfMillimetrePerMinute =
    formula::RoundingRule<unit::MillimetrePerMinute, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>;

// The fitted rate, rounded to 0.1 mm/min: 285/7 = 40.714... gives 40.7, where
// the secant (41/60 mm/s = 41 mm/min) gives 41.0 and x-on-y (40.79) 40.8.
constexpr auto fitMethod =
    formula::method(formula::variants(formula::variant<Fitted>(formula::opaque_output<"slope">(fit)),
                                      formula::variant<Nominal>(formula::constant<unit::MillimetrePerMinute>(rat(401, 10)))),
                    TenthOfMillimetrePerMinute {},
                    formula::constraints());

constexpr formula::Citation annex { .title = "Rate of change",
                                    .reference = "Example Standard 12:2021 NA",
                                    .section = "NA.5" };

// The secant from the first point to the last, as ordinary arithmetic: a
// jurisdiction that fits differently replaces the variant wholesale.
constexpr auto measuredCurve = formula::curve(formula::series<Elapsed, 4>, formula::series<Length, 4>);
constexpr auto secant = (formula::interpolate_at(measuredCurve, formula::constant<unit::Second>(rat(7)))
                         - formula::interpolate_at(measuredCurve, formula::constant<unit::Second>(rat(1))))
                        / (formula::constant<unit::Second>(rat(7)) - formula::constant<unit::Second>(rat(1)));

// The fit over scaled lengths, and the correction applied after it: a scale
// read inside the call and outside it, a correction outside it only.
constexpr auto scaledFit = formula::linear_least_squares(
    formula::curve(formula::series<Elapsed, 4>, formula::series<Length, 4>* formula::var<Scale>),
    { .reference = "Example Standard 12", .section = "5.2" });
constexpr auto scaledMethod =
    formula::method(formula::variants(formula::variant<Fitted>(formula::opaque_output<"slope">(scaledFit)
                                                               * formula::var<Scale> * formula::var<Correction>)),
                    TenthOfMillimetrePerMinute {},
                    formula::constraints());
constexpr auto correctedMethod = formula::method(
    formula::variants(formula::variant<Fitted>(formula::opaque_output<"slope">(fit) * formula::var<Correction>)),
    TenthOfMillimetrePerMinute {},
    formula::constraints());
} // namespace

TEST_CASE("a fit is a method variant, and the method's rule rounds it", "[least-squares][method]")
{
    // 40.7 mm/min is 407/600000 m/s in the coherent unit the method answers in.
    constexpr auto fitted = formula::evaluate_method<Fitted>(fitMethod, fitPoints);
    STATIC_REQUIRE(fitted.has_value());
    STATIC_REQUIRE(fitted->value() == rat(407, 600'000));
    constexpr auto nominal = formula::evaluate_method<Nominal>(fitMethod, fitPoints);
    STATIC_REQUIRE(nominal->value() == rat(401, 600'000));

    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Fitted>(fitMethod, fitPoints, formula::RecordingSink { recorded });
    std::string const text = formula::render_trace(recorded, { .maxSteps = 30 });
    CHECK(text.find("linear least squares(#3) = intercept = 19/2 mm; slope = 19/28 mm/s [inside not shown]")
          != std::string::npos);
    CHECK(text.find("round(#5, in mm/min) = 407/10 mm/min") != std::string::npos);
}

TEST_CASE("a jurisdiction that fits differently replaces the variant, and the trace shows no fit",
          "[least-squares][overlay]")
{
    constexpr auto replaced = formula::apply(formula::overlay(formula::replace_variant<Fitted>(secant, annex)), fitMethod);
    // The secant is 41/60 mm/s = 41 mm/min, 41.0 after rounding: not 40.7.
    constexpr auto outcome = formula::evaluate_method<Fitted>(replaced, fitPoints);
    STATIC_REQUIRE(outcome->value() == rat(41, 60'000));

    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Fitted>(replaced, fitPoints, formula::RecordingSink { recorded });
    bool sawReplacement = false;
    bool sawFit = false;
    for (formula::Step<> const& each: recorded.steps)
    {
        sawReplacement = sawReplacement || each.kind == formula::StepKind::ReplacedVariant;
        sawFit = sawFit || each.kind == formula::StepKind::OpaqueOperation;
    }
    CHECK(sawReplacement);
    CHECK(!sawFit);
}

TEST_CASE("a constant read outside the fit is rewritten and the call is left as it was", "[least-squares][overlay]")
{
    constexpr auto corrected =
        formula::apply(formula::overlay(formula::with_constant<Correction>(rat(89, 100), annex)), correctedMethod);
    // The call is the very type it was: nothing in it read the correction.
    using Rewritten = std::remove_cvref_t<decltype(std::get<0>(corrected.variantSet.cases).expression.lhs)>;
    STATIC_REQUIRE(std::is_same_v<Rewritten, std::remove_cvref_t<decltype(formula::opaque_output<"slope">(fit))>>);
    // 285/7 * 0.89 = 36.235... mm/min, 36.2 after rounding.
    STATIC_REQUIRE(formula::evaluate_method<Fitted>(corrected, fitPoints)->value() == rat(362, 600'000));
}

TEST_CASE("a constant read inside the fit's input is fixed for the whole call", "[least-squares][overlay]")
{
    // The environment has no scale and no correction at all, so a use of
    // either left reading it would not compile. 285/7 * 1.03 inside, * 1.03
    // and * 0.89 outside: 285/7 * 0.944201 = 38.44... mm/min, 38.4.
    constexpr auto scaled = formula::apply(formula::overlay(formula::with_constant<Scale>(rat(103, 100), annex),
                                                            formula::with_constant<Correction>(rat(89, 100), annex)),
                                           scaledMethod);
    STATIC_REQUIRE(formula::evaluate_method<Fitted>(scaled, fitPoints)->value() == rat(384, 600'000));
    // The rewrite knows the call all the way down: every result check of the
    // overlay runs over it (a call it did not know would silence them).
    STATIC_REQUIRE(formula::detail::ConstantRewriteOf<
                   formula::ConstantOverride<Scale>,
                   std::remove_cvref_t<decltype(formula::opaque_output<"slope">(scaledFit))>>::known);

    // The fixed scale inside the call is a step of the call's input, with the
    // overlay's citation.
    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Fitted>(scaled, fitPoints, formula::RecordingSink { recorded });
    std::string const text = formula::render_trace(recorded, { .maxSteps = 40 });
    CHECK(text.find("s = 103/100 [fixed by jurisdiction overlay") != std::string::npos);
    // The rebuilt call keeps the call's own citation.
    CHECK(text.find("[inside not shown] [Example Standard 12, 5.2]") != std::string::npos);
}

namespace
{
// The scale read only inside the fit, and nowhere else.
constexpr auto insideOnlyMethod =
    formula::method(formula::variants(formula::variant<Fitted>(formula::opaque_output<"slope">(scaledFit)),
                                      formula::variant<Nominal>(formula::constant<unit::MillimetrePerMinute>(rat(401, 10)))),
                    TenthOfMillimetrePerMinute {},
                    formula::constraints());
} // namespace

TEST_CASE("a derivation puts a use inside the fit that a later constant then fixes", "[least-squares][overlay]")
{
    // The scale is derived as twice the correction, which puts a use of the
    // correction inside the call's input; the constant listed after it fixes
    // that use: 0.515 * 2 = 1.03, and 285/7 * 1.03 = 41.93... mm/min, 41.9.
    // Listed the other way round, the constant fixes a correction nothing
    // reads yet, and is refused (negative overlay_constant_before_derivation_inside_opaque).
    constexpr auto derivedThenFixed =
        formula::apply(formula::overlay(formula::add_derived<Scale>(formula::var<Correction> * rat(2), annex),
                                        formula::with_constant<Correction>(rat(515, 1000), annex)),
                       insideOnlyMethod);
    STATIC_REQUIRE(formula::evaluate_method<Fitted>(derivedThenFixed, fitPoints)->value() == rat(419, 600'000));

    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Fitted>(derivedThenFixed, fitPoints, formula::RecordingSink { recorded });
    std::string const text = formula::render_trace(recorded, { .maxSteps = 40 });
    CHECK(text.find("c = 103/200 [fixed by jurisdiction overlay") != std::string::npos);
    CHECK(text.find("[derived by jurisdiction overlay") != std::string::npos);
    // The derivation is inside the call's input: the call's line comes after it.
    CHECK(text.find("[derived by jurisdiction overlay") < text.find("linear least squares("));
}

TEST_CASE("a quantity defined by a fit is derived, and a constant inside that fit fixes it", "[least-squares][overlay]")
{
    // Rate defined as the slope of the scaled fit -- a definition that is a
    // fit's output -- and the scale inside it fixed at 1.03 after it: the
    // method reads Rate plainly, and 285/7 * 1.03 = 41.93... mm/min, 41.9.
    constexpr auto readsRate = formula::method(formula::variants(formula::variant<Fitted>(formula::var<Rate>)),
                                               TenthOfMillimetrePerMinute {},
                                               formula::constraints());
    constexpr auto defined =
        formula::apply(formula::overlay(formula::add_derived<Rate>(formula::opaque_output<"slope">(scaledFit), annex),
                                        formula::with_constant<Scale>(rat(103, 100), annex)),
                       readsRate);
    STATIC_REQUIRE(formula::evaluate_method<Fitted>(defined, fitPoints)->value() == rat(419, 600'000));
}

TEST_CASE("two outputs of one rewritten call are each rewritten, and each keeps its citation", "[least-squares][overlay]")
{
    // intercept / 1 min + slope, over the scaled fit with the scale fixed at
    // 1.03: 9.785 mm/min + 41.936 mm/min = 51.72, 51.7.
    constexpr auto both =
        formula::method(formula::variants(formula::variant<Fitted>(formula::opaque_output<"intercept">(scaledFit)
                                                                       / formula::constant<unit::Minute>(rat(1))
                                                                   + formula::opaque_output<"slope">(scaledFit))),
                        TenthOfMillimetrePerMinute {},
                        formula::constraints());
    constexpr auto fixed = formula::apply(formula::overlay(formula::with_constant<Scale>(rat(103, 100), annex)), both);
    STATIC_REQUIRE(formula::evaluate_method<Fitted>(fixed, fitPoints)->value() == rat(517, 600'000));

    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Fitted>(fixed, fitPoints, formula::RecordingSink { recorded });
    std::string const text = formula::render_trace(recorded, { .maxSteps = 60 });
    std::size_t cited = 0;
    for (std::size_t at = text.find("[inside not shown] [Example Standard 12, 5.2]"); at != std::string::npos;
         at = text.find("[inside not shown] [Example Standard 12, 5.2]", at + 1))
        ++cited;
    CHECK(cited == 2);
}

TEST_CASE("a pinned or pruned variant holding a fit keeps the constant fixed inside it", "[least-squares][overlay]")
{
    // Pinned, in either order: 41.9. A pin that kept Nominal would give 40.1,
    // and a constant that missed the fit would not compile (no scale in the
    // environment).
    constexpr auto pinFirst = formula::apply(
        formula::overlay(formula::pin_variant<Fitted>(annex), formula::with_constant<Scale>(rat(103, 100), annex)),
        insideOnlyMethod);
    constexpr auto constantFirst = formula::apply(
        formula::overlay(formula::with_constant<Scale>(rat(103, 100), annex), formula::pin_variant<Fitted>(annex)),
        insideOnlyMethod);
    STATIC_REQUIRE(std::tuple_size_v<decltype(pinFirst.variantSet.cases)> == 1);
    STATIC_REQUIRE(std::tuple_size_v<decltype(constantFirst.variantSet.cases)> == 1);
    STATIC_REQUIRE(formula::evaluate_method<Fitted>(pinFirst, fitPoints)->value() == rat(419, 600'000));
    STATIC_REQUIRE(formula::evaluate_method<Fitted>(constantFirst, fitPoints)->value() == rat(419, 600'000));

    // Nominal pruned: the fit is left, with the scale fixed inside it.
    constexpr auto pruned = formula::apply(
        formula::overlay(formula::with_constant<Scale>(rat(103, 100), annex), formula::prune_variant<Nominal>(annex)),
        insideOnlyMethod);
    STATIC_REQUIRE(std::tuple_size_v<decltype(pruned.variantSet.cases)> == 1);
    STATIC_REQUIRE(formula::evaluate_method<Fitted>(pruned, fitPoints)->value() == rat(419, 600'000));
}
TEST_CASE("a constant read only inside the fit's input is still a use of it", "[least-squares][overlay]")
{
    constexpr auto insideOnly =
        formula::method(formula::variants(formula::variant<Fitted>(formula::opaque_output<"slope">(scaledFit))),
                        TenthOfMillimetrePerMinute {},
                        formula::constraints());
    constexpr auto fixed = formula::apply(formula::overlay(formula::with_constant<Scale>(rat(103, 100), annex)), insideOnly);
    // 285/7 * 1.03 = 41.93... mm/min, 41.9.
    STATIC_REQUIRE(formula::evaluate_method<Fitted>(fixed, fitPoints)->value() == rat(419, 600'000));
}

namespace
{
// The scaled fit's slope, rounded where it is used: the scale is read inside
// the call only.
constexpr auto roundedInsideMethod = formula::method(
    formula::variants(formula::variant<Fitted>(
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
            scaledFit))),
    TenthOfMillimetrePerMinute {},
    formula::constraints());
} // namespace

TEST_CASE("rounded output: a constant read inside a rounded output's call is fixed for the whole call",
          "[least-squares][overlay]")
{
    // 19/28 * 1.03 = 0.69892857... mm/s, 0.6989 at 4 dp; 41.934 mm/min, 41.9
    // after the method's rule. The environment has no scale: a use left
    // reading it would not compile.
    constexpr auto fixed = formula::apply(formula::overlay(formula::with_constant<Scale>(rat(103, 100), annex)), roundedInsideMethod);
    using Rewritten = formula::detail::ConstantRewriteOf<
        formula::ConstantOverride<Scale>,
        std::remove_cvref_t<decltype(formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 },
                                                             formula::RoundingMode::HalfEven>(scaledFit))>>;
    STATIC_REQUIRE(Rewritten::known);
    auto const outcome = formula::evaluate_method<Fitted>(fixed, fitPoints);
    REQUIRE(outcome.has_value());
    CHECK(outcome->value() == rat(419, 600'000));

    // The rewrite keeps the rounding: the value above cannot show it, since
    // the unrounded 0.698928... mm/s is 41.9 mm/min after the method's rule too.
    STATIC_REQUIRE(Rewritten::type::unit == MillimetrePerSecond);
    STATIC_REQUIRE(Rewritten::type::places == formula::DecimalPlaces { 4 });
    STATIC_REQUIRE(Rewritten::type::mode == formula::RoundingMode::HalfEven);
    CHECK(formula::render(std::get<0>(fixed.variantSet.cases).expression).starts_with("round(linear least squares("));
}

TEST_CASE("a scoped vocabulary renames a fit's input in the trace, the render and the page", "[least-squares][vocabulary]")
{
    constexpr auto north = formula::vocabulary(formula::renames<Length>("l"));
    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Fitted>(fitMethod, fitPoints, formula::RecordingSink { recorded, north });
    CHECK(formula::render_trace(recorded, { .maxSteps = 30 }).find("2. l = 51/5 mm") != std::string::npos);
    CHECK(formula::render(formula::opaque_output<"slope">(fit), north) == "linear least squares(t(i), l(i)).slope");
}

TEST_CASE("a scoped vocabulary reaches a constant fixed inside a rewritten fit", "[least-squares][vocabulary][overlay]")
{
    // Length and Scale renamed; the scale fixed inside the call and outside
    // it, the correction too. The rewritten variant renders and documents in
    // the vocabulary, the fixed scale inside the call included.
    constexpr auto north = formula::vocabulary(formula::renames<Length>("l"), formula::renames<Scale>("k"));
    constexpr auto fixed = formula::apply(formula::overlay(formula::with_constant<Scale>(rat(103, 100), annex),
                                                           formula::with_constant<Correction>(rat(89, 100), annex)),
                                          scaledMethod);
    auto const& rewritten = std::get<0>(fixed.variantSet.cases).expression;
    CHECK(formula::render(rewritten, north) == "linear least squares(t(i), l(i) * k).slope * k * c");
    formula::Documentation const page = formula::document(rewritten, north);
    std::vector<std::string_view> symbols;
    for (auto const& row: page.symbols)
        symbols.push_back(row.symbol);
    CHECK(symbols == std::vector<std::string_view> { "t", "l", "k", "c" });

    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Fitted>(fixed, fitPoints, formula::RecordingSink { recorded, north });
    CHECK(formula::render_trace(recorded, { .maxSteps = 40 }).find("k = 103/100 [fixed by jurisdiction overlay")
          != std::string::npos);
}

TEST_CASE("rounded output: a rounded slope renders as the rounding it states in every dialect", "[least-squares][render]")
{
    CHECK(formula::render(roundedSlope) == "round(linear least squares(t(i), L(i)).slope, to 4 dp of mm/s)");
    CHECK(formula::render<formula::Dialect::Markdown>(roundedSlope)
          == "round(linear least squares(`t(i)`, `L(i)`).slope, to 4 dp of mm/s)");
    CHECK(formula::render<formula::Dialect::LaTeX>(roundedSlope)
          == "\\operatorname{round}_{4\\,\\mathrm{mm/s}}(\\text{linear least squares}({t}_{i}, {L}_{i})_{\\text{slope}})");
    // The inputs follow the vocabulary; the operation's name and the unit do not.
    constexpr auto renamed = formula::vocabulary(formula::renames<Elapsed>("t_e"));
    CHECK(formula::render(roundedSlope, renamed) == "round(linear least squares(t_e(i), L(i)).slope, to 4 dp of mm/s)");
}

TEST_CASE("rounded output: a page lists the fit once however its outputs are used", "[least-squares][document]")
{
    constexpr auto both = formula::opaque_output<"intercept">(fit) + roundedSlope * formula::constant<unit::Second>(rat(1));
    formula::Documentation const page = formula::document(both);
    REQUIRE(page.opaqueOperations.size() == 1);
    CHECK(page.opaqueOperations[0].name == "linear least squares");
    CHECK(page.opaqueOperations[0].outputs == std::vector<std::string_view> { "intercept", "slope" });
    REQUIRE(page.citations.size() == 1);
    CHECK(page.citations[0].section == "5.1");
    CHECK(page.formula.find("round(linear least squares(t(i), L(i)).slope, to 4 dp of mm/s)") != std::string::npos);
    REQUIRE(page.symbols.size() == 2); // t and L, each once
    // Alone, a rounded output lists its call as well: above, the intercept
    // alone would list it.
    formula::Documentation const alone = formula::document(roundedSlope);
    REQUIRE(alone.opaqueOperations.size() == 1);
    CHECK(alone.opaqueOperations[0].name == "linear least squares");
    CHECK(alone.citations.size() == 1);
}

TEST_CASE("rounded output: the fit's trace names its outputs without values and states the rounding",
          "[least-squares][trace]")
{
    auto const explained = formula::explain<Rate>(roundedSlope, fitPoints);
    CHECK(explained.outcome.measurement().value() == rat(10179, 250)); // 0.6786 mm/s is 40.716 mm/min
    std::string const fractions = formula::render_trace(explained.trace, { .maxSteps = 30 });
    CHECK(fractions
          == "1. t = 1 s; 2 s; 4 s; 7 s\n"
             "2. L = 51/5 mm; 109/10 mm; 121/10 mm; 143/10 mm\n"
             "3. curve(#1, #2) = 1 s: 51/5 mm; 2 s: 109/10 mm; 4 s: 121/10 mm; 7 s: 143/10 mm\n"
             "4. linear least squares(#3) = intercept, slope: rounded where used [inside not shown] "
             "[Rate of change, Example Standard 12, 5.1]\n"
             "5. round(slope of #4, to 4 dp of mm/s) = 3393/5000 mm/s [nearest, ties to even]\n");
    std::string const decimals =
        formula::render_trace(explained.trace, { .maxSteps = 30, .numbers = formula::NumberStyle::exact_decimal() });
    CHECK(decimals
          == "1. t = 1 s; 2 s; 4 s; 7 s\n"
             "2. L = 10.2 mm; 10.9 mm; 12.1 mm; 14.3 mm\n"
             "3. curve(#1, #2) = 1 s: 10.2 mm; 2 s: 10.9 mm; 4 s: 12.1 mm; 7 s: 14.3 mm\n"
             "4. linear least squares(#3) = intercept, slope: rounded where used [inside not shown] "
             "[Rate of change, Example Standard 12, 5.1]\n"
             "5. round(slope of #4, to 4 dp of mm/s) = 0.6786 mm/s [nearest, ties to even]\n");
    // The rounded decimal is the step's exact value: no style marks it.
    std::string const approximate = formula::render_trace(
        explained.trace, { .maxSteps = 30, .numbers = formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven) });
    CHECK(approximate == decimals);

    // What the trace holds: the call's row names both outputs, holds no value,
    // and says the call answered; the output's step holds the rounding.
    formula::OpaqueStepData<> const* const callRow = formula::opaque_data(explained.trace, 3);
    REQUIRE(callRow != nullptr);
    CHECK(callRow->values == formula::OpaqueValues::RoundedWhereUsed);
    CHECK(callRow->answered);
    REQUIRE(callRow->outputs.size() == 2);
    CHECK(callRow->outputs[1].name == "slope");
    CHECK(!callRow->outputs[0].value.has_value());
    CHECK(!callRow->outputs[1].value.has_value());
    formula::Step<> const& rounding = explained.trace.steps[4];
    CHECK(rounding.kind == formula::StepKind::RoundedOpaqueOutput);
    CHECK(rounding.granularity == 4);
    CHECK(rounding.mode == formula::RoundingMode::HalfEven);
    CHECK(rounding.unit == MillimetrePerSecond);
    CHECK(rounding.value == rat(3393, 5'000'000));
    REQUIRE(formula::opaque_output_data(explained.trace, 4) != nullptr);
    CHECK(formula::opaque_output_data(explained.trace, 4)->outputIndex == 1);

    // The exact route's row is as it was: every value, Exact, and answered.
    auto const exactRoute = formula::explain<Rate>(formula::opaque_output<"slope">(fit), fitPoints);
    CHECK(formula::opaque_data(exactRoute.trace, 3)->values == formula::OpaqueValues::Exact);
    CHECK(formula::opaque_data(exactRoute.trace, 3)->answered);
    CHECK(formula::opaque_data(exactRoute.trace, 3)->outputs[1].value == rat(19, 28'000));
}

TEST_CASE("rounded output: a padded style pads the rounded value to its unit's decimals", "[least-squares][trace]")
{
    // 19/2 mm at 0 dp is a tie: 10 mm under HalfEven; the millimetre declares one decimal.
    constexpr auto roundedIntercept =
        formula::rounded_output<"intercept", unit::Millimetre, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfEven>(fit);
    auto const explained = formula::explain<Offset>(roundedIntercept, fitPoints);
    CHECK(explained.outcome.measurement().value() == rat(10));
    std::string const fractions = formula::render_trace(explained.trace, { .maxSteps = 30 });
    CHECK(fractions.find("5. round(intercept of #4, to 0 dp of mm) = 10 mm [nearest, ties to even]\n") != std::string::npos);
    std::string const padded = formula::render_trace(
        explained.trace, { .maxSteps = 30, .numbers = formula::NumberStyle::exact_decimal(formula::DecimalPadding::Padded) });
    CHECK(padded.find("5. round(intercept of #4, to 0 dp of mm) = 10.0 mm [nearest, ties to even]\n") != std::string::npos);
}

TEST_CASE("rounded output: a failure reads as the call's own or as carried up from it", "[least-squares][trace]")
{
    // One point: the fit's own DomainError.
    constexpr auto onePoint =
        formula::environment(formula::measured_series<Elapsed>(formula::Measured<Elapsed> { rat(3) }),
                             formula::measured_series<Length>(formula::Measured<Length> { rat(103, 10) }));
    constexpr auto single = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 1>, formula::series<Length, 1>), { .reference = "Example Standard 12" });
    formula::Trace<> own {};
    (void) formula::detail::dispatch<formula::Rational>(
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(single),
        onePoint, formula::RecordingSink { own });
    std::string const ownText = formula::render_trace(own, { .maxSteps = 30 });
    CHECK(ownText.find("4. linear least squares(#3) = argument outside the domain of the operation [inside not shown] "
                       "[the operation itself failed, not any input] [Example Standard 12]\n")
          != std::string::npos);
    CHECK(ownText.find("5. round(slope of #4, to 4 dp of mm/s) = argument outside the domain of the operation "
                       "[the operation itself failed, not any input]\n")
          != std::string::npos);

    // Repeated points: the curve's failure, relayed by the call.
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
    (void) formula::detail::dispatch<formula::Rational>(roundedSlope, repeated, formula::RecordingSink { relayed });
    CHECK(formula::render_trace(relayed, { .maxSteps = 30 })
              .find("5. round(slope of #4, to 4 dp of mm/s) = argument outside the domain of the operation [carried up from #4]\n")
          != std::string::npos);

    // An absent length: the call and the output are absent, and say so.
    constexpr auto gap =
        formula::environment(formula::measured_series<Elapsed>(formula::Measured<Elapsed> { rat(1) },
                                                               formula::Measured<Elapsed> { rat(2) },
                                                               formula::Measured<Elapsed> { rat(4) },
                                                               formula::Measured<Elapsed> { rat(7) }),
                             formula::measured_series<Length>(formula::Measured<Length> { rat(102, 10) },
                                                              formula::Measured<Length>::absent(),
                                                              formula::Measured<Length> { rat(121, 10) },
                                                              formula::Measured<Length> { rat(143, 10) }));
    auto const absent = formula::explain<Rate>(roundedSlope, gap);
    CHECK(absent.outcome.is_empty());
    std::string const absentText = formula::render_trace(absent.trace, { .maxSteps = 30 });
    CHECK(absentText.find("4. linear least squares(#3) = (not measured) [inside not shown] [Rate of change, Example Standard 12, 5.1]\n")
          != std::string::npos);
    CHECK(absentText.find("5. round(slope of #4, to 4 dp of mm/s) = (not measured) [nearest, ties to even]\n") != std::string::npos);
}

namespace
{
template <formula::detail::FixedString Name, formula::Unit U, formula::DecimalPlaces Places, formula::RoundingMode Mode,
          typename Call, typename Env>
void check_rounded_route(Call const& call, Env const& inputs)
{
    auto const fused =
        formula::checked_evaluate_si<formula::Rational>(formula::rounded_output<Name, U, Places, Mode>(call), inputs);
    auto const afterwards = formula::checked_evaluate_si<formula::Rational>(
        formula::rounded<U, Places, Mode>(formula::opaque_output<Name>(call)), inputs);
    REQUIRE(afterwards.has_value());
    REQUIRE(fused.has_value());
    CHECK(**fused == **afterwards);
}

template <formula::detail::FixedString Name, formula::Unit U, formula::DecimalPlaces Places, typename Call, typename Env>
void check_rounded_route_in_every_mode(Call const& call, Env const& inputs)
{
    check_rounded_route<Name, U, Places, formula::RoundingMode::HalfAwayFromZero>(call, inputs);
    check_rounded_route<Name, U, Places, formula::RoundingMode::HalfTowardZero>(call, inputs);
    check_rounded_route<Name, U, Places, formula::RoundingMode::HalfEven>(call, inputs);
    check_rounded_route<Name, U, Places, formula::RoundingMode::Ceiling>(call, inputs);
    check_rounded_route<Name, U, Places, formula::RoundingMode::Floor>(call, inputs);
    check_rounded_route<Name, U, Places, formula::RoundingMode::TowardZero>(call, inputs);
    check_rounded_route<Name, U, Places, formula::RoundingMode::AwayFromZero>(call, inputs);
}

/// The intercept of @p call over @p inputs, rounded to 4 dp of mm under @p Mode, in metres.
template <formula::RoundingMode Mode, typename Call, typename Env>
[[nodiscard]] formula::Rational rounded_intercept(Call const& call, Env const& inputs)
{
    auto const rounded = formula::checked_evaluate_si<formula::Rational>(
        formula::rounded_output<"intercept", unit::Millimetre, formula::DecimalPlaces { 4 }, Mode>(call), inputs);
    REQUIRE(rounded.has_value());
    REQUIRE(rounded->has_value());
    return **rounded;
}
} // namespace

TEST_CASE("rounded output: compute_exact states the fit exactly", "[least-squares]")
{
    // The hook is taken. A hook that is not noexcept, has fewer than 4 limbs or
    // returns another type is skipped silently for compute<Rational>, so this
    // is the only thing that notices one.
    STATIC_REQUIRE(formula::detail::declares_compute_exact<
                   formula::LinearLeastSquares,
                   std::remove_cvref_t<decltype(formula::curve(formula::series<Elapsed, 4>, formula::series<Length, 4>))>>);
    auto const exact = formula::LinearLeastSquares::compute_exact(std::span<formula::Rational const> { inOrderTimes },
                                                                  std::span<formula::Rational const> { inOrderLengths });
    REQUIRE(exact.has_value());
    CHECK(*formula::detail::narrow_wide_ratio((*exact)[0]) == rat(19, 2'000));  // 9.5 mm in metres
    CHECK(*formula::detail::narrow_wide_ratio((*exact)[1]) == rat(19, 28'000)); // 19/28 mm/s in m/s
    // The same refusals as compute: one point, all equal, spans of different lengths.
    CHECK(formula::LinearLeastSquares::compute_exact(std::span<formula::Rational const> { inOrderTimes }.first(1),
                                                     std::span<formula::Rational const> { inOrderLengths }.first(1))
          == std::unexpected { formula::ArithmeticError::DomainError });
    std::array<formula::Rational, 4> const sameTime { rat(3), rat(3), rat(3), rat(3) };
    CHECK(formula::LinearLeastSquares::compute_exact(std::span<formula::Rational const> { sameTime },
                                                     std::span<formula::Rational const> { inOrderLengths })
              .error()
          == formula::ArithmeticError::DomainError);
    CHECK(formula::LinearLeastSquares::compute_exact(std::span<formula::Rational const> { inOrderTimes }.first(3),
                                                     std::span<formula::Rational const> { inOrderLengths })
              .error()
          == formula::ArithmeticError::DomainError);
}

TEST_CASE("rounded output: wherever the exact fit answers the rounded fit is it rounded in every mode", "[least-squares]")
{
    // The fixture: the slope at 4 dp of mm/s and at 2 dp of mm/min (a factor
    // that is no power of ten), the intercept at 0 dp of mm (a tie) and to
    // tens of mm.
    check_rounded_route_in_every_mode<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }>(fit, fitPoints);
    check_rounded_route_in_every_mode<"slope", unit::MillimetrePerMinute, formula::DecimalPlaces { 2 }>(fit, fitPoints);
    check_rounded_route_in_every_mode<"intercept", unit::Millimetre, formula::DecimalPlaces { 0 }>(fit, fitPoints);
    check_rounded_route_in_every_mode<"intercept", unit::Millimetre, formula::DecimalPlaces { -1 }>(fit, fitPoints);
    // Five distinct denominators, where the exact route still answers.
    constexpr auto five = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 5>, formula::series<Length, 5>), { .reference = "Example Standard 12" });
    auto const fiveInputs = distinct_denominators<5>();
    check_rounded_route_in_every_mode<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }>(five, fiveInputs);
    check_rounded_route_in_every_mode<"intercept", unit::Millimetre, formula::DecimalPlaces { 4 }>(five, fiveInputs);
    // The pinned values, so that both routes agreeing on a wrong number cannot pass.
    auto const slope = formula::checked_evaluate<Rate>(roundedSlope, fitPoints);
    REQUIRE(slope.has_value());
    CHECK(slope->measurement().value() == rat(10179, 250)); // 0.6786 mm/s in mm/min
    auto const toTens = formula::checked_evaluate<Offset>(
        formula::rounded_output<"intercept", unit::Millimetre, formula::DecimalPlaces { -1 }, formula::RoundingMode::Floor>(
            fit),
        fitPoints);
    REQUIRE(toTens.has_value());
    CHECK(toTens->measurement().value() == rat(0)); // 9.5 mm down to tens
}

TEST_CASE("rounded output: the rounded fit answers where the exact fit overflows", "[least-squares]")
{
    constexpr auto fifteen = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 15>, formula::series<Length, 15>), { .reference = "Example Standard 12" });
    auto const exactRoute =
        formula::checked_evaluate<Rate>(formula::opaque_output<"slope">(fifteen), distinct_denominators<15>());
    REQUIRE(!exactRoute.has_value());
    CHECK(exactRoute.error() == formula::ArithmeticError::Overflow);
    // 1.93724895... mm/s: 1.9372 at 4 dp, 116.232 mm/min; 1.9373 upwards.
    constexpr auto evenSlope =
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
            fifteen);
    auto const even = formula::checked_evaluate<Rate>(evenSlope, distinct_denominators<15>());
    REQUIRE(even.has_value());
    CHECK(even->measurement().value() == rat(14529, 125));
    auto const upwards = formula::checked_evaluate_si<formula::Rational>(
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::Ceiling>(
            fifteen),
        distinct_denominators<15>());
    REQUIRE(upwards.has_value());
    REQUIRE(upwards->has_value());
    CHECK(**upwards == rat(19373, 10'000'000));
    // A negative intercept, -0.017688... mm: the sign decides the directed modes.
    auto const draws = distinct_denominators<15>();
    CHECK(rounded_intercept<formula::RoundingMode::HalfEven>(fifteen, draws) == rat(-177, 10'000'000));
    CHECK(rounded_intercept<formula::RoundingMode::Floor>(fifteen, draws) == rat(-177, 10'000'000));
    CHECK(rounded_intercept<formula::RoundingMode::AwayFromZero>(fifteen, draws) == rat(-177, 10'000'000));
    CHECK(rounded_intercept<formula::RoundingMode::Ceiling>(fifteen, draws) == rat(-11, 625'000));
    CHECK(rounded_intercept<formula::RoundingMode::TowardZero>(fifteen, draws) == rat(-11, 625'000));

    // Under the approximate style other lines carry the marker; the rounded line does not.
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(evenSlope, draws, formula::RecordingSink { recorded });
    std::string const approximate = formula::render_trace(
        recorded,
        { .maxSteps = 100, .numbers = formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven) });
    CHECK(approximate.find("\xe2\x89\x88") != std::string::npos);
    CHECK(approximate.find("5. round(slope of #4, to 4 dp of mm/s) = 1.9372 mm/s [nearest, ties to even]\n")
          != std::string::npos);
}

TEST_CASE("rounded output: a rounded fit that outgrows 256 bits is the operation's Overflow", "[least-squares]")
{
    // Measured with this algorithm: 57 points on distinct denominators fit,
    // 58 do not, in compute_exact itself.
    constexpr auto fiftySeven = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 57>, formula::series<Length, 57>), { .reference = "Example Standard 12" });
    constexpr auto fiftyEight = formula::linear_least_squares(
        formula::curve(formula::series<Elapsed, 58>, formula::series<Length, 58>), { .reference = "Example Standard 12" });
    constexpr auto slopeOfFiftySeven =
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
            fiftySeven);
    constexpr auto slopeOfFiftyEight =
        formula::rounded_output<"slope", MillimetrePerSecond, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
            fiftyEight);
    CHECK(formula::checked_evaluate_si<formula::Rational>(slopeOfFiftySeven, distinct_denominators<57>()).has_value());
    formula::Trace<> recorded {};
    auto const overflowing = formula::detail::dispatch<formula::Rational>(slopeOfFiftyEight, distinct_denominators<58>(),
                                                                          formula::RecordingSink { recorded });
    REQUIRE(!overflowing.has_value());
    CHECK(overflowing.error() == formula::ArithmeticError::Overflow);
    REQUIRE(formula::opaque_data(recorded, 3) != nullptr);
    CHECK(formula::opaque_data(recorded, 3)->failure == formula::OpaqueFailure::Own);
    CHECK(formula::render_trace(recorded, { .maxSteps = 400 })
              .find("5. round(slope of #4, to 4 dp of mm/s) = overflow in exact arithmetic "
                    "[the operation itself failed, not any input]\n")
          != std::string::npos);
}
