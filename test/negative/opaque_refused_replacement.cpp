// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque call passes an input of another shape than the operation declares
// REJECT: this overlay replaces a variant with a formula of a different dimension
//
// A call refused for its shape, then a variant of a method in grams replaced by the refused output:
// RequireReplacementKeepsDimension. The stand-in output is refused (`detail::refused_already`), so the check stays silent
// and the one mistake draws one message.
#include <formula-cpp/formula.hpp>

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>

namespace
{
struct Cube
{
};
struct Cylinder
{
};

struct Reading: formula::Quantity<Reading, "r", "an invented reading", formula::unit::Gram>
{
};
struct Probe: formula::Quantity<Probe, "p", "an invented probe reading", formula::unit::Gram>
{
};

// Declares a series; the call below passes one value, so the call is refused
// for its shape, and its output stands in with a dimensionless dimension.
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

inline constexpr auto bad = formula::opaque_output<"first">(
    formula::opaque<FirstReading>({ .reference = "Example Standard 12" }, formula::var<Probe>));

inline constexpr formula::BandTable<2> invented_bands { formula::band(103, 1, 163, 1), formula::band(163, 1, 331, 1) };
inline constexpr formula::BreakpointTable<3> invented_points { formula::breakpoint(103),
                                                               formula::breakpoint(127),
                                                               formula::breakpoint(163) };
inline constexpr auto rule =
    formula::rounding_rule<formula::unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>();
} // namespace
int main()
{
    constexpr auto m =
        formula::method(formula::variants(formula::variant<Cube>(formula::var<Reading>)), rule, formula::constraints());
    constexpr auto replaced = formula::apply(
        formula::overlay(formula::replace_variant<Cube>(bad, { .reference = "Example Standard 12:2021 NA" })), m);
    return sizeof(replaced) > 0 ? 0 : 1;
}
