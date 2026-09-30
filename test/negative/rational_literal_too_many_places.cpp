// SPDX-License-Identifier: Apache-2.0
// A denominator of 10^19 is above the range of the integers a Rational holds.
// This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 0.0000000000000000001_r;

int main()
{
}
