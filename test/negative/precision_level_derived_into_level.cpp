// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this precision_limit's level expression reads precision_level
//
// An overlay defines x_A as a level, and x_A is the outermost limit's level.
// Refused where `apply` builds the rewritten limit, as a level reading a
// level -- not as a placeholder evaluated outside any limit expression,
// which the author never wrote: the placeholder is in the overlay's
// definition, and what it ends up in is a level. This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct ResultA: formula::Quantity<ResultA, "x_A", "first determination", formula::unit::Gram>
{
};
struct ResultB: formula::Quantity<ResultB, "x_B", "second determination", formula::unit::Gram>
{
};
struct Cube
{
};

inline constexpr auto levelLimit = formula::precision_limit<formula::PrecisionKind::Repeatability>(
    formula::var<ResultA>, formula::Rational { 1, 50 } * formula::precision_level<ResultA>);
inline constexpr auto base = formula::method(
    formula::variants(formula::variant<Cube>(levelLimit)),
    formula::rounding_rule<formula::unit::Gram, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
} // namespace

int main()
{
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::add_derived<ResultA>(formula::precision_level<ResultB>)), base);
    auto const outcome = formula::evaluate_method<Cube>(
        overlaid, formula::environment(formula::Measured<ResultB> { formula::Rational { 40 } }));
    return outcome.has_value() ? 0 : 1;
}
