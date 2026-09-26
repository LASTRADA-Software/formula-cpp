// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a vocabulary holds only entries made by renames<Q>(symbol)
#include <formula-cpp/vocabulary.hpp>

#include <string_view>

struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", formula::unit::Megapascal>
{
};

// A bare spelling, with no quantity for it to name.
inline constexpr auto broken = formula::vocabulary(std::string_view { "R" });

int main()
{
    return formula::symbol_of<Strength>(broken).size() == 1 ? 0 : 1;
}
