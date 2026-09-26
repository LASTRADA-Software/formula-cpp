// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: pin_variant<Tag>() was given no citation
//
// A pin with no citation. Which variants a method keeps is a
// jurisdiction's decision, and the trace says whose; a pin nobody can
// attribute is refused, in the library's words rather than the compiler's
// "too few arguments".
//
// This must not compile.
#include <formula-cpp/overlay.hpp>

namespace
{
struct Cube
{
};
} // namespace

int main()
{
    constexpr auto operation = formula::pin_variant<Cube>();
    return sizeof(operation) > 0 ? 0 : 1;
}
