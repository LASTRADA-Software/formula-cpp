// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: TagName is specialised for a const-qualified tag
//
// A TagName specialisation written for `QualifiedCylinder const`. The library
// only ever asks `TagName<QualifiedCylinder>` -- a tag is never cv-qualified,
// and `method.hpp` refuses one that is -- so this specialisation could never
// take effect, and the variant would be recorded under its reflected name
// while the author believes it reads "cylinder 139 x 277 mm". Refused in the
// library's own words rather than silently ignored. This must not compile.
#include <formula-cpp/tag.hpp>

#include <string_view>

namespace
{
struct QualifiedCylinder;
} // namespace

template <>
struct formula::TagName<QualifiedCylinder const>
{
    static constexpr std::string_view of() noexcept
    {
        return "cylinder 139 x 277 mm";
    }
};

int main()
{
    constexpr std::string_view name = formula::tag_name<QualifiedCylinder>();
    return static_cast<int>(name.size());
}
