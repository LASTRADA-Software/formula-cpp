// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque operation does not accept inputs of these dimensions
// REJECT: compute cannot be called
// REJECT: must be noexcept
//
// Grams raised by a dimensionless level: the operation's output_dimensions
// answers std::nullopt. Refused once where the call is built. The output is
// only built, not evaluated under a result quantity: O1's amendment 3 is
// still open, and until the lead rules, a refused call's stand-in dimension
// would draw RequireResultDimension as well (plan, C12).
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

// The lowest reading raised by a shift of the same dimension -- and only the
// same dimension.
struct ShiftedLowest
{
    static constexpr std::string_view name = "shifted lowest";
    static constexpr std::array shapes { formula::InputShape::Series, formula::InputShape::Single };
    static constexpr std::array<std::string_view, 1> outputs { "shifted" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 2> declared) noexcept
    {
        if (!(declared[0] == declared[1]))
            return std::nullopt;
        return std::array { declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(std::span<Rep const> readings,
                                                                                         Rep raisedBy) noexcept
    {
        (void) raisedBy;
        return std::array { readings[0] };
    }
};

inline constexpr auto shifted = formula::opaque_output<"shifted">(formula::opaque<ShiftedLowest>(
    { .reference = "Example Standard 12" }, formula::series<Reading, 3>, formula::var<Level>));

int main()
{
    return shifted.refused ? 0 : 1;
}