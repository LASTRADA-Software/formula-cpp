// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque call passes an input of another shape than the operation declares
// REJECT: does not accept inputs of these dimensions
// REJECT: compute cannot be called
//
// One value where the operation declares a series. Refused once; the
// dimensions and compute are never asked about.
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

inline constexpr auto first = formula::opaque_output<"first">(
    formula::opaque<FirstReading>({ .reference = "Example Standard 12" }, formula::var<Reading>));

int main()
{
    return first.dimension == formula::dim::Mass ? 0 : 1;
}