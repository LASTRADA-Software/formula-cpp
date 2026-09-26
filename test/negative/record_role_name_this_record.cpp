// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record role is displayed as 'this record'
// REJECT: formula: this record role's displayed name is not identifier-like
//
// A role whose TagName is "This Record", bound in a context and read
// through a scope. Every value read from it would be traced `from record
// This Record (sample 23, test 3)`, which is how a trace names the record
// being evaluated, so it is refused in any case (the lead's ruling on the
// task 9 review's M2). The name is identifier-like, so the REJECT pins that
// the identifier rule does not also fire; the record and the scope ask one
// class template, so this draws one message.
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
    static constexpr std::string_view of() noexcept { return "This Record"; }
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
