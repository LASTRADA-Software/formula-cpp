// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this tag's name, as the compiler spells it, cannot be shown plainly
//
// A template specialization with an unnamed class as its argument:
// `Box<decltype(Holder::member)>`. Once the `::` cut has taken `Holder::`,
// GCC prints `Box<<unnamed struct>>` and cl `Box<<unnamed-type-member>>` --
// a placeholder, differing per compiler, where the variant's name belongs.
// Only the rule that every ARGUMENT starts with a letter, a digit, `_` or
// `-` sees it on those two; clang (`Box<(unnamed struct at ...)>`) is
// refused by the `(` as well. This must not compile.
#include <formula-cpp/tag.hpp>

#include <string_view>

namespace
{
template <typename T>
struct Box;

struct Holder
{
    struct
    {
    } member;
};

using NestedUnnamedTag = Box<decltype(Holder::member)>;
} // namespace

int main()
{
    constexpr std::string_view name = formula::tag_name<NestedUnnamedTag>();
    return static_cast<int>(name.size());
}
