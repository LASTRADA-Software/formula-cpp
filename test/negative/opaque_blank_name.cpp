// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an opaque operation's name must say something
// REJECT: may hold only ASCII letters, digits and single spaces
//
// A name of two spaces says nothing: the trace could not say what ran.
// Refused once, as blank -- not also as unreadable, which two spaces also
// are, since the readable check is gated on the name saying something.
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
    static constexpr std::string_view name = "  ";
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

inline constexpr auto first = formula::opaque_output<"first">(
    formula::opaque<FirstReading>({ .reference = "Example Standard 12" }, formula::series<Reading, 3>));

int main()
{
    return first.dimension == formula::dim::Mass ? 0 : 1;
}