// SPDX-License-Identifier: Apache-2.0
//
// Composing a formula out of other formulas.
//
// Every other example in this directory builds each formula directly out of
// var<> and operators. This one declares a formula, gives it a name and a
// citation, and then uses that name as a sub-expression of a second formula --
// the way a published method reuses a quantity another clause already defines.
//
// Generic physics and an invented price, no real standard cited. It shows:
//
//   - a named, cited formula used as an operand of another formula;
//   - the composed formula evaluating exactly, with no separate "compile" or
//     "link" step -- the outer formula is just a bigger expression tree;
//   - document() on the OUTER formula returning BOTH citations, so provenance
//     travels upward through composition rather than being lost at the seam;
//   - the trace naming the inner formula as its own cited step, so an auditor
//     can see the sub-result the outer formula consumed;
//   - a unit this library does not ship (a currency), because Unit is an
//     ordinary aggregate a caller can declare;
//   - one asymmetry worth knowing before you rely on it: reusing the same
//     sub-formula twice lists its citation twice, while its symbols still
//     appear once.

#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <cstdio>
#include <string>

namespace
{
namespace unit = formula::unit;
using formula::var;

// A unit this library does not ship. `Unit` is a plain aggregate, so a caller
// declares one the same way the library declares Metre. Its dimension is
// Scalar: the seven base dimensions are physical, and money is not among them,
// so a currency is dimensionless here -- it carries its own symbol and its own
// display precision, but the dimension system will not stop you adding euros
// to a bare ratio. Adding euros to a length it does stop, because Length is a
// dimension it knows.
inline constexpr formula::Unit Euro { .dimension = formula::dim::Scalar,
                                      .symbolText = formula::symbol("EUR"),
                                      .decimals = 2 };

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", unit::Litre>
{
};
struct UnitPrice: formula::Quantity<UnitPrice, "c_u", "price at a water/cement ratio of one", Euro>
{
};
struct MixCost: formula::Quantity<MixCost, "C", "cost of the mix", Euro>
{
};

// ---- The inner formula, declared and cited on its own ----
//
// Nothing about this declaration anticipates being reused. It is the same
// formula examples/citations.cpp declares, with the same shape.
constexpr auto waterCementRatio = formula::documented(var<WaterVolume> / var<CementVolume>,
                                                      { .title = "Water/cement ratio",
                                                        .reference = "Example Standard 1:2020",
                                                        .section = "5.4.2",
                                                        .equation = "(3)",
                                                        .text = "Ratio of water content to cement content." });

// ---- The outer formula, which uses the inner one by name ----
//
// `waterCementRatio` stands exactly where a variable or a constant would. It
// is an ordinary value of an ordinary node type, so an operator accepts it,
// its dimension takes part in the dimension check, and its citation stays
// attached to the sub-tree it describes.
constexpr auto mixCost = formula::documented(var<UnitPrice> * waterCementRatio,
                                             { .title = "Cost of a mix at a given water/cement ratio",
                                               .reference = "Example Standard 9:2021",
                                               .section = "2.1",
                                               .text = "Cost scales linearly with the water/cement ratio." });

// The same sub-formula used twice in one tree, for the asymmetry noted at the
// top of this file.
constexpr auto quadraticSurcharge = var<UnitPrice> * waterCementRatio * waterCementRatio;

} // namespace

int main()
{
    bool ok = true;
    auto check = [&ok](char const* what, bool condition) {
        std::printf("%-46s %s\n", what, condition ? "yes" : "NO");
        ok = ok && condition;
    };

    // ---- 1. The composed formula renders as one expression ----
    std::string const inner = formula::render(waterCementRatio);
    std::string const outer = formula::render(mixCost);
    std::printf("inner formula : %s\n", inner.c_str());
    std::printf("outer formula : %s\n", outer.c_str());
    std::printf("outer in LaTeX: %s\n", formula::render<formula::Dialect::LaTeX>(mixCost).c_str());

    check("inner renders as its own expression", inner == "V_w / V_c");

    // Rendering flattens the composition: the parentheses a reader might
    // expect around the reused formula are absent, because * and / share a
    // precedence and associate left to right, so no parenthesis is needed to
    // preserve the meaning. The value is identical either way -- this
    // arithmetic is exact. What the rendering does not show, the trace does.
    check("outer renders the whole composed tree", outer == "c_u * V_w / V_c");

    // ---- 2. It evaluates, exactly ----
    auto const inputs = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                             formula::Measured<CementVolume> { formula::Rational { 300 } },
                                             formula::Measured<UnitPrice> { formula::Rational { 250 } });

    auto const outcome = formula::checked_evaluate<MixCost>(mixCost, inputs);
    check("the composed formula evaluates", outcome.has_value() && outcome->is_value());
    if (!outcome.has_value() || !outcome->is_value())
    {
        std::printf("all checks passed: no\n");
        return 1;
    }

    // 250 EUR * (180 l / 300 l) = 250 * 3/5 = 150, with no rounding anywhere:
    // 3/5 is held as 3/5, not as 0.59999999999999998.
    formula::Rational const cost = outcome->measurement().value();
    std::printf("cost          : %lld/%lld = %.2f EUR\n",
                static_cast<long long>(cost.numerator()),
                static_cast<long long>(cost.denominator()),
                cost.to_double());
    check("the result is exactly 150", cost == formula::Rational { 150 });

    // ---- 3. Provenance travels upward through the seam ----
    //
    // The outer formula was never told about the inner one's citation. It
    // comes back because document() walks the whole tree, and the wrapped
    // sub-tree is part of that tree.
    formula::Documentation const documentation = formula::document(mixCost);
    std::printf("citations on the outer formula: %zu\n", documentation.citations.size());
    for (formula::Citation const& citation: documentation.citations)
        std::printf("  - %.*s [%.*s]\n",
                    static_cast<int>(citation.title.size()),
                    citation.title.data(),
                    static_cast<int>(citation.reference.size()),
                    citation.reference.data());

    check("both citations reach the outer formula", documentation.citations.size() == 2);

    // Three symbols, each once, although V_w and V_c are reached through the
    // inner formula rather than written in the outer one.
    std::printf("symbols on the outer formula  : %zu\n", documentation.symbols.size());
    for (formula::SymbolEntry const& entry: documentation.symbols)
        std::printf("  - %.*s (%.*s)\n",
                    static_cast<int>(entry.symbol.size()),
                    entry.symbol.data(),
                    static_cast<int>(entry.description.size()),
                    entry.description.data());
    check("the symbol table merges both formulas", documentation.symbols.size() == 3);

    // ---- 4. The trace shows the inner formula as its own step ----
    //
    // This is what the flattened rendering leaves out: step 5 below is the
    // water/cement ratio, carrying its own citation, and step 6 consumes it.
    // An auditor reading the trace sees the sub-result the outer formula was
    // built on, not just the final number.
    formula::Explained<MixCost> const explained = formula::explain<MixCost>(mixCost, inputs);
    std::printf("trace:\n%s", formula::render_trace(explained.trace, { .maxSteps = 20 }).c_str());

    // ---- 5. The asymmetry, stated because it is easy to be surprised by ----
    //
    // Reusing one sub-formula twice reaches its citation twice, and the
    // citation list reports it twice; the symbol table still reports each
    // symbol once. If you are building a reference list from .citations,
    // collapse duplicates yourself.
    formula::Documentation const twice = formula::document(quadraticSurcharge);
    std::printf("citations when the same formula is used twice: %zu\n", twice.citations.size());
    std::printf("symbols   when the same formula is used twice: %zu\n", twice.symbols.size());
    check("a doubly used citation is listed twice", twice.citations.size() == 2);
    check("a doubly used symbol is still listed once", twice.symbols.size() == 3);

    std::printf("all checks passed: %s\n", ok ? "yes" : "no");
    return ok ? 0 : 1;
}
