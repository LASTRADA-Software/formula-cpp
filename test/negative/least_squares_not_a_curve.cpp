// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: linear_least_squares fits a curve; pair the domain and the values with curve(domain, values)
// REJECT: no matching overloaded function
// REJECT: no matching function
//
// Two loose series handed to the fit: a curve already pairs a domain with
// values of one length, which two series would have to re-derive. Refused
// once, in this library's words, and not in the compiler's.
#include <formula-cpp/least_squares.hpp>

struct Elapsed: formula::Quantity<Elapsed, "t", "an invented elapsed time", formula::unit::Second>
{
};
struct Length: formula::Quantity<Length, "L", "an invented length", formula::unit::Millimetre>
{
};

inline constexpr auto fit = formula::linear_least_squares(
    formula::series<Elapsed, 4>, formula::series<Length, 4>, { .reference = "Example Standard 12" });

int main()
{
    return sizeof(fit) > 0 ? 0 : 1;
}