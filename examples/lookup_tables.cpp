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

using Diameter = formula::Quantity<struct DiameterTag, "d", "specimen diameter", unit::Millimetre>;
using MeasuredStrength =
    formula::Quantity<struct MeasuredStrengthTag, "f_m", "measured compressive strength", unit::Megapascal>;
using CorrectedStrength =
    formula::Quantity<struct CorrectedStrengthTag, "f_c", "corrected compressive strength", unit::Megapascal>;
using SizeCorrection = formula::Quantity<struct SizeCorrectionTag, "k", "size correction factor", unit::One>;

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
    formula::band(0, 1, 127, 1),   // 0 to under 127 mm
    formula::band(127, 1, 173, 1), // 127 to under 173 mm
    formula::band(173, 1, 211, 1), // 173 to under 211 mm -- 211 mm itself is NOT in it
};

// The same top row, written the way a published table meaning "173 mm to 211
// mm inclusive" must be written: the high bound at the next tick the domain
// can actually take on. `unit::Millimetre` declares one decimal, so that tick
// is 211.1 mm = 2111/10 -- a real, exact number, not an approximation.
//
// This is the caller's reconciliation to do, and there is deliberately no
// closed-upper-bound flag on `Band` to do it with (band.hpp says why).
inline constexpr formula::BandTable<1> TopRowInclusive {
    formula::band(173, 1, 2111, 10), // 173 to under 211.1 mm -- 211 mm IS in it
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
    formula::band(837, 10, 973, 10),   // 83.7 to under 97.3 %
    formula::band(973, 10, 1041, 10),  // 97.3 to under 104.1 %
    formula::band(1041, 10, 1179, 10), // 104.1 to under 117.9 %
};

// The corrections are declared in PERCENT while the factor a formula consumes
// is dimensionless -- so the result side of every table below is converted,
// exactly as the key side is. A table's contents are runtime state (the
// numbers a customer's registered table supplies); only its structure -- the
// bands, the keys, the breakpoints, and both units -- lives in the type.
[[nodiscard]] constexpr auto sizeFactor()
{
    return formula::banded_lookup<unit::Millimetre, SizeBands, unit::Percent>(
        var<Diameter>, { rat(913, 10), rat(1051, 10), rat(1127, 10) });
}

[[nodiscard]] constexpr auto topRowInclusiveFactor()
{
    return formula::banded_lookup<unit::Millimetre, TopRowInclusive, unit::Percent>(var<Diameter>, { rat(1127, 10) });
}

[[nodiscard]] constexpr auto shapeFactor(LookupExampleShape shape)
{
    return formula::exact_lookup<ShapeKeys, unit::Percent>(shape, { rat(1013, 10), rat(863, 10), rat(931, 10) });
}

[[nodiscard]] constexpr auto curingFactor(LookupExampleCuring curing)
{
    return formula::exact_lookup<CuringKeys, unit::Percent>(curing, { rat(1043, 10), rat(937, 10), rat(881, 10) });
}

[[nodiscard]] constexpr auto sizeCurveFactor()
{
    return formula::interpolating_lookup<unit::Millimetre, SizeCurve, unit::Percent>(
        var<Diameter>, { rat(913, 10), rat(1051, 10), rat(1127, 10) });
}

// The nested shape: a banded lookup whose operand is an interpolating lookup.
[[nodiscard]] constexpr auto classFactor()
{
    return formula::banded_lookup<unit::Percent, ClassBands, unit::Percent>(sizeCurveFactor(),
                                                                            { rat(919, 10), rat(1013, 10), rat(1087, 10) });
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
    // not a stylistic choice: `[0, 127)` opens Markdown link syntax, which
    // once silently dropped an operand from a published page of this
    // project's own documentation. The Markdown rendering below carries no
    // square bracket at all, and the assertion at the bottom of this file
    // checks that rather than trusting it.
    std::printf("banded:        %s\n", formula::render(sizeFactor()).c_str());
    std::string const bandedMarkdown = formula::render<formula::Dialect::Markdown>(sizeFactor());
    std::printf("banded (md):   %s\n", bandedMarkdown.c_str());

    std::optional<formula::Rational> const at139 = valueOf<SizeCorrection>(sizeFactor(), diameterOf(139));
    std::printf("d = 139 mm:    %s\n", exact_text(*at139).c_str());

    // ---- 2. Bands are half-open, and the boundary belongs to the band above --
    //
    // 127 mm is the boundary the first two bands share. It belongs to
    // the band whose LOW bound it is, never the band whose high bound it is.
    std::optional<formula::Rational> const at127 = valueOf<SizeCorrection>(sizeFactor(), diameterOf(127));
    std::printf("d = 127 mm:    %s (the band above the boundary, never the one below)\n", exact_text(*at127).c_str());

    // ---- 3. The table's own top bound is excluded, and that is the caller's --
    //          reconciliation to do
    //
    // SizeBands' last row runs 173 to under 211 mm, so 211 mm falls in NO band. A
    // published row meaning "173 mm to 211 mm inclusive" is written with its
    // high bound at the next tick past 211 -- 211.1 mm, one decimal being what
    // unit::Millimetre declares.
    std::optional<formula::ArithmeticError> const at211Missed = errorOf<SizeCorrection>(sizeFactor(), diameterOf(211));
    std::optional<formula::Rational> const at211Inclusive =
        valueOf<SizeCorrection>(topRowInclusiveFactor(), diameterOf(211));
    std::string_view const missText = formula::describe(*at211Missed);
    std::printf("d = 211 mm:    %.*s\n", static_cast<int>(missText.size()), missText.data());
    std::printf("inclusive top: %s\n", formula::render(topRowInclusiveFactor()).c_str());
    std::printf("d = 211 mm:    %s\n", exact_text(*at211Inclusive).c_str());

    // ---- 4. An interpolating table computes a number no row contains --------
    //
    // ... and its domain is CLOSED at both ends, deliberately unlike a band
    // table's. The same 211 mm that missed above is this table's last row, and
    // a row is a value the table states, not a boundary between two of them.
    std::printf("interpolating: %s\n", formula::render(sizeCurveFactor()).c_str());

    std::optional<formula::Rational> const curveAt139 = valueOf<SizeCorrection>(sizeCurveFactor(), diameterOf(139));
    std::optional<formula::Rational> const curveAt211 = valueOf<SizeCorrection>(sizeCurveFactor(), diameterOf(211));
    std::optional<formula::ArithmeticError> const curveAt233 = errorOf<SizeCorrection>(sizeCurveFactor(), diameterOf(233));
    std::string_view const curveMissText = formula::describe(*curveAt233);
    std::printf("d = 139 mm:    %s (between two rows -- in neither of them)\n", exact_text(*curveAt139).c_str());
    std::printf("d = 211 mm:    %s (the last row, reached -- where the band table missed)\n",
                exact_text(*curveAt211).c_str());
    std::printf("d = 233 mm:    %.*s (no extrapolation past the last row)\n",
                static_cast<int>(curveMissText.size()),
                curveMissText.data());

    // ---- 5. An exact lookup: a category key names a row ---------------------
    //
    // A key renders as its enumerator's NAME (`key Cylinder`), recovered at
    // compile time. Only a key that names no row of the table -- the miss --
    // falls back to its underlying value (`key 13`), and a trace of the miss
    // says the same.
    std::printf("exact:         %s\n", formula::render(shapeFactor(LookupExampleShape::Cylinder)).c_str());
    std::printf("exact miss:    %s\n", formula::render(shapeFactor(LookupExampleShape::DrilledCore)).c_str());
    std::printf(
        "%s",
        tracedEvaluation<SizeCorrection>(shapeFactor(LookupExampleShape::DrilledCore), formula::environment()).c_str());

    std::optional<formula::Rational> const cylinder =
        valueOf<SizeCorrection>(shapeFactor(LookupExampleShape::Cylinder), formula::environment());
    std::optional<formula::ArithmeticError> const core =
        errorOf<SizeCorrection>(shapeFactor(LookupExampleShape::DrilledCore), formula::environment());
    std::string_view const coreMissText = formula::describe(*core);
    std::printf("Cylinder:      %s\n", exact_text(*cylinder).c_str());
    std::printf("DrilledCore:   %.*s (a key no row of the table names)\n",
                static_cast<int>(coreMissText.size()),
                coreMissText.data());

    // ---- 5a. An exact lookup whose keys the author spells ---------------------
    //
    // `EnumeratorName<LookupExampleCuring>` words two rows the way the
    // published table does and leaves `Air` under its own name. render() and
    // the trace both follow it.
    std::printf("customized:    %s\n", formula::render(curingFactor(LookupExampleCuring::Sealed)).c_str());
    std::printf("%s",
                tracedEvaluation<SizeCorrection>(curingFactor(LookupExampleCuring::Sealed), formula::environment()).c_str());
    std::optional<formula::Rational> const sealed =
        valueOf<SizeCorrection>(curingFactor(LookupExampleCuring::Sealed), formula::environment());

    // ---- 6. A lookup nested inside another lookup's operand ------------------
    //
    // A curve produces a continuous factor; a band table buckets it into the
    // published class. All four surfaces at once: it renders, it documents, it
    // evaluates, and it traces.
    std::printf("nested:        %s\n", formula::render(classFactor()).c_str());

    std::optional<formula::Rational> const nestedAt139 = valueOf<SizeCorrection>(classFactor(), diameterOf(139));
    std::printf("d = 139 mm:    %s (curve gives 94.9 %%, which falls in the 83.7-to-under-97.3 %% band)\n",
                exact_text(*nestedAt139).c_str());

    formula::Documentation const nestedDocumentation = formula::document(classFactor());
    std::printf("nested symbols: %zu\n", nestedDocumentation.symbols.size());

    std::printf("%s", tracedEvaluation<SizeCorrection>(classFactor(), diameterOf(139)).c_str());

    // ---- 6a. An interpolating lookup's own trace clause, in both its forms ---
    //
    // A lookup step ends in a bracketed clause naming where its answer came
    // from, and the interpolating kind has TWO of those rather than one: the
    // two rows it drew on, or -- at a value sitting exactly on a row -- the
    // single row it read. They say genuinely different things. Between two
    // rows the answer appears in neither and a reader has an interpolation to
    // check; on a row the table stated the number directly and there is
    // nothing to check.
    std::printf("%s", tracedEvaluation<SizeCorrection>(sizeCurveFactor(), diameterOf(139)).c_str());
    std::printf("%s", tracedEvaluation<SizeCorrection>(sizeCurveFactor(), diameterOf(211)).c_str());

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
    auto const specimen =
        formula::environment(formula::Measured<MeasuredStrength> { rat(40) }, formula::Measured<Diameter> { rat(139) });
    std::optional<formula::Rational> const corrected = valueOf<CorrectedStrength>(method, specimen);
    std::printf("f_c:           %s MPa\n", exact_text(*corrected).c_str());
    std::printf("%s", tracedEvaluation<CorrectedStrength>(method, specimen).c_str());

    // ---- 9. A miss, traced ---------------------------------------------------
    //
    // The trace is where a miss stops being an opaque `DomainError`: the
    // bracketed clause says the value fell in no band AND what the table
    // actually covers, so a reader can tell a miss from a failure relayed
    // upward from the operand.
    std::printf("%s", tracedEvaluation<SizeCorrection>(sizeFactor(), diameterOf(211)).c_str());

    // ---- Every claim printed above, verified in code -------------------------
    bool const bandedSelects = at139 == rat(1051, 1000) && at127 == rat(1051, 1000);
    bool const markdownCarriesNoBracket = bandedMarkdown.find('[') == std::string::npos;
    bool const topBoundExcluded = at211Missed == formula::ArithmeticError::DomainError;
    bool const nextTickReachesIt = at211Inclusive == rat(1127, 1000);
    bool const curveComputes = curveAt139 == rat(949, 1000);
    bool const curveTopIncluded = curveAt211 == rat(1127, 1000);
    bool const noExtrapolation = curveAt233 == formula::ArithmeticError::DomainError;
    bool const exactSelects = cylinder == rat(863, 1000);
    bool const absentKeyMisses = core == formula::ArithmeticError::DomainError;
    bool const customizedSelects = sealed == rat(937, 1000);
    bool const nestedComposes = nestedAt139 == rat(919, 1000) && nestedDocumentation.symbols.size() == 1;
    bool const methodEvaluates = corrected == rat(907013, 25000);

    // The two domains disagree at 211 mm, and that disagreement is the point:
    // a band's top is excluded, a curve's last row is a row.
    bool const domainsDisagreeOnPurpose = topBoundExcluded && curveTopIncluded;

    bool const allChecksPassed = bandedSelects && markdownCarriesNoBracket && topBoundExcluded && nextTickReachesIt
                                 && curveComputes && curveTopIncluded && noExtrapolation && exactSelects && absentKeyMisses
                                 && customizedSelects && nestedComposes && methodEvaluates && domainsDisagreeOnPurpose;
    std::printf("all checks passed: %s\n", allChecksPassed ? "yes" : "no");
    return allChecksPassed ? 0 : 1;
}
