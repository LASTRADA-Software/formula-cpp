// SPDX-License-Identifier: Apache-2.0
// 10^39 is an integer no Rational holds (Int128 stops below 1.8 x 10^38), so it is refused however it is spelled.
// This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 1e39_r;

int main()
{
}
