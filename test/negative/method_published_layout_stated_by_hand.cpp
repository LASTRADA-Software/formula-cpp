// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a variant's published position is stated only by the library
//
// A two-variant pack stated, in an aggregate initialiser with no `detail::`
// written, as published at the 6th and 8th of 9: a well-formed layout, which
// the checking constructor used to accept, so that a method no overlay
// touched traced `[variant Cylinder (8th of 9), selected by tag]`. A layout is stated only by the library.
//
// This must not compile.
#include <formula-cpp/method.hpp>

#include <tuple>

namespace
{
struct Cube
{
};
struct Cylinder
{
};

struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

using Pack = formula::Variants<formula::VariantCase<Cube, formula::VarNode<EdgeX>>,
                               formula::VariantCase<Cylinder, formula::VarNode<EdgeX>>>;
} // namespace

int main()
{
    constexpr Pack pack { std::tuple { formula::variant<Cube>(var<EdgeX>), formula::variant<Cylinder>(var<EdgeX>) },
                          { { 5, 7 }, 9 } };
    return pack.published.count() == 9 ? 0 : 1;
}
