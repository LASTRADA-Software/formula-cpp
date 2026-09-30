// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this rounded_output names a unit with an offset, such as degrees Celsius; a fitted coefficient is a difference or a ratio, which an offset unit would misstate; name the unit without its offset
// REJECT: formula: this rounded_output names a unit that does not measure
//
// The span of two probe temperatures, rounded "to 0.1 degC". The dimension is
// right, so the dimension check passes and only the offset's message fires;
// but a span is a difference, and converting a difference into degrees
// Celsius adds the scale's offset to it.
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
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(std::span<Rep const> readings) noexcept
    {
        std::expected<Rep, formula::ArithmeticError> const apart = formula::RepTraits<Rep>::subtract(readings[1], readings[0]);
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

struct Probe: formula::Quantity<Probe, "T_p", "an invented probe temperature", formula::unit::Kelvin>
{
};

inline constexpr auto probeSpan = formula::opaque<ReadingSpan>({ .reference = "Example Standard 7" }, formula::series<Probe, 2>);
} // namespace

int main()
{
    auto const roundedSpan =
        formula::rounded_output<"span", formula::unit::Celsius, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(probeSpan);
    auto const twoProbes = formula::environment(formula::measured_series<Probe>(formula::Measured<Probe> { formula::Rational { 293 } },
                                                                             formula::Measured<Probe> { formula::Rational { 297 } }));
    return formula::checked_evaluate_si<formula::Rational>(roundedSpan, twoProbes).has_value() ? 0 : 1;
}
