// SPDX-License-Identifier: Apache-2.0
// 2^127, one more than the largest integer a Rational holds (Int128's maximum).
// This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 170'141'183'460'469'231'731'687'303'715'884'105'728_r;

int main()
{
}
