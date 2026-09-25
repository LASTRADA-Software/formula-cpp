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
//      Step 4 below evaluates both kinds at the same 200 mm and shows them
//      answering differently on purpose.
//
// Every citation here is invented -- generic physics with fictional Example
// Standard references, exactly as every other example in this repository is.

#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", unit::Millimetre>
{
};
struct MeasuredStrength: formula::Quantity<MeasuredStrength, "f_m", "measured compressive strength", unit::Megapascal>
{
};
struct CorrectedStrength: formula::Quantity<CorrectedStrength, "f_c", "corrected compressive strength", unit::Megapascal>
{
};
struct SizeCorrection: formula::Quantity<SizeCorrection, "k", "size correction factor", unit::One>
{
};

[[nodiscard]] constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// ---- The banded table -------------------------------------------------------
//
// Three bands in millimetres, each declared as a numerator/denominator pair
// for its low (inclusive) and high (EXCLUSIVE) bound. A gap or an overlap
// anywhere in here is a compile error naming the two offending bands -- see
// docs/lookup-tables.md, which shows the diagnostic.
inline constexpr formula::BandTable<3> SizeBands {
    formula::band(0, 1, 100, 1),   // 0 to under 100 mm
    formula::band(100, 1, 150, 1), // 100 to under 150 mm
    formula::band(150, 1, 200, 1), // 150 to under 200 mm -- 200 mm itself is NOT in it
};

// The same top row, written the way a published table meaning "150 mm to 200
// mm inclusive" must be written: the high bound at the next tick the domain
// can actually take on. `unit::Millimetre` declares one decimal, so that tick
// is 200.1 mm = 2001/10 -- a real, exact number, not an approximation.
//
// This is the caller's reconciliation to do, and there is deliberately no
// closed-upper-bound flag on `Band` to do it with (band.hpp says why).
inline constexpr formula::BandTable<1> TopRowInclusive {
    formula::band(150, 1, 2001, 10), // 150 to under 200.1 mm -- 200 mm IS in it
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
enum class LookupExampleShape : std::uint8_t
{
    Cube,
    Cylinder,
    Prism,
    DrilledCore, // deliberately absent from ShapeKeys below -- the miss
};

// Three of the four shapes. `DrilledCore` is left out on purpose: it is the
// realistic absent key -- a method whose enumeration grew and a registered
// table that did not -- rather than a value nobody could have written.
inline constexpr formula::KeyTable<LookupExampleShape, 3> ShapeKeys {
    LookupExampleShape::Cube,
    LookupExampleShape::Cylinder,
    LookupExampleShape::Prism,
};

// ---- The interpolating table ------------------------------------------------
//
// The same domain as SizeBands, stated as a curve instead of as steps: three
// breakpoints, each a key the table states a value AT. The domain is closed at
// both ends -- 100 mm and 200 mm are both hits -- because a breakpoint is a
// row, not a boundary between rows, and excluding the last would make the
// table's own final row unreachable.
inline constexpr formula::BreakpointTable<3> SizeCurve {
    formula::breakpoint(100),
    formula::breakpoint(150),
    formula::breakpoint(200),
};

// ---- The bands a nested lookup buckets that curve into -----------------------
//
// A lookup whose operand is another lookup is a real published-method shape:
// a curve produces a continuous factor, and a second table buckets that factor
// into the published class the method actually applies. Both tables are keyed
// in percent here, which is what the curve produces.
inline constexpr formula::BandTable<3> ClassBands {
    formula::band(90, 1, 100, 1),  // 90 to under 100 %
    formula::band(100, 1, 110, 1), // 100 to under 110 %
    formula::band(110, 1, 120, 1), // 110 to under 120 %
};

// The corrections are declared in PERCENT while the factor a formula consumes
// is dimensionless -- so the result side of every table below is converted,
// exactly as the key side is. A table's contents are runtime state (the
// numbers a customer's registered table supplies); only its structure -- the
// bands, the keys, the breakpoints, and both units -- lives in the type.
[[nodiscard]] constexpr auto sizeFactor()
{
    return formula::banded_lookup<unit::Millimetre, SizeBands, unit::Percent>(var<Diameter>,
                                                                              { rat(95), rat(100), rat(105) });
}

[[nodiscard]] constexpr auto topRowInclusiveFactor()
{
    return formula::banded_lookup<unit::Millimetre, TopRowInclusive, unit::Percent>(var<Diameter>, { rat(105) });
}

[[nodiscard]] constexpr auto shapeFactor(LookupExampleShape shape)
{
    return formula::exact_lookup<ShapeKeys, unit::Percent>(shape, { rat(100), rat(97), rat(92) });
}

[[nodiscard]] constexpr auto sizeCurveFactor()
{
    return formula::interpolating_lookup<unit::Millimetre, SizeCurve, unit::Percent>(var<Diameter>,
                                                                                     { rat(95), rat(100), rat(105) });
}

// The nested shape: a banded lookup whose operand is an interpolating lookup.
[[nodiscard]] constexpr auto classFactor()
{
    return formula::banded_lookup<unit::Percent, ClassBands, unit::Percent>(sizeCurveFactor(),
                                                                            { rat(95), rat(100), rat(105) });
}

// The whole method: a measured strength corrected by two tables at once. The
// key is a function parameter rather than a field on the node, because a
// category is not a quantity and `Environment` carries only measurements --
// so a formula whose key varies per specimen is a function of the key, and a
// node is a cheap aggregate.
[[nodiscard]] constexpr auto correctedStrength(LookupExampleShape shape)
{
    return formula::documented(var<MeasuredStrength> * sizeFactor() * shapeFactor(shape),
                               { .title = "Corrected compressive strength",
                                 .reference = "Example Standard 8:2020",
                                 .section = "7.3",
                                 .equation = "(5)",
                                 .text = "The measured strength is corrected for specimen size and for specimen "
                                         "shape, each factor taken from the table the method publishes for it." });
}

[[nodiscard]] constexpr auto diameterOf(std::int64_t millimetres)
{
    return formula::environment(formula::Measured<Diameter> { rat(millimetres) });
}

/// Evaluates @p node for @p Result and returns the exact rational it produced,
/// or nothing when it did not produce one.
template <typename Result, typename N, typename Env>
[[nodiscard]] constexpr std::optional<formula::Rational> valueOf(N const& node, Env const& environment)
{
    auto const outcome = formula::checked_evaluate<Result>(node, environment);
    if (!outcome.has_value() || !outcome->is_value())
        return std::nullopt;
    return outcome->measurement().value();
}

/// The `ArithmeticError` @p node produced, or nothing when it produced a value.
template <typename Result, typename N, typename Env>
[[nodiscard]] constexpr std::optional<formula::ArithmeticError> errorOf(N const& node, Env const& environment)
{
    auto const outcome = formula::checked_evaluate<Result>(node, environment);
    if (outcome.has_value())
        return std::nullopt;
    return outcome.error();
}

/// Evaluates @p node through a fresh `RecordingSink` and renders the trace.
///
/// Built by hand rather than through `formula::explain()`, and the reason is
/// worth knowing: `explain()` goes through the **throwing** `evaluate()`, so a
/// miss -- which is an ordinary outcome for a lookup, not a defect -- would
/// throw instead of handing back the derivation that says why it missed.
/// `checked_evaluate` with your own sink reports the miss and keeps the trace.
template <typename Result, typename N, typename Env>
[[nodiscard]] std::string tracedEvaluation(N const& node, Env const& environment)
{
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    [[maybe_unused]] auto const outcome = formula::checked_evaluate<Result>(node, environment, sink);
    return formula::render_trace(trace, { .maxSteps = 10 });
}

/// An exact rational as text: `4`, or `41/40` when it is not whole.
[[nodiscard]] std::string exact_text(formula::Rational value)
{
    if (value.denominator() == 1)
        return std::to_string(value.numerator());
    return std::to_string(value.numerator()) + "/" + std::to_string(value.denominator());
}

} // namespace

int main()
{
    // ---- 1. A banded lookup selects a correction an author wrote down -------
    //
    // A band is written `<low> to under <high>`, and that is the ONE spelling
    // of a half-open interval anywhere in this library. That is
    // not a stylistic choice: `[10, 20)` opens Markdown link syntax, which
    // once silently dropped an operand from a published page of this
    // project's own documentation. The Markdown rendering below carries no
    // square bracket at all, and the assertion at the bottom of this file
    // checks that rather than trusting it.
    std::printf("banded:        %s\n", formula::render(sizeFactor()).c_str());
    std::string const bandedMarkdown = formula::render<formula::Dialect::Markdown>(sizeFactor());
    std::printf("banded (md):   %s\n", bandedMarkdown.c_str());

    std::optional<formula::Rational> const at120 = valueOf<SizeCorrection>(sizeFactor(), diameterOf(120));
    std::printf("d = 120 mm:    %s\n", exact_text(*at120).c_str());

    // ---- 2. Bands are half-open, and the boundary belongs to the band above --
    //
    // 100 mm is the boundary the first two bands share. It belongs to
    // the band whose LOW bound it is, never the band whose high bound it is.
    std::optional<formula::Rational> const at100 = valueOf<SizeCorrection>(sizeFactor(), diameterOf(100));
    std::printf("d = 100 mm:    %s (the band above the boundary, never the one below)\n",
                exact_text(*at100).c_str());

    // ---- 3. The table's own top bound is excluded, and that is the caller's --
    //          reconciliation to do
    //
    // SizeBands' last row runs 150 to under 200 mm, so 200 mm falls in NO band. A
    // published row meaning "150 mm to 200 mm inclusive" is written with its
    // high bound at the next tick past 200 -- 200.1 mm, one decimal being what
    // unit::Millimetre declares.
    std::optional<formula::ArithmeticError> const at200Missed =
        errorOf<SizeCorrection>(sizeFactor(), diameterOf(200));
    std::optional<formula::Rational> const at200Inclusive =
        valueOf<SizeCorrection>(topRowInclusiveFactor(), diameterOf(200));
    std::string_view const missText = formula::describe(*at200Missed);
    std::printf("d = 200 mm:    %.*s\n", static_cast<int>(missText.size()), missText.data());
    std::printf("inclusive top: %s\n", formula::render(topRowInclusiveFactor()).c_str());
    std::printf("d = 200 mm:    %s\n", exact_text(*at200Inclusive).c_str());

    // ---- 4. An interpolating table computes a number no row contains --------
    //
    // ... and its domain is CLOSED at both ends, deliberately unlike a band
    // table's. The same 200 mm that missed above is this table's last row, and
    // a row is a value the table states, not a boundary between two of them.
    std::printf("interpolating: %s\n", formula::render(sizeCurveFactor()).c_str());

    std::optional<formula::Rational> const curveAt120 = valueOf<SizeCorrection>(sizeCurveFactor(), diameterOf(120));
    std::optional<formula::Rational> const curveAt200 = valueOf<SizeCorrection>(sizeCurveFactor(), diameterOf(200));
    std::optional<formula::ArithmeticError> const curveAt220 =
        errorOf<SizeCorrection>(sizeCurveFactor(), diameterOf(220));
    std::string_view const curveMissText = formula::describe(*curveAt220);
    std::printf("d = 120 mm:    %s (between two rows -- in neither of them)\n", exact_text(*curveAt120).c_str());
    std::printf("d = 200 mm:    %s (the last row, reached -- where the band table missed)\n",
                exact_text(*curveAt200).c_str());
    std::printf("d = 220 mm:    %.*s (no extrapolation past the last row)\n",
                static_cast<int>(curveMissText.size()),
                curveMissText.data());

    // ---- 5. An exact lookup: a category key names a row ---------------------
    //
    // A key renders as its UNDERLYING VALUE (`key 1`), not the enumerator's
    // name: a C++ enumerator has no name at run time. A reader reconciling
    // this against a published table carries the author's own `enum class`
    // across.
    std::printf("exact:         %s\n", formula::render(shapeFactor(LookupExampleShape::Cylinder)).c_str());

    std::optional<formula::Rational> const cylinder =
        valueOf<SizeCorrection>(shapeFactor(LookupExampleShape::Cylinder), formula::environment());
    std::optional<formula::ArithmeticError> const core =
        errorOf<SizeCorrection>(shapeFactor(LookupExampleShape::DrilledCore), formula::environment());
    std::string_view const coreMissText = formula::describe(*core);
    std::printf("Cylinder:      %s\n", exact_text(*cylinder).c_str());
    std::printf("DrilledCore:   %.*s (a key no row of the table names)\n",
                static_cast<int>(coreMissText.size()),
                coreMissText.data());

    // ---- 6. A lookup nested inside another lookup's operand ------------------
    //
    // A curve produces a continuous factor; a band table buckets it into the
    // published class. All four surfaces at once: it renders, it documents, it
    // evaluates, and it traces.
    std::printf("nested:        %s\n", formula::render(classFactor()).c_str());

    std::optional<formula::Rational> const nestedAt120 = valueOf<SizeCorrection>(classFactor(), diameterOf(120));
    std::printf("d = 120 mm:    %s (curve gives 97 %%, which falls in the 90-to-under-100 %% band)\n",
                exact_text(*nestedAt120).c_str());

    formula::Documentation const nestedDocumentation = formula::document(classFactor());
    std::printf("nested symbols: %zu\n", nestedDocumentation.symbols.size());

    std::printf("%s", tracedEvaluation<SizeCorrection>(classFactor(), diameterOf(120)).c_str());

    // ---- 7. The whole method, rendered and documented ------------------------
    auto const method = correctedStrength(LookupExampleShape::Cylinder);
    formula::Documentation const documentation = formula::document(method);
    formula::Citation const& citation = documentation.citations.front();

    std::printf("method:        %s\n", documentation.formula.c_str());
    std::printf("cited:         %.*s, %.*s, %.*s %.*s\n",
                static_cast<int>(citation.title.size()),
                citation.title.data(),
                static_cast<int>(citation.reference.size()),
                citation.reference.data(),
                static_cast<int>(citation.section.size()),
                citation.section.data(),
                static_cast<int>(citation.equation.size()),
                citation.equation.data());
    for (formula::SymbolEntry const& entry: documentation.symbols)
        std::printf("symbol:        %.*s = %.*s [%s]\n",
                    static_cast<int>(entry.symbol.size()),
                    entry.symbol.data(),
                    static_cast<int>(entry.description.size()),
                    entry.description.data(),
                    std::string { formula::view(entry.unit.symbolText) }.c_str());

    // ---- 8. The method evaluated, and its derivation -------------------------
    auto const specimen = formula::environment(formula::Measured<MeasuredStrength> { rat(40) },
                                               formula::Measured<Diameter> { rat(120) });
    std::optional<formula::Rational> const corrected = valueOf<CorrectedStrength>(method, specimen);
    std::printf("f_c:           %s MPa\n", exact_text(*corrected).c_str());
    std::printf("%s", tracedEvaluation<CorrectedStrength>(method, specimen).c_str());

    // ---- 9. A miss, traced ---------------------------------------------------
    //
    // The trace is where a miss stops being an opaque `DomainError`: the
    // bracketed clause says the value fell in no band AND what the table
    // actually covers, so a reader can tell a miss from a failure relayed
    // upward from the operand.
    std::printf("%s", tracedEvaluation<SizeCorrection>(sizeFactor(), diameterOf(200)).c_str());

    // ---- Every claim printed above, verified in code -------------------------
    bool const bandedSelects = at120 == rat(1) && at100 == rat(1);
    bool const markdownCarriesNoBracket = bandedMarkdown.find('[') == std::string::npos;
    bool const topBoundExcluded = at200Missed == formula::ArithmeticError::DomainError;
    bool const nextTickReachesIt = at200Inclusive == rat(21, 20);
    bool const curveComputes = curveAt120 == rat(97, 100);
    bool const curveTopIncluded = curveAt200 == rat(21, 20);
    bool const noExtrapolation = curveAt220 == formula::ArithmeticError::DomainError;
    bool const exactSelects = cylinder == rat(97, 100);
    bool const absentKeyMisses = core == formula::ArithmeticError::DomainError;
    bool const nestedComposes = nestedAt120 == rat(19, 20) && nestedDocumentation.symbols.size() == 1;
    bool const methodEvaluates = corrected == rat(194, 5);

    // The two domains disagree at 200 mm, and that disagreement is the point:
    // a band's top is excluded, a curve's last row is a row.
    bool const domainsDisagreeOnPurpose = topBoundExcluded && curveTopIncluded;

    bool const allChecksPassed = bandedSelects && markdownCarriesNoBracket && topBoundExcluded && nextTickReachesIt
                                 && curveComputes && curveTopIncluded && noExtrapolation && exactSelects
                                 && absentKeyMisses && nestedComposes && methodEvaluates && domainsDisagreeOnPurpose;
    std::printf("all checks passed: %s\n", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
