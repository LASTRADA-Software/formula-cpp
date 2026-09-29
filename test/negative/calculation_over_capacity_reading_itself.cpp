// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a calculation holds at most 64 quantities, inputs and definitions together, and this one holds more
// REJECT: formula: this definition reads the quantity it defines
// REJECT: formula: these definitions read one another in a cycle
//
// 65 quantities, the first of which reads itself. Refused for its size
// alone: the graph is not built, and nothing about what it would hold is
// judged until it fits.
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

/// The first quantity plus one, from itself; every other one, 1.
template <std::size_t I>
[[nodiscard]] constexpr auto numbered_definition()
{
    if constexpr (I == 0)
        return formula::define<Numbered<0>>(formula::var<Numbered<0>> + formula::number(formula::Rational { 1 }));
    else
        return formula::define<Numbered<I>>(formula::number(formula::Rational { 1 }));
}

template <std::size_t... Is>
[[nodiscard]] constexpr auto numbered(std::index_sequence<Is...>)
{
    return formula::calculation(numbered_definition<Is>()...);
}
} // namespace

int main()
{
    [[maybe_unused]] constexpr auto crowded = numbered(std::make_index_sequence<65>{});
    return 0;
}
