// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque operation's compute cannot be called with the values its declared inputs give
// REJECT: must be noexcept
//
// A series declared, and a compute that takes one value: refused once, and
// whether the call is noexcept is not asked of a call that cannot be made.
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
    static constexpr std::expected<std::array<Rep, 2>, formula::ArithmeticError> compute(Rep reading) noexcept
    {
        return std::array { reading, reading };
    }
};

inline constexpr auto first = formula::opaque_output<"first">(
    formula::opaque<FirstReading>({ .reference = "Example Standard 12" }, formula::series<Reading, 3>));

int main()
{
    return first.dimension == formula::dim::Mass ? 0 : 1;
}