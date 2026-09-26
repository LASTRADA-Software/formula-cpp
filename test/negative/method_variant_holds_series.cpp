// SPDX-License-Identifier: Apache-2.0
// EXPECT: this expression is a series, not a single value; evaluate it with checked_evaluate_series
// REJECT: is not a variant of a method
// REJECT: RequireVariantsAgree
// REJECT: RequireRoundingRuleMeasuresVariants
//
// A method reports one value, so a variant holds a single-valued expression.
// A series handed to variant<Tag> is refused there, once: what variant<Tag>
// returns after refusing is a variant of the right tag and dimension, so
// variants(...) and method(...) around it find nothing further to say about
// the same mistake.
#include <formula-cpp/method.hpp>
#include <formula-cpp/series.hpp>

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};
struct Cube
{
};
struct Cylinder
{
};

inline constexpr auto screening = formula::method(
    formula::variants(formula::variant<Cube>(formula::var<Retained>),
                      formula::variant<Cylinder>(formula::series<Retained, 3>)),
    formula::rounding_rule<formula::unit::Gram, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());

int main()
{
    return static_cast<int>(sizeof(screening));
}
