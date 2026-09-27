// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this EnumeratorName specialisation does not have the shape the library reads
//
// An EnumeratorName specialisation whose `of` is misspelt `Of`. It is plainly
// meant as a customization -- the author wrote the specialisation -- and the
// silent reading of it, "no `of`, so not customized", would render the key
// under its reflected name `Cube` while the author believes it reads
// "cube 139 mm". Nothing would say the wording had been dropped. So it must be
// refused in the library's own words. This must not compile.
//
// Reached through render(), the way an author meets it, rather than through
// `enumerator_name` directly: the point is that rendering an exact lookup
// keyed on this enumeration asks for the name, and the gate fires there.
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/render.hpp>

#include <string_view>

namespace
{
enum class MisspeltShape
{
    Cube,
};

inline constexpr formula::KeyTable<MisspeltShape, 1> MisspeltKeys { MisspeltShape::Cube };
} // namespace

template <>
struct formula::EnumeratorName<MisspeltShape>
{
    static constexpr std::string_view Of(MisspeltShape) noexcept
    {
        return "cube 139 mm";
    }
};

int main()
{
    constexpr auto node =
        formula::exact_lookup<MisspeltKeys, formula::unit::One>(MisspeltShape::Cube, { formula::Rational { 1127, 1000 } });
    return static_cast<int>(formula::render(node).size());
}
