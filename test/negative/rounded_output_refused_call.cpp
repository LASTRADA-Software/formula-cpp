// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque call passes an input of another shape than the operation declares
// REJECT: formula: this rounded_output names a unit
// REJECT: this result quantity does not measure the dimension
//
// A rounded output of a call refused for its input: one value where the
// operation declares a series. The call is refused where it is built; its
// outputs' dimensions are then stand-ins, so neither the unit check nor the
// result quantity's asks about them.
//
// This must not compile.
#include <formula-cpp/formula.hpp>

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace
{
struct Reading: formula::Quantity<Reading, "r", "an invented reading", formula::unit::Gram>
{
};
struct Span: formula::Quantity<Span, "r_sp", "the span of the readings", formula::unit::Gram>
{
};

// The span of a series of readings, in the readings' dimension.
struct ReadingSpan
{
    static constexpr std::string_view name = "reading span";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 1> outputs { "span" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        std::expected<Rep, formula::ArithmeticError> const apart =
            formula::RepTraits<Rep>::subtract(readings[1], readings[0]);
        if (!apart.has_value())
            return std::unexpected { apart.error() };
        return std::array { *apart };
    }
};

inline constexpr auto spanCall =
    formula::opaque<ReadingSpan>({ .reference = "Example Standard 7" }, formula::series<Reading, 2>);
inline constexpr auto twoReadings =
    formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { formula::Rational { 1273, 10 } },
                                                           formula::Measured<Reading> { formula::Rational { 1394, 10 } }));
} // namespace

int main()
{
    auto const refusedCall = formula::opaque<ReadingSpan>({ .reference = "Example Standard 7" }, formula::var<Reading>);
    auto const roundedSpan =
        formula::rounded_output<"span", formula::unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(
            refusedCall);
    auto const oneReading = formula::environment(formula::Measured<Reading> { formula::Rational { 1273, 10 } });
    return formula::checked_evaluate<Span>(roundedSpan, oneReading).has_value() ? 0 : 1;
}
