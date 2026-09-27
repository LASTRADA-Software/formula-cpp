// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: TagName<Tag>::of() is not usable in a constant expression
//
// A TagName specialisation of the right shape whose `of` is not `constexpr`.
// The library reads a customized name at compile time -- it is part of what
// makes the name safe to keep in a trace -- so an `of` that can only run at
// run time cannot be read at all. Treating it as "not customized" and falling
// back to the reflected name would drop the author's wording without a word,
// so it is refused in the library's own words instead of the compiler's
// "call to non-constexpr function". This must not compile.
#include <formula-cpp/tag.hpp>

#include <string_view>

namespace
{
struct RuntimeCylinder;
} // namespace

template <>
struct formula::TagName<RuntimeCylinder>
{
    static std::string_view of() noexcept
    {
        return "cylinder 139 x 277 mm";
    }
};

int main()
{
    constexpr std::string_view name = formula::tag_name<RuntimeCylinder>();
    return static_cast<int>(name.size());
}
