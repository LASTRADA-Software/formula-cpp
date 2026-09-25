// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this tag is not a plain class type
//
// A variant tagged `const Cylinder` where `Cylinder` is meant.
//
// Silent before the tag rule existed: `const Cylinder` is a class type, the
// variant's expression is well formed, and the pack satisfies every rule that
// asks about expressions. It matters beyond this one variant because
// `const Cylinder` and `Cylinder` are different types, so a pack holding both
// would slip past any duplicate-tag check that compares tags with
// `std::is_same_v`.
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

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

inline constexpr auto broken = formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) ),
                                                 formula::variant<const Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ));
} // namespace

int main()
{
    return static_cast<int>(std::tuple_size_v<decltype(broken.cases)>);
}
