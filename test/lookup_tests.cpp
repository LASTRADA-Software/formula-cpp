// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;
using formula::band;
using formula::BandTable;
using formula::banded_lookup;

/// A specimen diameter, in millimetres -- the value a band table buckets.
struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", unit::Millimetre>
{
};

/// The correction the table selects, dimensionless.
struct SizeCorrection: formula::Quantity<SizeCorrection, "k", "size correction factor", unit::One>
{
};

/// A second input, used only by the composition test below, to prove a
/// banded lookup stands where a number stands and combines with `*` like any
/// other node.
struct NominalSize: formula::Quantity<NominalSize, "d0", "nominal specimen size", unit::Millimetre>
{
};
struct CorrectedSize: formula::Quantity<CorrectedSize, "d_c", "corrected specimen size", unit::Millimetre>
{
};

[[nodiscard]] constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

/// Three bands, declared in **centimetres** -- deliberately not millimetres,
/// the unit `Diameter` itself is declared in -- so evaluating this table
/// exercises a real unit conversion (coherent SI -> the table's own key
/// unit) rather than an identity one. In millimetres these are [0,13.9),
/// [13.9,27.1) and [27.1,41.1): a first, a middle and a last band (a defect that only shows up in the middle is strictly
/// stronger than one visible at either end).
inline constexpr BandTable<3> SizeBands {
    band(0, 1, 139, 100),     // [0, 1.39) cm     == [0, 13.9) mm
    band(139, 100, 271, 100), // [1.39, 2.71) cm  == [13.9, 27.1) mm
    band(271, 100, 411, 100), // [2.71, 4.11) cm  == [27.1, 41.1) mm
};

/// The corrections each band selects, declared in **percent** -- again
/// deliberately not `SizeCorrection`'s own unit (`One`) -- so the result side
/// of the table is converted too, not merely passed through.
[[nodiscard]] constexpr auto lookup()
{
    return banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(var<Diameter>,
                                                                     { rat(863, 10), rat(1043, 10), rat(1127, 10) });
}

[[nodiscard]] constexpr auto millimetresOfDiameter(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::environment(formula::Measured<Diameter> { rat(numerator, denominator) });
}

/// An always-empty table: see `band.hpp`'s file comment for why this
/// validates rather than being refused -- an empty table is always missing,
/// never malformed, and a lookup against it must produce the same "not a
/// value" outcome as any other miss.
inline constexpr BandTable<0> EmptyTable {};
} // namespace

TEST_CASE("a banded lookup is a Node and produces the result quantity's own dimension", "[lookup]")
{
    STATIC_REQUIRE(formula::Node<decltype(lookup())>);
    STATIC_REQUIRE(decltype(lookup())::dimension == formula::Describe<SizeCorrection>::dimension);
}

TEST_CASE("a value in the first band selects that band's correction", "[lookup]")
{
    // 5 mm == 0.5 cm, inside [0, 1.39) cm.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(5));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(863, 1000));
}

TEST_CASE("a value in a middle band selects that band's correction", "[lookup]")
{
    // 15 mm == 1.5 cm, inside [1.39, 2.71) cm -- not the first band and not the
    // last, the position that is strictly stronger.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(15));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(1043, 1000));
}

TEST_CASE("a value in the last band selects that band's correction", "[lookup]")
{
    // 35 mm == 3.5 cm, inside [2.71, 4.11) cm.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(35));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(1127, 1000));
}

TEST_CASE("a value exactly on a shared boundary belongs to the higher band -- half-open", "[lookup]")
{
    // 13.9 mm == 1.39 cm exactly: the shared boundary between [0,1.39) and
    // [1.39,2.71). Half-open means this belongs to [1.39,2.71), the SAME band
    // as 15 mm above -- an implementation that instead closed the LOWER band
    // (`<=` on the high bound) would silently report the first band's
    // correction here instead.
    constexpr auto atFirstBoundary = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(139, 10));
    STATIC_REQUIRE(atFirstBoundary.has_value());
    STATIC_REQUIRE(atFirstBoundary->is_value());
    STATIC_REQUIRE(atFirstBoundary->measurement().value() == rat(1043, 1000));

    // 27.1 mm == 2.71 cm exactly: the shared boundary between [1.39,2.71) and
    // [2.71,4.11).
    constexpr auto atSecondBoundary = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(271, 10));
    STATIC_REQUIRE(atSecondBoundary.has_value());
    STATIC_REQUIRE(atSecondBoundary->is_value());
    STATIC_REQUIRE(atSecondBoundary->measurement().value() == rat(1127, 1000));
}

TEST_CASE("exactly the table's own lowest bound is a hit -- the low bound is inclusive", "[lookup]")
{
    // 0 mm == 0 cm: SizeBands[0]'s own low bound, with no neighbouring band
    // below it to share the boundary with. [0, 1.39) includes it. Distinct from
    // the shared-boundary test above: that one exercises an *internal*
    // boundary two bands agree on, this one the table's own outer edge,
    // which a half-open mutation confined to just that edge would not
    // otherwise be caught by.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(0));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(863, 1000));
}

TEST_CASE("exactly the table's own highest bound is a miss -- the high bound is exclusive", "[lookup]")
{
    // 41.1 mm == 4.11 cm: SizeBands[2]'s own high bound, with no neighbouring
    // band above it. [2.71, 4.11) excludes it, and there is no band above, so
    // this is a miss -- not the last band's value. This is also the concrete
    // proof behind band.hpp's own instruction to authors: a table whose last
    // row means "up to and including the maximum" must state its high bound
    // as the next tick past it, and that instruction is only trustworthy if
    // the library actually excludes the top bound.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(411, 10));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a value below the lowest band is reported as a miss, not a value", "[lookup]")
{
    // -5 mm == -0.5 cm, below SizeBands[0]'s low bound (0). A lazy
    // implementation that clamped to the nearest band would answer with the
    // first band's correction (863/1000) here instead of missing -- see the
    // mutation in this task's report.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(-5));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a value above the highest band is reported as a miss, not a value", "[lookup]")
{
    // 47 mm == 4.7 cm, above SizeBands[2]'s high bound (4.11). A lazy
    // implementation that clamped to the nearest band would answer with the
    // last band's correction (1127/1000) here instead of missing.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(47));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an absent operand stays absent -- absence and a miss are not the same thing", "[lookup]")
{
    // Never measured is a different fact from measured-but-in-no-band, and
    // this library has exactly one representation for each: `Outcome::Empty`
    // for the former, `ArithmeticError::DomainError` (never an `Outcome` at
    // all) for the latter. Collapsing them would silently misreport a
    // specimen that was simply never measured as one whose diameter falls
    // outside every band this table declares.
    constexpr auto environment = formula::environment(formula::Measured<Diameter>::absent());
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), environment);
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_empty());
}

TEST_CASE("a banded lookup composes with other nodes, exactly like any other Node", "[lookup]")
{
    // A lookup produces a quantity, so it stands where a number stands
    // and combines with `*` the same way a `ConstantNode` would -- no
    // separate entry point, unlike `Constraint`.
    constexpr auto corrected = var<NominalSize> * lookup();
    STATIC_REQUIRE(decltype(corrected)::dimension == formula::Describe<CorrectedSize>::dimension);

    constexpr auto environment =
        formula::environment(formula::Measured<Diameter> { rat(15) }, formula::Measured<NominalSize> { rat(211) });
    // 15 mm diameter selects the middle band's correction, 1043/1000
    // (dimensionless); 211 mm nominal size times 1043/1000 is 220.073 mm.
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(corrected, environment);
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(220073, 1000));
}

TEST_CASE("a miss propagates through composition too, not only when the lookup is the whole formula",
          "[lookup]")
{
    constexpr auto corrected = var<NominalSize> * lookup();
    constexpr auto environment =
        formula::environment(formula::Measured<Diameter> { rat(-5) }, formula::Measured<NominalSize> { rat(211) });
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(corrected, environment);
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an empty band table always misses -- the same 'not a value' outcome, not a special case",
          "[lookup]")
{
    constexpr auto node = banded_lookup<unit::Millimetre, EmptyTable, unit::One>(var<Diameter>, {});
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(5));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an integer literal is accepted as a correction, not only a Rational", "[lookup]")
{
    // Corrections<N>'s element constraint is convertible_to<Rational>, not
    // same_as<Rational>: same_as would silently stop `{2, 3, 7}` -- a
    // correction table whose rows are whole numbers -- from compiling, and
    // every other Rational-typed parameter in this library already accepts
    // an integer literal. Kept as its own test so nobody re-tightens this
    // constraint later without noticing.
    constexpr auto node = banded_lookup<unit::Centimetre, SizeBands, unit::One>(var<Diameter>, { 2, 3, 7 });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(5));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(2));
}

// ---------------------------------------------------------------------------
// Exact lookup: a category key names a row.
// ---------------------------------------------------------------------------

namespace
{
using formula::exact_lookup;
using formula::KeyTable;

/// An invented specimen-shape discriminator -- no real published standard's
/// shapes, names or factors appear anywhere in this file. A scoped
/// enumeration, per `lookup.hpp`'s "How a key is spelled": a misspelled
/// `SpecimenVariant::Cylindr` is then a compile error from the language itself
/// rather than a silent runtime miss.
///
/// The underlying type is fixed deliberately. Without it a scoped
/// enumeration's value range is only as wide as its enumerators require, so
/// `static_cast<SpecimenVariant>(99)` -- which the "not an enumerator at all"
/// test below needs -- would be undefined behaviour and, in a constant
/// expression, a compile error. Fixing the underlying type makes every
/// `std::uint8_t` value a well-defined `SpecimenVariant`, which is also what a
/// key deserialised from stored data really is.
///
/// **Named for this file rather than generically, and that is a rule not a
/// preference.** Two translation units whose anonymous-namespace key
/// enumerations share a name and whose tables share their element values fail
/// to link under clang, with a dangling relocation and no diagnostic naming the
/// key type. This file spelled its `SpecimenShape` and got away with it only
/// because none of its tables needed an address at all. See
/// `trace_render_tests.cpp`'s `RenderedShape` for the measured mechanism, and
/// `lookup.hpp`'s file comment for what it costs a consumer.
enum class SpecimenVariant : std::uint8_t
{
    CubeSmall,
    CubeLarge,
    CylinderShort,
    CylinderTall,
    Prism,
};

/// Four of the five shapes. `Prism` is deliberately left out: it is a key
/// that exists in the enumeration and not in this table, which is the
/// realistic "absent key" -- an enumeration the method grew and a registered
/// table that did not -- rather than a value nobody could have written.
inline constexpr KeyTable<SpecimenVariant, 4> ShapeKeys {
    SpecimenVariant::CubeSmall,
    SpecimenVariant::CubeLarge,
    SpecimenVariant::CylinderShort,
    SpecimenVariant::CylinderTall,
};

/// The correction each shape selects, declared in **percent** -- deliberately
/// not `SizeCorrection`'s own unit (`One`) -- so the result side of the table
/// is converted rather than merely passed through, exactly as the banded
/// table above does it.
[[nodiscard]] constexpr auto shapeLookup(SpecimenVariant shape)
{
    return exact_lookup<ShapeKeys, unit::Percent>(shape, { rat(1093, 10), rat(1031, 10), rat(917, 10), rat(853, 10) });
}

/// An always-empty table: the exact-lookup analogue of `EmptyTable` above,
/// valid for the same reason and always missing for the same reason.
inline constexpr KeyTable<SpecimenVariant, 0> NoShapes {};

/// A one-row table: the other degenerate shape, where the only row is
/// simultaneously the first and the last.
inline constexpr KeyTable<SpecimenVariant, 1> OnlyPrism { SpecimenVariant::Prism };
} // namespace

TEST_CASE("an exact lookup is a Node and produces the result quantity's own dimension", "[lookup]")
{
    STATIC_REQUIRE(formula::Node<decltype(shapeLookup(SpecimenVariant::CubeSmall))>);
    STATIC_REQUIRE(decltype(shapeLookup(SpecimenVariant::CubeSmall))::dimension
                   == formula::Describe<SizeCorrection>::dimension);
}

TEST_CASE("the first key in an exact table selects its own row", "[lookup]")
{
    // The table's own first row, which a scan that skipped index 0 -- or
    // started at 1 to "skip the header" -- would miss while every interior
    // row still answered correctly.
    constexpr auto computed =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenVariant::CubeSmall), formula::environment());
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(1093, 1000));
}

TEST_CASE("a middle key in an exact table selects its own row", "[lookup]")
{
    // Neither first nor last: the position that is strictly stronger, here
    // on the hit side.
    constexpr auto computed =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenVariant::CylinderShort), formula::environment());
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(917, 1000));

    // The other interior row, so that a scan returning a fixed interior index
    // cannot pass this test case by accident.
    constexpr auto second =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenVariant::CubeLarge), formula::environment());
    STATIC_REQUIRE(second.has_value());
    STATIC_REQUIRE(second->is_value());
    STATIC_REQUIRE(second->measurement().value() == rat(1031, 1000));
}

TEST_CASE("the last key in an exact table selects its own row", "[lookup]")
{
    // The table's own last row: a scan whose bound was `index + 1 < size`
    // would miss exactly this one and nothing else.
    constexpr auto computed =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenVariant::CylinderTall), formula::environment());
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(853, 1000));
}

TEST_CASE("a key the table does not declare is reported as a miss, not a value", "[lookup]")
{
    // `Prism` is a perfectly good `SpecimenVariant` that this table has no row
    // for. An implementation that fell back to the first row would answer
    // 1093/1000 here; one that fell back to a default would answer 0. Both are
    // lies, and the honest answer is that nothing was found.
    constexpr auto computed =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenVariant::Prism), formula::environment());
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a key that is not an enumerator at all is a miss, not an index", "[lookup]")
{
    // A key that arrived from stored data and means nothing. The scan must
    // find no row rather than indexing with it -- which is what an
    // implementation that treated the key as an offset into the table would
    // do.
    constexpr auto computed =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(static_cast<SpecimenVariant>(99)), formula::environment());
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an exact miss and a banded miss are the same failure, reported the same way", "[lookup]")
{
    // The one test that pins this phase's stated drift risk directly. Two
    // different tables, two different reasons nothing was found, and exactly
    // one vocabulary for "found nothing" -- so a later change that gives
    // either kind its own spelling fails here rather than in a consumer.
    constexpr auto bandedMiss = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(47));
    constexpr auto exactMiss =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenVariant::Prism), formula::environment());

    STATIC_REQUIRE(!bandedMiss.has_value());
    STATIC_REQUIRE(!exactMiss.has_value());
    STATIC_REQUIRE(bandedMiss.error() == exactMiss.error());
    STATIC_REQUIRE(exactMiss.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an empty exact table always misses -- the same not-a-value outcome, not a special case", "[lookup]")
{
    constexpr auto node = exact_lookup<NoShapes, unit::One>(SpecimenVariant::CubeSmall, {});
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, formula::environment());
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a one-row exact table hits its only key and misses every other", "[lookup]")
{
    // The degenerate table where the only row is both the first and the last,
    // so a bound that is off by one in either direction shows up here.
    constexpr auto hit = formula::checked_evaluate<SizeCorrection>(
        exact_lookup<OnlyPrism, unit::One>(SpecimenVariant::Prism, { rat(781, 1000) }), formula::environment());
    STATIC_REQUIRE(hit.has_value());
    STATIC_REQUIRE(hit->is_value());
    STATIC_REQUIRE(hit->measurement().value() == rat(781, 1000));

    constexpr auto miss = formula::checked_evaluate<SizeCorrection>(
        exact_lookup<OnlyPrism, unit::One>(SpecimenVariant::CubeSmall, { rat(781, 1000) }), formula::environment());
    STATIC_REQUIRE(!miss.has_value());
    STATIC_REQUIRE(miss.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a row whose correction is zero is a hit worth zero, never a miss", "[lookup]")
{
    // The hit side of the same distinction `Corrections` exists to protect:
    // zero is a legitimate correction a table may genuinely publish, and it
    // must come back as the value 0 rather than being confused with "nothing
    // was found". Placed on a middle row, so neither a first-row nor a
    // last-row special case can produce it.
    constexpr auto node = exact_lookup<ShapeKeys, unit::One>(SpecimenVariant::CylinderShort,
                                                             { rat(1127, 1000), rat(863, 1000), rat(0), rat(1043, 1000) });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, formula::environment());
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(0));
}

TEST_CASE("an exact lookup composes with other nodes, exactly like any other Node", "[lookup]")
{
    // Again, for the second table kind: a lookup produces a quantity, so
    // it stands where a number stands and combines with `*` the same way a
    // `ConstantNode` would.
    constexpr auto corrected = var<NominalSize> * shapeLookup(SpecimenVariant::CubeLarge);
    STATIC_REQUIRE(decltype(corrected)::dimension == formula::Describe<CorrectedSize>::dimension);

    constexpr auto environment = formula::environment(formula::Measured<NominalSize> { rat(211) });
    // CubeLarge selects 103.1 % == 1031/1000, so 211 mm times 1031/1000 is
    // 217.541 mm.
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(corrected, environment);
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(217541, 1000));
}

TEST_CASE("an exact miss propagates through composition too, not only when the lookup is the whole formula",
          "[lookup]")
{
    constexpr auto corrected = var<NominalSize> * shapeLookup(SpecimenVariant::Prism);
    constexpr auto environment = formula::environment(formula::Measured<NominalSize> { rat(211) });
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(corrected, environment);
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an absent operand beside a hitting exact lookup stays absent, and is not a miss", "[lookup]")
{
    // Never measured is a different fact from looked-up-and-not-found, and an
    // exact lookup that hits must not turn one into the other. The lookup
    // itself succeeds here; the missing measurement is what makes the result
    // empty.
    constexpr auto corrected = var<NominalSize> * shapeLookup(SpecimenVariant::CubeLarge);
    constexpr auto environment = formula::environment(formula::Measured<NominalSize>::absent());
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(corrected, environment);
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_empty());
}

TEST_CASE("an integer literal is accepted as an exact lookup's correction, not only a Rational", "[lookup]")
{
    // The same guard the banded case carries, for the same reason: both
    // factories take the same `Corrections<N>`, whose element constraint is
    // convertible_to<Rational> rather than same_as<Rational>.
    constexpr auto node = exact_lookup<ShapeKeys, unit::One>(SpecimenVariant::CubeSmall, { 2, 3, 5, 7 });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, formula::environment());
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(2));
}

TEST_CASE("key_table_is_well_formed answers for a table that only arrives at runtime", "[lookup]")
{
    // The runtime half of the same question `RequireValidKeyTable` asks at
    // compile time, built on the same `keys_match` predicate so the two
    // cannot drift. A loader that reads a customer's registered table has no
    // static_assert available to it and needs this.
    STATIC_REQUIRE(formula::key_table_is_well_formed(ShapeKeys));
    STATIC_REQUIRE(formula::key_table_is_well_formed(NoShapes));
    STATIC_REQUIRE(formula::key_table_is_well_formed(OnlyPrism));

    // A duplicate in the middle of the table, neither first nor last, and not
    // in adjacent rows either: an exact table has no declared order, so a
    // check that only compared neighbours would pass this.
    constexpr KeyTable<SpecimenVariant, 5> Repeated {
        SpecimenVariant::CubeSmall,
        SpecimenVariant::CubeLarge,
        SpecimenVariant::CylinderShort,
        SpecimenVariant::CubeLarge, // already declared two rows above
        SpecimenVariant::Prism,
    };
    STATIC_REQUIRE(!formula::key_table_is_well_formed(Repeated));

    // The table's own FINAL pair. `Repeated` above cannot catch a sweep whose
    // outer bound stops one row early -- its duplicate is at rows 1 and 3 of
    // 5, so rows 3 and 4 never need to be compared for it to be found.
    // Measured: widening the outer bound from `first + 1 < N` to
    // `first + 2 < N` leaves every other assertion in this file green.
    constexpr KeyTable<SpecimenVariant, 3> RepeatedAtTheEnd {
        SpecimenVariant::CubeSmall,
        SpecimenVariant::CubeLarge,
        SpecimenVariant::CubeLarge, // the last pair, and nothing after it
    };
    STATIC_REQUIRE(!formula::key_table_is_well_formed(RepeatedAtTheEnd));

    // TWO rows -- the smallest table that can be malformed at all, and the
    // boundary between "no pair to compare" (a one-row table, above) and "one
    // pair to compare". Both directions, so a predicate that answered `false`
    // for every two-row table would fail here too rather than look correct.
    constexpr KeyTable<SpecimenVariant, 2> TwoRowsRepeated { SpecimenVariant::Prism, SpecimenVariant::Prism };
    constexpr KeyTable<SpecimenVariant, 2> TwoRowsDistinct { SpecimenVariant::Prism, SpecimenVariant::CubeSmall };
    STATIC_REQUIRE(!formula::key_table_is_well_formed(TwoRowsRepeated));
    STATIC_REQUIRE(formula::key_table_is_well_formed(TwoRowsDistinct));
}

TEST_CASE("a two-row exact table selects each of its rows and misses everything else", "[lookup]")
{
    // The compile-time side of the same boundary the predicate test covers
    // just above: two rows is the smallest table with a pair, and the phase
    // otherwise only ever uses 0, 1, 4 and 5. Both rows, so neither a
    // first-only nor a last-only scan passes.
    constexpr KeyTable<SpecimenVariant, 2> TwoShapes { SpecimenVariant::CubeSmall, SpecimenVariant::Prism };

    constexpr auto first = formula::checked_evaluate<SizeCorrection>(
        exact_lookup<TwoShapes, unit::One>(SpecimenVariant::CubeSmall, { rat(917, 1000), rat(853, 1000) }),
        formula::environment());
    STATIC_REQUIRE(first.has_value());
    STATIC_REQUIRE(first->is_value());
    STATIC_REQUIRE(first->measurement().value() == rat(917, 1000));

    constexpr auto second = formula::checked_evaluate<SizeCorrection>(
        exact_lookup<TwoShapes, unit::One>(SpecimenVariant::Prism, { rat(917, 1000), rat(853, 1000) }),
        formula::environment());
    STATIC_REQUIRE(second.has_value());
    STATIC_REQUIRE(second->is_value());
    STATIC_REQUIRE(second->measurement().value() == rat(853, 1000));

    constexpr auto absent = formula::checked_evaluate<SizeCorrection>(
        exact_lookup<TwoShapes, unit::One>(SpecimenVariant::CubeLarge, { rat(917, 1000), rat(853, 1000) }),
        formula::environment());
    STATIC_REQUIRE(!absent.has_value());
    STATIC_REQUIRE(absent.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("keys_match is exact equality, with no ordering and no conversion", "[lookup]")
{
    STATIC_REQUIRE(formula::keys_match(SpecimenVariant::CubeLarge, SpecimenVariant::CubeLarge));
    STATIC_REQUIRE(!formula::keys_match(SpecimenVariant::CubeLarge, SpecimenVariant::CubeSmall));
}

// ---------------------------------------------------------------------------
// Interpolating lookup: a value between two rows produces a value appearing in
// no row. See the file comment's "Interpolating lookup" section for why the
// domain is closed at both ends where a band table's is half-open, and for why
// there is no extrapolation.
// ---------------------------------------------------------------------------

namespace
{
using formula::breakpoint;
using formula::BreakpointTable;
using formula::interpolating_lookup;

/// Four breakpoints, declared in **centimetres** -- deliberately not
/// millimetres, the unit `Diameter` itself is declared in -- so evaluating this
/// curve exercises a real unit conversion on the key side, exactly as
/// `SizeBands` does for the banded table. In millimetres these rows sit at 0,
/// 13.7, 27.4 and 41.1: a first row, two middle rows and a last row -- the last
/// on `SizeBands`' own top bound, which the test comparing the two kinds' top
/// ends relies on.
inline constexpr BreakpointTable<4> CurvePoints {
    breakpoint(0),
    breakpoint(137, 100),
    breakpoint(274, 100),
    breakpoint(411, 100),
};

/// The value stated at each breakpoint, in **percent** -- again deliberately
/// not `SizeCorrection`'s own unit (`One`) -- so the result side is converted
/// too. The middle segment descends (97.9 -> 91.7) on purpose: a slope this
/// library computes with `checked_sub` must be allowed to be negative, and an
/// implementation that took a magnitude somewhere would answer 101 % instead of
/// 94.8 % halfway along it.
[[nodiscard]] constexpr auto curve()
{
    return interpolating_lookup<unit::Centimetre, CurvePoints, unit::Percent>(
        var<Diameter>, { rat(813, 10), rat(979, 10), rat(917, 10), rat(1139, 10) });
}

/// An always-empty curve: the interpolating analogue of `EmptyTable` and
/// `NoShapes`, valid for the same reason and always missing for the same one.
inline constexpr BreakpointTable<0> NoPoints {};

/// A one-row curve. There is nothing to interpolate *between*, and the table is
/// still well-formed: it states a value at exactly one key and says nothing
/// about any other, which is a thing this library can report honestly.
inline constexpr BreakpointTable<1> OnePoint { breakpoint(137, 100) };

/// Two rows -- the smallest curve that can interpolate at all, and the smallest
/// that can be malformed at all.
inline constexpr BreakpointTable<2> TwoPoints { breakpoint(0), breakpoint(274, 100) };
} // namespace

TEST_CASE("an interpolating lookup is a Node and produces the result quantity's own dimension", "[lookup]")
{
    STATIC_REQUIRE(formula::Node<decltype(curve())>);
    STATIC_REQUIRE(decltype(curve())::dimension == formula::Describe<SizeCorrection>::dimension);
}

TEST_CASE("a value exactly on the table's first row returns that row, not an interpolation", "[lookup]")
{
    // 0 mm == 0 cm: `CurvePoints[0]` itself, with no row below it to
    // interpolate from. The table's own lower end, where in-range-versus-miss
    // is decided rather than which pair to interpolate between.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(0));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(813, 1000));
}

TEST_CASE("a value exactly on a middle row returns that row, not an interpolation", "[lookup]")
{
    // 13.7 mm == 1.37 cm, `CurvePoints[1]`: interior, so both neighbouring
    // segments exist and either could have been used by mistake.
    constexpr auto atSecondRow = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(137, 10));
    STATIC_REQUIRE(atSecondRow.has_value());
    STATIC_REQUIRE(atSecondRow->is_value());
    STATIC_REQUIRE(atSecondRow->measurement().value() == rat(979, 1000));

    // 27.4 mm == 2.74 cm, `CurvePoints[2]` -- the other interior row, so a scan
    // that answered a fixed interior index cannot pass this case by accident.
    constexpr auto atThirdRow = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(274, 10));
    STATIC_REQUIRE(atThirdRow.has_value());
    STATIC_REQUIRE(atThirdRow->is_value());
    STATIC_REQUIRE(atThirdRow->measurement().value() == rat(917, 1000));
}

TEST_CASE("a value exactly on the table's last row returns that row -- the domain is closed at the top",
          "[lookup]")
{
    // 41.1 mm == 4.11 cm: `CurvePoints[3]` itself, the table's own highest
    // breakpoint. THIS is the case the exactly-on-a-row rule is actually
    // observable in: everywhere else, interpolating across the segment a row
    // begins would return that row's own value anyway, because the weight is
    // exactly zero. Here there is no segment above, so an implementation whose
    // segments were half-open at the top -- the band table's rule, copied
    // across where it does not belong -- would report a miss for the table's
    // own last row.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(411, 10));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(1139, 1000));
}

TEST_CASE("a value between the first two rows is interpolated", "[lookup]")
{
    // 6.85 mm == 0.685 cm, halfway along [0, 1.37] cm:
    // 81.3 % + (1/2)(97.9 - 81.3) % = 89.6 %.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(137, 20));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(896, 1000));
}

TEST_CASE("a value between two middle rows is interpolated, and a descending segment descends", "[lookup]")
{
    // 17.125 mm == 1.7125 cm, a QUARTER of the way along [1.37, 2.74] cm, whose
    // value FALLS from 97.9 % to 91.7 %: 97.9 % + (1/4)(91.7 - 97.9) % =
    // 96.35 %. An implementation that took the magnitude of the slope, or
    // subtracted in the wrong order, answers 99.45 % here and still answers
    // correctly on every ascending segment.
    //
    // A quarter rather than the midpoint, deliberately. Measured: pairing each
    // row's VALUE with the other row's KEY -- an ordinary off-by-one in the
    // scan -- is invisible at the midpoint of any segment, because the mirror
    // of a line about its own midpoint passes through the same point there.
    // Every halfway assertion in this file is blind to it; this one is not.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(137, 8));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(9635, 10000));
}

TEST_CASE("a value between the last two rows is interpolated", "[lookup]")
{
    // 37.675 mm == 3.7675 cm, three quarters of the way along [2.74, 4.11] cm:
    // 91.7 % + (3/4)(113.9 - 91.7) % = 108.35 %. Off the midpoint for the
    // reason the middle-segment case above gives, and on the far side of it,
    // so the two asymmetric cases do not share a weight either.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(1507, 40));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(10835, 10000));
}

TEST_CASE("an interpolated value stays exact, even when no finite decimal could hold it", "[lookup]")
{
    // 137/30 mm == 137/300 cm, one third of the way along [0, 1.37] cm:
    // 81.3 % + (1/3)(97.9 - 81.3) % = 521/6 %, which is 521/600 once the
    // percent is converted away -- 0.868333... in decimal, a number no
    // rounding of any fixed precision holds exactly.
    //
    // This is this task's exactness question, asserted rather than asserted
    // about: the answer is the exact rational the two rows imply, and nothing
    // anywhere rounded it to get there. The denominator is checked as well as
    // the value, because an implementation that computed in a fixed decimal
    // precision could still compare equal to a rounded literal while having
    // thrown the remainder away.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(137, 30));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(521, 600));
    STATIC_REQUIRE(computed->measurement().value().denominator() == 600);
}

TEST_CASE("a value below the table's first row is a miss -- interpolation does not extrapolate", "[lookup]")
{
    // -5 mm == -0.5 cm, below `CurvePoints[0]`. An implementation that ran the
    // first segment's slope backwards would answer about 75.2 % here,
    // confidently, for an input the table never defined -- which is the one
    // thing this phase refuses everywhere. A miss, not a value.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(-5));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a value above the table's last row is a miss -- interpolation does not extrapolate", "[lookup]")
{
    // 47 mm == 4.7 cm, above `CurvePoints[3]`. Running the last segment's
    // slope onwards would answer about 123.5 %; clamping to the last row would
    // answer 113.9 %. Both are values the published table never stated,
    // invented from where the table happened to stop.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(47));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an interpolating table's own last row is a hit where a banded table's own top bound is a miss",
          "[lookup]")
{
    // The two table kinds disagree about their own top end, deliberately, and
    // this is the test that keeps the disagreement intentional rather than
    // accidental. A band's high bound is EXCLUSIVE, so 41.1 mm falls off the
    // top of `SizeBands` and misses; an interpolating table's last breakpoint
    // is a ROW, so 41.1 mm hits it exactly. Harmonising the two -- in either
    // direction -- fails here rather than in a consumer.
    constexpr auto banded = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(411, 10));
    constexpr auto interpolated = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(411, 10));

    STATIC_REQUIRE(!banded.has_value());
    STATIC_REQUIRE(banded.error() == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(interpolated.has_value());
    STATIC_REQUIRE(interpolated->is_value());
    STATIC_REQUIRE(interpolated->measurement().value() == rat(1139, 1000));
}

TEST_CASE("an interpolated value of exactly zero is a value, never a miss", "[lookup]")
{
    // The hit side of the distinction the whole file is built on, in the one
    // form only this table kind has: zero here is not a row the author typed,
    // it is a number the interpolation PRODUCED, so an implementation that
    // treated a zero result as "nothing found" would fail only here.
    constexpr BreakpointTable<2> Crossing { breakpoint(0), breakpoint(274, 100) };
    constexpr auto node =
        interpolating_lookup<unit::Centimetre, Crossing, unit::One>(var<Diameter>, { rat(-863, 1000), rat(863, 1000) });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(137, 10));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(0));
}

TEST_CASE("a two-row curve interpolates between its only pair and misses outside it", "[lookup]")
{
    // Two rows is the smallest table that can interpolate at all. Both rows and
    // the point between them, so neither a first-only nor a last-only scan
    // passes, plus both directions of miss.
    constexpr auto node =
        interpolating_lookup<unit::Centimetre, TwoPoints, unit::One>(var<Diameter>, { rat(781, 1000), rat(1127, 1000) });

    constexpr auto atFirst = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(0));
    STATIC_REQUIRE(atFirst.has_value());
    STATIC_REQUIRE(atFirst->measurement().value() == rat(781, 1000));

    constexpr auto between = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(137, 10));
    STATIC_REQUIRE(between.has_value());
    STATIC_REQUIRE(between->measurement().value() == rat(954, 1000));

    constexpr auto atLast = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(274, 10));
    STATIC_REQUIRE(atLast.has_value());
    STATIC_REQUIRE(atLast->measurement().value() == rat(1127, 1000));

    constexpr auto below = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(-1));
    STATIC_REQUIRE(!below.has_value());
    STATIC_REQUIRE(below.error() == formula::ArithmeticError::DomainError);

    constexpr auto above = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(284, 10));
    STATIC_REQUIRE(!above.has_value());
    STATIC_REQUIRE(above.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a one-row curve states a value at exactly one key and misses every other", "[lookup]")
{
    // A single row is well-formed for interpolation and is NOT special-cased:
    // there is no pair, so nothing is ever interpolated, and the table answers
    // only where it actually states something. Refusing it would be refusing a
    // table that is merely narrow rather than wrong -- the same judgement
    // `band.hpp` makes for an empty band table.
    constexpr auto node = interpolating_lookup<unit::Centimetre, OnePoint, unit::One>(var<Diameter>, { rat(917, 1000) });

    constexpr auto onTheRow = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(137, 10));
    STATIC_REQUIRE(onTheRow.has_value());
    STATIC_REQUIRE(onTheRow->is_value());
    STATIC_REQUIRE(onTheRow->measurement().value() == rat(917, 1000));

    constexpr auto below = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(5));
    STATIC_REQUIRE(!below.has_value());
    STATIC_REQUIRE(below.error() == formula::ArithmeticError::DomainError);

    constexpr auto above = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(15));
    STATIC_REQUIRE(!above.has_value());
    STATIC_REQUIRE(above.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an empty curve always misses -- the same not-a-value outcome, not a special case", "[lookup]")
{
    constexpr auto node = interpolating_lookup<unit::Millimetre, NoPoints, unit::One>(var<Diameter>, {});
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(5));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an absent operand stays absent for an interpolating lookup too", "[lookup]")
{
    // Never measured is a different fact from measured-but-outside-the-curve,
    // exactly as it is for a band table.
    constexpr auto environment = formula::environment(formula::Measured<Diameter>::absent());
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), environment);
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_empty());
}

TEST_CASE("an interpolating lookup composes with other nodes, exactly like any other Node", "[lookup]")
{
    // The same, for the third table kind.
    constexpr auto corrected = var<NominalSize> * curve();
    STATIC_REQUIRE(decltype(corrected)::dimension == formula::Describe<CorrectedSize>::dimension);

    constexpr auto environment =
        formula::environment(formula::Measured<Diameter> { rat(411, 20) }, formula::Measured<NominalSize> { rat(211) });
    // 20.55 mm, halfway along the middle segment, interpolates to 94.8 % ==
    // 237/250, and 211 mm times 237/250 is 200.028 mm.
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(corrected, environment);
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(200028, 1000));
}

TEST_CASE("an interpolating miss propagates through composition too", "[lookup]")
{
    constexpr auto corrected = var<NominalSize> * curve();
    constexpr auto environment =
        formula::environment(formula::Measured<Diameter> { rat(47) }, formula::Measured<NominalSize> { rat(211) });
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(corrected, environment);
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an integer literal is accepted as an interpolating lookup's row value, not only a Rational",
          "[lookup]")
{
    // The same guard both other kinds carry, for the same reason: all three
    // factories take the same `Corrections<N>`, whose element constraint is
    // convertible_to<Rational> rather than same_as<Rational>. 5 mm is 50/137
    // of the way from the first row's 2 to the second row's 3.
    constexpr auto node =
        interpolating_lookup<unit::Centimetre, CurvePoints, unit::One>(var<Diameter>, { 2, 3, 5, 7 });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(5));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(324, 137));
}

TEST_CASE("breakpoints_ascend is strict, and refuses a bound that is not a rational number", "[lookup]")
{
    STATIC_REQUIRE(formula::breakpoints_ascend(breakpoint(103), breakpoint(197)));
    // Equal is NOT ascending: two rows at one key state two values there and
    // leave a segment of zero width to divide by.
    STATIC_REQUIRE(!formula::breakpoints_ascend(breakpoint(197), breakpoint(197)));
    STATIC_REQUIRE(!formula::breakpoints_ascend(breakpoint(293), breakpoint(197)));
    // Compared as rationals, not as numerator/denominator pairs: 1/2 < 2/3.
    STATIC_REQUIRE(formula::breakpoints_ascend(breakpoint(1, 2), breakpoint(2, 3)));
    // A bound that is not a representable rational is not ascending with
    // anything, in either position -- the same judgement `bands_are_adjacent`
    // makes, so a malformed row is never waved through.
    STATIC_REQUIRE(!formula::breakpoints_ascend(breakpoint(103, 0), breakpoint(197)));
    STATIC_REQUIRE(!formula::breakpoints_ascend(breakpoint(103), breakpoint(197, 0)));
}

TEST_CASE("breakpoint_is_well_formed asks only whether the row's key is a number at all", "[lookup]")
{
    STATIC_REQUIRE(formula::breakpoint_is_well_formed(breakpoint(0)));
    STATIC_REQUIRE(formula::breakpoint_is_well_formed(breakpoint(-7, 2)));
    STATIC_REQUIRE(!formula::breakpoint_is_well_formed(breakpoint(103, 0)));
}

TEST_CASE("breakpoint_table_is_well_formed answers for a curve that only arrives at runtime", "[lookup]")
{
    // The runtime half of the same question `RequireValidBreakpointTable` asks
    // at compile time, built on the same two predicates so the two cannot
    // drift. A loader reading a customer's registered curve has no
    // static_assert available to it and needs this.
    STATIC_REQUIRE(formula::breakpoint_table_is_well_formed(CurvePoints));
    STATIC_REQUIRE(formula::breakpoint_table_is_well_formed(NoPoints));
    STATIC_REQUIRE(formula::breakpoint_table_is_well_formed(OnePoint));
    STATIC_REQUIRE(formula::breakpoint_table_is_well_formed(TwoPoints));

    // A repeated key in the MIDDLE pair of four rows -- neither the first pair
    // nor the last -- which is what fails when the sweep is confined to either
    // end.
    constexpr BreakpointTable<4> RepeatedInTheMiddle {
        breakpoint(0),
        breakpoint(103),
        breakpoint(103), // the same key as the row above
        breakpoint(293),
    };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(RepeatedInTheMiddle));

    // Two middle rows in the wrong ORDER, which the same rule catches: 197 is
    // not strictly below 103.
    constexpr BreakpointTable<4> DescendingInTheMiddle {
        breakpoint(0),
        breakpoint(197),
        breakpoint(103),
        breakpoint(293),
    };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(DescendingInTheMiddle));

    // The table's own FINAL pair, which the middle cases above cannot catch: a
    // sweep whose bound stopped one pair early passes both of them.
    constexpr BreakpointTable<3> DescendingAtTheEnd { breakpoint(0), breakpoint(197), breakpoint(103) };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(DescendingAtTheEnd));

    // And the table's own FIRST pair, which a sweep starting at index 1 would
    // step over.
    constexpr BreakpointTable<3> DescendingAtTheStart { breakpoint(197), breakpoint(0), breakpoint(293) };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(DescendingAtTheStart));

    // Two rows: the smallest table that can be malformed at all, in both
    // directions, so a predicate answering `false` for every two-row table
    // would fail here rather than look correct.
    constexpr BreakpointTable<2> TwoRepeated { breakpoint(163), breakpoint(163) };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(TwoRepeated));
    STATIC_REQUIRE(formula::breakpoint_table_is_well_formed(TwoPoints));

    // A row whose key is not a rational number at all, in the middle, where the
    // ordering sweep alone would report it as merely out of order.
    constexpr BreakpointTable<3> MalformedInTheMiddle { breakpoint(0), breakpoint(103, 0), breakpoint(293) };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(MalformedInTheMiddle));

    // A SINGLE row whose key is not a rational number: there is no pair here,
    // so only the per-row half of well-formedness can catch it.
    constexpr BreakpointTable<1> LoneMalformed { breakpoint(103, 0) };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(LoneMalformed));
}

TEST_CASE("all three lookup kinds report finding nothing the same way", "[lookup]")
{
    // The one test that pins this phase's stated drift risk across every table
    // kind at once. Three different tables, three different reasons nothing was
    // found, and exactly one vocabulary for "found nothing" -- so a later change
    // that gives any kind its own spelling fails here rather than in a consumer.
    constexpr auto bandedMiss = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(47));
    constexpr auto exactMiss =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenVariant::Prism), formula::environment());
    constexpr auto interpolatingMiss = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(47));

    STATIC_REQUIRE(!bandedMiss.has_value());
    STATIC_REQUIRE(!exactMiss.has_value());
    STATIC_REQUIRE(!interpolatingMiss.has_value());
    STATIC_REQUIRE(bandedMiss.error() == exactMiss.error());
    STATIC_REQUIRE(exactMiss.error() == interpolatingMiss.error());
    STATIC_REQUIRE(interpolatingMiss.error() == formula::ArithmeticError::DomainError);
}

namespace
{
/// A curve whose segments are **unequal** (1.93 cm then 2.94 cm) and **none of
/// whose keys equals its own row index**. `CurvePoints` above is evenly spaced
/// -- every segment is 1.37 cm wide -- so one real defect is invisible against
/// it: taking the span from the table's first pair rather than from the
/// bracketing one. It survives a suite whose only multi-segment table is evenly
/// spaced. A second defect, reading a row's key as its row number, is invisible
/// against any table whose keys equal their indices (`{0, 1, 2, 3}` cm, say);
/// this curve pins both, whatever `CurvePoints` is later changed to.
///
/// This is the lesson the off-centre probes taught, one level up: a probe must
/// not sit at a point of symmetry, and neither must the **table**. Uniform
/// spacing makes span-from-the-first-pair indistinguishable from
/// span-per-segment; keys equal to indices make a key indistinguishable from an
/// index. Vary the fixture's shape, not only the probe's position.
inline constexpr BreakpointTable<3> UnevenPoints {
    breakpoint(0),
    breakpoint(193, 100),
    breakpoint(487, 100),
};

/// Two rows whose values are far enough apart that the order the interpolation
/// divides and multiplies in decides whether it can answer at all. Not a table
/// anyone would publish -- that is the point: this is the manufactured extreme
/// that pins a choice ordinary tables cannot distinguish, the way
/// `rational_tests.cpp` pins `IntMin`.
inline constexpr BreakpointTable<2> WideValueRange { breakpoint(0), breakpoint(10) };

/// The opposite shape: keys so coarse, and a probe so fine, that the weight
/// `offset / span` cannot cancel. See `SteepKeyRange`'s test for what it
/// records.
inline constexpr BreakpointTable<2> SteepKeyRange { breakpoint(0), breakpoint(4000000000) };

/// A quarter of the way along, against a value whose scale leaves no room for
/// the product. See the overflow test for what makes this one different from
/// the two above.
inline constexpr BreakpointTable<2> UnrepresentableAnswer { breakpoint(0), breakpoint(4) };

/// `2^62`, an ordinary representable `Rational`, used as a row value where the
/// scale rather than the arithmetic is the point.
constexpr std::int64_t Huge = std::int64_t { 1 } << 62;
} // namespace

TEST_CASE("an unevenly spaced curve interpolates against the bracketing pair, not the first one", "[lookup]")
{
    // Segments of 1.93 cm and 2.94 cm, and no key equal to its own row index.
    // 29.1 mm == 2.91 cm is a THIRD of the way along the 1.93..4.87 cm segment
    // -- neither a breakpoint nor a midpoint -- so
    // 97.1 % + (1/3)(127.7 - 97.1) % = 107.3 %.
    //
    // Measured: against an evenly spaced table whose keys are 0, 1, 2, 3, two
    // defects pass the whole suite. Taking the span from the table's FIRST pair
    // is indistinguishable from taking it from the bracketing pair when every
    // segment is the same width; reading the lower row's key as its ROW INDEX
    // is indistinguishable from reading its key when the keys are 0, 1, 2, 3.
    // Here the first-pair span is 1.93 where the real one is 2.94, and the
    // lower row's index is 1 where its key is 1.93, so they answer about
    // 112.6 % and 117.0 % instead of 107.3 %.
    constexpr auto node = interpolating_lookup<unit::Centimetre, UnevenPoints, unit::Percent>(
        var<Diameter>, { rat(827, 10), rat(971, 10), rat(1277, 10) });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(291, 10));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(1073, 1000));

    // The first segment of the same table, so an implementation that got the
    // uneven case right only by reaching for a fixed second pair is caught too.
    // 9.65 mm == 0.965 cm is halfway along the 0..1.93 cm segment:
    // 82.7 % + (1/2)(97.1 - 82.7) % = 89.9 %.
    constexpr auto inTheFirstSegment = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(193, 20));
    STATIC_REQUIRE(inTheFirstSegment.has_value());
    STATIC_REQUIRE(inTheFirstSegment->measurement().value() == rat(899, 1000));
}

TEST_CASE("the interpolation divides the span out before multiplying the rise in", "[lookup]")
{
    // The order is a real choice with observable consequences, and until this
    // test existed nothing pinned it: the entire suite compiled unchanged under
    // either order.
    //
    // Keys on a common grid (0 and 10) and a value at the top of `Rational`'s
    // range. Dividing first cancels `5/10` to the weight `1/2` before the value
    // is ever touched, and answers 2^61 exactly. Multiplying first forms
    // `5 * 2^62`, the one product that mixes key magnitude with value
    // magnitude, and reports Overflow -- for a table whose exact answer is a
    // plain integer.
    //
    // Common-grid keys are what published curves actually have, which is why
    // this direction was chosen. The other direction exists and is asserted in
    // the test just below.
    constexpr auto node =
        interpolating_lookup<unit::Millimetre, WideValueRange, unit::One>(var<Diameter>, { rat(0), rat(Huge) });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(5));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(Huge / 2));
}

TEST_CASE("dividing the span out first is a trade-off, and this is the table it loses on", "[lookup]")
{
    // The honest other half of the test above: neither order dominates, and a
    // comment saying so is worth less than a table saying so.
    //
    // Keys 0 and 4e9 with a probe at 1/4e9 mm: the weight `(1/4e9) / 4e9`
    // cannot cancel, and forming it overflows -- where multiplying first would
    // have cancelled the offset against the rise and answered 1/4e9 exactly.
    // The library refuses rather than approximating, which is the property that
    // matters; that it refuses here at all is the price of the order chosen
    // above.
    //
    // If this assertion ever starts failing because the answer came back, that
    // is not a regression -- it means the order changed, and the test above is
    // where to look.
    constexpr auto node = interpolating_lookup<unit::Millimetre, SteepKeyRange, unit::One>(
        var<Diameter>, { rat(0), rat(4000000000) });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(1, 4000000000));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::Overflow);
}

TEST_CASE("an interpolation whose exact answer is not representable is reported, never rounded", "[lookup]")
{
    // The claim this whole table kind rests on, asserted rather than reasoned
    // about: where the exact rational the two rows imply does not exist inside
    // `Rational`, the library says so and hands back nothing.
    //
    // Keys 0 and 4, values 0 and 2^62 - 1 (odd, so nothing cancels), probed at
    // 3. The exact answer is 3(2^62 - 1)/4, whose reduced numerator is
    // 13835058055282163709 -- above `Rational`'s maximum, so the answer is not
    // merely awkward to reach, it does not exist. A representation that rounded
    // would hand back something near it and say nothing; this reports
    // `Overflow`, which is the only honest answer.
    //
    // `Overflow`, not `DomainError`: the value is inside the table's domain and
    // was found. Confusing the two would misreport an arithmetic limit as a
    // curve that does not cover the specimen. Both orders of the interpolation
    // overflow here, so this test says nothing about that choice -- deliberately.
    constexpr auto node = interpolating_lookup<unit::Millimetre, UnrepresentableAnswer, unit::One>(
        var<Diameter>, { rat(0), rat(Huge - 1) });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(3));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::Overflow);
}

TEST_CASE("a row hit can still overflow in the result-unit conversion, and says so", "[lookup]")
{
    // The half of the overflow story that is NOT about interpolation, pinned
    // because the header used to claim it could not happen.
    //
    // 0 mm sits exactly on the first row, so the interpolation performs no
    // arithmetic at all and cannot overflow. The value is then converted out of
    // the node's result unit (kilometres) into the coherent SI unit (metres) --
    // and 2^62 km is a perfectly representable `Rational` that does not survive
    // being multiplied by 1000.
    //
    // This path is shared with the banded and the exact lookup, which convert
    // their selected row the same way for the same reason; nothing about it is
    // particular to interpolation. It is asserted here because this is the file
    // where the claim was made.
    constexpr auto node = interpolating_lookup<unit::Centimetre, TwoPoints, unit::Kilometre>(
        var<Diameter>, { rat(Huge), rat(1) });
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(node, millimetresOfDiameter(0));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::Overflow);
}

TEST_CASE("breakpoint: a key given as an exact number", "[lookup]")
{
    using namespace formula::literals;
    STATIC_REQUIRE(formula::breakpoint(12.7_r) == formula::breakpoint(127, 10));
    STATIC_REQUIRE(formula::breakpoint(127) == formula::Breakpoint { 127, 1 }); // the integer overload still wins
}

TEST_CASE("breakpoint: integer keys still take the integer overload", "[lookup]")
{
    STATIC_REQUIRE(formula::breakpoint(127) == formula::Breakpoint { 127, 1 });
    STATIC_REQUIRE(formula::breakpoint(127, 10) == formula::Breakpoint { 127, 10 });
    STATIC_REQUIRE(formula::breakpoint(std::int64_t { 3 }, std::int64_t { 4 }) == formula::Breakpoint { 3, 4 });
}
