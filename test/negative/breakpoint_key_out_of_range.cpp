// SPDX-License-Identifier: Apache-2.0
// A Breakpoint's key is a 64-bit pair, so that it stays a template argument;
// a key beyond 64 bits fails to compile, naming the guard.
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/rational.hpp>

constexpr formula::Breakpoint tooWide = formula::breakpoint(formula::Rational { formula::Int128 { 1 } << 70 });

int main()
{
    return tooWide.numerator == 0 ? 1 : 0;
}
