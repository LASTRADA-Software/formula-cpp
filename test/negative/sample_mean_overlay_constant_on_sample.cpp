// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay fixes a quantity the method reads as a series or as raw observations; one constant cannot stand for many values
//
// `with_constant<Mass>` on a method whose variant is the mean of six
// determinations of `Mass`. The statistic reads its quantity as a series, so
// the overlay refuses to fix it with one constant, in the message above; the
// "nobody reads it" refusal stays silent, since a variant does read it. This
// must not compile.
#include <formula-cpp/formula.hpp>

#include <tuple>

namespace
{
struct MeanOfSix
{
};

struct Mass: formula::Quantity<Mass, "m", "mass of a determination", formula::unit::Gram>
{
};

inline constexpr auto meanMethod = formula::method(
    formula::variants(formula::variant<MeanOfSix>(formula::sample_mean(formula::series<Mass, 6>))),
    formula::rounding_rule<formula::unit::Gram, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
} // namespace

int main()
{
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::with_constant<Mass>(formula::Rational { 40 })), meanMethod);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 0 ? 1 : 0;
}
