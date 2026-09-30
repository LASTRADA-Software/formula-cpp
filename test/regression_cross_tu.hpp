// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// One multiple regression, its environment and one of its outputs, `inline
/// constexpr` in a header and used from `multiple_least_squares_tests.cpp` and
/// `regression_cross_tu_b.cpp`. The other unit takes the output node as a
/// parameter, so an output whose type differs between the two -- another
/// position, another operation -- names another function and fails to link;
/// and it returns the operation's output names, which must read the same.

#include <formula-cpp/least_squares.hpp>

#include <cstddef>
#include <expected>
#include <string_view>

namespace regression_cross_tu
{
struct Temperature: formula::Quantity<Temperature, "T", "an invented temperature", formula::unit::Celsius>
{
};
struct Content: formula::Quantity<Content, "w", "an invented content", formula::unit::Percent>
{
};
struct Length: formula::Quantity<Length, "L", "an invented length", formula::unit::Millimetre>
{
};

inline constexpr auto fit = formula::multiple_least_squares(
    formula::regressors(formula::observations<Temperature, 8>, formula::observations<Content, 8>),
    formula::observations<Length, 8>,
    { .title = "Length by temperature and content", .reference = "Example Standard 12", .section = "5.3" });

inline constexpr auto secondCoefficient = formula::opaque_output<"coefficient 2">(fit);

// Six rows, invented: 11.3 ... 29.7 degC, 2.3 ... 4.3 %, 103.52 ... 106 mm.
inline constexpr auto sixRows =
    formula::environment(formula::MeasuredObservations<Temperature, 8>(formula::Rational { 113, 10 },
                                                                       formula::Rational { 137, 10 },
                                                                       formula::Rational { 179, 10 },
                                                                       formula::Rational { 191, 10 },
                                                                       formula::Rational { 233, 10 },
                                                                       formula::Rational { 297, 10 }),
                         formula::MeasuredObservations<Content, 8>(formula::Rational { 23, 10 },
                                                                   formula::Rational { 31, 10 },
                                                                   formula::Rational { 29, 10 },
                                                                   formula::Rational { 41, 10 },
                                                                   formula::Rational { 37, 10 },
                                                                   formula::Rational { 43, 10 }),
                         formula::MeasuredObservations<Length, 8>(formula::Rational { 2588, 25 },
                                                                  formula::Rational { 10413, 100 },
                                                                  formula::Rational { 10433, 100 },
                                                                  formula::Rational { 1051, 10 },
                                                                  formula::Rational { 10521, 100 },
                                                                  formula::Rational { 106 }));
} // namespace regression_cross_tu

/// Defined in `regression_cross_tu_b.cpp`: @p output evaluated there, in SI.
[[nodiscard]] formula::Evaluated<formula::Rational> second_coefficient_in_other_tu(
    decltype(regression_cross_tu::secondCoefficient) const& output) noexcept;

/// Defined in `regression_cross_tu_b.cpp`: the two-regressor operation's output
/// name at zero-based position @p at, as that unit sees it.
[[nodiscard]] std::string_view output_name_in_other_tu(std::size_t at) noexcept;
