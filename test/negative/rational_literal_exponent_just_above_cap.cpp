// SPDX-License-Identifier: Apache-2.0
// An exponent just above the cap of 1000 is refused.
// This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 1e1001_r;

int main()
{
    return 0;
}
