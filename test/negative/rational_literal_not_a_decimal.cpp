// SPDX-License-Identifier: Apache-2.0
// A hexadecimal spelling is not a decimal.
// This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 0x1F_r;

int main()
{
}
