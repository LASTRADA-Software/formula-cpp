// SPDX-License-Identifier: Apache-2.0
// An exponent too large for an int is refused by the cap on the exponent, while it is being read.
// Without the cap, the exponent would overflow an int during the compile-time evaluation and the
// compiler would give its own diagnostic, not the library's. This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 1e99999999999_r;

int main()
{
    return 0;
}
