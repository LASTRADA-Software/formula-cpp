// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record role's displayed name is not identifier-like
//
// A role that is a template specialisation with no TagName, bound in a
// context and read through a scope. Its own name, `Batch<2>`, would typeset
// as two comparisons in LaTeX and read as one in every dialect, so it is
// refused and the author is pointed at TagName. The record and the scope
// both ask one class template, so this draws one message, not one per site
// -- counted by hand on g++ and clang++.
//
// This must not compile.
#include <formula-cpp/environment.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/record.hpp>

namespace
{
template <int Number>
struct Batch
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 60'000 } });
} // namespace

int main()
{
    constexpr auto records = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::record<Batch<2>>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
    constexpr auto read = formula::checked_evaluate_si<formula::Rational>(
        formula::from_record<Batch<2>>(formula::var<Force>), records);
    return read.has_value() ? 0 : 1;
}
