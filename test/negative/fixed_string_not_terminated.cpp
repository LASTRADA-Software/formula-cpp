// SPDX-License-Identifier: Apache-2.0
// The constructor takes any char array, not only a string literal, and `view()`
// drops the last byte on the assumption that it is a terminator. For an array
// that is not terminated, that assumption silently loses a character -- measured
// on all three compilers before this was refused. This must not compile.
#include <formula-cpp/detail/fixed_string.hpp>

namespace
{
constexpr char const NotTerminated[] = { 'a', 'b', 'c' };
}

constexpr formula::detail::FixedString<3> Bad { NotTerminated };

int main()
{
    return static_cast<int>(Bad.view().size());
}
