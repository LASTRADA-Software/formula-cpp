// SPDX-License-Identifier: Apache-2.0
//
// Citations and generated documentation.
//
// Generic physics only, no real standard cited: a formula wrapped in
// formula::documented(), rendered in two dialects, walked for its symbol
// table and its citation, and then evaluated -- showing that the citation
// travels with the formula through all four of those, and that none of them
// changes the number the bare formula would have produced.
//
// Every citation in this file is invented: naming a real standard would put
// copyrighted material in a public repository. See docs/citations.md.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>

#include <print>
#include <string>

namespace
{
namespace unit = formula::unit;
using formula::var;

// A quantity carries its own symbol, description and unit, and its tag makes it a type of its own.
using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", unit::Litre>;
using CementVolume = formula::Quantity<struct CementVolumeTag, "V_c", "cement content", unit::Litre>;
using WaterCementRatio = formula::Quantity<struct WaterCementRatioTag, "w/c", "ratio of water to cement", unit::One>;

// The formula and its citation, declared together: documented() attaches the
// citation to the division, and forwards that division's dimension unchanged.
constexpr auto ratio = formula::documented(var<WaterVolume> / var<CementVolume>,
                                           { .title = "Water/cement ratio",
                                             .reference = "Example Standard 1:2020",
                                             .section = "5.4.2",
                                             .equation = "(3)",
                                             .text = "Ratio of water content to cement content." });

} // namespace

int main()
{
    using namespace formula::literals;

    // One declaration, four questions. render() answers what the formula is,
    // as plain text or LaTeX, and the citation appears in neither; document()
    // walks the same tree for what render() leaves out, the symbol table and
    // the citation; and checked_evaluate() gives the number, exactly what the
    // bare division would have produced, because wrapping is invisible to
    // arithmetic.
    std::string const plain = formula::render(ratio);
    std::string const latex = formula::render<formula::Dialect::LaTeX>(ratio);
    formula::Documentation const documentation = formula::document(ratio);
    auto const inputs = formula::environment(formula::Measured<WaterVolume> { 180 },
                                             formula::Measured<CementVolume> { 300 });
    auto const result = formula::checked_evaluate<WaterCementRatio>(ratio, inputs);

    std::println("plain: {}", plain);
    std::println("latex: {}", latex);
    for (formula::SymbolEntry const& entry: documentation.symbols)
        std::println("symbol: {} = {} [{}]", entry.symbol, entry.description, entry.unit);
    formula::Citation const& citation = documentation.citations.at(0);
    std::println("citation: {}, {}, {}, {}", citation.title, citation.reference, citation.section, citation.equation);

    // Checked before dereferencing: result is a std::expected, and calling
    // operator-> on one that holds an error is undefined behaviour. Nothing
    // in this program can make checked_evaluate fail here, but an example is
    // teaching material, and the check costs nothing to show.
    if (!result.has_value())
    {
        std::println("{}: {}", formula::symbol_of<WaterCementRatio>(), result.error());
        return 1;
    }
    std::println("{} = {} ({})", formula::symbol_of<WaterCementRatio>(), *result, result->source());

    bool const renderedCorrectly = plain == "V_w / V_c" && latex == "\\frac{V_w}{V_c}";
    bool const documentedCorrectly = documentation.symbols.size() == 2 && documentation.citations.size() == 1;
    bool const evaluatedCorrectly = formula::number_of(result) == 0.6_r;

    bool const allChecksPassed = renderedCorrectly && documentedCorrectly && evaluatedCorrectly;
    std::println("all checks passed: {}", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
