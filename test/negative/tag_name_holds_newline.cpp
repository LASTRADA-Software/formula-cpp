// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this TagName spelling holds a square bracket or a control character
//
// A TagName spelling holding a newline: printed as written, the trace would
// gain a numbered line that is no step. A different kind of character to the
// check from a bracket, so a case of its own.
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
        return "Cube\n9. a line that is no step";
    }
};

int main()
{
    constexpr std::string_view name = formula::tag_name<Cube>();
    return static_cast<int>(name.size());
}
