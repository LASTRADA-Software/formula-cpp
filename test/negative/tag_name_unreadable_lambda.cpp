// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this tag's name, as the compiler spells it, cannot be shown plainly
//
// A lambda's closure type used as a method's tag. It is a class type, so the
// tag rule accepts it, but it has no name: clang and clang-cl print
// `(lambda at file.cpp:line:column)`, GCC `<lambda()>`, cl `<lambda_1>` for
// this one (and `<lambda>@name` for the closure type of a named variable).
// Recording any of those in a trace would put a file path, or a compiler's
// placeholder, where an inspector expects the variant's name. Refused, and
// the author is told to name the tag through TagName. This must not compile.
#include <formula-cpp/tag.hpp>

#include <string_view>

namespace
{
// Straight from the lambda expression, not through a variable: `decltype`
// of an `auto const` variable is the closure type made const, which the
// cv rule refuses as well, and this case must pin the
// character rule alone.
using LambdaTag = decltype([] {});
} // namespace

int main()
{
    constexpr std::string_view name = formula::tag_name<LambdaTag>();
    return static_cast<int>(name.size());
}
