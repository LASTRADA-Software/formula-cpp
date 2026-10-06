// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 11: methods and overlays. One method gives the strength of a
// cube and of a cylinder, and a jurisdiction's overlay changes how it rounds.

#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <print>

namespace
{
using formula::var;
namespace unit = formula::unit;

// --8<-- [start:tags]
struct Cube
{
};
struct Cylinder
{
};
// --8<-- [end:tags]

// --8<-- [start:quantities]
using Load = formula::Quantity<struct LoadTag, "F", "maximum load", unit::Kilonewton>;
using SideA = formula::Quantity<struct SideATag, "a", "first side of the loaded face", unit::Millimetre>;
using SideB = formula::Quantity<struct SideBTag, "b", "second side of the loaded face", unit::Millimetre>;
using Diameter = formula::Quantity<struct DiameterTag, "d", "diameter of the cylinder", unit::Millimetre>;
// --8<-- [end:quantities]

// --8<-- [start:method]
constexpr formula::DecimalRounding tenthMpa { unit::Megapascal,
                                              formula::DecimalPlaces { 1 },
                                              formula::RoundingMode::HalfAwayFromZero };

inline constexpr auto compressiveStrength = formula::method(
    formula::variants(
        formula::variant<Cube>(var<Load> / (var<SideA> * var<SideB>)),
        formula::variant<Cylinder>(var<Load> / (formula::pi * formula::pow<2>(var<Diameter>) / formula::number(4)))),
    formula::rounding_rule<tenthMpa>(),
    formula::constraints());
// --8<-- [end:method]

// --8<-- [start:overlay]
constexpr formula::DecimalRounding wholeMpa { unit::Megapascal,
                                              formula::DecimalPlaces { 0 },
                                              formula::RoundingMode::HalfAwayFromZero };

inline constexpr formula::Citation exampleRounding { .title = "Example jurisdiction",
                                                     .reference = "Example Standard 12:2020 NA",
                                                     .section = "NA.4" };

inline constexpr auto exampleJurisdiction = formula::overlay(formula::with_rounding<wholeMpa>(exampleRounding));

inline constexpr auto inExampleJurisdiction = formula::apply(exampleJurisdiction, compressiveStrength);
// --8<-- [end:overlay]

// --8<-- [start:show]
/// Evaluates @p method for the variant @p Tag names, and prints @p heading and
/// the trace. False when the method fails or gives no value.
template <typename Tag, typename M, typename Env>
bool show(char const* heading, M const& method, Env const& specimen)
{
    auto const run = formula::explain_method<Tag>(method, specimen);
    if (!run.outcome)
    {
        std::println("{}: {}", heading, run.outcome.error());
        return false;
    }
    if (!run.outcome->has_value())
    {
        std::println("{}: no value", heading);
        return false;
    }
    std::println("{}:", heading);
    std::print("{}", formula::render_trace(run.trace, { .maxSteps = 20 }));
    return true;
}
// --8<-- [end:show]
} // namespace

int main()
{
    // --8<-- [start:specimens]
    auto const cube = formula::environment(
        formula::Measured<Load> { 675 }, formula::Measured<SideA> { 150 }, formula::Measured<SideB> { 150 });
    auto const cylinder = formula::environment(formula::Measured<Load> { 540 }, formula::Measured<Diameter> { 150 });
    // --8<-- [end:specimens]

    // --8<-- [start:base]
    if (!show<Cube>("cube, base method", compressiveStrength, cube))
        return 1;
    std::println("");
    if (!show<Cylinder>("cylinder, base method", compressiveStrength, cylinder))
        return 1;
    // --8<-- [end:base]
    std::println("");

    // --8<-- [start:jurisdiction]
    if (!show<Cube>("cube, Example jurisdiction", inExampleJurisdiction, cube))
        return 1;
    std::println("");
    if (!show<Cylinder>("cylinder, Example jurisdiction", inExampleJurisdiction, cylinder))
        return 1;
    // --8<-- [end:jurisdiction]
    return 0;
}
