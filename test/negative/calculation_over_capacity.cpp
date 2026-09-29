// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a calculation holds at most 64 quantities, inputs and definitions together, and this one holds more
// REJECT: formula: these definitions read one another in a cycle
// REJECT: formula: this calculation neither defines nor reads this quantity
// REJECT: shift
//
// 65 definitions, one more than a calculation's graph has bits for. Refused
// once; the graph is not built, so nothing shifts a bit past the 64th, and a
// query answers nothing.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

#include <cstddef>
#include <utility>

namespace
{
template <std::size_t I>
struct Tag
{
};

template <std::size_t I>
using Numbered = formula::Quantity<Tag<I>, "q", "an invented quantity", formula::unit::One>;

template <std::size_t... Is>
[[nodiscard]] constexpr auto numbered(std::index_sequence<Is...>)
{
    return formula::calculation(formula::define<Numbered<Is>>(formula::number(formula::Rational { 1 }))...);
}
} // namespace

int main()
{
    constexpr auto crowded = numbered(std::make_index_sequence<65>{});
    return formula::upstream_of<Numbered<64>>(crowded).size() == 0 ? 0 : 1;
}
