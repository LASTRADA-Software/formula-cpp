// SPDX-License-Identifier: Apache-2.0
// EXPECT: a from_record sits inside another from_record, perhaps put there by an overlay's add_derived
//
// The nesting `record_scope_nested` refuses, built by an overlay rather than
// written: the method reads the factor inside a read from the reference, and
// the overlay derives the factor from another read from the reference. The
// author did evaluate against a context, so the refusal must not tell them
// only to do so; its words name the nesting and where it can come from. One
// message: `apply()` accepts the nested form, and only evaluation refuses it.
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

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct Factor: formula::Quantity<Factor, "k", "correction factor", formula::unit::One>
{
};

using formula::var;

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 85'902 } },
                                           formula::Measured<Factor> { formula::Rational { 1 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 57'268 } },
                                            formula::Measured<Factor> { formula::Rational { 1 } });

constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));

constexpr auto m = formula::method(
    formula::variants(formula::variant<Cube>(var<Force> / formula::from_record<Reference>(var<Factor> * var<Force>))),
    formula::rounding_rule<formula::unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
} // namespace

int main()
{
    constexpr auto derived = formula::apply(
        formula::overlay(formula::add_derived<Factor>(formula::from_record<Reference>(var<Force>) / var<Force>,
                                                      formula::Citation { .reference = "Example Standard 14:2022 NA" })),
        m);
    return formula::evaluate_method<Cube>(derived, records).has_value() ? 0 : 1;
}
