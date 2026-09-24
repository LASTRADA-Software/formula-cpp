// SPDX-License-Identifier: Apache-2.0
// A zero denominator names no number. Accepting it would let `exponent(0, 0)`
// hold {0, 0} while `exponent(0, 1)` holds {0, 1} -- two distinct types for one
// physical dimension, which is the exact failure Exponent exists to prevent.
// This must not compile.
#include <formula-cpp/dimension.hpp>

constexpr formula::Exponent bad = formula::exponent(1, 0);

int main()
{
    return bad.denominator;
}
