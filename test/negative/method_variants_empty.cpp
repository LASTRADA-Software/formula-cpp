// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this method declares no variants at all
//
// `variants()` with nothing in it. Measured on cl 19.51 at `/W4 /WX` before
// the guard existed: exit 0, no diagnostics -- an empty pack was a perfectly
// good value, ready to be handed to anything that takes one.
//
// Separate from `method_variants_not_a_variant.cpp` because the two rules are
// independent and each can be broken while the other still fires: an empty
// pack passes the every-argument-is-a-variant fold vacuously, and a pack of
// non-variants is not empty. Neither case can stand in for the other.
//
// This must not compile.
#include <formula-cpp/method.hpp>

namespace
{
inline constexpr auto broken = formula::variants();
} // namespace

int main()
{
    return static_cast<int>(std::tuple_size_v<decltype(broken.cases)>);
}
