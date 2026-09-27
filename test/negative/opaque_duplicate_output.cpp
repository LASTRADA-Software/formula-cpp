// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an opaque operation declares two outputs with the same name
// REJECT: may hold only ASCII letters, digits and single spaces
//
// Two outputs both named "first": opaque_output<"first"> could not tell them
// apart. Refused once, and the name asked for is found, so the unknown-output
// refusal does not fire either.
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
    static constexpr std::array<std::string_view, 2> outputs { "first", "first" };

    static consteval std::optional<std::array<formula::Dimension, 2>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0], declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 2>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        return std::array { readings[0], readings[0] };
    }
};

inline constexpr auto first = formula::opaque_output<"first">(
    formula::opaque<FirstReading>({ .reference = "Example Standard 12" }, formula::series<Reading, 3>));

int main()
{
    return first.dimension == formula::dim::Mass ? 0 : 1;
}