// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: prune_variant<Tag>() was given no citation
//
// A prune with no citation. Which variants a method keeps is a
// jurisdiction's decision, and the trace says whose; a prune nobody can
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
    constexpr auto operation = formula::prune_variant<Cube>();
    return sizeof(operation) > 0 ? 0 : 1;
}
