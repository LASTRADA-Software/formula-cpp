// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record role is displayed as 'this record'
// REJECT: formula: this record role's displayed name is not identifier-like
//
// `record_role_name_this_record` spelt as an identifier: a role whose
// TagName is "this_record". A lineage check against it would name it as the
// library names its own ThisRecord, and a reader could tell the two apart by
// nothing, so it is refused as "This Record" is (the final review's L4). The
// name is identifier-like, so the REJECT pins that the identifier rule does
// not also fire; this draws one message.
//
// This must not compile.
#include <formula-cpp/environment.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/record.hpp>

#include <string_view>

namespace
{
struct Misnamed
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 60'000 } });
} // namespace

template <>
struct formula::TagName<Misnamed>
{
    static constexpr std::string_view of() noexcept { return "this_record"; }
};

int main()
{
    constexpr auto records = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::record<Misnamed>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
    constexpr auto read =
        formula::checked_evaluate_si<formula::Rational>(formula::from_record<Misnamed>(formula::var<Force>), records);
    return read.has_value() ? 0 : 1;
}
