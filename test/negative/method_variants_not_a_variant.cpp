// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this argument of variants(...) is not a variant of a method
//
// A bare expression node handed to `variants(...)` where a variant belongs.
//
// This is the silent case, not a theoretical one. Measured on cl 19.51 at
// `/W4 /WX` before the guard existed: `variants(var<EdgeX>, var<EdgeX>)`
// compiled with exit 0 and no diagnostics at all, because the dimension
// agreement rule was satisfied -- a `VarNode` publishes a `dimension` just as
// a variant does -- and nothing else was asking. The result was a
// `Variants<...>` with no tags anywhere in it, which no selection could ever
// use.
//
// The offender sits third of four, but its position is not what gives this
// case its strength -- unlike the agreement rule in
// `method_variants_disagree.cpp`, which compares the first variant against
// each later one and so has a first and a last comparison to mutate, this
// rule is an unordered fold over every argument, and every argument's check
// is instantiated wherever the offender sits. What makes the guard strong is
// that `IsVariantCase` recognises a `VariantCase` specialisation rather than
// duck-typing its members, so a look-alike struct that carries `tag`,
// `expression` and `dimension` still does not pass.
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
struct Core
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

// The third argument is an expression, not a variant of one. It measures the
// same dimension as its neighbours, so the agreement rule has nothing to say
// about it -- only the rule this file pins can.
inline constexpr auto broken = formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                                 formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ),
                                                 var<Force> / (var<EdgeY> * var<EdgeY>),
                                                 formula::variant<Core>(var<Force> / (var<EdgeY> * var<EdgeX>) ));
} // namespace

int main()
{
    return static_cast<int>(std::tuple_size_v<decltype(broken.cases)>);
}
