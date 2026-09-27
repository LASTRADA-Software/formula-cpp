// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: from_record was given a series, raw observations or a curve
//
// The refused series-valued read of `record_scope_of_series`, in a method an
// overlay then fixes a constant of. The refused scope is a leaf to the
// overlay, so it adds no "cannot see inside" refusal of its own -- a second
// library message that cl, too, would show.
//
// This must not compile.
#include <formula-cpp/formula.hpp>

#include <tuple>

namespace
{
struct Reference
{
};
struct Cube
{
};

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};
struct Factor: formula::Quantity<Factor, "k", "correction factor", formula::unit::One>
{
};

using formula::var;

inline constexpr auto read = var<Factor> * formula::sum(formula::series<Retained, 2>)
                             / formula::from_record<Reference>(formula::series<Retained, 2>);
} // namespace

int main()
{
    constexpr auto m = formula::method(
        formula::variants(formula::variant<Cube>(read)),
        formula::rounding_rule<formula::unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    constexpr auto overlaid = formula::apply(
        formula::overlay(formula::with_constant<Factor>(formula::Rational { 97, 100 },
                                                        formula::Citation { .reference = "Example Standard 14:2022 NA" })),
        m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 0 ? 1 : 0;
}
