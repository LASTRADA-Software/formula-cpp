// SPDX-License-Identifier: Apache-2.0
// A hexadecimal spelling with an E in it is not a decimal either, though the E looks like an exponent.
// This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 0x1E_r;

int main()
{
}
