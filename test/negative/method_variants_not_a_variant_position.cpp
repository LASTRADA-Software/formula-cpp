// SPDX-License-Identifier: Apache-2.0
// EXPECT: RequireVariant<2,
//
// Pins the POSITION the not-a-variant refusal reports, which
// `method_variants_not_a_variant.cpp` cannot: that file's EXPECT is the
// message, and the message is the same whatever index comes with it. Here the
// offender is the third of four arguments, so a correct refusal names index 2
// -- zero-based, as the message says -- and an index reported one too high or
// one too low, or pinned to the first or the last argument, does not.
//
// `RequireVariant<2,` because that is the one spelling all four compilers
// share. Measured on cl 19.51, clang-cl, clang++ and g++: each prints the
// index as a bare `2` followed directly by the comma, with no `ul` suffix;
// cl then runs straight into the next argument, the others add a space.
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

inline constexpr auto broken = formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                                 formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ),
                                                 var<Force> / (var<EdgeY> * var<EdgeY>),
                                                 formula::variant<Core>(var<Force> / (var<EdgeY> * var<EdgeX>) ));
} // namespace

int main()
{
    return static_cast<int>(std::tuple_size_v<decltype(broken.cases)>);
}
