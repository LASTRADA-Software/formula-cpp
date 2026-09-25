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
