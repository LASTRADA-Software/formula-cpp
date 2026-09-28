// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a variant's published position is stated only by the library
//
// The same fabrication at run time: an existing pack's layout reassigned to
// `{ { 0, 8 }, 9 }`, again with no `detail::` written. Not a constant expression's initialiser this time, so a
// case of its own.
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
    Pack pack { std::tuple { formula::variant<Cube>(var<EdgeX>), formula::variant<Cylinder>(var<EdgeX>) } };
    pack.published = { { 0, 8 }, 9 };
    return pack.published.count() == 9 ? 0 : 1;
}
