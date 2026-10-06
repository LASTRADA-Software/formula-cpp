// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 7: citations, rendering and documentation. The strength
// formula cites its source, and is written out as text, LaTeX and a symbol table.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>

#include <print>

namespace
{
using formula::var;
namespace unit = formula::unit;

using Load = formula::Quantity<struct LoadTag, "F", "maximum load", unit::Kilonewton>;
using Area = formula::Quantity<struct AreaTag, "A_c", "loaded area", unit::SquareMillimetre>;
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", unit::Megapascal>;

using SideA = formula::Quantity<struct SideATag, "a", "first side of the loaded face", unit::Millimetre>;
using SideB = formula::Quantity<struct SideBTag, "b", "second side of the loaded face", unit::Millimetre>;

constexpr formula::DecimalRounding tenthMpa { unit::Megapascal,
                                              formula::DecimalPlaces { 1 },
                                              formula::RoundingMode::HalfAwayFromZero };

// --8<-- [start:formulas]
constexpr auto loadedArea = formula::yields<Area>(var<SideA> * var<SideB>);
constexpr auto strength =
    formula::yields<Strength>(formula::documented(formula::rounded<tenthMpa>(var<Load> / loadedArea),
                                                  { .title = "Compressive strength",
                                                    .reference = "Example Standard 12:2020",
                                                    .section = "6.1",
                                                    .equation = "(1)",
                                                    .text = "The maximum load divided by the area of the loaded face." }));
// --8<-- [end:formulas]
} // namespace

int main()
{
    // --8<-- [start:render]
    std::println("text:  {}", formula::render(strength));
    std::println("LaTeX: {}", formula::render<formula::Dialect::LaTeX>(strength));
    // --8<-- [end:render]

    // --8<-- [start:document]
    formula::Documentation const page = formula::document(strength);
    std::println("symbols:");
    for (formula::SymbolEntry const& entry: page.symbols)
        std::println("  {:<5} {:<6} {}", entry.symbol, entry.unit, entry.description);
    for (formula::Citation const& citation: page.citations)
        std::println("cited: {}, {}", citation.title, citation.reference);
    // --8<-- [end:document]

    // --8<-- [start:evaluate]
    auto const specimen = formula::environment(
        formula::Measured<SideA> { 150 }, formula::Measured<SideB> { 150 }, formula::Measured<Load> { 675 });
    auto const result = formula::checked_evaluate(strength, specimen);
    if (!result)
    {
        std::println("cannot calculate the strength: {}", result.error());
        return 1;
    }
    std::println("{} = {}", formula::symbol_of<Strength>(), *result);
    // --8<-- [end:evaluate]
    return 0;
}
