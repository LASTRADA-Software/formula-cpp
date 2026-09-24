// SPDX-License-Identifier: Apache-2.0
// A reduced exponent that does not fit std::int32_t must be rejected, not
// narrowed. The narrowing is well-defined modular wrapping and so is perfectly
// legal in a constant expression: it would turn 4294967294 into -2 in silence.
// This must not compile.
#include <formula-cpp/dimension.hpp>

constexpr formula::Exponent bad = formula::exponent(2147483647, 1) * 2;

int main()
{
    return bad.numerator;
}
