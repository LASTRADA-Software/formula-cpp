// SPDX-License-Identifier: Apache-2.0
//
// The regression kernel behind the fits over raw observations, called
// directly: exact in wide integers, approximate in double. Every expected
// value was computed with Python's fractions from the data beside it.
#include <formula-cpp/detail/least_squares_kernel.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace
{
using formula::Rational;

constexpr Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return Rational { numerator, denominator };
}

template <std::size_t K, std::size_t N>
constexpr std::expected<std::array<Rational, K + 3>, formula::ArithmeticError> exact_fit(
    std::array<std::array<Rational, N>, K> const& regressorValues, std::array<Rational, N> const& responseValues)
{
    std::array<std::span<Rational const>, K> regressorColumns;
    for (std::size_t at = 0; at < K; ++at)
        regressorColumns[at] = std::span<Rational const> { regressorValues[at] };
    auto const wide = formula::detail::exact_regression<K>(regressorColumns, std::span<Rational const> { responseValues });
    if (!wide.has_value())
        return std::unexpected { wide.error() };
    return formula::detail::narrowed_all(*wide);
}

template <std::size_t K, std::size_t N>
std::expected<std::array<double, K + 3>, formula::ArithmeticError> approximate_fit(
    std::array<std::array<Rational, N>, K> const& regressorValues, std::array<Rational, N> const& responseValues)
{
    std::array<std::array<double, N>, K> regressorDoubles;
    std::array<double, N> responseDoubles;
    for (std::size_t rowAt = 0; rowAt < N; ++rowAt)
    {
        for (std::size_t at = 0; at < K; ++at)
            regressorDoubles[at][rowAt] = regressorValues[at][rowAt].to_double();
        responseDoubles[rowAt] = responseValues[rowAt].to_double();
    }
    std::array<std::span<double const>, K> regressorColumns;
    for (std::size_t at = 0; at < K; ++at)
        regressorColumns[at] = std::span<double const> { regressorDoubles[at] };
    return formula::detail::approximate_regression<double, K>(regressorColumns, std::span<double const> { responseDoubles });
}

[[nodiscard]] bool close(double approximate, Rational exact, double relative)
{
    return std::abs(approximate - exact.to_double()) <= relative * std::abs(exact.to_double());
}

// The line: t = 1, 2, 4, 7 s against L = 10.2, 10.9, 12.1, 14.3 mm, in metres.
constexpr std::array<std::array<Rational, 4>, 1> lineTimes { { { rat(1), rat(2), rat(4), rat(7) } } };
constexpr std::array<Rational, 4> lineLengths { rat(102, 10'000), rat(109, 10'000), rat(121, 10'000), rat(143, 10'000) };

// Two regressors: 11.3 ... 29.7 degC in kelvin, 2.3 ... 4.3 % as fractions,
// against 103.52 ... 106 mm in metres.
constexpr std::array<std::array<Rational, 6>, 2> twoRegressors { {
    { rat(28445, 100), rat(28685, 100), rat(29105, 100), rat(29225, 100), rat(29645, 100), rat(30285, 100) },
    { rat(23, 1000), rat(31, 1000), rat(29, 1000), rat(41, 1000), rat(37, 1000), rat(43, 1000) },
} };
constexpr std::array<Rational, 6> twoRegressorLengths { rat(2588, 25'000), rat(10413, 100'000), rat(10433, 100'000),
                                                        rat(1051, 10'000), rat(10521, 100'000), rat(106, 1000) };
} // namespace

TEST_CASE("the kernel's widths are the estimate's", "[least-squares][kernel]")
{
    STATIC_REQUIRE(formula::detail::regression_limbs(1) == 12);
    STATIC_REQUIRE(formula::detail::regression_limbs(2) == 19);
    STATIC_REQUIRE(formula::detail::regression_limbs(3) == 28);
    STATIC_REQUIRE(formula::detail::regression_limbs(8) == 73);
    STATIC_REQUIRE(formula::detail::regression_limbs(1) == formula::detail::regressionSumLimbs);
}

TEST_CASE("the exact kernel's line is the closed form", "[least-squares][kernel]")
{
    // Intercept 19/2 mm, slope 19/28 mm/s, R^2 1083/1085, four points -- not
    // the secant 41/60 mm/s, not x-on-y.
    // At compile time: four points are few enough for a constant evaluation.
    // The fixtures below, of more rows or more regressors, run at run time.
    constexpr auto line = exact_fit(lineTimes, lineLengths);
    STATIC_REQUIRE(line.has_value());
    STATIC_REQUIRE(*line == std::array { rat(19, 2000), rat(19, 28'000), rat(1083, 1085), rat(4) });
}

TEST_CASE("the order of the rows does not change the kernel's exact fit", "[least-squares][kernel]")
{
    constexpr std::array<std::array<Rational, 4>, 1> shuffledTimes { { { rat(4), rat(1), rat(7), rat(2) } } };
    constexpr std::array<Rational, 4> shuffledLengths {
        rat(121, 10'000), rat(102, 10'000), rat(143, 10'000), rat(109, 10'000)
    };
    CHECK(exact_fit(shuffledTimes, shuffledLengths) == exact_fit(lineTimes, lineLengths));
    // Rows 6, 3, 1, 5, 2, 4 of the two-regressor fixture.
    constexpr std::array<std::array<Rational, 6>, 2> shuffledRegressors { {
        { twoRegressors[0][5],
          twoRegressors[0][2],
          twoRegressors[0][0],
          twoRegressors[0][4],
          twoRegressors[0][1],
          twoRegressors[0][3] },
        { twoRegressors[1][5],
          twoRegressors[1][2],
          twoRegressors[1][0],
          twoRegressors[1][4],
          twoRegressors[1][1],
          twoRegressors[1][3] },
    } };
    constexpr std::array<Rational, 6> shuffledResponses { twoRegressorLengths[5], twoRegressorLengths[2],
                                                          twoRegressorLengths[0], twoRegressorLengths[4],
                                                          twoRegressorLengths[1], twoRegressorLengths[3] };
    CHECK(exact_fit(shuffledRegressors, shuffledResponses) == exact_fit(twoRegressors, twoRegressorLengths));
}

TEST_CASE("each kernel pre-check is the fit's own domain error, in both representations", "[least-squares][kernel]")
{
    using formula::ArithmeticError;
    // Counts differ: three times, four lengths -- never a read past the shorter.
    std::array<Rational, 3> const threeTimes { rat(1), rat(2), rat(4) };
    std::array<std::span<Rational const>, 1> const shortColumn { std::span<Rational const> { threeTimes } };
    CHECK(formula::detail::exact_regression<1>(shortColumn, std::span<Rational const> { lineLengths }).error()
          == ArithmeticError::DomainError);
    std::array<double, 3> const threeDoubles { 1.0, 2.0, 4.0 };
    std::array<double, 4> const fourDoubles { 0.0102, 0.0109, 0.0121, 0.0143 };
    std::array<std::span<double const>, 1> const shortDoubles { std::span<double const> { threeDoubles } };
    CHECK(formula::detail::approximate_regression<double, 1>(shortDoubles, std::span<double const> { fourDoubles }).error()
          == ArithmeticError::DomainError);
    // None made, and as many rows as regressors: no fit.
    std::array<std::span<Rational const>, 1> const none { std::span<Rational const> {} };
    CHECK(formula::detail::exact_regression<1>(none, std::span<Rational const> {}).error() == ArithmeticError::DomainError);
    constexpr std::array<std::array<Rational, 2>, 2> twoByTwo { { { rat(1), rat(2) }, { rat(3), rat(5) } } };
    CHECK(exact_fit(twoByTwo, std::array { rat(7), rat(11) }).error() == ArithmeticError::DomainError);
    CHECK(approximate_fit(twoByTwo, std::array { rat(7), rat(11) }).error() == ArithmeticError::DomainError);
    // Flat values: R^2 would be 0/0, so the whole fit is refused.
    constexpr std::array<Rational, 4> flat { rat(127, 10'000), rat(127, 10'000), rat(127, 10'000), rat(127, 10'000) };
    CHECK(exact_fit(lineTimes, flat).error() == ArithmeticError::DomainError);
    CHECK(approximate_fit(lineTimes, flat).error() == ArithmeticError::DomainError);
    // A flat regressor: no line along it. In double, three 0.1 s have a mean
    // that is not 0.1; decided on the values themselves, it is still refused.
    constexpr std::array<std::array<Rational, 4>, 1> sameTime { { { rat(1, 10), rat(1, 10), rat(1, 10), rat(1, 10) } } };
    CHECK(exact_fit(sameTime, lineLengths).error() == ArithmeticError::DomainError);
    CHECK(approximate_fit(sameTime, lineLengths).error() == ArithmeticError::DomainError);
}

TEST_CASE("the kernel solves two and three regressors exactly", "[least-squares][kernel]")
{
    // Two: the constant is the length at 0 K, not at 0 degC.
    auto const two = exact_fit(twoRegressors, twoRegressorLengths);
    REQUIRE(two.has_value());
    CHECK(*two
          == std::array { rat(22365154943, 276592800000),
                          rat(346407, 4609880000),
                          rat(3842851, 69148200),
                          rat(27398849648, 27403085919),
                          rat(6) });
    // Three, in integers.
    constexpr std::array<std::array<Rational, 7>, 3> threeRegressors { {
        { rat(3), rat(7), rat(2), rat(9), rat(4), rat(8), rat(5) },
        { rat(11), rat(13), rat(17), rat(19), rat(23), rat(29), rat(31) },
        { rat(2), rat(1), rat(4), rat(3), rat(6), rat(5), rat(7) },
    } };
    constexpr std::array<Rational, 7> threeResponses { rat(41), rat(57), rat(49), rat(71), rat(66), rat(83), rat(79) };
    auto const three = exact_fit(threeRegressors, threeResponses);
    REQUIRE(three.has_value());
    CHECK(*three
          == std::array { rat(1222381, 72160),
                          rat(14161, 4510),
                          rat(84499, 72160),
                          rat(52383, 36080),
                          rat(362845727, 364047200),
                          rat(7) });
}

TEST_CASE("the kernel fits as many rows as regressors plus one exactly, with R^2 of 1", "[least-squares][kernel]")
{
    constexpr std::array<std::array<Rational, 3>, 2> regressorsOfThree { { { rat(1), rat(2), rat(4) },
                                                                           { rat(2), rat(1), rat(3) } } };
    CHECK(exact_fit(regressorsOfThree, std::array { rat(5), rat(7), rat(2) })
          == std::array { rat(39, 4), rat(-1, 4), rat(-9, 4), rat(1), rat(3) });
}

TEST_CASE("the kernel's fraction-free solve exchanges rows at a zero pivot", "[least-squares][kernel]")
{
    // [[0, 2 | 4], [3, 1 | 5]]: the first pivot is 0 and the row below is
    // not, so the rows are exchanged. 3 g0 + g1 = 5 and 2 g1 = 4 give g0 = 1,
    // g1 = 2: numerators 6 and 12 over the determinant 6. The centred normal
    // equations are positive semi-definite, where a zero pivot leaves its
    // column zero below it, so a fit never needs the exchange; the solve is
    // still correct for any matrix it is given, and this proves it.
    using Wide = formula::detail::WideSigned<4>;
    auto const whole = [](std::uint64_t held) {
        return Wide { false, formula::detail::WideUnsigned<4>::from_u64(held) };
    };
    std::array<std::array<Wide, 3>, 2> const augmented { { { whole(0), whole(2), whole(4) },
                                                           { whole(3), whole(1), whole(5) } } };
    auto const solved = formula::detail::fraction_free_solve<2, 4>(augmented);
    REQUIRE(solved.has_value());
    CHECK((*solved)[0].magnitude == formula::detail::WideUnsigned<4>::from_u64(6));
    CHECK((*solved)[1].magnitude == formula::detail::WideUnsigned<4>::from_u64(12));
    CHECK((*solved)[2].magnitude == formula::detail::WideUnsigned<4>::from_u64(6));
    CHECK(!(*solved)[0].negative);
    CHECK(!(*solved)[2].negative);
    // A matrix whose second column has no non-zero pivot is singular.
    std::array<std::array<Wide, 3>, 2> const singular { { { whole(1), whole(2), whole(4) },
                                                          { whole(2), whole(4), whole(5) } } };
    CHECK(formula::detail::fraction_free_solve<2, 4>(singular).error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("the kernel refuses a singular design exactly and in double", "[least-squares][kernel]")
{
    // x1 = 1, 2, 4, 7 and a second regressor that is x1 again, up to scale and
    // offset: whatever was observed, the design has no unique solution.
    constexpr std::array<Rational, 4> lengths { rat(102, 10), rat(109, 10), rat(121, 10), rat(143, 10) };
    constexpr std::array<std::array<Rational, 4>, 2> doubled { { { rat(1), rat(2), rat(4), rat(7) },
                                                                 { rat(2), rat(4), rat(8), rat(14) } } };
    constexpr std::array<std::array<Rational, 4>, 2> shifted { {
        { rat(1), rat(2), rat(4), rat(7) },
        { rat(27415, 100), rat(27515, 100), rat(27715, 100), rat(28015, 100) },
    } };
    constexpr std::array<std::array<Rational, 4>, 2> affine { { { rat(1), rat(2), rat(4), rat(7) },
                                                                { rat(-4), rat(-1), rat(5), rat(14) } } };
    for (auto const& design: { doubled, shifted, affine })
    {
        CHECK(exact_fit(design, lengths).error() == formula::ArithmeticError::DomainError);
        CHECK(approximate_fit(design, lengths).error() == formula::ArithmeticError::DomainError);
    }
}

namespace
{
// x1 = 1..8, x2 = x1 + e / delta with e = +1, -1, -1, +1, +1, -1, -1, +1,
// orthogonal to x1; y = 3 + 2 x1 - 5 x2 exactly. 1 - R^2 of x2 on x1 is
// 8 / (42 delta^2 + 8).
template <std::int64_t Delta>
constexpr std::array<std::array<Rational, 8>, 2> near_collinear()
{
    constexpr std::array<std::int64_t, 8> signs { 1, -1, -1, 1, 1, -1, -1, 1 };
    std::array<std::array<Rational, 8>, 2> made;
    for (std::size_t at = 0; at < 8; ++at)
    {
        auto const position = static_cast<std::int64_t>(at) + 1;
        made[0][at] = rat(position);
        made[1][at] = rat(position * Delta + signs[at], Delta);
    }
    return made;
}

template <std::int64_t Delta>
constexpr std::array<Rational, 8> exactly_linear()
{
    constexpr auto made = near_collinear<Delta>();
    std::array<Rational, 8> responses;
    for (std::size_t at = 0; at < 8; ++at)
        responses[at] = *formula::checked_sub(*formula::checked_add(rat(3), *formula::checked_mul(rat(2), made[0][at])),
                                              *formula::checked_mul(rat(5), made[1][at]));
    return responses;
}
} // namespace

TEST_CASE("the kernel answers a nearly collinear design exactly, and in double down to the stated tolerance",
          "[least-squares][kernel]")
{
    constexpr std::array exact { rat(3), rat(2), rat(-5), rat(1), rat(8) };
    // 1 - R^2 = 1/1312501, about 7.6e-7: both answer; double loses about six
    // digits to the condition of the design.
    CHECK(exact_fit(near_collinear<500>(), exactly_linear<500>()) == exact);
    auto const at500 = approximate_fit(near_collinear<500>(), exactly_linear<500>());
    REQUIRE(at500.has_value());
    CHECK(close((*at500)[1], rat(2), 1e-8));
    // 1/525000001, about 1.9e-9, above the tolerance of 1e-9: both answer.
    CHECK(exact_fit(near_collinear<10'000>(), exactly_linear<10'000>()) == exact);
    CHECK(approximate_fit(near_collinear<10'000>(), exactly_linear<10'000>()).has_value());
    // 1/2100000001, about 4.8e-10, below it: exact still answers, double
    // refuses -- it cannot tell this design from a singular one.
    CHECK(exact_fit(near_collinear<20'000>(), exactly_linear<20'000>()) == exact);
    CHECK(approximate_fit(near_collinear<20'000>(), exactly_linear<20'000>()).error()
          == formula::ArithmeticError::DomainError);
}

TEST_CASE("the kernel's double route agrees with its exact route on well-conditioned data", "[least-squares][kernel]")
{
    auto const line = approximate_fit(lineTimes, lineLengths);
    REQUIRE(line.has_value());
    CHECK(close((*line)[0], rat(19, 2000), 1e-12));
    CHECK(close((*line)[1], rat(19, 28'000), 1e-12));
    CHECK(close((*line)[2], rat(1083, 1085), 1e-12));
    CHECK((*line)[3] == 4.0);
    auto const two = approximate_fit(twoRegressors, twoRegressorLengths);
    REQUIRE(two.has_value());
    CHECK(close((*two)[0], rat(22365154943, 276592800000), 1e-12));
    CHECK(close((*two)[1], rat(346407, 4609880000), 1e-12));
    CHECK(close((*two)[2], rat(3842851, 69148200), 1e-12));
    CHECK(close((*two)[3], rat(27398849648, 27403085919), 1e-12));
}

TEST_CASE("an exact fit that outgrows the kernel's width is Overflow, never a line", "[least-squares][kernel]")
{
    // Point k at ((k + 1)/(k + 2), (2k + 3)/(k + 3)): a different denominator
    // on every point. 15 points fit; 128 need about 744 bits, past 384.
    auto const distinct = [](std::size_t pointCount) {
        std::array<Rational, 128> xs;
        std::array<Rational, 128> ys;
        for (std::size_t at = 0; at < pointCount; ++at)
        {
            auto const position = static_cast<std::int64_t>(at);
            xs[at] = rat(position + 1, position + 2);
            ys[at] = rat(2 * position + 3, position + 3);
        }
        std::array<std::span<Rational const>, 1> const regressorColumns { std::span<Rational const> { xs }.first(
            pointCount) };
        return formula::detail::exact_regression<1>(regressorColumns, std::span<Rational const> { ys }.first(pointCount));
    };
    CHECK(distinct(15).has_value());
    CHECK(distinct(128).error() == formula::ArithmeticError::Overflow);
}
