// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this precision_limit's level expression reads precision_level
//
// An overlay defines x_B as twice the level, and x_B is a nested limit's
// level: the inner level would silently be computed from the outer one. The
// level check sees through the overlay's derived quantity, so the rewritten
// inner limit is refused where `apply` builds it -- the same message as the
// same limit written directly. This must not compile.
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

inline constexpr auto inner = formula::precision_limit<formula::PrecisionKind::Repeatability>(
    formula::var<ResultB>, formula::precision_level<ResultB>);
inline constexpr auto outer =
    formula::precision_limit<formula::PrecisionKind::Reproducibility>(formula::var<ResultA>, inner);
inline constexpr auto base = formula::method(
    formula::variants(formula::variant<Cube>(outer)),
    formula::rounding_rule<formula::unit::Gram, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
} // namespace

int main()
{
    constexpr auto overlaid = formula::apply(
        formula::overlay(formula::add_derived<ResultB>(formula::Rational { 2 } * formula::precision_level<ResultA>)), base);
    auto const outcome = formula::evaluate_method<Cube>(
        overlaid, formula::environment(formula::Measured<ResultA> { formula::Rational { 40 } }));
    return outcome.has_value() ? 0 : 1;
}
