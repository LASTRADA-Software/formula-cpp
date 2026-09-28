// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque output's position names no output of its operation
// REJECT: subscript
//
// An output node built by hand, as a public aggregate, at position 5 of an
// operation with one output. Refused where it is built, so that it can never
// be evaluated past the end of its call's outputs.
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
    static constexpr std::array<std::string_view, 1> outputs { "first" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        return std::array { readings[0] };
    }
};

inline constexpr auto call =
    formula::opaque<FirstReading>({ .reference = "Example Standard 12" }, formula::series<Reading, 3>);

int main()
{
    formula::OpaqueOutputNode<5, decltype(call)> const forged { {}, call };
    return forged.index == 5 ? 0 : 1;
}