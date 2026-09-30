// SPDX-License-Identifier: Apache-2.0
// A binary spelling is not a decimal either.
// This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 0b101_r;

int main()
{
    return 0;
}
