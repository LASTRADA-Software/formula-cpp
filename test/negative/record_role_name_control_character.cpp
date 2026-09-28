// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record role's displayed name is not identifier-like
//
// A role whose TagName holds a newline, on a record alone. A blank line in
// a role's name ends a paragraph inside LaTeX math mode, which stops the
// document compiling, and in Markdown it can turn the whole formula into a
// heading. The record refuses it before any formula
// reads from it.
//
// This must not compile.
#include <formula-cpp/environment.hpp>
#include <formula-cpp/record.hpp>

#include <string_view>

namespace
{
struct Broken
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};

constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 60'000 } });
} // namespace

template <>
struct formula::TagName<Broken>
{
    static constexpr std::string_view of() noexcept { return "Ref\n\nB"; }
};

int main()
{
    constexpr auto made =
        formula::record<Broken>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there);
    return made.is_bound() ? 0 : 1;
}
