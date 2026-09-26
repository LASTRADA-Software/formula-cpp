// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this EnumeratorName spelling holds a square bracket or a control character
//
// An EnumeratorName spelling holding a tab, a control character like the
// newline that would write a line of its own. A different kind of character
// to the check from a bracket, so a case of its own.
//
// This must not compile.
#include <formula-cpp/enumerator.hpp>

#include <cstdint>
#include <string_view>

namespace
{
enum class Mould : std::uint8_t
{
    Steel,
    Plastic,
};
} // namespace

template <>
struct formula::EnumeratorName<Mould>
{
    static constexpr std::string_view of(Mould m) noexcept
    {
        return m == Mould::Steel ? "steel\tmould" : "";
    }
};

int main()
{
    constexpr std::string_view name = formula::enumerator_name<Mould::Steel>();
    return static_cast<int>(name.size());
}
