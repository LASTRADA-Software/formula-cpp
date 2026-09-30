// SPDX-License-Identifier: Apache-2.0
//
// Lookup tables: a method publishes rows, and something about the specimen
// selects one of them. Three kinds, and they are not interchangeable:
//
//   - banded         -- a measured value falls in an interval that starts at
//                       its low bound and stops UNDER its high bound, and that
//                       interval selects a correction someone wrote down.
//   - exact          -- a category key (a shape, an apparatus, a regime)
//                       names a row directly. Not a quantity: no unit, no
//                       order.
//   - interpolating  -- a measured value sits between two rows, and the
//                       answer is the number those two rows imply at that
//                       point -- a number that appears in no row at all.
//
// The two things this example exists to make undeniable:
//
//   1. **A miss is not a value.** A value in no band, a key in no row, a
//      value off the ends of a curve: each produces
//      ArithmeticError::DomainError, never a zero, never the nearest row,
//      never an extrapolation.
//   2. **The banded and the interpolating domains deliberately disagree at
//      their top end.** A band's high bound is excluded; a curve's last
//      breakpoint is a row the table states a value at, so it is reached.
//      Step 4 below evaluates both kinds at the same 211 mm and shows them
//      answering differently on purpose.
//
// Every citation here is invented -- generic physics with fictional Example
// Standard references, exactly as every other example in this repository is.

#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <cstdint>
#include <print>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::var;
using namespace formula::literals;

using Diameter = formula::Quantity<struct DiameterTag, "d", "specimen diameter", unit::Millimetre>;
using MeasuredStrength =
    formula::Quantity<struct MeasuredStrengthTag, "f_m", "measured compressive strength", unit::Megapascal>;
using CorrectedStrength =
    formula::Quantity<struct CorrectedStrengthTag, "f_c", "corrected compressive strength", unit::Megapascal>;
using SizeCorrection = formula::Quantity<struct SizeCorrectionTag, "k", "size correction factor", unit::One>;

// ---- The banded table -------------------------------------------------------
//
// Three bands in millimetres, each declared as its low (inclusive) and high
// (EXCLUSIVE) bound. A gap or an overlap anywhere in here is a compile error
// naming the two offending bands -- see docs/lookup-tables.md, which shows
// the diagnostic.
inline constexpr formula::BandTable<3> SizeBands {
    formula::band(0, 127),   // 0 to under 127 mm
    formula::band(127, 173), // 127 to under 173 mm
    formula::band(173, 211), // 173 to under 211 mm -- 211 mm itself is NOT in it
};

// The same top row, written the way a published table meaning "173 mm to 211
// mm inclusive" must be written: the high bound at the next tick the domain
// can actually take on. `unit::Millimetre` declares one decimal, so that tick
// is 211.1 mm -- a real, exact number, not an approximation.
//
// This is the caller's reconciliation to do, and there is deliberately no
// closed-upper-bound flag on `Band` to do it with (band.hpp says why).
inline constexpr formula::BandTable<1> TopRowInclusive {
    formula::band(173, 211.1_r), // 173 to under 211.1 mm -- 211 mm IS in it
};

// ---- The exact table --------------------------------------------------------
//
// **A key enumeration needs a name of its own per translation unit.** This
// one is `LookupExampleShape`, not the plain `Shape` it would like to be: two
// translation units declaring a same-named internal-linkage enumeration, used
// as a `KeyTable` non-type template parameter with equal values, silently
// mislink on clang -- a dangling relocation, no diagnostic. lookup.hpp's file
// comment measures the mechanism. An enumeration declared in a *header* has
// external linkage instead and the question does not arise.
//
// **Numbered explicitly, and not contiguously, on purpose.** An exact lookup
// renders a key by its name -- `key Cylinder` -- and falls back to the
// underlying value only for a key that names no row, which `DrilledCore`
// below is. Enumerators left to default would print that fallback as 0, 1, 2
// -- which a reader could just as easily take for row indices. 13 can only be
// the enumerator's own value, so the miss below demonstrates the rule rather
// than merely being consistent with it.
enum class LookupExampleShape : std::uint8_t
{
    Cube = 3,
    Cylinder = 7,
    Prism = 11,
    DrilledCore = 13, // deliberately absent from ShapeKeys below -- the miss
};

// Three of the four shapes. `DrilledCore` is left out on purpose: it is the
// realistic absent key -- a method whose enumeration grew and a registered
// table that did not -- rather than a value nobody could have written.
inline constexpr formula::KeyTable<LookupExampleShape, 3> ShapeKeys {
    LookupExampleShape::Cube,
    LookupExampleShape::Cylinder,
    LookupExampleShape::Prism,
};

// ---- An exact table whose rows the published method words its own way -------
//
// A key is shown under its enumerator's name by default. Where the published
// table words a row differently, say so once, for the enumeration, by
// specializing `formula::EnumeratorName` -- every table keyed on it, and every
// trace of one, then follows. `of` is read at compile time, returns a string
// literal (a trace keeps the view), and returns an empty view to leave an
// enumerator under its own name: `Air` below.
enum class LookupExampleCuring : std::uint8_t
{
    Water = 1,
    Sealed = 2,
    Air = 3,
};
} // namespace

// A specialization is declared outside the anonymous namespace, where the
// primary template's namespace encloses it.
template <>
struct formula::EnumeratorName<LookupExampleCuring>
{
    static constexpr std::string_view of(LookupExampleCuring curing) noexcept
    {
        switch (curing)
        {
            case LookupExampleCuring::Water:
                return "water bath";
            case LookupExampleCuring::Sealed:
                return "sealed in foil";
            case LookupExampleCuring::Air:
                return {};
        }
        return {};
    }
};

// Constant expressions, so a spelling can be checked where it is declared.
static_assert(formula::enumerator_name<LookupExampleCuring::Sealed>() == "sealed in foil");
static_assert(formula::enumerator_name<LookupExampleCuring::Air>() == "Air");

namespace
{
inline constexpr formula::KeyTable<LookupExampleCuring, 3> CuringKeys {
    LookupExampleCuring::Water,
    LookupExampleCuring::Sealed,
    LookupExampleCuring::Air,
};

// ---- The interpolating table ------------------------------------------------
//
// The same domain as SizeBands, stated as a curve instead of as steps: three
// breakpoints, each a key the table states a value AT. The domain is closed at
// both ends -- 127 mm and 211 mm are both hits -- because a breakpoint is a
// row, not a boundary between rows, and excluding the last would make the
// table's own final row unreachable.
inline constexpr formula::BreakpointTable<3> SizeCurve {
    formula::breakpoint(127),
    formula::breakpoint(173),
    formula::breakpoint(211),
};

// ---- The bands a nested lookup buckets that curve into -----------------------
//
// A lookup whose operand is another lookup is a real published-method shape:
// a curve produces a continuous factor, and a second table buckets that factor
// into the published class the method actually applies. Both tables are keyed
// in percent here, which is what the curve produces.
inline constexpr formula::BandTable<3> ClassBands {
    formula::band(83.7_r, 97.3_r),   // 83.7 to under 97.3 %
    formula::band(97.3_r, 104.1_r),  // 97.3 to under 104.1 %
    formula::band(104.1_r, 117.9_r), // 104.1 to under 117.9 %
};

// The corrections are declared in PERCENT while the factor a formula consumes
// is dimensionless -- so the result side of every table below is converted,
// exactly as the key side is. A table's contents are runtime state (the
// numbers a customer's registered table supplies); only its structure -- the
// bands, the keys, the breakpoints, and both units -- lives in the type.
[[nodiscard]] constexpr auto sizeFactor()
{
    return formula::yields<SizeCorrection>(
        formula::banded_lookup<unit::Millimetre, SizeBands, unit::Percent>(var<Diameter>, { 91.3_r, 105.1_r, 112.7_r }));
}

[[nodiscard]] constexpr auto topRowInclusiveFactor()
{
    return formula::yields<SizeCorrection>(
        formula::banded_lookup<unit::Millimetre, TopRowInclusive, unit::Percent>(var<Diameter>, { 112.7_r }));
}

[[nodiscard]] constexpr auto shapeFactor(LookupExampleShape shape)
{
    return formula::yields<SizeCorrection>(
        formula::exact_lookup<ShapeKeys, unit::Percent>(shape, { 101.3_r, 86.3_r, 93.1_r }));
}

[[nodiscard]] constexpr auto curingFactor(LookupExampleCuring curing)
{
    return formula::yields<SizeCorrection>(
        formula::exact_lookup<CuringKeys, unit::Percent>(curing, { 104.3_r, 93.7_r, 88.1_r }));
}

[[nodiscard]] constexpr auto sizeCurveFactor()
{
    return formula::yields<SizeCorrection>(
        formula::interpolating_lookup<unit::Millimetre, SizeCurve, unit::Percent>(var<Diameter>, { 91.3_r, 105.1_r, 112.7_r }));
}

// The nested shape: a banded lookup whose operand is an interpolating lookup.
[[nodiscard]] constexpr auto classFactor()
{
    return formula::yields<SizeCorrection>(formula::banded_lookup<unit::Percent, ClassBands, unit::Percent>(
        sizeCurveFactor().expression, { 91.9_r, 101.3_r, 108.7_r }));
}

// The whole method: a measured strength corrected by two tables at once. The
// key is a function parameter rather than a field on the node, because a
// category is not a quantity and `Environment` carries only measurements --
// so a formula whose key varies per specimen is a function of the key, and a
// node is a cheap aggregate.
[[nodiscard]] constexpr auto correctedStrength(LookupExampleShape shape)
{
    return formula::yields<CorrectedStrength>(
        formula::documented(var<MeasuredStrength> * sizeFactor().expression * shapeFactor(shape).expression,
                            { .title = "Corrected compressive strength",
                              .reference = "Example Standard 8:2020",
                              .section = "7.3",
                              .equation = "(5)",
                              .text = "The measured strength is corrected for specimen size and for specimen "
                                      "shape, each factor taken from the table the method publishes for it." }));
}

constexpr auto diameter127 = formula::environment(formula::Measured<Diameter> { 127 });
constexpr auto diameter139 = formula::environment(formula::Measured<Diameter> { 139 });
constexpr auto diameter211 = formula::environment(formula::Measured<Diameter> { 211 });
constexpr auto diameter233 = formula::environment(formula::Measured<Diameter> { 233 });
constexpr auto specimen =
    formula::environment(formula::Measured<MeasuredStrength> { 40 }, formula::Measured<Diameter> { 139 });
constexpr auto noInputs = formula::environment();

} // namespace

int main()
{
    // ---- 1. A banded lookup selects a correction an author wrote down -------
    //
    // A band is written `<low> to under <high>`, and that is the ONE spelling
    // of a half-open interval anywhere in this library. That is
    // not a stylistic choice: `[0, 127)` opens Markdown link syntax, which
    // once silently dropped an operand from a published page of this
    // project's own documentation. The Markdown rendering below carries no
    // square bracket at all, and the assertion at the bottom of this file
    // checks that rather than trusting it.
    std::println("banded:        {}", formula::render(sizeFactor()));
    auto const bandedMarkdown = formula::render<formula::Dialect::Markdown>(sizeFactor());
    std::println("banded (md):   {}", bandedMarkdown);

    constexpr auto at139 = formula::checked_evaluate(sizeFactor(), diameter139);
    static_assert(at139.has_value());
    std::println("d = 139 mm:    {}", *at139);

    // ---- 2. Bands are half-open, and the boundary belongs to the band above --
    //
    // 127 mm is the boundary the first two bands share. It belongs to
    // the band whose LOW bound it is, never the band whose high bound it is.
    constexpr auto at127 = formula::checked_evaluate(sizeFactor(), diameter127);
    static_assert(at127.has_value());
    std::println("d = 127 mm:    {} (the band above the boundary, never the one below)", *at127);

    // ---- 3. The table's own top bound is excluded, and that is the caller's --
    //          reconciliation to do
    //
    // SizeBands' last row runs 173 to under 211 mm, so 211 mm falls in NO band. A
    // published row meaning "173 mm to 211 mm inclusive" is written with its
    // high bound at the next tick past 211 -- 211.1 mm, one decimal being what
    // unit::Millimetre declares.
    auto const at211Missed = formula::checked_explain(sizeFactor(), diameter211);
    if (at211Missed)
    {
        std::println("d = 211 mm: the band table gave a value, where it must miss");
        return 1;
    }
    constexpr auto at211Inclusive = formula::checked_evaluate(topRowInclusiveFactor(), diameter211);
    static_assert(at211Inclusive.has_value());
    std::println("d = 211 mm:    {}", at211Missed.error().error);
    std::println("inclusive top: {}", formula::render(topRowInclusiveFactor()));
    std::println("d = 211 mm:    {}", *at211Inclusive);

    // ---- 4. An interpolating table computes a number no row contains --------
    //
    // ... and its domain is CLOSED at both ends, deliberately unlike a band
    // table's. The same 211 mm that missed above is this table's last row, and
    // a row is a value the table states, not a boundary between two of them.
    std::println("interpolating: {}", formula::render(sizeCurveFactor()));

    auto const curveAt139 = formula::checked_explain(sizeCurveFactor(), diameter139);
    if (!curveAt139)
    {
        std::println("interpolating at 139 mm: {}", curveAt139.error().error);
        return 1;
    }
    auto const curveAt211 = formula::checked_explain(sizeCurveFactor(), diameter211);
    if (!curveAt211)
    {
        std::println("interpolating at 211 mm: {}", curveAt211.error().error);
        return 1;
    }
    constexpr auto curveAt233 = formula::checked_evaluate(sizeCurveFactor(), diameter233);
    static_assert(!curveAt233.has_value());
    std::println("d = 139 mm:    {} (between two rows -- in neither of them)", curveAt139->outcome);
    std::println("d = 211 mm:    {} (the last row, reached -- where the band table missed)", curveAt211->outcome);
    std::println("d = 233 mm:    {} (no extrapolation past the last row)", curveAt233.error());

    // ---- 5. An exact lookup: a category key names a row ---------------------
    //
    // A key renders as its enumerator's NAME (`key Cylinder`), recovered at
    // compile time. Only a key that names no row of the table -- the miss --
    // falls back to its underlying value (`key 13`), and a trace of the miss
    // says the same.
    std::println("exact:         {}", formula::render(shapeFactor(LookupExampleShape::Cylinder)));
    std::println("exact miss:    {}", formula::render(shapeFactor(LookupExampleShape::DrilledCore)));
    auto const coreMissed = formula::checked_explain(shapeFactor(LookupExampleShape::DrilledCore), noInputs);
    if (coreMissed)
    {
        std::println("DrilledCore: the exact table gave a value, where it must miss");
        return 1;
    }
    std::print("{}", formula::render_trace(coreMissed.error().trace, { .maxSteps = 10 }));

    constexpr auto cylinder = formula::checked_evaluate(shapeFactor(LookupExampleShape::Cylinder), noInputs);
    static_assert(cylinder.has_value());
    std::println("Cylinder:      {}", *cylinder);
    std::println("DrilledCore:   {} (a key no row of the table names)", coreMissed.error().error);

    // ---- 5a. An exact lookup whose keys the author spells ---------------------
    //
    // `EnumeratorName<LookupExampleCuring>` words two rows the way the
    // published table does and leaves `Air` under its own name. render() and
    // the trace both follow it.
    std::println("customized:    {}", formula::render(curingFactor(LookupExampleCuring::Sealed)));
    auto const sealed = formula::checked_explain(curingFactor(LookupExampleCuring::Sealed), noInputs);
    if (!sealed)
    {
        std::println("customized: {}", sealed.error().error);
        return 1;
    }
    std::print("{}", formula::render_trace(sealed->trace, { .maxSteps = 10 }));

    // ---- 6. A lookup nested inside another lookup's operand ------------------
    //
    // A curve produces a continuous factor; a band table buckets it into the
    // published class. All four surfaces at once: it renders, it documents,
    // it evaluates, and it traces.
    std::println("nested:        {}", formula::render(classFactor()));

    auto const nestedAt139 = formula::checked_explain(classFactor(), diameter139);
    if (!nestedAt139)
    {
        std::println("nested at 139 mm: {}", nestedAt139.error().error);
        return 1;
    }
    std::println("d = 139 mm:    {} (curve gives 94.9 %, which falls in the 83.7-to-under-97.3 % band)",
                 nestedAt139->outcome);

    formula::Documentation const nestedDocumentation = formula::document(classFactor());
    std::println("nested symbols: {}", nestedDocumentation.symbols.size());

    std::print("{}", formula::render_trace(nestedAt139->trace, { .maxSteps = 10 }));

    // ---- 6a. An interpolating lookup's own trace clause, in both its forms ---
    //
    // A lookup step ends in a bracketed clause naming where its answer came
    // from, and the interpolating kind has TWO of those rather than one: the
    // two rows it drew on, or -- at a value sitting exactly on a row -- the
    // single row it read. They say genuinely different things. Between two
    // rows the answer appears in neither and a reader has an interpolation to
    // check; on a row the table stated the number directly and there is
    // nothing to check.
    std::print("{}", formula::render_trace(curveAt139->trace, { .maxSteps = 10 }));
    std::print("{}", formula::render_trace(curveAt211->trace, { .maxSteps = 10 }));

    // ---- 7. The whole method, rendered and documented ------------------------
    auto const method = correctedStrength(LookupExampleShape::Cylinder);
    formula::Documentation const documentation = formula::document(method);
    formula::Citation const& citation = documentation.citations.front();

    std::println("method:        {}", documentation.formula);
    std::println("cited:         {}, {}, {} {}", citation.title, citation.reference, citation.section, citation.equation);
    for (formula::SymbolEntry const& entry: documentation.symbols)
        std::println("symbol:        {} = {} [{}]", entry.symbol, entry.description, entry.unit);

    // ---- 8. The method evaluated, and its derivation -------------------------
    auto const corrected = formula::checked_explain(method, specimen);
    if (!corrected)
    {
        std::println("f_c: {}", corrected.error().error);
        return 1;
    }
    std::println("f_c:           {}", corrected->outcome);
    std::print("{}", formula::render_trace(corrected->trace, { .maxSteps = 10 }));

    // ---- 9. A miss, traced ---------------------------------------------------
    //
    // The trace is where a miss stops being an opaque `DomainError`: the
    // bracketed clause says the value fell in no band AND what the table
    // actually covers, so a reader can tell a miss from a failure relayed
    // upward from the operand.
    std::print("{}", formula::render_trace(at211Missed.error().trace, { .maxSteps = 10 }));

    // ---- Every claim printed above, verified in code -------------------------
    bool const bandedSelects = formula::number_of(at139) == 1.051_r && formula::number_of(at127) == 1.051_r;
    bool const markdownCarriesNoBracket = !bandedMarkdown.contains('[');
    bool const topBoundExcluded = at211Missed.error().error == formula::ArithmeticError::DomainError;
    bool const nextTickReachesIt = formula::number_of(at211Inclusive) == 1.127_r;
    bool const curveComputes = formula::number_of(curveAt139->outcome) == 0.949_r;
    bool const curveTopIncluded = formula::number_of(curveAt211->outcome) == 1.127_r;
    bool const noExtrapolation = curveAt233.error() == formula::ArithmeticError::DomainError;
    bool const exactSelects = formula::number_of(cylinder) == 0.863_r;
    bool const absentKeyMisses = coreMissed.error().error == formula::ArithmeticError::DomainError;
    bool const customizedSelects = formula::number_of(sealed->outcome) == 0.937_r;
    bool const nestedComposes =
        formula::number_of(nestedAt139->outcome) == 0.919_r && nestedDocumentation.symbols.size() == 1;
    bool const methodEvaluates = formula::number_of(corrected->outcome) == 36.28052_r;

    // The two domains disagree at 211 mm, and that disagreement is the point:
    // a band's top is excluded, a curve's last row is a row.
    bool const domainsDisagreeOnPurpose = topBoundExcluded && curveTopIncluded;

    bool const allChecksPassed = bandedSelects && markdownCarriesNoBracket && topBoundExcluded && nextTickReachesIt
                                 && curveComputes && curveTopIncluded && noExtrapolation && exactSelects && absentKeyMisses
                                 && customizedSelects && nestedComposes && methodEvaluates && domainsDisagreeOnPurpose;
    std::println("all checks passed: {}", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
