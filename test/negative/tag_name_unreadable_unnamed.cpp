// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this tag's name, as the compiler spells it, cannot be shown plainly
//
// An unnamed class used as a method's tag: the type of a member declared with
// an unnamed struct. clang and clang-cl print `(unnamed struct at
// file.cpp:line:column)`, GCC `<unnamed struct>`, cl `<unnamed-type-member>`.
// None of them is a name. Refused, and the author is told to name the tag
// through TagName. This must not compile.
//
// Not `using Tag = struct {};`: clang and cl treat the alias as naming the
// class and print `Tag`, which is shown as it is; only GCC prints it
// `<unnamed struct>`. Measured on all four, so that case would not be refused
// everywhere and cannot pin the rule.
#include <formula-cpp/tag.hpp>

#include <string_view>

namespace
{
struct Holder
{
    struct
    {
    } member;
};
using UnnamedTag = decltype(Holder::member);
} // namespace

int main()
{
    constexpr std::string_view name = formula::tag_name<UnnamedTag>();
    return static_cast<int>(name.size());
}
