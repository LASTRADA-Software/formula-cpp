// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 10: lookup tables. A size correction factor is read from a
// published table by the specimen's edge length, and an edge outside it has none.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>

#include <print>

namespace
{
using formula::var;
namespace unit = formula::unit;
using namespace formula::literals;

// --8<-- [start:quantities]
using Edge = formula::Quantity<struct EdgeTag, "a", "edge length of the specimen", unit::Millimetre>;
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", unit::Megapascal>;
using SizeFactor = formula::Quantity<struct SizeFactorTag, "k", "size correction factor", unit::One>;
using CorrectedStrength =
    formula::Quantity<struct CorrectedStrengthTag, "f_cs", "compressive strength corrected for size", unit::Megapascal>;
// --8<-- [end:quantities]

// --8<-- [start:bands]
inline constexpr formula::BandTable<3> EdgeBands {
    formula::band(0, 100),   // 0 to under 100 mm
    formula::band(100, 200), // 100 to under 200 mm
    formula::band(200, 300), // 200 to under 300 mm
};
// --8<-- [end:bands]

// --8<-- [start:lookup]
constexpr auto sizeFactor = formula::yields<SizeFactor>(
    formula::banded_lookup<unit::Millimetre, EdgeBands, unit::One>(var<Edge>, { 1.05_r, 1, 0.95_r }));

constexpr auto correctedStrength = formula::yields<CorrectedStrength>(var<Strength> * sizeFactor.expression);
// --8<-- [end:lookup]

// --8<-- [start:report]
/// Prints the size factor and the corrected strength of a specimen with an
/// edge of @p edge mm and a strength of 30 MPa. False when either has no value.
bool report(int edge)
{
    auto const specimen = formula::environment(formula::Measured<Edge> { edge }, formula::Measured<Strength> { 30 });
    auto const factor = formula::checked_evaluate(sizeFactor, specimen);
    if (!factor)
    {
        std::println("a = {} mm: no size factor: {}", edge, factor.error());
        return false;
    }
    auto const corrected = formula::checked_evaluate(correctedStrength, specimen);
    if (!corrected)
    {
        std::println("a = {} mm: no corrected strength: {}", edge, corrected.error());
        return false;
    }
    std::println("a = {} mm: {} = {}, {} = {}",
                 edge,
                 formula::symbol_of<SizeFactor>(),
                 *factor,
                 formula::symbol_of<CorrectedStrength>(),
                 *corrected);
    return true;
}
// --8<-- [end:report]
} // namespace

int main()
{
    // --8<-- [start:render]
    std::println("{} = {}", formula::symbol_of<SizeFactor>(), formula::render(sizeFactor));
    // --8<-- [end:render]

    // --8<-- [start:evaluate]
    if (!report(150) || !report(100) || !report(250))
        return 1;
    // --8<-- [end:evaluate]

    // --8<-- [start:miss]
    // 300 mm is in no band: the lookup must report an error, not a number.
    if (report(300))
        return 1;
    // --8<-- [end:miss]
    return 0;
}
