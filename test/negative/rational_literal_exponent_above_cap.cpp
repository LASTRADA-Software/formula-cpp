// SPDX-License-Identifier: Apache-2.0
// A decimal exponent above the cap of 1000 is refused while it is being read, before any value is built.
// This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 1e1001_r;

int main()
{
    return 0;
}
