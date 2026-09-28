// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an opaque operation declares no outputs
// REJECT: declares two outputs with the same name
//
// An operation with nothing to use. Refused once where the call is built.
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
    static constexpr std::array<std::string_view, 0> outputs {};

    static consteval std::optional<std::array<formula::Dimension, 0>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        (void) declared;
        return std::array<formula::Dimension, 0> {};
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 0>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        (void) readings;
        return std::array<Rep, 0> {};
    }
};

inline constexpr auto call =
    formula::opaque<FirstReading>({ .reference = "Example Standard 12" }, formula::series<Reading, 3>);

int main()
{
    return call.inputCount == 1 ? 0 : 1;
}