// SPDX-License-Identifier: Apache-2.0
// A Band's bounds are 64-bit pairs, so that it stays a template argument; a
// bound beyond 64 bits fails to compile, naming the guard.
#include <formula-cpp/band.hpp>
#include <formula-cpp/rational.hpp>

constexpr formula::Band tooWide =
    formula::band(formula::Rational { formula::Int128 { 1 } << 70 }, formula::Rational { formula::Int128 { 1 } << 71 });

int main()
{
    return tooWide.lowNumerator == 0 ? 1 : 0;
}
