// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque output's position names no output of its operation
// REJECT: subscript
// REJECT: RequireAddendsAgree
// REJECT: RequireResultDimension
//
// An output node built by hand at the very position opaque_output uses for a
// name it did not find, over a sound call. Refused where it is built, in its
// own words: were it silently refused instead, every check over it would go
// quiet -- here the sum of a mass and a mass squared, evaluated as a mass.
#include <formula-cpp/opaque.hpp>

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

struct Reading: formula::Quantity<Reading, "r", "an invented reading", formula::unit::Gram>
{
};
struct Probe: formula::Quantity<Probe, "p", "an invented probe mass", formula::unit::Gram>
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
    using Call = std::remove_cv_t<decltype(call)>;
    formula::OpaqueOutputNode<static_cast<std::size_t>(-1), Call> const forged { {}, call };
    auto const specimen =
        formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { formula::Rational { 163 } },
                                                               formula::Measured<Reading> { formula::Rational { 127 } },
                                                               formula::Measured<Reading> { formula::Rational { 197 } }),
                             formula::Measured<Probe> { formula::Rational { 103 } });
    return formula::checked_evaluate<Reading>(forged + formula::var<Probe> * formula::var<Probe>, specimen).has_value() ? 0
                                                                                                                        : 1;
}