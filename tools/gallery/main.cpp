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

#include <cstdint>
#include <cstdio>
#include <expected>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

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

// ---- 2a: a constraint, alongside the circular area it validates ------------
//
// Not every relationship a standard states computes something -- some exist
// purely to validate a measured input before it ever reaches a formula like
// circularArea above. A specimen wider than the die diameter cannot be
// tested at all, so the method rejects it outright rather than reporting an
// area for it.
constexpr auto maximumDiameter =
    formula::constraint(var<Diameter> <= formula::constant<unit::Millimetre>(formula::Rational { 150 }),
                        formula::Verdict { "specimen exceeds diameter tolerance" },
                        { .title = "Maximum specimen diameter",
                          .reference = "Example Standard 6:2020",
                          .section = "4.1",
                          .text = "A specimen wider than the die diameter cannot be tested and is rejected "
                                  "outright." });

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

// ---- 6: three lookup tables, one per kind ----------------------------------
//
// A method's own algebra sometimes needs a number no formula computes -- the
// method's procedure publishes it as a table instead. Three kinds, and they
// are not interchangeable: a banded table buckets a measurement into an
// interval, an exact table is keyed on a category, and an interpolating table
// computes a value between two rows that appears in no row at all.
//
// **None of the three states a value in percent, and that is deliberate.**
// `unit::Percent`'s symbol is `%`, which is TeX's comment character, and this
// page publishes every formula as a `$$ ... $$` block. Measured with tectonic
// 0.15.0 over a three-way control: the same row with no percent sign typesets
// (exit 0), the exact bytes `render<Dialect::LaTeX>` emits for a percent-valued
// row fail with `!File ended while scanning use of \text@` (exit 1), and the
// row with the sign escaped as `\%` typesets again (exit 0). The library does
// not escape it today, so a percent-valued table renders to LaTeX that does not
// compile. That is a defect in `render.hpp` rather than in this page, and it is
// reported as one -- but a page whose whole claim is that its snippets came
// from a real run is the wrong place to publish a formula that is known not to
// typeset, so the tables here are stated in megapascals and in plain
// dimensionless factors.

struct CrushingStrength: formula::Quantity<CrushingStrength, "f", "measured crushing strength", unit::Megapascal>
{
};
struct CuringAge: formula::Quantity<CuringAge, "t", "curing age at test", unit::Hour>
{
};
struct SizeAllowance: formula::Quantity<SizeAllowance, "k_d", "size allowance", unit::Megapascal>
{
};
struct CorrectedStrength: formula::Quantity<CorrectedStrength, "f_c", "size- and age-corrected strength", unit::Megapascal>
{
};

// Three bands over the diameter `Diameter` already declares, each stated as a
// numerator/denominator pair for its low (inclusive) and high (EXCLUSIVE)
// bound. A gap or an overlap anywhere here is a compile error naming both
// offending bands, so this table cannot reach the page mis-bucketing anything.
inline constexpr formula::BandTable<3> GallerySizeBands {
    formula::band(0, 1, 100, 1),   // 0 to under 100 mm
    formula::band(100, 1, 150, 1), // 100 to under 150 mm
    formula::band(150, 1, 200, 1), // 150 to under 200 mm -- 200 mm itself is in NO band
};

// A key enumeration wants a name of its own per translation unit: two files
// declaring a same-named internal-linkage enumeration, used as a `KeyTable`
// non-type template parameter with equal values, silently mislink on clang
// (`lookup.hpp` measures the mechanism). Hence `GalleryMould` and not `Mould`.
//
// **Numbered explicitly, and not contiguously, on purpose.** An exact lookup
// renders its rows as `key <underlying value>` -- a C++ enumerator has no name
// at run time, so the value is the only thing that survives into the rendered
// formula. Leaving these to default would print 0, 1, 2, which a reader could
// just as easily take for row indices; 3, 7 and 11 can only be the
// enumerators' own values, so the page demonstrates the rule rather than
// leaving docs/lookup-tables.md to assert it in prose alone.
enum class GalleryMould : std::uint8_t
{
    Cube = 3,
    Cylinder = 7,
    Prism = 11,
};

inline constexpr formula::KeyTable<GalleryMould, 3> GalleryMouldKeys {
    GalleryMould::Cube,
    GalleryMould::Cylinder,
    GalleryMould::Prism,
};

// Breakpoints, not bands: each is one key the curve states a value AT, and the
// value between two of them is computed rather than stored. The domain is
// closed at both ends -- 24 h and 168 h are both hits -- because a breakpoint
// is a row and not a boundary between rows.
inline constexpr formula::BreakpointTable<3> GalleryAgeCurve {
    formula::breakpoint(24),
    formula::breakpoint(72),
    formula::breakpoint(168),
};

// The structure (the bands, the keys, the breakpoints, and the two units) is
// the method and lives in each node's type; the contents -- the number each row
// gives -- are a registered table's data and arrive at runtime.
constexpr auto sizeAllowanceTable =
    formula::banded_lookup<unit::Millimetre, GallerySizeBands, unit::Megapascal>(
        var<Diameter>, { formula::Rational { 2 }, formula::Rational { 1 }, formula::Rational { 0 } });

constexpr auto mouldFactorTable = formula::exact_lookup<GalleryMouldKeys, unit::One>(
    GalleryMould::Cylinder, { formula::Rational { 1 }, formula::Rational { 19, 20 }, formula::Rational { 9, 10 } });

constexpr auto maturityFactorTable =
    formula::interpolating_lookup<unit::Hour, GalleryAgeCurve, unit::One>(
        var<CuringAge>,
        { formula::Rational { 3, 5 }, formula::Rational { 17, 20 }, formula::Rational { 1 } });

constexpr auto sizeAllowance =
    formula::documented(sizeAllowanceTable,
                        { .title = "Size allowance by specimen diameter",
                          .reference = "Example Standard 7:2020",
                          .section = "8.2",
                          .text = "The allowance deducted from a measured crushing strength, selected by the band "
                                  "the specimen's diameter falls in. A diameter in no band is not a value: the "
                                  "method defined no allowance there and this library reports that rather than "
                                  "inventing one." });

constexpr auto mouldFactor =
    formula::documented(mouldFactorTable,
                        { .title = "Mould factor by specimen mould",
                          .reference = "Example Standard 7:2020",
                          .section = "8.3",
                          .text = "A category key names a row directly. The key renders as its underlying value, "
                                  "not the enumerator's name, because a C++ enumerator has no name at run time -- "
                                  "a reader reconciling this against a published table carries the author's own "
                                  "enum class across." });

constexpr auto maturityFactor =
    formula::documented(maturityFactorTable,
                        { .title = "Maturity factor by curing age",
                          .reference = "Example Standard 7:2020",
                          .section = "8.4",
                          .equation = "(7)",
                          .text = "A curve stated at three ages. A specimen tested between two of them gets the "
                                  "value those two rows imply at that age -- a number appearing in no row of the "
                                  "table. A specimen younger or older than the curve gets nothing at all: there "
                                  "is no extrapolation." });

// All three at once, wrapped exactly once so the section below has exactly one
// citation, the same way every other formula on this page does.
constexpr auto correctedStrength = formula::documented(
    (var<CrushingStrength> - sizeAllowanceTable) * mouldFactorTable * maturityFactorTable,
    { .title = "Size- and age-corrected crushing strength",
      .reference = "Example Standard 7:2020",
      .section = "8.5",
      .equation = "(8)",
      .text = "The measured strength less its size allowance, scaled by the mould factor and by the maturity "
              "factor -- one banded, one exact and one interpolating table inside a single expression." });

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

/// Writes the symbol table for a section, or nothing when there are no symbols
/// to put in it.
///
/// **The empty case is reachable, which is why it is handled rather than
/// asserted away.** An exact lookup has no operand at all -- its row is chosen
/// by a category key, which is data on the node and not a sub-expression
/// (`lookup.hpp`) -- so a formula that is nothing but an exact lookup reads no
/// quantity and contributes no `SymbolEntry`. Emitting the header and the
/// separator with no rows under them publishes an empty table, which tells a
/// reader the section has a symbol table and then shows them none.
///
/// One function called from both section writers rather than the same six
/// lines twice, for the reason this file's own `write_constraint` comment
/// gives: a constraint should document exactly the way a formula does, and two
/// copies of a rule are how two surfaces that must agree begin to disagree.
void write_symbol_table(std::ofstream& out, std::vector<formula::SymbolEntry> const& symbols)
{
    if (symbols.empty())
        return;

    out << "| Symbol | Description | Unit |\n";
    out << "| --- | --- | --- |\n";
    for (formula::SymbolEntry const& row: symbols)
        out << "| " << row.symbol << " | " << row.description << " | " << unit_cell(row.unit) << " |\n";
    out << "\n";
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

    write_symbol_table(out, markdown.symbols);

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

/// Writes one constraint's section: its citation's title as a heading, the
/// plain and LaTeX renderings of its rule -- never its verdict, which
/// belongs to the trace, not the rule (docs/constraints.md explains why) --
/// and its symbol table.
///
/// Not `write_formula` above only because a `Constraint` is not a `Node`
/// (`constraint.hpp`'s file comment) and so cannot be passed to the
/// `Node`-constrained `document<D>()` overload; it goes through
/// `document()`'s other overload, for `Constraint`, instead, added
/// alongside the `Node` one in `document.hpp` for exactly this reason.
/// Everything downstream of that call is identical to `write_formula`'s own
/// shape, including the symbol table -- a constraint documents exactly the
/// way a formula does, and this page should not read as though it doesn't.
///
/// Returns false, with a message on stderr, rather than assuming a citation
/// the way `write_formula` above assumes one: `write_formula`'s own
/// `citations.front()` leans on every `Node` here being wrapped by
/// `documented(inner, citation)`, whose `citation` parameter has no
/// default, so a wrapped node always contributes one citation regardless of
/// what it says. `constraint(predicate, verdict, citation = {})`
/// (`constraint.hpp`) has no such backstop -- its citation is optional by
/// design, used uncited in this project's own guide, example and tests --
/// so nothing here can lean on the type system the way `write_formula` can;
/// "every gallery constraint is cited" is a convention this file happens to
/// follow today, not a guarantee. `.front()` on an empty vector is
/// undefined behaviour, and on an MSVC debug build that means an assertion
/// dialog, not a clean crash -- exactly the modal-dialog failure mode
/// `examples/CMakeLists.txt`'s own comment warns about, and this tool is
/// run from `ctest` (`gallery.is-current`) just as an example is. Checking
/// first and failing loudly costs one `if`.
template <formula::Predicate P>
[[nodiscard]] bool write_constraint(std::ofstream& out, formula::Constraint<P> const& node)
{
    formula::Documentation const plain = formula::document(node);
    formula::Documentation const markdown = formula::document<formula::Dialect::Markdown>(node);
    formula::Documentation const latex = formula::document<formula::Dialect::LaTeX>(node);

    if (plain.citations.empty())
    {
        std::fprintf(stderr, "formula-cpp-gallery: a gallery constraint must be cited, and this one is not\n");
        return false;
    }
    formula::Citation const& citation = plain.citations.front();

    out << "## " << citation.title << "\n\n";

    out << "```\n" << plain.formula << "\n```\n\n";

    out << "$$\n" << latex.formula << "\n$$\n\n";

    write_symbol_table(out, markdown.symbols);

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

    return true;
}

} // namespace

/// A worked section shows the expression it is working, not only the trace
/// of having worked it. A reader who lands on one directly -- which is what
/// a deep link into this page does -- otherwise has to reconstruct the
/// formula from its own numbered steps.
///
/// Rendered, never hand-typed. A literal spelling here would be a second
/// source for the formula, free to drift from what `render()` actually
/// produces -- and a page that exists to prove its snippets came from a real
/// run is the worst place in the repository to keep one.
template <typename N>
void write_worked_formula(std::ofstream& out, N const& node)
{
    out << "```\n" << formula::render(node) << "\n```\n\n";
}

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
    if (!write_constraint(out, maximumDiameter))
        return 1;
    write_formula(out, flowRate);
    write_formula(out, waterCementRatio);
    write_formula(out, compactionAdjustedDensity);
    write_formula(out, sizeAllowance);
    write_formula(out, mouldFactor);
    write_formula(out, maturityFactor);
    write_formula(out, correctedStrength);

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

    write_worked_formula(out, waterCementRatio);

    out << "```\n";
    out << "with V_w = 180 l and V_c = 300 l: " << exact_text(result) << " = " << result.to_double() << "\n";
    out << "```\n\n";

    // ---- A worked derivation, so the page shows how a number was reached, not only what it is ----

    out << "## Worked derivation: bulk density\n\n";
    out << "`m` = 1200 kg, `V` = 0.5 m3, `formula::explain()` and `formula::render_trace()`:\n\n";

    write_worked_formula(out, density);

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

    write_worked_formula(out, compactionAdjustedDensity);

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

    // ---- A worked constraint, so the page shows a verdict as its own trace step ----

    out << "## Worked derivation: maximum specimen diameter, alongside the circular area it validates\n\n";
    out << "`d` = 200 mm -- above the 150 mm tolerance, so the constraint is violated and its verdict "
           "appears in the trace, `formula::check()` and `formula::render_trace()`:\n\n";

    // Both, because the heading promises both: the constraint that was
    // checked, and the formula it guards.
    write_worked_formula(out, maximumDiameter);
    write_worked_formula(out, circularArea);

    auto const oversizedSpecimen = formula::environment(formula::Measured<Diameter> { formula::Rational { 200 } });
    formula::Trace<> constraintTrace {};
    formula::RecordingSink<> constraintSink { constraintTrace };
    formula::ConstraintOutcome const diameterOutcome =
        formula::check(maximumDiameter, oversizedSpecimen, constraintSink);
    if (!diameterOutcome.is_violated())
    {
        std::fprintf(stderr, "formula-cpp-gallery: the worked constraint did not violate as expected\n");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(constraintTrace, { .maxSteps = 5 });
    out << "```\n\n";

    // ---- Three lookup tables in one derivation, so the page shows each kind naming the row it used ----

    out << "## Worked derivation: size- and age-corrected crushing strength\n\n";
    // The key is written out of the enumerator rather than typed, so this
    // sentence cannot drift from the row the lookup below actually selects --
    // which is the whole hazard of an enumerator whose value is not its index.
    out << "`f` = 32 MPa, `d` = 120 mm, `t` = 48 h, mould `key "
        << static_cast<int>(GalleryMould::Cylinder)
        << "`. Each table names the row it answered "
           "from: the banded one its interval, the interpolating one the two rows it drew on. The exact "
           "lookup adds nothing there -- its key is already the subject of its own line.\n\n";

    write_worked_formula(out, correctedStrength);

    auto const correctedInputs = formula::environment(formula::Measured<CrushingStrength> { formula::Rational { 32 } },
                                                      formula::Measured<Diameter> { formula::Rational { 120 } },
                                                      formula::Measured<CuringAge> { formula::Rational { 48 } });
    formula::Explained<CorrectedStrength> const explainedCorrected =
        formula::explain<CorrectedStrength>(correctedStrength, correctedInputs);
    if (!explainedCorrected.outcome.is_value())
    {
        std::fprintf(stderr, "formula-cpp-gallery: the worked lookup derivation did not produce a value\n");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(explainedCorrected.trace, { .maxSteps = 20 });
    out << "```\n\n";

    // ---- A lookup that found nothing, because a miss is not a number ----
    //
    // Traced through a RecordingSink rather than `explain()`: `explain()` goes
    // through the THROWING `evaluate()`, so a miss -- an ordinary outcome for a
    // lookup, not a defect -- would throw instead of handing back the
    // derivation that says why it missed.

    out << "## Worked derivation: a lookup that found nothing\n\n";
    out << "The same size-allowance table at `d` = 250 mm. The table's last band stops below 200 mm, so "
           "250 mm falls in no band -- and a miss is not a value: not zero, not the nearest band, not the "
           "last one. The bracketed clause is what keeps the line from being read as a failure relayed up "
           "from somewhere below it.\n\n";

    write_worked_formula(out, sizeAllowance);

    auto const uncoveredSpecimen = formula::environment(formula::Measured<Diameter> { formula::Rational { 250 } });
    formula::Trace<> missTrace {};
    formula::RecordingSink<> missSink { missTrace };
    std::expected<formula::Outcome<SizeAllowance>, formula::ArithmeticError> const missed =
        formula::checked_evaluate<SizeAllowance>(sizeAllowance, uncoveredSpecimen, missSink);
    if (missed.has_value() || missed.error() != formula::ArithmeticError::DomainError)
    {
        std::fprintf(stderr, "formula-cpp-gallery: the uncovered diameter did not report a domain error\n");
        return 1;
    }

    out << "```\n";
    out << formula::render_trace(missTrace, { .maxSteps = 5 });
    out << "```\n\n";

    out.flush();
    return out ? 0 : 1;
}
