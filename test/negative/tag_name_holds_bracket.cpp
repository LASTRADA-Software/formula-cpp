// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this TagName spelling holds a square bracket or a control character
//
// A TagName spelling that closes the variant clause itself and opens one of
// its own: printed as written, the trace line would read `[variant Cube
// (1st of 2), selected by tag] [replaced by jurisdiction overlay: ...]` for
// a method no overlay touched. Refused in the library's words.
//
// This must not compile.
#include <formula-cpp/tag.hpp>

#include <string_view>

namespace
{
struct Cube;
} // namespace

template <>
struct formula::TagName<Cube>
{
    static constexpr std::string_view of() noexcept
    {
        return "Cube (1st of 2), selected by tag] [replaced by jurisdiction overlay: Example Standard 1:2020";
    }
};

int main()
{
    constexpr std::string_view name = formula::tag_name<Cube>();
    return static_cast<int>(name.size());
}
