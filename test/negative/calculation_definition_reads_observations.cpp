// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a calculation holds single values, and this definition reads a quantity as a series or as raw observations
// REJECT: formula: this definition holds a node kind the calculation cannot see inside
// REJECT: has no detail::LevelChildren specialisation
// REJECT: no matching
//
// Raw observations binned into classes and counted: one value in the end,
// but read from raw observations, which a calculation does not hold. One
// message, although the observations are read by two nodes.
//
// This must not compile.
#include <formula-cpp/calculation.hpp>

namespace
{
namespace unit = formula::unit;

struct Particle: formula::Quantity<Particle, "d_p", "an invented particle size", unit::Millimetre>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};

// Invented classes: 0 to under 163 mm, and 163 to under 277 mm.
inline constexpr formula::BandTable<2> classes { formula::band(0, 1, 163, 1), formula::band(163, 1, 277, 1) };
} // namespace

int main()
{
    constexpr auto counted = formula::binned<unit::Millimetre, classes>(formula::observations<Particle, 4>);
    [[maybe_unused]] constexpr auto share = formula::define<Share>(
        formula::sum(counted * formula::series_constant<unit::One>(formula::Rational { 0 }, formula::Rational { 1 }))
        / formula::sum(counted));
    return 0;
}
