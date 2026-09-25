// SPDX-License-Identifier: Apache-2.0
//
// Generates docs/gallery.md: a page produced by RUNNING this library over a
// handful of invented formulas, rather than written by hand, so the page
// cannot drift from the code that documents itself the moment a symbol or a
// unit changes.
//
// docs/gallery.md is generated and checked in -- a reader browsing GitHub
// sees the page without building anything. After changing a formula below,
// regenerate it with:
//
//     <build-dir>/tools/gallery/formula-cpp-gallery docs/gallery.md
//
// `gallery.is-current` (tools/gallery/CMakeLists.txt,
// cmake/CheckGalleryIsCurrent.cmake) runs that same comparison in CI and
// fails, naming this exact command, when the checked-in page has drifted from
// what this program produces.
//
// Every formula and citation below is INVENTED for this library's own
// documentation -- generic physics with fictional `Example Standard`
// citations, matching every test and example elsewhere in this repository.
// No real standard is named or transcribed anywhere in this file.

#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::var;

// Two composed units this gallery needs that unit.hpp does not name: a
// density (mass over volume) and a flow rate (volume over time). Declared
// locally, exactly as a downstream formula library would declare a unit of
// its own -- neither is common enough across the library's own tests to earn
// a place in unit.hpp.
inline constexpr formula::Unit KilogramPerCubicMetre { .dimension = formula::dim::Density,
                                                       .symbolText = formula::symbol("kg/m3"),
                                                       .decimals = 1 };
inline constexpr formula::Unit LitrePerSecond { .dimension = formula::dim::Volume / formula::dim::Time,
                                                .magnitudeNumerator = 1,
                                                .magnitudeDenominator = 1000,
                                                .symbolText = formula::symbol("l/s"),
                                                .decimals = 2 };

// ---- 1: a density ----------------------------------------------------------

struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "specimen mass", unit::Kilogram>
{
};
struct SpecimenVolume: formula::Quantity<SpecimenVolume, "V", "specimen volume", unit::CubicMetre>
{
};
struct BulkDensity: formula::Quantity<BulkDensity, "rho", "bulk density", KilogramPerCubicMetre>
{
};

constexpr auto density =
    formula::documented(var<SpecimenMass> / var<SpecimenVolume>,
                        { .title = "Bulk density of a compacted specimen",
                          .reference = "Example Standard 1:2020",
                          .section = "4.2",
                          .equation = "(3)",
                          .text = "Bulk density is the specimen's mass divided by its volume, both measured "
                                  "under standard conditions." });

// ---- 2: a circular area -----------------------------------------------------

struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", unit::Millimetre>
{
};
struct CircularArea: formula::Quantity<CircularArea, "A", "cross-sectional area", unit::SquareMetre>
{
};

// No equation number this time, deliberately: the symbol table and the
// citation block below both show only the fields a formula actually has.
constexpr auto circularArea =
    formula::documented(formula::pi * formula::pow<2>(var<Diameter>) / formula::Rational { 4 },
                        { .title = "Circular cross-sectional area",
                          .reference = "Example Standard 2:2020",
                          .section = "5.1",
                          .text = "The area of a circular cross-section computed from its diameter." });

// ---- 3: a flow rate ----------------------------------------------------------

struct DischargedVolume: formula::Quantity<DischargedVolume, "V", "volume discharged", unit::Litre>
{
};
struct ElapsedTime: formula::Quantity<ElapsedTime, "t", "elapsed time", unit::Second>
{
};
struct FlowRate: formula::Quantity<FlowRate, "Q", "volumetric flow rate", LitrePerSecond>
{
};

// Sparser still: no section, no equation, just a reference and the prose.
constexpr auto flowRate =
    formula::documented(var<DischargedVolume> / var<ElapsedTime>,
                        { .title = "Volumetric flow rate",
                          .reference = "Example Standard 3:2020",
                          .text = "Flow rate is the volume discharged divided by the time taken to discharge it." });

// ---- 4: a water/cement ratio, evaluated below too ----------------------------

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", unit::Litre>
{
};
struct WaterCementRatio: formula::Quantity<WaterCementRatio, "w/c", "ratio of water to cement", unit::One>
{
};

constexpr auto waterCementRatio =
    formula::documented(var<WaterVolume> / var<CementVolume>,
                        { .title = "Water/cement ratio",
                          .reference = "Example Standard 4:2020",
                          .section = "6.3",
                          .equation = "(2)",
                          .text = "Ratio of the effective water content to the cement content of a batch." });

// ---- 5: a conditional, evaluated below for its worked derivation ------------

// A density read directly off a specimen, distinct from `BulkDensity` above
// (which is a formula's *result*, not a variable a formula can read): this
// conditional's predicate and both its branches need a leaf they can read
// from the environment.
struct MeasuredDensity: formula::Quantity<MeasuredDensity, "rho_m", "measured bulk density", KilogramPerCubicMetre>
{
};
struct AdjustedBulkDensity
    : formula::Quantity<AdjustedBulkDensity, "rho_adj", "compaction-adjusted bulk density", KilogramPerCubicMetre>
{
};

// A numeric threshold selects between two formulas, not an if/else a caller
// has to remember to apply: a specimen compacted below the reference density
// is corrected upward by a fixed factor, and one at or above it is reported
// exactly as measured.
constexpr auto compactionAdjustedDensity = formula::documented(
    formula::when(var<MeasuredDensity> < formula::constant<KilogramPerCubicMetre>(formula::Rational { 1800 }),
                 var<MeasuredDensity> * formula::Rational { 11, 10 }, var<MeasuredDensity>),
    { .title = "Compaction-adjusted bulk density",
      .reference = "Example Standard 5:2020",
      .section = "4.5",
      .text = "A specimen compacted below the reference density is corrected upward by a fixed factor; "
              "one at or above it is reported as measured." });

/// An exact rational as text: `4`, or `3/5` when it is not whole.
///
/// Not reused from render.hpp's own `detail::number_text`, which does exactly
/// this: that name lives in an implementation-detail namespace, and this
/// program is a consumer of the library, not a part of it.
[[nodiscard]] std::string exact_text(formula::Rational value)
{
    if (value.denominator() == 1)
        return std::to_string(value.numerator());
    return std::to_string(value.numerator()) + "/" + std::to_string(value.denominator());
}

/// A unit's symbol for a table cell, or a word for the one unit that has none.
[[nodiscard]] std::string unit_cell(formula::Unit unitOfValue)
{
    std::string_view const symbolText = formula::view(unitOfValue.symbolText);
    return symbolText.empty() ? std::string { "dimensionless" } : std::string { symbolText };
}

/// Writes one formula's section: its citation's title as a heading, the plain
/// and LaTeX renderings, its symbol table, and whichever citation fields are
/// non-empty.
///
/// Three separate `document<D>()` calls, not one reused across dialects: the
/// symbol table asks for `Dialect::Markdown` and the display form asks for
/// `Dialect::LaTeX`, each the dialect it is actually for, rather than
/// assuming today's `collect()` ignores `D` for the symbol table -- an
/// assumption a later phase could quietly invalidate.
template <formula::Node N>
void write_formula(std::ofstream& out, N const& node)
{
    formula::Documentation const plain = formula::document(node);
    formula::Documentation const markdown = formula::document<formula::Dialect::Markdown>(node);
    formula::Documentation const latex = formula::document<formula::Dialect::LaTeX>(node);

    // Exactly one documented() wrap per formula in this file, so exactly one
    // citation comes back.
    formula::Citation const& citation = plain.citations.front();

    out << "## " << citation.title << "\n\n";

    out << "```\n" << plain.formula << "\n```\n\n";

    out << "$$\n" << latex.formula << "\n$$\n\n";

    out << "| Symbol | Description | Unit |\n";
    out << "| --- | --- | --- |\n";
    for (formula::SymbolEntry const& row: markdown.symbols)
        out << "| " << row.symbol << " | " << row.description << " | " << unit_cell(row.unit) << " |\n";
    out << "\n";

    bool const hasBibliographicFields =
        !citation.reference.empty() || !citation.section.empty() || !citation.equation.empty();
    if (!citation.reference.empty())
        out << "- Reference: " << citation.reference << "\n";
    if (!citation.section.empty())
        out << "- Section: " << citation.section << "\n";
    if (!citation.equation.empty())
        out << "- Equation: " << citation.equation << "\n";
    if (hasBibliographicFields)
        out << "\n";

    if (!citation.text.empty())
        out << citation.text << "\n\n";
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: formula-cpp-gallery <output-file>\n");
        return 1;
    }

    std::ofstream out { argv[1], std::ios::trunc };
    if (!out)
    {
        std::fprintf(stderr, "formula-cpp-gallery: could not open '%s' for writing\n", argv[1]);
        return 1;
    }

    out << "# Gallery\n\n";
    out << "This page is generated by running `tools/gallery/main.cpp`, which builds each "
           "formula below with the library's own operators and asks the library to render "
           "and evaluate it. **Do not edit it by hand** -- change the generator and "
           "regenerate instead; `gallery.is-current` fails CI when the two disagree.\n\n";
    out << "Every formula and citation on this page is invented -- generic physics with "
           "fictional `Example Standard` citations, exactly as every test and example "
           "elsewhere in this repository is. See [the home page](index.md).\n\n";

    write_formula(out, density);
    write_formula(out, circularArea);
    write_formula(out, flowRate);
    write_formula(out, waterCementRatio);
    write_formula(out, compactionAdjustedDensity);

    // ---- A worked evaluation, so the page proves the numbers as well as the text ----

    out << "## Worked evaluation: water/cement ratio\n\n";
    out << "`V_w` = 180 l, `V_c` = 300 l:\n\n";

    auto const inputs = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                             formula::Measured<CementVolume> { formula::Rational { 300 } });
    auto const outcome = formula::checked_evaluate<WaterCementRatio>(waterCementRatio, inputs);
    if (!outcome.has_value() || !outcome->is_value())
    {
        std::fprintf(stderr, "formula-cpp-gallery: the worked evaluation did not produce a value\n");
        return 1;
    }
    formula::Rational const result = outcome->measurement().value();

    out << "```\n";
    out << "w/c = V_w / V_c = 180 l / 300 l = " << exact_text(result) << " = " << result.to_double() << "\n";
    out << "```\n\n";

    // ---- A worked derivation, so the page shows how a number was reached, not only what it is ----

    out << "## Worked derivation: bulk density\n\n";
    out << "`m` = 1200 kg, `V` = 0.5 m3, `formula::explain()` and `formula::render_trace()`:\n\n";

    auto const densityInputs = formula::environment(formula::Measured<SpecimenMass> { formula::Rational { 1200 } },
                                                     formula::Measured<SpecimenVolume> { formula::Rational { 1, 2 } });
    formula::Explained<BulkDensity> const explained = formula::explain<BulkDensity>(density, densityInputs);
    if (!explained.outcome.is_value())
    {
        std::fprintf(stderr, "formula-cpp-gallery: the worked derivation did not produce a value\n");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(explained.trace, { .maxSteps = 20 });
    out << "```\n\n";

    // ---- A worked conditional, so the page shows a when() naming the branch it took ----

    out << "## Worked derivation: compaction-adjusted bulk density\n\n";
    out << "`rho_m` = 1500 kg/m3 -- below the 1800 kg/m3 reference density, so the predicate holds "
           "and the correction factor is applied:\n\n";

    auto const compactionInputs = formula::environment(formula::Measured<MeasuredDensity> { formula::Rational { 1500 } });
    formula::Explained<AdjustedBulkDensity> const explainedCompaction =
        formula::explain<AdjustedBulkDensity>(compactionAdjustedDensity, compactionInputs);
    if (!explainedCompaction.outcome.is_value())
    {
        std::fprintf(stderr, "formula-cpp-gallery: the worked conditional did not produce a value\n");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(explainedCompaction.trace, { .maxSteps = 20 });
    out << "```\n\n";

    out.flush();
    return out ? 0 : 1;
}
