// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay fixes a quantity the method reads as a series or as raw observations; one constant cannot stand for many values
// REJECT: overrides a quantity that no variant or constraint of the method uses
//
// `with_constant<Retained>` on a method whose only use of the retained mass
// is `sum(series<Retained, 5>)`. One constant cannot stand for a value at
// every screen, and the series would go on reading the environment. The
// REJECT pins that the "nobody reads it" refusal stays silent: a variant does
// read the quantity, so that sentence would be false.
//
// This must not compile.
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/series.hpp>

#include <tuple>

namespace
{
struct Share
{
};

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};
struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", formula::unit::Gram>
{
};

inline constexpr auto m = formula::method(
    formula::variants(formula::variant<Share>(formula::sum(formula::series<Retained, 5>) / formula::var<TotalMass>)),
    formula::rounding_rule<formula::unit::Percent, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
} // namespace

int main()
{
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::with_constant<Retained>(formula::Rational { 100 })), m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 0 ? 1 : 0;
}
