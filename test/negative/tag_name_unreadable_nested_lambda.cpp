// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this tag's name, as the compiler spells it, cannot be shown plainly
//
// A template specialization with a lambda's closure type as its argument:
// `Box<decltype([] {})>`. Once the `::` cut has taken the lambda's scope, cl
// prints `Box<<lambda_1_>>` -- a placeholder, TU-local and cl's own, where
// the variant's name belongs. The whole name starts with a letter, so only
// the rule that every ARGUMENT starts with a letter, a digit, `_` or `-`
// sees it on cl. clang (`Box<(lambda at ...)>`) and GCC (`Box<<lambda()>>`)
// are refused by the `(` as well. This must not compile.
#include <formula-cpp/tag.hpp>

#include <string_view>

namespace
{
template <typename T>
struct Box;

using NestedLambdaTag = Box<decltype([] {})>;
} // namespace

int main()
{
    constexpr std::string_view name = formula::tag_name<NestedLambdaTag>();
    return static_cast<int>(name.size());
}
