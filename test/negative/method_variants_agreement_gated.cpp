// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this argument of variants(...) is not a variant of a method
// REJECT: RequireVariantsAgree
// REJECT: RequireDistinctVariantTags
//
// Pins that the dimension-agreement rule and the distinct-tag rule are
// GATED: neither must be asked about a pack that holds a non-variant, because
// a non-variant has no `dimension` and no `tag`, and asking anyway buries our
// message under the compiler's own "no such member" errors. It pins nothing
// about the order of any other rule.
//
// Any rule added behind the same gate -- anything that reads a member of a
// variant -- must add its own template name to this case's REJECT list in
// test/CMakeLists.txt, or this case goes on passing while the new rule's
// cascade appears; see `RequireWellFormedVariants` in method.hpp.
//
// The offender is a bare `int` for exactly that reason. It genuinely has no
// `dimension`, which is what `method_variants_not_a_variant.cpp` deliberately
// avoids -- its offender is an expression node that publishes one, so the
// agreement rule has nothing to say there whether it is asked or not, and
// that file cannot see this ordering at all.
//
// The REJECT is what makes this a test. Our refusal fires whether or not the
// agreement rule is also asked, so EXPECT alone passes either way; only the
// ABSENCE of `RequireVariantsAgree` tells the two apart. That name appears in
// the instantiation stack once the rule is asked on each of the four
// toolchains measured locally -- cl 19.51, clang-cl 22, clang++ 20 and g++ 13;
// CI's g++-14 and Apple clang legs were not measured. The template name rather
// than an error code, because the codes are cl's own.
//
// This must not compile.
#include <formula-cpp/method.hpp>

namespace
{
struct Cube
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", formula::unit::Millimetre>
{
};

using formula::var;

inline constexpr auto broken = formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeX>) ), 42);
} // namespace

int main()
{
    return static_cast<int>(std::tuple_size_v<decltype(broken.cases)>);
}
