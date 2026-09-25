// SPDX-License-Identifier: Apache-2.0
// EXPECT: RequireTagDeclaredOnce<1,
//
// The fixture of `method_duplicate_tag.cpp`, pinned on the positions its
// diagnostic reports rather than on its message: the repeat is at 1 and 3,
// so the diagnostic must name `RequireTagDeclaredOnce<1, 3, Cylinder>`.
//
// Only the FIRST position is matched. Measured: cl 19.51 prints
// `RequireTagDeclaredOnce<1,3,` with no spaces, while clang-cl 22, clang++ 20
// and g++ 13 print `RequireTagDeclaredOnce<1, 3,` -- so `<1,` is the longest
// prefix all four share, and the second position has no spelling common to
// them. The tag's own spelling differs too (`anonymous-namespace'::Cylinder
// against (anonymous namespace)::Cylinder and {anonymous}::Cylinder).
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
