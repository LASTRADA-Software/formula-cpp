// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record role's displayed name is not identifier-like
//
// A role whose TagName holds a hyphen, read through a scope. In LaTeX math
// mode `\mathrm{Reference-B}` typesets as a subtraction, `Reference - B`,
// which the method does not perform, so the name is refused rather than
// escaped (the task 7 review's M2, and the lead's ruling).
//
// This must not compile.
#include <formula-cpp/environment.hpp>
#include <formula-cpp/record.hpp>

#include <string_view>

namespace
{
struct Hyphenated
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
} // namespace

template <>
struct formula::TagName<Hyphenated>
{
    static constexpr std::string_view of() noexcept { return "Reference-B"; }
};

int main()
{
    constexpr auto read = formula::from_record<Hyphenated>(formula::var<Force>);
    return sizeof(read) == 0 ? 1 : 0;
}
