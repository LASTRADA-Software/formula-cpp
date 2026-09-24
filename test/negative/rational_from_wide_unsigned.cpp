// SPDX-License-Identifier: Apache-2.0
// An unsigned type as wide as Int can hold values above Rational's maximum.
// Letting it convert implicitly made `std::size_t { 1 } << 63` a NEGATIVE
// Rational and SIZE_MAX equal to -1, silently. Every other failure path in this
// library reports rather than lying, so this must not compile.
#include <formula-cpp/rational.hpp>

#include <cstddef>

formula::Rational const tooWide = std::size_t { 1 } << 63;

int main()
{
    return tooWide.is_zero() ? 1 : 0;
}
