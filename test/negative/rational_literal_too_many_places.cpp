// SPDX-License-Identifier: Apache-2.0
// A denominator of 10^39 is above Int128's range, the integers a Rational holds.
// This must not compile.
#include <formula-cpp/rational.hpp>

using namespace formula::literals;

constexpr formula::Rational refused = 0.000'000'000'000'000'000'000'000'000'000'000'000'001_r;

int main()
{
}
