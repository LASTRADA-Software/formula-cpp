// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this tag's name, as the compiler spells it, cannot be shown plainly
//
// A template specialization over a function type used as a method's tag:
// `Box<void(specimen::Cube)>`. The name's grammar tracks nesting through `<`
// and `>` only, so the qualifier inside the parentheses cuts back through
// the `(`, leaving `Box<Cube)>` on all four compilers (cl adds `__cdecl`
// first, and loses it the same way). A fragment is not a name. This must not
// compile.
#include <formula-cpp/tag.hpp>

#include <string_view>

namespace
{
namespace specimen
{
    struct Cube;
} // namespace specimen

template <typename T>
struct Box;
} // namespace

int main()
{
    constexpr std::string_view name = formula::tag_name<Box<void(specimen::Cube)>>();
    return static_cast<int>(name.size());
}
