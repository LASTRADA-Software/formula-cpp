// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record_context binds two roles whose names are displayed alike
// REJECT: formula: this record_context binds the same role to more than one record
//
// Two roles, each a type of its own, one spelt through TagName as the other
// reads. A trace names a record by its role's name, so their lines would be
// told apart by their keys alone, and the page's two rows not at all (the
// final review's L4). Two distinct types, so the same-role refusal must not
// also fire: the REJECT.
//
// This must not compile.
#include <formula-cpp/environment.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/record.hpp>

#include <string_view>

namespace
{
struct Reference
{
};
struct OtherReference
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 85'902 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 57'268 } });
} // namespace

template <>
struct formula::TagName<OtherReference>
{
    static constexpr std::string_view of() noexcept { return "Reference"; }
};

int main()
{
    constexpr auto records = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there),
        formula::record<OtherReference>(formula::record_key(formula::sample_id(29), formula::test_id(1)), there));
    constexpr auto read =
        formula::checked_evaluate_si<formula::Rational>(formula::from_record<Reference>(formula::var<Force>), records);
    return read.has_value() ? 0 : 1;
}
