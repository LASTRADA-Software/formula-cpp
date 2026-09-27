// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque operation's series inputs differ in length
// REJECT: does not accept inputs of these dimensions
//
// Two series of three and four readings for an operation over both. Refused
// once; the dimensions -- which disagree too -- are not asked about.
#include <formula-cpp/opaque.hpp>

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

struct Reading: formula::Quantity<Reading, "r", "an invented reading", formula::unit::Gram>
{
};

struct Level: formula::Quantity<Level, "q", "an invented level", formula::unit::One>
{
};

struct PairedFirst
{
    static constexpr std::string_view name = "paired first";
    static constexpr std::array shapes { formula::InputShape::Series, formula::InputShape::Series };
    static constexpr std::array<std::string_view, 1> outputs { "first" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 2> declared) noexcept
    {
        if (!(declared[0] == declared[1]))
            return std::nullopt;
        return std::array { declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(
        std::span<Rep const> readings, std::span<Rep const> others) noexcept
    {
        (void) others;
        return std::array { readings[0] };
    }
};

inline constexpr auto first = formula::opaque_output<"first">(formula::opaque<PairedFirst>(
    { .reference = "Example Standard 12" }, formula::series<Reading, 3>, formula::series<Level, 4>));

int main()
{
    return first.refused ? 0 : 1;
}