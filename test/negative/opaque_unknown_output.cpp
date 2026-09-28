// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque operation has no output of that name
// REJECT: this result quantity does not measure the dimension
//
// "fist" for "first": refused once, naming the operation and the name asked
// for. The two outputs differ in dimension -- a mass and a ratio -- so a node
// standing in for either would disagree with some result quantity. The node is
// refused instead, and the result's dimension check over it stays silent.
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
    static constexpr std::array<std::string_view, 2> outputs { "first", "ratio" };

    static consteval std::optional<std::array<formula::Dimension, 2>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0], formula::dim::Scalar };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 2>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        return std::array { readings[0], readings[1] };
    }
};

inline constexpr auto first = formula::opaque_output<"fist">(
    formula::opaque<FirstReading>({ .reference = "Example Standard 12" }, formula::series<Reading, 3>));

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { formula::Rational { 163 } },
                                                           formula::Measured<Reading> { formula::Rational { 127 } },
                                                           formula::Measured<Reading> { formula::Rational { 197 } }));

int main()
{
    return formula::checked_evaluate<Reading>(first, inputs).has_value() ? 0 : 1;
}