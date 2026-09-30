// SPDX-License-Identifier: Apache-2.0
// 1e19 is an integer no Rational holds, so it is refused however it is spelled.
// This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 1e19_r;

int main()
{
}
