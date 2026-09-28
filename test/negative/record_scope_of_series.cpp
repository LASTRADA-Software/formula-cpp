// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: from_record was given a series, raw observations or a curve
//
// A read from another record over a series itself, and used in a formula. A
// scope holds one value, which its trace line attributes to the record; a
// series-valued scope has none. The author is told to reduce it inside
// the scope. The refused scope is answered as absent, so the division it is
// used in draws no message of its own.
//
// This must not compile.
#include <formula-cpp/formula.hpp>

namespace
{
struct Reference
{
};

struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", formula::unit::Gram>
{
};

constexpr auto screens = formula::environment(formula::measured_series<Retained>(
    formula::Measured<Retained> { formula::Rational { 130 } }, formula::Measured<Retained> { formula::Rational { 210 } }));
constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), screens),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), screens));
} // namespace

int main()
{
    constexpr auto read =
        formula::sum(formula::series<Retained, 2>) / formula::from_record<Reference>(formula::series<Retained, 2>);
    return formula::checked_evaluate_si<formula::Rational>(read, records).has_value() ? 0 : 1;
}
