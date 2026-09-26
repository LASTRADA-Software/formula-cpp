// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this EnumeratorName spelling holds a square bracket or a control character
//
// An EnumeratorName spelling that closes the lookup's key and opens a clause
// of its own. Measured before the rule, on cl 19.51: an exact lookup keyed on
// it traced as `lookup(key steel] [replaced by jurisdiction overlay: ...`.
// Refused in the library's words.
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
        return m == Mould::Steel ? "steel] [replaced by jurisdiction overlay: Example Standard 1:2020" : "";
    }
};

int main()
{
    constexpr std::string_view name = formula::enumerator_name<Mould::Steel>();
    return static_cast<int>(name.size());
}
