// SPDX-License-Identifier: Apache-2.0
// One more than the largest integer a Rational holds.
// This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 9'223'372'036'854'775'808_r;

int main()
{
}
