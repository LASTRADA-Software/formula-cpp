// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque call passes an input of another shape than the operation declares
// REJECT: does not accept inputs of these dimensions
//
// Raw observations passed where an operation declares a series: refused once,
// as any other shape is, and the dimensions are not asked about. The operation
// accepts no dimension at all, so a dimension check reached past the shape's
// refusal would add its own message.
#include <formula-cpp/opaque.hpp>

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

struct Reading: formula::Quantity<Reading, "r", "an invented reading", formula::unit::Gram>
{
};

struct SeriesSpan
{
    static constexpr std::string_view name = "series span";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 1> outputs { "span" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 1>) noexcept
    {
        return std::nullopt;
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        return std::array { readings[0] };
    }
};

inline constexpr auto spanOf = formula::opaque_output<"span">(
    formula::opaque<SeriesSpan>({ .reference = "Example Standard 12" }, formula::observations<Reading, 8>));

int main()
{
    return spanOf.refused ? 0 : 1;
}
