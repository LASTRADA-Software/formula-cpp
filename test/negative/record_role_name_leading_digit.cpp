// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this record role's displayed name is not identifier-like
//
// A role whose TagName starts with a digit, read through a scope. `f_c of 2nd
// reference` -- and a name that is all digits, `f_c of 9` -- reads as
// arithmetic on a number, so a role's name must start with a letter (the
// lead's ruling on the task 9 review's L5).
//
// This must not compile.
#include <formula-cpp/environment.hpp>
#include <formula-cpp/record.hpp>

#include <string_view>

namespace
{
struct SecondReference
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
} // namespace

template <>
struct formula::TagName<SecondReference>
{
    static constexpr std::string_view of() noexcept { return "2nd reference"; }
};

int main()
{
    constexpr auto read = formula::from_record<SecondReference>(formula::var<Force>);
    return sizeof(read) == 0 ? 1 : 0;
}
