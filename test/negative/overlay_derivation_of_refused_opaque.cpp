// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this opaque operation does not accept inputs of these dimensions
// REJECT: no matching overloaded function
// REJECT: no matching function
// REJECT: cannot see inside
//
// A quantity derived as the output of a refused opaque call: the call's one
// message, and not also the derivation's "cannot see inside", which a
// refused call's unknown rewrite would otherwise draw
// (`RequireDerivationSeesNode`, gated on `refused_already`).
#include <formula-cpp/method.hpp>
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/overlay.hpp>

#include <array>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>

namespace
{
struct Only
{
};

struct Reading: formula::Quantity<Reading, "r", "an invented reading", formula::unit::Gram>
{
};
struct Level: formula::Quantity<Level, "q", "an invented level", formula::unit::One>
{
};
struct Lifted: formula::Quantity<Lifted, "r_s", "an invented lifted reading", formula::unit::One>
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
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(std::span<Rep const> values,
                                                                                         Rep raisedBy) noexcept
    {
        (void) raisedBy;
        return std::array { values[0] };
    }
};

constexpr formula::Citation annex { .reference = "Example Standard 12:2021 NA", .section = "NA.5" };

// Refused where it is built: grams raised by a dimensionless level.
inline constexpr auto shifted = formula::opaque_output<"shifted">(formula::opaque<ShiftedLowest>(
    { .reference = "Example Standard 12" }, formula::series<Reading, 3>, formula::var<Level>));

using OneThousandth =
    formula::RoundingRule<formula::unit::One, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>;
} // namespace
int main()
{
    constexpr auto m = formula::method(
        formula::variants(formula::variant<Only>(formula::var<Lifted>)), OneThousandth {}, formula::constraints());
    constexpr auto overlaid = formula::apply(formula::overlay(formula::add_derived<Lifted>(shifted, annex)), m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 0 ? 1 : 0;
}
