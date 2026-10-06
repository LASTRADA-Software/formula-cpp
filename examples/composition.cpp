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
//   - a unit this library does not ship (a currency) in a base dimension of
//     its own, because Unit is an ordinary aggregate a caller can declare and
//     base_dimension() makes the dimension;
//   - one asymmetry worth knowing before you rely on it: reusing the same
//     sub-formula twice lists its citation twice, while its symbols still
//     appear once.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <print>
#include <string>

namespace
{
namespace unit = formula::unit;
using formula::var;

// A unit this library does not ship. `Unit` is a plain aggregate, so a caller
// declares one the same way the library declares Metre. Money is not among the
// seven SI base dimensions, so the euro gets a base dimension of its own:
// base_dimension("EUR") names one, and the unit named after it is one euro.
// The dimension system then refuses euros plus a bare ratio, or euros plus
// yen, as it refuses euros plus a length.
inline constexpr formula::Unit Euro { .dimension = formula::base_dimension("EUR"),
                                      .symbolText = formula::symbol("EUR"),
                                      .decimals = 2 };

// Money is not a bare number. `var<UnitPrice> + 0.5_r` does not compile --
// test/negative/money_plus_number.cpp pins the library's message for it.
static_assert(!formula::SameDimension<Euro.dimension, formula::dim::Scalar>);

using Rise = formula::Quantity<struct RiseTag, "h", "height gained", unit::Metre>;
using Run = formula::Quantity<struct RunTag, "L", "horizontal distance covered", unit::Metre>;
using UnitPrice = formula::Quantity<struct UnitPriceTag, "c_u", "earthworks price at a gradient of one", Euro>;
using EarthworksCost = formula::Quantity<struct EarthworksCostTag, "C", "earthworks cost of the climb", Euro>;

// ---- The inner formula, declared and cited on its own ----
//
// Nothing about this declaration anticipates being reused. It is the same
// formula examples/citations.cpp declares, with the same shape.
constexpr auto gradient = formula::documented(var<Rise> / var<Run>,
                                              { .title = "Road gradient",
                                                .reference = "Example Standard 1:2020",
                                                .section = "5.4.2",
                                                .equation = "(3)",
                                                .text = "Height a road gains over the horizontal distance it covers." });

// ---- The outer formula, which uses the inner one by name ----
//
// `gradient` stands exactly where a variable or a constant would. It
// is an ordinary value of an ordinary node type, so an operator accepts it,
// its dimension takes part in the dimension check, and its citation stays
// attached to the sub-tree it describes.
//
// This one is evaluated and explained below, so `yields<EarthworksCost>` names
// what it computes once, here. The citation goes inside, on the formula it cites.
constexpr auto earthworksCost = formula::yields<EarthworksCost>(
    formula::documented(var<UnitPrice> * gradient,
                        { .title = "Earthworks cost of a road at a given gradient",
                          .reference = "Example Standard 9:2021",
                          .section = "2.1",
                          .text = "Earthworks cost scales linearly with the road gradient." }));

// The same sub-formula used twice in one tree, for the asymmetry noted at the
// top of this file.
constexpr auto quadraticSurcharge = var<UnitPrice> * gradient * gradient;

} // namespace

int main()
{
    bool ok = true;
    auto check = [&ok](char const* what, bool condition) {
        std::println("{:<46} {}", what, condition ? "yes" : "NO");
        ok = ok && condition;
    };

    // ---- 1. The composed formula renders as one expression ----
    std::string const inner = formula::render(gradient);
    std::string const outer = formula::render(earthworksCost);
    std::println("inner formula : {}", inner);
    std::println("outer formula : {}", outer);
    std::println("outer in LaTeX: {}", formula::render<formula::Dialect::LaTeX>(earthworksCost));

    check("inner renders as its own expression", inner == "h / L");

    // Rendering flattens the composition: the parentheses a reader might
    // expect around the reused formula are absent, because * and / share a
    // precedence and associate left to right, so no parenthesis is needed to
    // preserve the meaning. The value is identical either way -- this
    // arithmetic is exact. What the rendering does not show, the trace does.
    check("outer renders the whole composed tree", outer == "c_u * h / L");

    // ---- 2. It evaluates, exactly ----
    auto const inputs = formula::environment(
        formula::Measured<Rise> { 90 }, formula::Measured<Run> { 3000 }, formula::Measured<UnitPrice> { 250'000 });

    auto const outcome = formula::checked_evaluate(earthworksCost, inputs);
    if (!outcome)
    {
        std::println("the composed formula failed: {}", outcome.error());
        return 1;
    }
    auto const cost = formula::number_of(outcome);
    check("the composed formula evaluates", cost.has_value());

    // 250000 EUR * (90 m / 3000 m) = 250000 * 3/100 = 7500, with no rounding
    // anywhere: 3/100 is held as 3/100, not as 0.029999999999999999.
    std::println("cost          : {}", *outcome);
    check("the result is exactly 7500", cost == 7500);

    // ---- 3. Provenance travels upward through the seam ----
    //
    // The outer formula was never told about the inner one's citation. It
    // comes back because document() walks the whole tree, and the wrapped
    // sub-tree is part of that tree.
    formula::Documentation const documentation = formula::document(earthworksCost);
    std::println("citations on the outer formula: {}", documentation.citations.size());
    for (formula::Citation const& citation: documentation.citations)
        std::println("  - {} [{}]", citation.title, citation.reference);

    check("both citations reach the outer formula", documentation.citations.size() == 2);

    // Three symbols, each once, although h and L are reached through the
    // inner formula rather than written in the outer one.
    std::println("symbols on the outer formula  : {}", documentation.symbols.size());
    for (formula::SymbolEntry const& entry: documentation.symbols)
        std::println("  - {} ({})", entry.symbol, entry.description);
    check("the symbol table merges both formulas", documentation.symbols.size() == 3);

    // ---- 4. The trace shows the inner formula as its own step ----
    //
    // This is what the flattened rendering leaves out: step 5 below is the
    // road gradient, carrying its own citation, and step 6 consumes it.
    // An auditor reading the trace sees the sub-result the outer formula was
    // built on, not just the final number.
    std::print("trace:\n{}", formula::render_trace(formula::trace_of(earthworksCost, inputs), { .maxSteps = 20 }));

    // ---- 5. The asymmetry, stated because it is easy to be surprised by ----
    //
    // Reusing one sub-formula twice reaches its citation twice, and the
    // citation list reports it twice; the symbol table still reports each
    // symbol once. If you are building a reference list from .citations,
    // collapse duplicates yourself.
    formula::Documentation const twice = formula::document(quadraticSurcharge);
    std::println("citations when the same formula is used twice: {}", twice.citations.size());
    std::println("symbols   when the same formula is used twice: {}", twice.symbols.size());
    check("a doubly used citation is listed twice", twice.citations.size() == 2);
    check("a doubly used symbol is still listed once", twice.symbols.size() == 3);

    std::println("all checks passed: {}", ok ? "yes" : "no");
    return ok ? 0 : 1;
}
