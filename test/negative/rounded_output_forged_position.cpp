// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque output's position names no output of its operation
// REJECT: formula: this rounded_output names a unit
//
// A rounded output built by hand, as a public aggregate, at position 5 of an
// operation with one output. Refused where it is built, so that it can never
// be evaluated past the end of its call's outputs -- once: the node's own
// position check and that of the output it reads its dimension from are one
// check of one position. The unit check is silent over the refused output.
//
// This must not compile.
#include <formula-cpp/formula.hpp>

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

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
    constexpr auto forged = formula::RoundedOpaqueOutputNode<5,
                                                             std::remove_cvref_t<decltype(spanCall)>,
                                                             formula::unit::Gram,
                                                             formula::DecimalPlaces { 1 },
                                                             formula::RoundingMode::HalfEven> { {}, spanCall };
    return formula::checked_evaluate<Span>(forged, twoReadings).has_value() ? 0 : 1;
}
