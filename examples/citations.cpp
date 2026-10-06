// SPDX-License-Identifier: Apache-2.0
//
// Citations and generated documentation.
//
// Generic physics only, no real standard cited: a formula wrapped in
// formula::documented(), rendered in all three dialects, walked for its symbol
// table and its citation, and then evaluated -- showing that the citation
// travels with the formula through all of those, and that none of them
// changes the number the bare formula would have produced. Then a citation
// naming only the one field that applies, and a documented formula nested
// inside another, whose two citations come back outermost first.
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
using Rise = formula::Quantity<struct RiseTag, "h", "height gained", unit::Metre>;
using Run = formula::Quantity<struct RunTag, "L", "horizontal distance covered", unit::Metre>;
using Gradient = formula::Quantity<struct GradientTag, "s", "road gradient", unit::One>;
using GradientPerCent = formula::Quantity<struct GradientPerCentTag, "s_pc", "road gradient, per cent", unit::Percent>;

// The formula and its citation, declared together: documented() attaches the
// citation to the division, and forwards that division's dimension unchanged.
constexpr auto gradient = formula::documented(var<Rise> / var<Run>,
                                              { .title = "Road gradient",
                                                .reference = "Example Standard 1:2020",
                                                .section = "5.4.2",
                                                .equation = "(3)",
                                                .text = "Height a road gains over the horizontal distance it covers." });

// A citation naming only the field that applies is just as valid; every field
// left unnamed reads back empty.
constexpr auto sparse = formula::documented(var<Rise>, { .title = "A height" });

// A documented formula used inside another, documented in turn: the gradient
// stated per cent, carrying a citation of its own. It needs no factor of 100:
// evaluated into a quantity in per cent, the unit does the scaling.
constexpr auto perCent = formula::documented(
    gradient, { .title = "Road gradient, per cent", .reference = "Example Standard 1:2020", .section = "5.4.3" });

} // namespace

int main()
{
    using namespace formula::literals;

    // One declaration, five questions. render() answers what the formula is,
    // as plain text, Markdown or LaTeX, and the citation appears in none of
    // them; document() walks the same tree for what render() leaves out, the
    // symbol table and the citation; and checked_evaluate() gives the number,
    // exactly what the bare division would have produced, because wrapping is
    // invisible to arithmetic.
    std::string const plain = formula::render(gradient);
    std::string const markdown = formula::render<formula::Dialect::Markdown>(gradient);
    std::string const latex = formula::render<formula::Dialect::LaTeX>(gradient);
    formula::Documentation const documentation = formula::document(gradient);
    auto const inputs = formula::environment(formula::Measured<Rise> { 90 }, formula::Measured<Run> { 3000 });
    auto const result = formula::checked_evaluate<Gradient>(gradient, inputs);

    std::println("plain: {}", plain);
    std::println("markdown: {}", markdown);
    std::println("latex: {}", latex);
    for (formula::SymbolEntry const& entry: documentation.symbols)
        std::println("symbol: {} = {} [{}]", entry.symbol, entry.description, entry.unit);
    if (documentation.citations.empty())
    {
        std::println("the gradient's formula cites nothing");
        return 1;
    }
    formula::Citation const& citation = documentation.citations.front();
    std::println("citation: {}, {}, {}, {}", citation.title, citation.reference, citation.section, citation.equation);

    // Checked before dereferencing: result is a std::expected, and calling
    // operator-> on one that holds an error is undefined behaviour. Nothing
    // in this program can make checked_evaluate fail here, but an example is
    // teaching material, and the check costs nothing to show.
    if (!result.has_value())
    {
        std::println("{}: {}", formula::symbol_of<Gradient>(), result.error());
        return 1;
    }
    std::println("{} = {} ({})", formula::symbol_of<Gradient>(), *result, result->source());

    // The sparse citation: a title, and every other field empty, not absent.
    std::println("sparse citation: title \"{}\", reference empty: {}",
                 sparse.citation.title,
                 sparse.citation.reference.empty() ? "yes" : "no");

    // The nested formula: two citations, the outer one first.
    formula::Documentation const nested = formula::document(perCent);
    for (formula::Citation const& each: nested.citations)
        std::println("nested citation: {}, {}, {}", each.title, each.reference, each.section);
    auto const perCentResult = formula::checked_evaluate<GradientPerCent>(perCent, inputs);
    if (!perCentResult.has_value())
    {
        std::println("{}: {}", formula::symbol_of<GradientPerCent>(), perCentResult.error());
        return 1;
    }
    std::println("{} = {} ({})", formula::symbol_of<GradientPerCent>(), *perCentResult, perCentResult->source());

    bool const renderedCorrectly = plain == "h / L" && markdown == "`h` / `L`" && latex == "\\frac{h}{L}";
    bool const documentedCorrectly = documentation.symbols.size() == 2 && documentation.citations.size() == 1;
    bool const evaluatedCorrectly = formula::number_of(result) == 0.03_r;
    bool const sparseIsEmptyElsewhere = sparse.citation.title == "A height" && sparse.citation.reference.empty()
                                        && sparse.citation.section.empty() && sparse.citation.equation.empty()
                                        && sparse.citation.text.empty();
    bool const nestedComesBackOutermostFirst = nested.citations.size() == 2
                                               && nested.citations[0].title == "Road gradient, per cent"
                                               && nested.citations[1].title == "Road gradient";
    bool const perCentEvaluatedCorrectly = formula::number_of(perCentResult) == formula::Rational { 3 };

    bool const allChecksPassed = renderedCorrectly && documentedCorrectly && evaluatedCorrectly && sparseIsEmptyElsewhere
                                 && nestedComesBackOutermostFirst && perCentEvaluatedCorrectly;
    std::println("all checks passed: {}", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
