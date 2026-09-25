// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method declares two variants for the same tag
//
// Five variants, with the repeated tag at positions 1 and 3: neither the
// first pair nor the last, and not adjacent. The rule compares every pair,
// and this placement is what tells that apart from its three likeliest
// narrowings -- the first pair only (0,1), the last pair only (3,4), and
// neighbours only -- each of which finds no repeat here and lets the pack
// compile. One case kills all three; the task report records each.
//
// The five variants agree in dimension, so the agreement rule has nothing to
// say, and every tag is a plain class type, so the tag rule has nothing
// either: the repeat is the only thing wrong.
//
// This must not compile.
#include <formula-cpp/method.hpp>

namespace
{
struct Cube
{
};
struct Cylinder
{
};
struct Prism
{
};
struct Slab
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

// Two variants for Cylinder: whichever is selected, the other is dead code.
inline constexpr auto broken = formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                                 formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ),
                                                 formula::variant<Prism>(var<Force> / (var<EdgeY> * var<EdgeY>) ),
                                                 formula::variant<Cylinder>(var<Force> / (var<EdgeY> * var<EdgeX>) ),
                                                 formula::variant<Slab>(var<Force> / (var<EdgeX> * var<EdgeY>) ));
} // namespace

int main()
{
    return static_cast<int>(std::tuple_size_v<decltype(broken.cases)>);
}
