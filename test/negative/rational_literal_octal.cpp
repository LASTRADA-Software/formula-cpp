// SPDX-License-Identifier: Apache-2.0
// C++ reads a leading zero as octal, so this is not the decimal 17 a reader may expect.
// This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 017_r;

int main()
{
}
