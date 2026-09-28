// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: an opaque operation's compute must be noexcept
// REJECT: ~expected
// REJECT: flows off the end
//
// A compute declared without noexcept: an exception escaping it would end the
// program, since evaluation is noexcept throughout. Refused once, where the
// call is built; the evaluator's body is gated on the call being sound, so
// g++ 14.2 adds none of its own errors (phase 15's spike, step 7) -- the
// REJECTs name the ones it added when the body was not gated.
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
    static constexpr std::expected<std::array<Rep, 2>, formula::ArithmeticError> compute(std::span<Rep const> readings)
    {
        return std::array { readings[0], readings[1] };
    }
};

inline constexpr auto first = formula::opaque_output<"first">(
    formula::opaque<FirstReading>({ .reference = "Example Standard 12" }, formula::series<Reading, 3>));

inline constexpr auto inputs =
    formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { formula::Rational { 163 } },
                                                           formula::Measured<Reading> { formula::Rational { 127 } },
                                                           formula::Measured<Reading> { formula::Rational { 197 } }));

int main()
{
    return formula::checked_evaluate<Reading>(first, inputs).has_value() ? 0 : 1;
}