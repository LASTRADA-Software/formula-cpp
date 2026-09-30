// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a calculation holds single values, and this definition reads a quantity as a series or as raw observations
// REJECT: formula: this definition holds a node kind the calculation cannot see inside
// REJECT: has no detail::LevelChildren specialisation
// REJECT: no matching
//
// A definition that reads a line through raw observations: the calculation
// walk reaches the call's inputs and refuses each quantity read as
// observations, once each -- two quantities, two messages. This is the walk's
// existing behaviour, not this fit's: a definition over a curve fit of two
// series draws two messages as well (measured with g++ 14). It is pinned as it is; a walk that stops
// after its first refused read would make this one message.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/least_squares.hpp>

namespace
{
struct Elapsed: formula::Quantity<Elapsed, "t", "an invented elapsed time", formula::unit::Second>
{
};
struct Length: formula::Quantity<Length, "L", "an invented length", formula::unit::Millimetre>
{
};
struct Rate: formula::Quantity<Rate, "v", "an invented rate of change", formula::unit::MillimetrePerMinute>
{
};
} // namespace

int main()
{
    constexpr auto fit = formula::linear_least_squares(
        formula::observations<Elapsed, 4>, formula::observations<Length, 4>, { .reference = "Example Standard 12" });
    [[maybe_unused]] constexpr auto rate = formula::define<Rate>(formula::opaque_output<"slope">(fit));
    return 0;
}
