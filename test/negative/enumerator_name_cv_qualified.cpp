// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: EnumeratorName is specialised for a const-qualified enumeration
//
// An EnumeratorName specialisation written for `QualifiedShape const`. The
// library only ever asks `EnumeratorName<QualifiedShape>` -- a key's type is
// the unqualified enumeration -- so this specialisation could never take
// effect, and the key would render under its reflected name while the author
// believes it reads "cube 139 mm". Refused in the library's own words rather
// than silently ignored. This must not compile.
#include <formula-cpp/enumerator.hpp>

#include <string_view>

namespace
{
enum class QualifiedShape
{
    Cube,
};
} // namespace

template <>
struct formula::EnumeratorName<QualifiedShape const>
{
    static constexpr std::string_view of(QualifiedShape) noexcept
    {
        return "cube 139 mm";
    }
};

int main()
{
    constexpr std::string_view name = formula::enumerator_name<QualifiedShape::Cube>();
    return static_cast<int>(name.size());
}
