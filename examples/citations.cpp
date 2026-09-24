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
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>

#include <cstdio>
#include <string>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", unit::Litre>
{
};
struct WaterCementRatio: formula::Quantity<WaterCementRatio, "w/c", "ratio of water to cement", unit::One>
{
};

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
    // Plain and LaTeX renderings of the same wrapped formula. The citation
    // does not appear in either: render() answers only what the formula is,
    // not where it comes from.
    std::string const plain = formula::render(ratio);
    std::string const latex = formula::render<formula::Dialect::LaTeX>(ratio);
    std::printf("plain: %s\n", plain.c_str());
    std::printf("latex: %s\n", latex.c_str());

    // document() walks the same tree for what render() leaves out: the
    // symbol table and the citation.
    formula::Documentation const documentation = formula::document(ratio);

    // entry.symbol and entry.description are std::string_view, not owning,
    // null-terminated strings -- citation.hpp permits a Citation (and, the
    // same way, a SymbolEntry) built from a runtime std::string, for which
    // .data() handed to printf's "%s" would read past the view looking for a
    // terminator that need not be there. "%.*s" with the view's own length
    // is correct regardless of what the view was built from.
    for (formula::SymbolEntry const& entry: documentation.symbols)
        std::printf("symbol: %.*s = %.*s [%s]\n",
                    static_cast<int>(entry.symbol.size()),
                    entry.symbol.data(),
                    static_cast<int>(entry.description.size()),
                    entry.description.data(),
                    std::string { formula::view(entry.unit.symbolText) }.c_str());

    formula::Citation const& citation = documentation.citations.at(0);
    std::printf("citation: %.*s, %.*s, %.*s, %.*s\n",
                static_cast<int>(citation.title.size()),
                citation.title.data(),
                static_cast<int>(citation.reference.size()),
                citation.reference.data(),
                static_cast<int>(citation.section.size()),
                citation.section.data(),
                static_cast<int>(citation.equation.size()),
                citation.equation.data());

    // Evaluating the wrapped formula: the number is exactly what the bare
    // formula would have produced, because wrapping is invisible to
    // arithmetic.
    auto const inputs = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                             formula::Measured<CementVolume> { formula::Rational { 300 } });
    auto const result = formula::checked_evaluate<WaterCementRatio>(ratio, inputs);
    // Checked before dereferencing: result is a std::expected, and calling
    // operator-> on one that holds an error is undefined behaviour. Nothing
    // in this program can make checked_evaluate fail here, but an example is
    // teaching material, and the check costs nothing to show.
    if (!result.has_value())
    {
        std::printf("evaluation failed\n");
        return 1;
    }
    std::printf("%.*s = %f (%s)\n",
                static_cast<int>(formula::Describe<WaterCementRatio>::symbol.size()),
                formula::Describe<WaterCementRatio>::symbol.data(),
                result->measurement().value().to_double(),
                result->is_value() ? "computed" : "no value");

    bool const renderedCorrectly = plain == "V_w / V_c" && latex == "\\frac{V_w}{V_c}";
    bool const documentedCorrectly = documentation.symbols.size() == 2 && documentation.citations.size() == 1;
    bool const evaluatedCorrectly =
        result.has_value() && result->is_value() && result->measurement().value() == formula::Rational { 3, 5 };

    bool const allChecksPassed = renderedCorrectly && documentedCorrectly && evaluatedCorrectly;
    std::printf("all checks passed: %s\n", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
