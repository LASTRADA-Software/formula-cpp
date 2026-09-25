// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this tag is not a plain class type
// REJECT: RequireTagDeclaredOnce
//
// Two variants tagged `int`. The tag is repeated, but the mistake is the tag
// itself: `int` names nothing a specimen can be, and replacing it with a
// class type clears the repeat as well. So only the tag rule may report it.
//
// The distinct-tag rule is asked only once every tag is a plain class type,
// the way `RequireSelectableTag` asks its match only of a plain tag. EXPECT
// alone cannot see that gate, since the tag rule fires either way; the
// absence of `RequireTagDeclaredOnce` can.
//
// This must not compile.
#include <formula-cpp/method.hpp>

namespace
{
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

inline constexpr auto broken = formula::variants(formula::variant<int>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                                 formula::variant<int>(var<Force> / (var<EdgeX> * var<EdgeX>) ));
} // namespace

int main()
{
    return static_cast<int>(std::tuple_size_v<decltype(broken.cases)>);
}
