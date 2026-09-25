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
/// other node (D5).
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
/// unit) rather than an identity one. In millimetres these are [0,10),
/// [10,20) and [20,30): a first, a middle and a last band, as task 1's
/// review requires (a defect that only shows up in the middle is strictly
/// stronger than one visible at either end).
inline constexpr BandTable<3> SizeBands {
    band(0, 1, 1, 1), // [0, 1) cm  == [0, 10) mm
    band(1, 1, 2, 1), // [1, 2) cm  == [10, 20) mm
    band(2, 1, 3, 1), // [2, 3) cm  == [20, 30) mm
};

/// The corrections each band selects, declared in **percent** -- again
/// deliberately not `SizeCorrection`'s own unit (`One`) -- so the result side
/// of the table is converted too, not merely passed through.
[[nodiscard]] constexpr auto lookup()
{
    return banded_lookup<unit::Centimetre, SizeBands, unit::Percent>(var<Diameter>, { rat(95), rat(100), rat(105) });
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
    // 5 mm == 0.5 cm, inside [0, 1) cm.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(5));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(95, 100));
}

TEST_CASE("a value in a middle band selects that band's correction", "[lookup]")
{
    // 15 mm == 1.5 cm, inside [1, 2) cm -- not the first band and not the
    // last, the position task 1's review established as strictly stronger.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(15));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(1));
}

TEST_CASE("a value in the last band selects that band's correction", "[lookup]")
{
    // 25 mm == 2.5 cm, inside [2, 3) cm.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(25));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(105, 100));
}

TEST_CASE("a value exactly on a shared boundary belongs to the higher band -- half-open", "[lookup]")
{
    // 10 mm == 1 cm exactly: the shared boundary between [0,1) and [1,2).
    // Half-open means this belongs to [1,2), the SAME band as 15 mm above --
    // an implementation that instead closed the LOWER band (`<=` on the high
    // bound) would silently report the first band's correction here instead.
    constexpr auto atTenMillimetres = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(10));
    STATIC_REQUIRE(atTenMillimetres.has_value());
    STATIC_REQUIRE(atTenMillimetres->is_value());
    STATIC_REQUIRE(atTenMillimetres->measurement().value() == rat(1));

    // 20 mm == 2 cm exactly: the shared boundary between [1,2) and [2,3).
    constexpr auto atTwentyMillimetres = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(20));
    STATIC_REQUIRE(atTwentyMillimetres.has_value());
    STATIC_REQUIRE(atTwentyMillimetres->is_value());
    STATIC_REQUIRE(atTwentyMillimetres->measurement().value() == rat(105, 100));
}

TEST_CASE("exactly the table's own lowest bound is a hit -- the low bound is inclusive", "[lookup]")
{
    // 0 mm == 0 cm: SizeBands[0]'s own low bound, with no neighbouring band
    // below it to share the boundary with. [0, 1) includes it. Distinct from
    // the shared-boundary test above: that one exercises an *internal*
    // boundary two bands agree on, this one the table's own outer edge,
    // which a half-open mutation confined to just that edge would not
    // otherwise be caught by.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(0));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(95, 100));
}

TEST_CASE("exactly the table's own highest bound is a miss -- the high bound is exclusive", "[lookup]")
{
    // 30 mm == 3 cm: SizeBands[2]'s own high bound, with no neighbouring band
    // above it. [2, 3) excludes it, and there is no band above, so this is a
    // miss -- not the last band's value. This is also the concrete proof
    // behind band.hpp's own instruction to authors: a table whose last row
    // means "up to and including the maximum" must state its high bound as
    // the next tick past it, and that instruction is only trustworthy if the
    // library actually excludes the top bound.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(30));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a value below the lowest band is reported as a miss, not a value", "[lookup]")
{
    // -5 mm == -0.5 cm, below SizeBands[0]'s low bound (0). A lazy
    // implementation that clamped to the nearest band would answer with the
    // first band's correction (95/100) here instead of missing -- see the
    // mutation in this task's report.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(-5));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a value above the highest band is reported as a miss, not a value", "[lookup]")
{
    // 35 mm == 3.5 cm, above SizeBands[2]'s high bound (3). A lazy
    // implementation that clamped to the nearest band would answer with the
    // last band's correction (105/100) here instead of missing.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(35));
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
    // D5: a lookup produces a quantity, so it stands where a number stands
    // and combines with `*` the same way a `ConstantNode` would -- no
    // separate entry point, unlike `Constraint`.
    constexpr auto corrected = var<NominalSize> * lookup();
    STATIC_REQUIRE(decltype(corrected)::dimension == formula::Describe<CorrectedSize>::dimension);

    constexpr auto environment =
        formula::environment(formula::Measured<Diameter> { rat(15) }, formula::Measured<NominalSize> { rat(200) });
    // 15 mm diameter selects the middle band's correction, 1 (dimensionless);
    // 200 mm nominal size times 1 is 200 mm.
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(corrected, environment);
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(200));
}

TEST_CASE("a miss propagates through composition too, not only when the lookup is the whole formula",
          "[lookup]")
{
    constexpr auto corrected = var<NominalSize> * lookup();
    constexpr auto environment =
        formula::environment(formula::Measured<Diameter> { rat(-5) }, formula::Measured<NominalSize> { rat(200) });
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
    // same_as<Rational>: same_as would silently stop `{1, 1, 1}` -- a
    // correction table whose rows are all unity, the common case -- from
    // compiling, and every other Rational-typed parameter in this library
    // already accepts an integer literal. Kept as its own test so nobody
    // re-tightens this constraint later without noticing.
    constexpr auto node = banded_lookup<unit::Centimetre, SizeBands, unit::One>(var<Diameter>, { 1, 1, 1 });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(5));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(1));
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
/// `SpecimenShape::Cylindr` is then a compile error from the language itself
/// rather than a silent runtime miss.
///
/// The underlying type is fixed deliberately. Without it a scoped
/// enumeration's value range is only as wide as its enumerators require, so
/// `static_cast<SpecimenShape>(99)` -- which the "not an enumerator at all"
/// test below needs -- would be undefined behaviour and, in a constant
/// expression, a compile error. Fixing the underlying type makes every
/// `std::uint8_t` value a well-defined `SpecimenShape`, which is also what a
/// key deserialised from stored data really is.
enum class SpecimenShape : std::uint8_t
{
    Cube100,
    Cube150,
    CylinderShort,
    CylinderTall,
    Prism,
};

/// Four of the five shapes. `Prism` is deliberately left out: it is a key
/// that exists in the enumeration and not in this table, which is the
/// realistic "absent key" -- an enumeration the method grew and a registered
/// table that did not -- rather than a value nobody could have written.
inline constexpr KeyTable<SpecimenShape, 4> ShapeKeys {
    SpecimenShape::Cube100,
    SpecimenShape::Cube150,
    SpecimenShape::CylinderShort,
    SpecimenShape::CylinderTall,
};

/// The correction each shape selects, declared in **percent** -- deliberately
/// not `SizeCorrection`'s own unit (`One`) -- so the result side of the table
/// is converted rather than merely passed through, exactly as the banded
/// table above does it.
[[nodiscard]] constexpr auto shapeLookup(SpecimenShape shape)
{
    return exact_lookup<ShapeKeys, unit::Percent>(shape, { rat(106), rat(100), rat(97), rat(92) });
}

/// An always-empty table: the exact-lookup analogue of `EmptyTable` above,
/// valid for the same reason and always missing for the same reason.
inline constexpr KeyTable<SpecimenShape, 0> NoShapes {};

/// A one-row table: the other degenerate shape, where the only row is
/// simultaneously the first and the last.
inline constexpr KeyTable<SpecimenShape, 1> OnlyPrism { SpecimenShape::Prism };
} // namespace

TEST_CASE("an exact lookup is a Node and produces the result quantity's own dimension", "[lookup]")
{
    STATIC_REQUIRE(formula::Node<decltype(shapeLookup(SpecimenShape::Cube100))>);
    STATIC_REQUIRE(decltype(shapeLookup(SpecimenShape::Cube100))::dimension
                   == formula::Describe<SizeCorrection>::dimension);
}

TEST_CASE("the first key in an exact table selects its own row", "[lookup]")
{
    // The table's own first row, which a scan that skipped index 0 -- or
    // started at 1 to "skip the header" -- would miss while every interior
    // row still answered correctly.
    constexpr auto computed =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenShape::Cube100), formula::environment());
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(106, 100));
}

TEST_CASE("a middle key in an exact table selects its own row", "[lookup]")
{
    // Neither first nor last: the position task 1's review established as
    // strictly stronger, here on the hit side.
    constexpr auto computed =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenShape::CylinderShort), formula::environment());
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(97, 100));

    // The other interior row, so that a scan returning a fixed interior index
    // cannot pass this test case by accident.
    constexpr auto second =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenShape::Cube150), formula::environment());
    STATIC_REQUIRE(second.has_value());
    STATIC_REQUIRE(second->is_value());
    STATIC_REQUIRE(second->measurement().value() == rat(1));
}

TEST_CASE("the last key in an exact table selects its own row", "[lookup]")
{
    // The table's own last row: a scan whose bound was `index + 1 < size`
    // would miss exactly this one and nothing else.
    constexpr auto computed =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenShape::CylinderTall), formula::environment());
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(92, 100));
}

TEST_CASE("a key the table does not declare is reported as a miss, not a value", "[lookup]")
{
    // `Prism` is a perfectly good `SpecimenShape` that this table has no row
    // for. An implementation that fell back to the first row would answer
    // 106/100 here; one that fell back to a default would answer 0. Both are
    // lies, and the honest answer is that nothing was found.
    constexpr auto computed =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenShape::Prism), formula::environment());
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
        formula::checked_evaluate<SizeCorrection>(shapeLookup(static_cast<SpecimenShape>(99)), formula::environment());
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an exact miss and a banded miss are the same failure, reported the same way", "[lookup]")
{
    // The one test that pins this phase's stated drift risk directly. Two
    // different tables, two different reasons nothing was found, and exactly
    // one vocabulary for "found nothing" -- so a later change that gives
    // either kind its own spelling fails here rather than in a consumer.
    constexpr auto bandedMiss = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(35));
    constexpr auto exactMiss =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenShape::Prism), formula::environment());

    STATIC_REQUIRE(!bandedMiss.has_value());
    STATIC_REQUIRE(!exactMiss.has_value());
    STATIC_REQUIRE(bandedMiss.error() == exactMiss.error());
    STATIC_REQUIRE(exactMiss.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an empty exact table always misses -- the same not-a-value outcome, not a special case", "[lookup]")
{
    constexpr auto node = exact_lookup<NoShapes, unit::One>(SpecimenShape::Cube100, {});
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, formula::environment());
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a one-row exact table hits its only key and misses every other", "[lookup]")
{
    // The degenerate table where the only row is both the first and the last,
    // so a bound that is off by one in either direction shows up here.
    constexpr auto hit = formula::checked_evaluate<SizeCorrection>(
        exact_lookup<OnlyPrism, unit::One>(SpecimenShape::Prism, { rat(3, 4) }), formula::environment());
    STATIC_REQUIRE(hit.has_value());
    STATIC_REQUIRE(hit->is_value());
    STATIC_REQUIRE(hit->measurement().value() == rat(3, 4));

    constexpr auto miss = formula::checked_evaluate<SizeCorrection>(
        exact_lookup<OnlyPrism, unit::One>(SpecimenShape::Cube100, { rat(3, 4) }), formula::environment());
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
    constexpr auto node =
        exact_lookup<ShapeKeys, unit::One>(SpecimenShape::CylinderShort, { rat(1), rat(1), rat(0), rat(1) });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, formula::environment());
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(0));
}

TEST_CASE("an exact lookup composes with other nodes, exactly like any other Node", "[lookup]")
{
    // D5 again, for the second table kind: a lookup produces a quantity, so
    // it stands where a number stands and combines with `*` the same way a
    // `ConstantNode` would.
    constexpr auto corrected = var<NominalSize> * shapeLookup(SpecimenShape::Cube150);
    STATIC_REQUIRE(decltype(corrected)::dimension == formula::Describe<CorrectedSize>::dimension);

    constexpr auto environment = formula::environment(formula::Measured<NominalSize> { rat(200) });
    // Cube150 selects 100 % == 1, so 200 mm times 1 is 200 mm.
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(corrected, environment);
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(200));
}

TEST_CASE("an exact miss propagates through composition too, not only when the lookup is the whole formula",
          "[lookup]")
{
    constexpr auto corrected = var<NominalSize> * shapeLookup(SpecimenShape::Prism);
    constexpr auto environment = formula::environment(formula::Measured<NominalSize> { rat(200) });
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
    constexpr auto corrected = var<NominalSize> * shapeLookup(SpecimenShape::Cube150);
    constexpr auto environment = formula::environment(formula::Measured<NominalSize>::absent());
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(corrected, environment);
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_empty());
}

TEST_CASE("an integer literal is accepted as an exact lookup's correction, not only a Rational", "[lookup]")
{
    // The same guard the banded case carries, for the same reason: both
    // factories take the same `Corrections<N>`, whose element constraint is
    // convertible_to<Rational> rather than same_as<Rational>, and a table
    // whose rows are all unity is the common case.
    constexpr auto node = exact_lookup<ShapeKeys, unit::One>(SpecimenShape::Cube100, { 1, 1, 1, 1 });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, formula::environment());
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(1));
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
    constexpr KeyTable<SpecimenShape, 5> Repeated {
        SpecimenShape::Cube100,
        SpecimenShape::Cube150,
        SpecimenShape::CylinderShort,
        SpecimenShape::Cube150, // already declared two rows above
        SpecimenShape::Prism,
    };
    STATIC_REQUIRE(!formula::key_table_is_well_formed(Repeated));

    // The table's own FINAL pair. `Repeated` above cannot catch a sweep whose
    // outer bound stops one row early -- its duplicate is at rows 1 and 3 of
    // 5, so rows 3 and 4 never need to be compared for it to be found.
    // Measured: widening the outer bound from `first + 1 < N` to
    // `first + 2 < N` leaves every other assertion in this file green.
    constexpr KeyTable<SpecimenShape, 3> RepeatedAtTheEnd {
        SpecimenShape::Cube100,
        SpecimenShape::Cube150,
        SpecimenShape::Cube150, // the last pair, and nothing after it
    };
    STATIC_REQUIRE(!formula::key_table_is_well_formed(RepeatedAtTheEnd));

    // TWO rows -- the smallest table that can be malformed at all, and the
    // boundary between "no pair to compare" (a one-row table, above) and "one
    // pair to compare". Both directions, so a predicate that answered `false`
    // for every two-row table would fail here too rather than look correct.
    constexpr KeyTable<SpecimenShape, 2> TwoRowsRepeated { SpecimenShape::Prism, SpecimenShape::Prism };
    constexpr KeyTable<SpecimenShape, 2> TwoRowsDistinct { SpecimenShape::Prism, SpecimenShape::Cube100 };
    STATIC_REQUIRE(!formula::key_table_is_well_formed(TwoRowsRepeated));
    STATIC_REQUIRE(formula::key_table_is_well_formed(TwoRowsDistinct));
}

TEST_CASE("a two-row exact table selects each of its rows and misses everything else", "[lookup]")
{
    // The compile-time side of the same boundary the predicate test covers
    // just above: two rows is the smallest table with a pair, and the phase
    // otherwise only ever uses 0, 1, 4 and 5. Both rows, so neither a
    // first-only nor a last-only scan passes.
    constexpr KeyTable<SpecimenShape, 2> TwoShapes { SpecimenShape::Cube100, SpecimenShape::Prism };

    constexpr auto first = formula::checked_evaluate<SizeCorrection>(
        exact_lookup<TwoShapes, unit::One>(SpecimenShape::Cube100, { rat(1, 2), rat(1, 4) }),
        formula::environment());
    STATIC_REQUIRE(first.has_value());
    STATIC_REQUIRE(first->is_value());
    STATIC_REQUIRE(first->measurement().value() == rat(1, 2));

    constexpr auto second = formula::checked_evaluate<SizeCorrection>(
        exact_lookup<TwoShapes, unit::One>(SpecimenShape::Prism, { rat(1, 2), rat(1, 4) }), formula::environment());
    STATIC_REQUIRE(second.has_value());
    STATIC_REQUIRE(second->is_value());
    STATIC_REQUIRE(second->measurement().value() == rat(1, 4));

    constexpr auto absent = formula::checked_evaluate<SizeCorrection>(
        exact_lookup<TwoShapes, unit::One>(SpecimenShape::Cube150, { rat(1, 2), rat(1, 4) }),
        formula::environment());
    STATIC_REQUIRE(!absent.has_value());
    STATIC_REQUIRE(absent.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("keys_match is exact equality, with no ordering and no conversion", "[lookup]")
{
    STATIC_REQUIRE(formula::keys_match(SpecimenShape::Cube150, SpecimenShape::Cube150));
    STATIC_REQUIRE(!formula::keys_match(SpecimenShape::Cube150, SpecimenShape::Cube100));
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
/// 10, 20 and 30: a first row, two middle rows and a last row.
inline constexpr BreakpointTable<4> CurvePoints {
    breakpoint(0),
    breakpoint(1),
    breakpoint(2),
    breakpoint(3),
};

/// The value stated at each breakpoint, in **percent** -- again deliberately
/// not `SizeCorrection`'s own unit (`One`) -- so the result side is converted
/// too. The middle segment descends (100 -> 96) on purpose: a slope this
/// library computes with `checked_sub` must be allowed to be negative, and an
/// implementation that took a magnitude somewhere would answer 102 % instead of
/// 98 % halfway along it.
[[nodiscard]] constexpr auto curve()
{
    return interpolating_lookup<unit::Centimetre, CurvePoints, unit::Percent>(
        var<Diameter>, { rat(90), rat(100), rat(96), rat(120) });
}

/// An always-empty curve: the interpolating analogue of `EmptyTable` and
/// `NoShapes`, valid for the same reason and always missing for the same one.
inline constexpr BreakpointTable<0> NoPoints {};

/// A one-row curve. There is nothing to interpolate *between*, and the table is
/// still well-formed: it states a value at exactly one key and says nothing
/// about any other, which is a thing this library can report honestly.
inline constexpr BreakpointTable<1> OnePoint { breakpoint(1) };

/// Two rows -- the smallest curve that can interpolate at all, and the smallest
/// that can be malformed at all.
inline constexpr BreakpointTable<2> TwoPoints { breakpoint(0), breakpoint(2) };
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
    STATIC_REQUIRE(computed->measurement().value() == rat(90, 100));
}

TEST_CASE("a value exactly on a middle row returns that row, not an interpolation", "[lookup]")
{
    // 10 mm == 1 cm, `CurvePoints[1]`: interior, so both neighbouring segments
    // exist and either could have been used by mistake.
    constexpr auto atOneCentimetre = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(10));
    STATIC_REQUIRE(atOneCentimetre.has_value());
    STATIC_REQUIRE(atOneCentimetre->is_value());
    STATIC_REQUIRE(atOneCentimetre->measurement().value() == rat(1));

    // 20 mm == 2 cm, `CurvePoints[2]` -- the other interior row, so a scan that
    // answered a fixed interior index cannot pass this case by accident.
    constexpr auto atTwoCentimetres = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(20));
    STATIC_REQUIRE(atTwoCentimetres.has_value());
    STATIC_REQUIRE(atTwoCentimetres->is_value());
    STATIC_REQUIRE(atTwoCentimetres->measurement().value() == rat(96, 100));
}

TEST_CASE("a value exactly on the table's last row returns that row -- the domain is closed at the top",
          "[lookup]")
{
    // 30 mm == 3 cm: `CurvePoints[3]` itself, the table's own highest
    // breakpoint. THIS is the case the exactly-on-a-row rule is actually
    // observable in: everywhere else, interpolating across the segment a row
    // begins would return that row's own value anyway, because the weight is
    // exactly zero. Here there is no segment above, so an implementation whose
    // segments were half-open at the top -- the band table's rule, copied
    // across where it does not belong -- would report a miss for the table's
    // own last row.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(30));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(120, 100));
}

TEST_CASE("a value between the first two rows is interpolated", "[lookup]")
{
    // 5 mm == 0.5 cm, halfway along [0, 1] cm: 90 % + (1/2)(100 - 90) % = 95 %.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(5));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(95, 100));
}

TEST_CASE("a value between two middle rows is interpolated, and a descending segment descends", "[lookup]")
{
    // 12.5 mm == 1.25 cm, a QUARTER of the way along [1, 2] cm, whose value
    // FALLS from 100 % to 96 %: 100 % + (1/4)(96 - 100) % = 99 %. An
    // implementation that took the magnitude of the slope, or subtracted in the
    // wrong order, answers 101 % here and still answers correctly on every
    // ascending segment.
    //
    // A quarter rather than the midpoint, deliberately. Measured: pairing each
    // row's VALUE with the other row's KEY -- an ordinary off-by-one in the
    // scan -- is invisible at the midpoint of any segment, because the mirror
    // of a line about its own midpoint passes through the same point there.
    // Every halfway assertion in this file is blind to it; this one is not.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(25, 2));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(99, 100));
}

TEST_CASE("a value between the last two rows is interpolated", "[lookup]")
{
    // 27.5 mm == 2.75 cm, three quarters of the way along [2, 3] cm:
    // 96 % + (3/4)(120 - 96) % = 114 %. Off the midpoint for the reason the
    // middle-segment case above gives, and on the far side of it, so the two
    // asymmetric cases do not share a weight either.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(55, 2));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(114, 100));
}

TEST_CASE("an interpolated value stays exact, even when no finite decimal could hold it", "[lookup]")
{
    // 10/3 mm == 1/3 cm, one third of the way along [0, 1] cm:
    // 90 % + (1/3)(100 - 90) % = 280/3 %, which is 14/15 once the percent is
    // converted away -- 0.9333... in decimal, a number no rounding of any
    // fixed precision holds exactly.
    //
    // This is this task's exactness question, asserted rather than asserted
    // about: the answer is the exact rational the two rows imply, and nothing
    // anywhere rounded it to get there. The denominator is checked as well as
    // the value, because an implementation that computed in a fixed decimal
    // precision could still compare equal to a rounded literal while having
    // thrown the remainder away.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(10, 3));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(14, 15));
    STATIC_REQUIRE(computed->measurement().value().denominator() == 15);
}

TEST_CASE("a value below the table's first row is a miss -- interpolation does not extrapolate", "[lookup]")
{
    // -5 mm == -0.5 cm, below `CurvePoints[0]`. An implementation that ran the
    // first segment's slope backwards would answer 85 % here, confidently, for
    // an input the table never defined -- which is the one thing this phase
    // refuses everywhere. A miss, not a value.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(-5));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("a value above the table's last row is a miss -- interpolation does not extrapolate", "[lookup]")
{
    // 35 mm == 3.5 cm, above `CurvePoints[3]`. Running the last segment's
    // slope onwards would answer 132 %; clamping to the last row would answer
    // 120 %. Both are values the published table never stated, invented from
    // where the table happened to stop.
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(35));
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an interpolating table's own last row is a hit where a banded table's own top bound is a miss",
          "[lookup]")
{
    // The two table kinds disagree about their own top end, deliberately, and
    // this is the test that keeps the disagreement intentional rather than
    // accidental. A band's high bound is EXCLUSIVE, so 30 mm falls off the top
    // of `SizeBands` and misses; an interpolating table's last breakpoint is a
    // ROW, so 30 mm hits it exactly. Harmonising the two -- in either
    // direction -- fails here rather than in a consumer.
    constexpr auto banded = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(30));
    constexpr auto interpolated = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(30));

    STATIC_REQUIRE(!banded.has_value());
    STATIC_REQUIRE(banded.error() == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(interpolated.has_value());
    STATIC_REQUIRE(interpolated->is_value());
    STATIC_REQUIRE(interpolated->measurement().value() == rat(120, 100));
}

TEST_CASE("an interpolated value of exactly zero is a value, never a miss", "[lookup]")
{
    // The hit side of the distinction the whole file is built on, in the one
    // form only this table kind has: zero here is not a row the author typed,
    // it is a number the interpolation PRODUCED, so an implementation that
    // treated a zero result as "nothing found" would fail only here.
    constexpr BreakpointTable<2> Crossing { breakpoint(0), breakpoint(2) };
    constexpr auto node =
        interpolating_lookup<unit::Centimetre, Crossing, unit::One>(var<Diameter>, { rat(-1), rat(1) });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(10));
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
        interpolating_lookup<unit::Centimetre, TwoPoints, unit::One>(var<Diameter>, { rat(1, 4), rat(3, 4) });

    constexpr auto atFirst = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(0));
    STATIC_REQUIRE(atFirst.has_value());
    STATIC_REQUIRE(atFirst->measurement().value() == rat(1, 4));

    constexpr auto between = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(10));
    STATIC_REQUIRE(between.has_value());
    STATIC_REQUIRE(between->measurement().value() == rat(1, 2));

    constexpr auto atLast = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(20));
    STATIC_REQUIRE(atLast.has_value());
    STATIC_REQUIRE(atLast->measurement().value() == rat(3, 4));

    constexpr auto below = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(-1));
    STATIC_REQUIRE(!below.has_value());
    STATIC_REQUIRE(below.error() == formula::ArithmeticError::DomainError);

    constexpr auto above = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(21));
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
    constexpr auto node = interpolating_lookup<unit::Centimetre, OnePoint, unit::One>(var<Diameter>, { rat(3, 4) });

    constexpr auto onTheRow = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(10));
    STATIC_REQUIRE(onTheRow.has_value());
    STATIC_REQUIRE(onTheRow->is_value());
    STATIC_REQUIRE(onTheRow->measurement().value() == rat(3, 4));

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
    // D5 for the third table kind.
    constexpr auto corrected = var<NominalSize> * curve();
    STATIC_REQUIRE(decltype(corrected)::dimension == formula::Describe<CorrectedSize>::dimension);

    constexpr auto environment =
        formula::environment(formula::Measured<Diameter> { rat(15) }, formula::Measured<NominalSize> { rat(200) });
    // 15 mm interpolates to 98 % == 49/50, and 200 mm times 49/50 is 196 mm.
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(corrected, environment);
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(196));
}

TEST_CASE("an interpolating miss propagates through composition too", "[lookup]")
{
    constexpr auto corrected = var<NominalSize> * curve();
    constexpr auto environment =
        formula::environment(formula::Measured<Diameter> { rat(35) }, formula::Measured<NominalSize> { rat(200) });
    constexpr auto computed = formula::checked_evaluate<CorrectedSize>(corrected, environment);
    STATIC_REQUIRE(!computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("an integer literal is accepted as an interpolating lookup's row value, not only a Rational",
          "[lookup]")
{
    // The same guard both other kinds carry, for the same reason: all three
    // factories take the same `Corrections<N>`, whose element constraint is
    // convertible_to<Rational> rather than same_as<Rational>.
    constexpr auto node =
        interpolating_lookup<unit::Centimetre, CurvePoints, unit::One>(var<Diameter>, { 1, 1, 1, 1 });
    constexpr auto computed = formula::checked_evaluate<SizeCorrection>(node, millimetresOfDiameter(5));
    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(1));
}

TEST_CASE("breakpoints_ascend is strict, and refuses a bound that is not a rational number", "[lookup]")
{
    STATIC_REQUIRE(formula::breakpoints_ascend(breakpoint(1), breakpoint(2)));
    // Equal is NOT ascending: two rows at one key state two values there and
    // leave a segment of zero width to divide by.
    STATIC_REQUIRE(!formula::breakpoints_ascend(breakpoint(2), breakpoint(2)));
    STATIC_REQUIRE(!formula::breakpoints_ascend(breakpoint(3), breakpoint(2)));
    // Compared as rationals, not as numerator/denominator pairs: 1/2 < 2/3.
    STATIC_REQUIRE(formula::breakpoints_ascend(breakpoint(1, 2), breakpoint(2, 3)));
    // A bound that is not a representable rational is not ascending with
    // anything, in either position -- the same judgement `bands_are_adjacent`
    // makes, so a malformed row is never waved through.
    STATIC_REQUIRE(!formula::breakpoints_ascend(breakpoint(1, 0), breakpoint(2)));
    STATIC_REQUIRE(!formula::breakpoints_ascend(breakpoint(1), breakpoint(2, 0)));
}

TEST_CASE("breakpoint_is_well_formed asks only whether the row's key is a number at all", "[lookup]")
{
    STATIC_REQUIRE(formula::breakpoint_is_well_formed(breakpoint(0)));
    STATIC_REQUIRE(formula::breakpoint_is_well_formed(breakpoint(-7, 2)));
    STATIC_REQUIRE(!formula::breakpoint_is_well_formed(breakpoint(1, 0)));
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
        breakpoint(1),
        breakpoint(1), // the same key as the row above
        breakpoint(3),
    };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(RepeatedInTheMiddle));

    // Two middle rows in the wrong ORDER, which the same rule catches: 2 is
    // not strictly below 1.
    constexpr BreakpointTable<4> DescendingInTheMiddle {
        breakpoint(0),
        breakpoint(2),
        breakpoint(1),
        breakpoint(3),
    };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(DescendingInTheMiddle));

    // The table's own FINAL pair, which the middle cases above cannot catch: a
    // sweep whose bound stopped one pair early passes both of them.
    constexpr BreakpointTable<3> DescendingAtTheEnd { breakpoint(0), breakpoint(2), breakpoint(1) };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(DescendingAtTheEnd));

    // And the table's own FIRST pair, which a sweep starting at index 1 would
    // step over.
    constexpr BreakpointTable<3> DescendingAtTheStart { breakpoint(2), breakpoint(0), breakpoint(3) };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(DescendingAtTheStart));

    // Two rows: the smallest table that can be malformed at all, in both
    // directions, so a predicate answering `false` for every two-row table
    // would fail here rather than look correct.
    constexpr BreakpointTable<2> TwoRepeated { breakpoint(5), breakpoint(5) };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(TwoRepeated));
    STATIC_REQUIRE(formula::breakpoint_table_is_well_formed(TwoPoints));

    // A row whose key is not a rational number at all, in the middle, where the
    // ordering sweep alone would report it as merely out of order.
    constexpr BreakpointTable<3> MalformedInTheMiddle { breakpoint(0), breakpoint(1, 0), breakpoint(3) };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(MalformedInTheMiddle));

    // A SINGLE row whose key is not a rational number: there is no pair here,
    // so only the per-row half of well-formedness can catch it.
    constexpr BreakpointTable<1> LoneMalformed { breakpoint(1, 0) };
    STATIC_REQUIRE(!formula::breakpoint_table_is_well_formed(LoneMalformed));
}

TEST_CASE("all three lookup kinds report finding nothing the same way", "[lookup]")
{
    // The one test that pins this phase's stated drift risk across every table
    // kind at once. Three different tables, three different reasons nothing was
    // found, and exactly one vocabulary for "found nothing" -- so a later change
    // that gives any kind its own spelling fails here rather than in a consumer.
    constexpr auto bandedMiss = formula::checked_evaluate<SizeCorrection>(lookup(), millimetresOfDiameter(35));
    constexpr auto exactMiss =
        formula::checked_evaluate<SizeCorrection>(shapeLookup(SpecimenShape::Prism), formula::environment());
    constexpr auto interpolatingMiss = formula::checked_evaluate<SizeCorrection>(curve(), millimetresOfDiameter(35));

    STATIC_REQUIRE(!bandedMiss.has_value());
    STATIC_REQUIRE(!exactMiss.has_value());
    STATIC_REQUIRE(!interpolatingMiss.has_value());
    STATIC_REQUIRE(bandedMiss.error() == exactMiss.error());
    STATIC_REQUIRE(exactMiss.error() == interpolatingMiss.error());
    STATIC_REQUIRE(interpolatingMiss.error() == formula::ArithmeticError::DomainError);
}
