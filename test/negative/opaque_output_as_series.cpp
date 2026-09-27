// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an opaque call is not a value, since an operation may have several outputs
// REJECT: this expression is a series, not a single value
//
// A whole call handed to checked_evaluate. A call is neither a value nor a
// series, and the refusal says which output to choose -- not the series
// refusal, which is about a different mistake.
#include <formula-cpp/opaque.hpp>

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

struct Reading: formula::Quantity<Reading, "r", "an invented reading", formula::unit::Gram>
{
};

struct FirstReading
{
    static constexpr std::string_view name = "first reading";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 2> outputs { "first", "second" };

    static consteval std::optional<std::array<formula::Dimension, 2>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0], declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 2>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        return std::array { readings[0], readings[1] };
    }
};

inline constexpr auto call =
    formula::opaque<FirstReading>({ .reference = "Example Standard 12" }, formula::series<Reading, 3>);

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { formula::Rational { 163 } },
                                                           formula::Measured<Reading> { formula::Rational { 127 } },
                                                           formula::Measured<Reading> { formula::Rational { 197 } }));

int main()
{
    return formula::checked_evaluate<Reading>(call, inputs).has_value() ? 0 : 1;
}