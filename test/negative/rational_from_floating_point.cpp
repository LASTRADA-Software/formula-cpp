// SPDX-License-Identifier: Apache-2.0
// A double is a binary fraction. Letting it convert implicitly would make
// `Rational r = 0.45;` mean 8106479329266893 / 2^54 rather than 9 / 20, and
// nothing downstream could tell. This must not compile.
#include <formula-cpp/rational.hpp>

formula::Rational const ratio = 0.45;

int main()
{
    return ratio.is_zero() ? 1 : 0;
}
