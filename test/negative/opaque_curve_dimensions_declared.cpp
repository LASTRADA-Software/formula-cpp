// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque operation's output_dimensions must be a static function taking std::array<Dimension, D>
// REJECT: no matching overloaded function
// REJECT: no matching function
//
// A curve operation that forgot a curve contributes two dimensions, points
// then values: its output_dimensions takes one. Refused once, in this
// library's words, rather than as the compiler's "no matching function".
#include <formula-cpp/opaque.hpp>

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

struct Opening: formula::Quantity<Opening, "d", "an invented opening", formula::unit::Millimetre>
{
};
struct Load: formula::Quantity<Load, "F_h", "an invented load held", formula::unit::Newton>
{
};

struct EndToEnd
{
    static constexpr std::string_view name = "end to end";
    static constexpr std::array shapes { formula::InputShape::Curve };
    static constexpr std::array<std::string_view, 1> outputs { "rise" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(
        std::span<Rep const> points, std::span<Rep const> pointValues) noexcept
    {
        (void) points;
        return std::array { pointValues.back() };
    }
};

inline constexpr auto call = formula::opaque<EndToEnd>(
    { .reference = "Example Standard 12" }, formula::curve(formula::series<Opening, 3>, formula::series<Load, 3>));

int main()
{
    return call.inputCount == 1 ? 0 : 1;
}