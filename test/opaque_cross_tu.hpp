// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// One opaque call and one environment, `inline constexpr` in a header and
/// used from two translation units: `opaque_tests.cpp` and
/// `opaque_cross_tu_b.cpp`. The other unit's function takes the output node
/// as a **parameter**, so an output whose type differs between the two units
/// -- a different operation, input or output position -- names a different
/// function in each and the test fails to link. See `series_cross_tu.hpp` for
/// what this does and does not catch.
///
/// Everything shared lives in a named namespace, never an anonymous one.

#include <formula-cpp/opaque.hpp>

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace opaque_cross_tu
{
struct Reading: formula::Quantity<Reading, "r", "an invented reading", formula::unit::Gram>
{
};

/// A consumer's operation: the lowest and the highest element of a series.
struct SeriesExtent
{
    static constexpr std::string_view name = "series extent";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 2> outputs { "lowest", "highest" };

    static consteval std::optional<std::array<formula::Dimension, 2>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0], declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 2>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        Rep least = readings[0];
        Rep most = readings[0];
        for (Rep const& each: readings)
        {
            if (each < least)
                least = each;
            if (most < each)
                most = each;
        }
        return std::array { least, most };
    }
};

inline constexpr auto extent = formula::opaque<SeriesExtent>(
    { .title = "Extent of readings", .reference = "Example Standard 12", .section = "4.3" }, formula::series<Reading, 3>);

inline constexpr auto highest = formula::opaque_output<"highest">(extent);

// Invented readings, three significant digits: 163, 127, 197 g.
inline constexpr auto inputs =
    formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { formula::Rational { 163 } },
                                                           formula::Measured<Reading> { formula::Rational { 127 } },
                                                           formula::Measured<Reading> { formula::Rational { 197 } }));
} // namespace opaque_cross_tu

/// Defined in `opaque_cross_tu_b.cpp`: @p highestOutput evaluated there.
[[nodiscard]] std::expected<formula::Outcome<opaque_cross_tu::Reading>, formula::ArithmeticError> highest_in_other_tu(
    decltype(opaque_cross_tu::highest) const& highestOutput) noexcept;
