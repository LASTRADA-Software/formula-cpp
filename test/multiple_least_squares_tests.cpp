// SPDX-License-Identifier: Apache-2.0
//
// Multiple regression over raw observations. Every expected value was
// computed with Python's fractions from the data beside it.
#include <formula-cpp/document.hpp>
#include <formula-cpp/least_squares.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include "regression_cross_tu.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace
{
namespace unit = formula::unit;
using regression_cross_tu::Content;
using regression_cross_tu::Length;
using regression_cross_tu::Temperature;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

struct Elapsed: formula::Quantity<Elapsed, "t", "an invented elapsed time", unit::Second>
{
};
struct Delay: formula::Quantity<Delay, "t_d", "an invented delay", unit::Second>
{
};
struct Offset: formula::Quantity<Offset, "L_0", "an invented starting length", unit::Millimetre>
{
};
inline constexpr formula::Unit MillimetrePerPercent { .dimension = formula::dim::Length,
                                                      .magnitudeNumerator = 1,
                                                      .magnitudeDenominator = 10,
                                                      .symbolText = formula::symbol("mm/%"),
                                                      .decimals = 4 };
struct PerContent: formula::Quantity<PerContent, "k_w", "an invented length per percent of content", MillimetrePerPercent>
{
};

using regression_cross_tu::fit;
using regression_cross_tu::sixRows;

[[nodiscard]] formula::Rational exact_output(auto const& output, auto const& environment)
{
    auto const evaluated = formula::checked_evaluate_si(output, environment);
    REQUIRE(evaluated.has_value());
    REQUIRE(evaluated->has_value());
    return **evaluated;
}
} // namespace

TEST_CASE("two regressors are fitted exactly: constant, coefficients, r squared and points", "[least-squares][multiple]")
{
    // In coherent SI: the constant in metres, at 0 K; coefficient 1 in m/K;
    // coefficient 2 in metres per unit of content (a fraction, not a percent).
    // Coefficient 2 is checked at compile time too: six rows of two
    // regressors are small enough for a constant evaluation. The larger
    // fixtures below, of three and eight regressors, run at run time.
    constexpr auto compileTimeCoefficient =
        formula::checked_evaluate_si(formula::opaque_output<"coefficient 2">(fit), sixRows);
    STATIC_REQUIRE(compileTimeCoefficient.has_value());
    STATIC_REQUIRE(compileTimeCoefficient->has_value());
    STATIC_REQUIRE(**compileTimeCoefficient == rat(3'842'851, 69'148'200));
    CHECK(exact_output(formula::opaque_output<"coefficient 1">(fit), sixRows) == rat(346407, 4'609'880'000));
    CHECK(exact_output(formula::opaque_output<"constant">(fit), sixRows) == rat(22'365'154'943, 276'592'800'000));
    CHECK(exact_output(formula::opaque_output<"coefficient 2">(fit), sixRows) == rat(3'842'851, 69'148'200));
    CHECK(exact_output(formula::opaque_output<"r squared">(fit), sixRows) == rat(27'398'849'648, 27'403'085'919));
    CHECK(exact_output(formula::opaque_output<"points">(fit), sixRows) == rat(6));
}

TEST_CASE("each output of a multiple regression has its own dimension", "[least-squares][multiple]")
{
    STATIC_REQUIRE(decltype(formula::opaque_output<"constant">(fit))::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(formula::opaque_output<"coefficient 1">(fit))::dimension
                   == formula::dim::Length / formula::dim::Temperature);
    STATIC_REQUIRE(decltype(formula::opaque_output<"coefficient 2">(fit))::dimension == formula::dim::Length);
    STATIC_REQUIRE(decltype(formula::opaque_output<"r squared">(fit))::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(decltype(formula::opaque_output<"points">(fit))::dimension == formula::dim::Scalar);
    STATIC_REQUIRE(formula::MultipleLeastSquares<2>::exact_limbs == 19);
    // The hook is found for one, two and eight regressors: a malformed one
    // would fall back to compute<Rational> silently.
    using TimeRead = formula::ObservationsVarNode<Elapsed, 8>;
    using LengthRead = formula::ObservationsVarNode<Length, 8>;
    STATIC_REQUIRE(formula::detail::declares_compute_exact<formula::MultipleLeastSquares<1>, TimeRead, LengthRead>);
    STATIC_REQUIRE(formula::detail::declares_compute_exact<formula::MultipleLeastSquares<2>,
                                                           formula::ObservationsVarNode<Temperature, 8>,
                                                           formula::ObservationsVarNode<Content, 8>, LengthRead>);
    STATIC_REQUIRE(formula::detail::declares_compute_exact<formula::MultipleLeastSquares<8>, TimeRead, TimeRead,
                                                           TimeRead, TimeRead, TimeRead, TimeRead, TimeRead,
                                                           TimeRead, LengthRead>);
    STATIC_REQUIRE(formula::MultipleLeastSquares<8>::outputs.size() == 11);
    STATIC_REQUIRE(formula::MultipleLeastSquares<8>::outputs[8] == "coefficient 8");
    STATIC_REQUIRE(formula::MultipleLeastSquares<8>::outputs[10] == "points");
}

TEST_CASE("one regressor is the line", "[least-squares][multiple]")
{
    constexpr auto fourRows = formula::environment(
        formula::MeasuredObservations<Elapsed, 8>(rat(1), rat(2), rat(4), rat(7)),
        formula::MeasuredObservations<Length, 8>(rat(102, 10), rat(109, 10), rat(121, 10), rat(143, 10)));
    constexpr auto line = formula::linear_least_squares(formula::observations<Elapsed, 8>, formula::observations<Length, 8>,
                                                        { .reference = "Example Standard 12" });
    constexpr auto single = formula::multiple_least_squares(formula::regressors(formula::observations<Elapsed, 8>),
                                                            formula::observations<Length, 8>,
                                                            { .reference = "Example Standard 12" });
    CHECK(exact_output(formula::opaque_output<"constant">(single), fourRows)
          == exact_output(formula::opaque_output<"intercept">(line), fourRows));
    CHECK(exact_output(formula::opaque_output<"coefficient 1">(single), fourRows)
          == exact_output(formula::opaque_output<"slope">(line), fourRows));
    CHECK(exact_output(formula::opaque_output<"r squared">(single), fourRows) == rat(1083, 1085));
}

TEST_CASE("the order of the rows does not change a multiple regression", "[least-squares][multiple]")
{
    // Rows 6, 3, 1, 5, 2, 4.
    constexpr auto shuffled = formula::environment(
        formula::MeasuredObservations<Temperature, 8>(rat(297, 10), rat(179, 10), rat(113, 10), rat(233, 10), rat(137, 10),
                                                      rat(191, 10)),
        formula::MeasuredObservations<Content, 8>(rat(43, 10), rat(29, 10), rat(23, 10), rat(37, 10), rat(31, 10),
                                                  rat(41, 10)),
        formula::MeasuredObservations<Length, 8>(rat(106), rat(10433, 100), rat(2588, 25), rat(10521, 100),
                                                 rat(10413, 100), rat(1051, 10)));
    CHECK(exact_output(formula::opaque_output<"coefficient 2">(fit), shuffled)
          == exact_output(formula::opaque_output<"coefficient 2">(fit), sixRows));
    CHECK(exact_output(formula::opaque_output<"r squared">(fit), shuffled)
          == exact_output(formula::opaque_output<"r squared">(fit), sixRows));
}

TEST_CASE("a singular design is a multiple regression's own domain error, exactly and in double",
          "[least-squares][multiple]")
{
    // The delay is twice the elapsed time on every row: no unique fit,
    // whatever the lengths were.
    constexpr auto collinear = formula::multiple_least_squares(
        formula::regressors(formula::observations<Elapsed, 8>, formula::observations<Delay, 8>),
        formula::observations<Length, 8>,
        { .reference = "Example Standard 12" });
    constexpr auto doubledDelay = formula::environment(
        formula::MeasuredObservations<Elapsed, 8>(rat(1), rat(2), rat(4), rat(7)),
        formula::MeasuredObservations<Delay, 8>(rat(2), rat(4), rat(8), rat(14)),
        formula::MeasuredObservations<Length, 8>(rat(102, 10), rat(109, 10), rat(121, 10), rat(143, 10)));
    auto const exact = formula::checked_evaluate_si(formula::opaque_output<"coefficient 1">(collinear), doubledDelay);
    REQUIRE(!exact.has_value());
    CHECK(exact.error() == formula::ArithmeticError::DomainError);
    auto const approximate =
        formula::checked_evaluate_si<double>(formula::opaque_output<"coefficient 1">(collinear), doubledDelay);
    REQUIRE(!approximate.has_value());
    CHECK(approximate.error() == formula::ArithmeticError::DomainError);
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(formula::opaque_output<"coefficient 1">(collinear), doubledDelay,
                                                        formula::RecordingSink { recorded });
    REQUIRE(formula::opaque_data(recorded, 3) != nullptr);
    CHECK(formula::opaque_data(recorded, 3)->failure == formula::OpaqueFailure::Own);
}

TEST_CASE("fewer rows than regressors plus one are refused, and one more fits exactly", "[least-squares][multiple]")
{
    constexpr auto twoRows = formula::environment(
        formula::MeasuredObservations<Temperature, 8>(rat(113, 10), rat(137, 10)),
        formula::MeasuredObservations<Content, 8>(rat(23, 10), rat(31, 10)),
        formula::MeasuredObservations<Length, 8>(rat(2588, 25), rat(10413, 100)));
    auto const tooFew = formula::checked_evaluate_si(formula::opaque_output<"constant">(fit), twoRows);
    REQUIRE(!tooFew.has_value());
    CHECK(tooFew.error() == formula::ArithmeticError::DomainError);
    constexpr auto threeRows = formula::environment(
        formula::MeasuredObservations<Temperature, 8>(rat(113, 10), rat(137, 10), rat(179, 10)),
        formula::MeasuredObservations<Content, 8>(rat(23, 10), rat(31, 10), rat(29, 10)),
        formula::MeasuredObservations<Length, 8>(rat(2588, 25), rat(10413, 100), rat(10433, 100)));
    CHECK(exact_output(formula::opaque_output<"r squared">(fit), threeRows) == rat(1));
}

TEST_CASE("a regressor in degrees Celsius: the constant is at 0 K, and the value at 0 degC is a formula",
          "[least-squares][multiple]")
{
    // The constant, 80.859... mm, is the length at 0 K and 0 % content. At
    // 0 degC it is the constant plus coefficient 1 times 273.15 K:
    // 101.385... mm, 101.39 at 2 dp.
    constexpr auto atZeroCelsius =
        formula::opaque_output<"constant">(fit)
        + formula::opaque_output<"coefficient 1">(fit) * formula::constant<unit::Kelvin>(rat(27315, 100));
    CHECK(exact_output(atZeroCelsius, sixRows) == rat(14'021'209'633, 138'296'400'000));
    auto const rounded = formula::checked_evaluate<Offset>(
        formula::rounded<unit::Millimetre, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(atZeroCelsius),
        sixRows);
    REQUIRE(rounded.has_value());
    CHECK(rounded->measurement().value() == rat(10139, 100));
}

TEST_CASE("a regressor in percent: its coefficient is per unit, and a unit per percent reports it per percent",
          "[least-squares][multiple]")
{
    // 55.574... mm per unit of content is 0.5557 mm per percent at 4 dp. At
    // compile time, as the exact coefficient is checked above.
    constexpr auto perPercent = formula::checked_evaluate<PerContent>(
        formula::rounded_output<"coefficient 2", MillimetrePerPercent, formula::DecimalPlaces { 4 },
                                formula::RoundingMode::HalfEven>(fit),
        sixRows);
    STATIC_REQUIRE(perPercent.has_value());
    STATIC_REQUIRE(perPercent->measurement().value() == rat(5557, 10'000));
}

TEST_CASE("a multiple regression is traced as one call, rendered and documented", "[least-squares][multiple][trace]")
{
    formula::Trace<> recorded {};
    (void) formula::detail::dispatch<formula::Rational>(formula::opaque_output<"coefficient 2">(fit), sixRows,
                                                        formula::RecordingSink { recorded });
    std::string const text = formula::render_trace(recorded, { .maxSteps = 40 });
    INFO(text);
    CHECK(text.find("4. multiple least squares(#1, #2, #3) = constant = 22365154943/276592800 mm; "
                    "coefficient 1 = 346407/4609880000 m/K; coefficient 2 = 19214255/345741 mm; "
                    "r squared = 27398849648/27403085919; points = 6 [inside not shown] "
                    "[Length by temperature and content, Example Standard 12, 5.3]\n")
          != std::string::npos);
    CHECK(formula::render(formula::opaque_output<"coefficient 2">(fit))
          == "multiple least squares(T(i), w(i), L(i)).coefficient 2");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::opaque_output<"coefficient 2">(fit))
          == "\\text{multiple least squares}({T}_{i}, {w}_{i}, {L}_{i})_{\\text{coefficient 2}}");
    formula::Documentation const page =
        formula::document(formula::opaque_output<"constant">(fit) + formula::opaque_output<"coefficient 2">(fit));
    REQUIRE(page.opaqueOperations.size() == 1);
    CHECK(page.opaqueOperations[0].outputs
          == std::vector<std::string_view> { "constant", "coefficient 1", "coefficient 2", "r squared", "points" });
    REQUIRE(page.symbols.size() == 3);
    CHECK(page.symbols[2].shape == formula::ValueShape::Observations);
}

TEST_CASE("a multiple regression in double agrees with the exact fit", "[least-squares][multiple]")
{
    auto const approximate = [](auto const& output) {
        auto const evaluated = formula::checked_evaluate_si<double>(output, sixRows);
        REQUIRE(evaluated.has_value());
        REQUIRE(evaluated->has_value());
        return **evaluated;
    };
    CHECK(std::abs(approximate(formula::opaque_output<"coefficient 1">(fit)) - 346407.0 / 4'609'880'000.0)
          <= 1e-12 * (346407.0 / 4'609'880'000.0));
    CHECK(std::abs(approximate(formula::opaque_output<"coefficient 2">(fit)) - 3'842'851.0 / 69'148'200.0)
          <= 1e-12 * (3'842'851.0 / 69'148'200.0));
    CHECK(approximate(formula::opaque_output<"points">(fit)) == 6.0);
}

TEST_CASE("a multiple regression's outputs are the same in two translation units", "[least-squares][multiple]")
{
    auto const here = formula::checked_evaluate_si(regression_cross_tu::secondCoefficient, sixRows);
    auto const there = second_coefficient_in_other_tu(regression_cross_tu::secondCoefficient);
    REQUIRE(here.has_value());
    REQUIRE(here->has_value());
    REQUIRE(there.has_value());
    REQUIRE(there->has_value());
    CHECK(**here == **there);
    for (std::size_t at = 0; at < formula::MultipleLeastSquares<2>::outputs.size(); ++at)
        CHECK(output_name_in_other_tu(at) == formula::MultipleLeastSquares<2>::outputs[at]);
}

namespace
{
struct FirstFactor: formula::Quantity<FirstFactor, "f_1", "an invented factor", unit::One>
{
};
struct SecondFactor: formula::Quantity<SecondFactor, "f_2", "an invented factor", unit::One>
{
};
struct ThirdFactor: formula::Quantity<ThirdFactor, "f_3", "an invented factor", unit::One>
{
};
struct Response: formula::Quantity<Response, "y_r", "an invented response", unit::One>
{
};

// x1 = 3, 7, 2, 9, 4, 8, 5; x2 = 11, 13, 17, 19, 23, 29, 31; x3 = 2, 1, 4, 3, 6, 5, 7;
// y = 41, 57, 49, 71, 66, 83, 79.
constexpr auto threeFactors = formula::multiple_least_squares(
    formula::regressors(formula::observations<FirstFactor, 8>, formula::observations<SecondFactor, 8>,
                        formula::observations<ThirdFactor, 8>),
    formula::observations<Response, 8>,
    { .reference = "Example Standard 12" });
constexpr auto sevenRows = formula::environment(
    formula::MeasuredObservations<FirstFactor, 8>(rat(3), rat(7), rat(2), rat(9), rat(4), rat(8), rat(5)),
    formula::MeasuredObservations<SecondFactor, 8>(rat(11), rat(13), rat(17), rat(19), rat(23), rat(29), rat(31)),
    formula::MeasuredObservations<ThirdFactor, 8>(rat(2), rat(1), rat(4), rat(3), rat(6), rat(5), rat(7)),
    formula::MeasuredObservations<Response, 8>(rat(41), rat(57), rat(49), rat(71), rat(66), rat(83), rat(79)));
} // namespace

TEST_CASE("three regressors are fitted exactly, each coefficient in its own place", "[least-squares][multiple]")
{
    // Computed with Python's fractions from the columns above: no two
    // coefficients are alike, so swapped or dropped columns would show.
    CHECK(exact_output(formula::opaque_output<"constant">(threeFactors), sevenRows) == rat(1'222'381, 72'160));
    CHECK(exact_output(formula::opaque_output<"coefficient 1">(threeFactors), sevenRows) == rat(14'161, 4'510));
    CHECK(exact_output(formula::opaque_output<"coefficient 2">(threeFactors), sevenRows) == rat(84'499, 72'160));
    CHECK(exact_output(formula::opaque_output<"coefficient 3">(threeFactors), sevenRows) == rat(52'383, 36'080));
    CHECK(exact_output(formula::opaque_output<"r squared">(threeFactors), sevenRows) == rat(362'845'727, 364'047'200));
    CHECK(exact_output(formula::opaque_output<"points">(threeFactors), sevenRows) == rat(7));
}

namespace
{
struct FourthFactor: formula::Quantity<FourthFactor, "f_4", "an invented factor", unit::One>
{
};
struct FifthFactor: formula::Quantity<FifthFactor, "f_5", "an invented factor", unit::One>
{
};
struct SixthFactor: formula::Quantity<SixthFactor, "f_6", "an invented factor", unit::One>
{
};
struct SeventhFactor: formula::Quantity<SeventhFactor, "f_7", "an invented factor", unit::One>
{
};
struct EighthFactor: formula::Quantity<EighthFactor, "f_8", "an invented factor", unit::One>
{
};

// Ten rows: x1, x2, x3 and y are the seven rows above and three more
// (x1 = 1, 6, 10; x2 = 37, 41, 43; x3 = 9, 8, 10; y = 62, 75, 90), and x4 to
// x8 are orderings of 1 to 10: x4 = 5, 3, 8, 1, 9, 2, 7, 4, 6, 10;
// x5 = 4, 9, 1, 7, 3, 10, 6, 2, 8, 5; x6 = 8, 6, 10, 2, 1, 7, 3, 9, 5, 4;
// x7 = 1, 4, 9, 6, 8, 3, 10, 5, 2, 7; x8 = 6, 2, 5, 10, 7, 1, 4, 8, 3, 9.
constexpr auto tenRows = formula::environment(
    formula::MeasuredObservations<FirstFactor, 16>(rat(3), rat(7), rat(2), rat(9), rat(4), rat(8), rat(5), rat(1), rat(6),
                                                   rat(10)),
    formula::MeasuredObservations<SecondFactor, 16>(rat(11), rat(13), rat(17), rat(19), rat(23), rat(29), rat(31),
                                                    rat(37), rat(41), rat(43)),
    formula::MeasuredObservations<ThirdFactor, 16>(rat(2), rat(1), rat(4), rat(3), rat(6), rat(5), rat(7), rat(9), rat(8),
                                                   rat(10)),
    formula::MeasuredObservations<FourthFactor, 16>(rat(5), rat(3), rat(8), rat(1), rat(9), rat(2), rat(7), rat(4),
                                                    rat(6), rat(10)),
    formula::MeasuredObservations<FifthFactor, 16>(rat(4), rat(9), rat(1), rat(7), rat(3), rat(10), rat(6), rat(2), rat(8),
                                                   rat(5)),
    formula::MeasuredObservations<SixthFactor, 16>(rat(8), rat(6), rat(10), rat(2), rat(1), rat(7), rat(3), rat(9), rat(5),
                                                   rat(4)),
    formula::MeasuredObservations<SeventhFactor, 16>(rat(1), rat(4), rat(9), rat(6), rat(8), rat(3), rat(10), rat(5),
                                                     rat(2), rat(7)),
    formula::MeasuredObservations<EighthFactor, 16>(rat(6), rat(2), rat(5), rat(10), rat(7), rat(1), rat(4), rat(8),
                                                    rat(3), rat(9)),
    formula::MeasuredObservations<Response, 16>(rat(41), rat(57), rat(49), rat(71), rat(66), rat(83), rat(79), rat(62),
                                                rat(75), rat(90)));

constexpr auto eightFactors = formula::multiple_least_squares(
    formula::regressors(formula::observations<FirstFactor, 16>, formula::observations<SecondFactor, 16>,
                        formula::observations<ThirdFactor, 16>, formula::observations<FourthFactor, 16>,
                        formula::observations<FifthFactor, 16>, formula::observations<SixthFactor, 16>,
                        formula::observations<SeventhFactor, 16>, formula::observations<EighthFactor, 16>),
    formula::observations<Response, 16>,
    { .reference = "Example Standard 12" });

constexpr auto fourFactors = formula::multiple_least_squares(
    formula::regressors(formula::observations<FirstFactor, 16>, formula::observations<SecondFactor, 16>,
                        formula::observations<ThirdFactor, 16>, formula::observations<FourthFactor, 16>),
    formula::observations<Response, 16>,
    { .reference = "Example Standard 12" });
} // namespace

TEST_CASE("eight regressors, the most a fit takes, are fitted exactly, rounded where used and in double",
          "[least-squares][multiple]")
{
    // Computed with Python's fractions from the columns above. At run time:
    // ten rows of eight regressors exceed the constant-evaluation limit of
    // cl 19.51 (1048576 steps) and of g++ 14 (33554432 operations), measured.
    constexpr formula::Rational::Int shared = 156'879'508'961;
    CHECK(exact_output(formula::opaque_output<"constant">(eightFactors), tenRows) == rat(11'058'652'968'024, shared));
    CHECK(exact_output(formula::opaque_output<"coefficient 1">(eightFactors), tenRows) == rat(758'096'997'271, shared));
    CHECK(exact_output(formula::opaque_output<"coefficient 2">(eightFactors), tenRows) == rat(-183'676'518'433, shared));
    CHECK(exact_output(formula::opaque_output<"coefficient 3">(eightFactors), tenRows) == rat(1'386'201'954'356, shared));
    CHECK(exact_output(formula::opaque_output<"coefficient 4">(eightFactors), tenRows) == rat(-417'311'735'278, shared));
    CHECK(exact_output(formula::opaque_output<"coefficient 5">(eightFactors), tenRows) == rat(-332'278'407'103, shared));
    CHECK(exact_output(formula::opaque_output<"coefficient 6">(eightFactors), tenRows) == rat(-200'096'516'911, shared));
    CHECK(exact_output(formula::opaque_output<"coefficient 7">(eightFactors), tenRows) == rat(80'015'914'148, shared));
    CHECK(exact_output(formula::opaque_output<"coefficient 8">(eightFactors), tenRows)
          == rat(-69'144'339'324, 22'411'358'423));
    CHECK(exact_output(formula::opaque_output<"r squared">(eightFactors), tenRows)
          == rat(3'332'062'487'912'201, 3'347'965'600'736'701));
    CHECK(exact_output(formula::opaque_output<"points">(eightFactors), tenRows) == rat(10));

    // Rounded where used: coefficient 8, -3.08523..., is -3.0852 at 4 dp.
    auto const rounded = formula::checked_evaluate_si(
        formula::rounded_output<"coefficient 8", unit::One, formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfEven>(
            eightFactors),
        tenRows);
    REQUIRE(rounded.has_value());
    REQUIRE(rounded->has_value());
    CHECK(**rounded == rat(-30'852, 10'000));

    // In double, the untraced route agrees.
    auto const approximate = formula::checked_evaluate_si<double>(formula::opaque_output<"coefficient 8">(eightFactors),
                                                                  tenRows);
    REQUIRE(approximate.has_value());
    REQUIRE(approximate->has_value());
    constexpr double exactEighth = -69'144'339'324.0 / 22'411'358'423.0;
    CHECK(std::abs(**approximate - exactEighth) <= 1e-12 * std::abs(exactEighth));
}

TEST_CASE("four regressors are fitted exactly", "[least-squares][multiple]")
{
    // The first four columns and the values above, with Python's fractions.
    CHECK(exact_output(formula::opaque_output<"constant">(fourFactors), tenRows) == rat(331'781'457, 10'094'680));
    CHECK(exact_output(formula::opaque_output<"coefficient 1">(fourFactors), tenRows) == rat(6'798'719, 2'018'936));
    CHECK(exact_output(formula::opaque_output<"coefficient 2">(fourFactors), tenRows) == rat(4371, 1'009'468));
    CHECK(exact_output(formula::opaque_output<"coefficient 3">(fourFactors), tenRows) == rat(35'351'997, 10'094'680));
    CHECK(exact_output(formula::opaque_output<"coefficient 4">(fourFactors), tenRows) == rat(-3'178'563, 5'047'340));
    CHECK(exact_output(formula::opaque_output<"r squared">(fourFactors), tenRows) == rat(19'180'134'579, 21'543'056'588));
}

TEST_CASE("an affinely dependent design is refused: one regressor is twice another plus three",
          "[least-squares][multiple]")
{
    // Delay = 2 t + 3 s on every row, a design no scale alone would show:
    // singular exactly, whatever the lengths. Refused exactly, and in double
    // with a pivot of exactly 0.
    constexpr auto affine = formula::multiple_least_squares(
        formula::regressors(formula::observations<Elapsed, 8>, formula::observations<Delay, 8>),
        formula::observations<Length, 8>,
        { .reference = "Example Standard 12" });
    constexpr auto shifted = formula::environment(
        formula::MeasuredObservations<Elapsed, 8>(rat(1), rat(2), rat(4), rat(7)),
        formula::MeasuredObservations<Delay, 8>(rat(5), rat(7), rat(11), rat(17)),
        formula::MeasuredObservations<Length, 8>(rat(102, 10), rat(109, 10), rat(121, 10), rat(143, 10)));
    auto const exact = formula::checked_evaluate_si(formula::opaque_output<"constant">(affine), shifted);
    REQUIRE(!exact.has_value());
    CHECK(exact.error() == formula::ArithmeticError::DomainError);
    auto const approximate = formula::checked_evaluate_si<double>(formula::opaque_output<"constant">(affine), shifted);
    REQUIRE(!approximate.has_value());
    CHECK(approximate.error() == formula::ArithmeticError::DomainError);
}

namespace
{
// t = 1 ... 8 s and a second column, t + e / delta with e = +1, -1, -1, +1, +1, -1, -1, +1
// (orthogonal to t): not singular, but 1 - R^2 of one on the other is 8 / (42 delta^2 + 8).
// The lengths, in millimetres, are 3 + 2 t - 5 (second column), exactly.
[[nodiscard]] auto nearly_collinear(std::int64_t delta)
{
    constexpr std::array<std::int64_t, 8> signs { 1, -1, -1, 1, 1, -1, -1, 1 };
    std::array<formula::Rational, 8> elapsed;
    std::array<formula::Rational, 8> delayed;
    std::array<formula::Rational, 8> lengths;
    for (std::size_t at = 0; at < 8; ++at)
    {
        auto const position = static_cast<std::int64_t>(at) + 1;
        elapsed[at] = rat(position);
        delayed[at] = rat(position * delta + signs[at], delta);
        // 3 + 2 position - 5 (position + sign / delta) millimetres.
        lengths[at] = rat(3 * delta - 3 * position * delta - 5 * signs[at], delta);
    }
    return formula::environment(*formula::MeasuredObservations<Elapsed, 16>::from(elapsed),
                                *formula::MeasuredObservations<Delay, 16>::from(delayed),
                                *formula::MeasuredObservations<Length, 16>::from(lengths));
}

constexpr auto nearlyCollinearFit = formula::multiple_least_squares(
    formula::regressors(formula::observations<Elapsed, 16>, formula::observations<Delay, 16>),
    formula::observations<Length, 16>,
    { .reference = "Example Standard 12" });
} // namespace

TEST_CASE("a design that is nearly but not exactly singular is answered exactly", "[least-squares][multiple]")
{
    // With Python's fractions, for both deltas: constant 3/1000 m, coefficient 1
    // 1/500 m/s, coefficient 2 -1/200 m/s, R^2 1, eight points -- made of
    // eight of the sixteen places. 1 - R^2 of the columns is 1/1312501 at
    // delta 500 and 1/2100000001 at delta 20000: exact answers either way.
    for (std::int64_t const delta: { std::int64_t { 500 }, std::int64_t { 20'000 } })
    {
        INFO("delta " << delta);
        auto const observed = nearly_collinear(delta);
        CHECK(exact_output(formula::opaque_output<"constant">(nearlyCollinearFit), observed) == rat(3, 1000));
        CHECK(exact_output(formula::opaque_output<"coefficient 1">(nearlyCollinearFit), observed) == rat(1, 500));
        CHECK(exact_output(formula::opaque_output<"coefficient 2">(nearlyCollinearFit), observed) == rat(-1, 200));
        CHECK(exact_output(formula::opaque_output<"r squared">(nearlyCollinearFit), observed) == rat(1));
        CHECK(exact_output(formula::opaque_output<"points">(nearlyCollinearFit), observed) == rat(8));
    }
}

TEST_CASE("in double a nearly singular design is answered above the tolerance and refused below it",
          "[least-squares][multiple]")
{
    // The untraced route's own tolerance: a pivot at or below 1e-9 of its
    // diagonal is taken for singular (`detail/least_squares_kernel.hpp`).
    // Delta 500 leaves 1 - R^2 of the columns at 1/1312501 (7.6e-7): answered.
    // Delta 20000 leaves 1/2100000001 (4.8e-10), below the tolerance: refused,
    // though the exact route answers.
    auto const answered = formula::checked_evaluate_si<double>(
        formula::opaque_output<"coefficient 1">(nearlyCollinearFit), nearly_collinear(500));
    REQUIRE(answered.has_value());
    REQUIRE(answered->has_value());
    CHECK(std::abs(**answered - 1.0 / 500.0) <= 1e-8 * (1.0 / 500.0));
    auto const refused = formula::checked_evaluate_si<double>(
        formula::opaque_output<"coefficient 1">(nearlyCollinearFit), nearly_collinear(20'000));
    REQUIRE(!refused.has_value());
    CHECK(refused.error() == formula::ArithmeticError::DomainError);
}

