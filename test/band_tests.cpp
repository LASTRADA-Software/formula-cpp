// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/band.hpp>

#include <catch2/catch_test_macros.hpp>

using formula::Band;
using formula::BandTable;
using formula::band;
using formula::band_is_well_formed;
using formula::band_table_is_well_formed;
using formula::bands_are_adjacent;

namespace
{
    // ---- well-formed tables, of the shapes Step 1 asks for ----

    inline constexpr BandTable<3> WellFormedThreeBands {
        band(0, 1, 10, 1),
        band(10, 1, 20, 1),
        band(20, 1, 30, 1),
    };

    inline constexpr BandTable<1> SingleBand { band(0, 1, 10, 1) };

    inline constexpr BandTable<0> EmptyTable {};

    // ---- a gap or an overlap between the middle two bands, not the last two ----
    //
    // Four bands, three adjacent pairs: (0,1), (1,2), (2,3). The defect sits at
    // pair (1,2) -- the middle one -- leaving both the first and the last pair
    // individually well-formed. A validator that only checks the final
    // adjacency would pass either of these tables outright.

    inline constexpr BandTable<4> GapInMiddle {
        band(0, 1, 10, 1),
        band(10, 1, 20, 1),
        band(25, 1, 35, 1), // gap: band[1]'s high (20) != band[2]'s low (25)
        band(35, 1, 45, 1),
    };

    inline constexpr BandTable<4> OverlapInMiddle {
        band(0, 1, 10, 1),
        band(10, 1, 20, 1),
        band(18, 1, 28, 1), // overlap: band[2]'s low (18) < band[1]'s high (20)
        band(28, 1, 38, 1),
    };

    // ---- a gap or an overlap between the last two bands, for the same reason
    // the guide keeps a middle-position case: a validator with the opposite
    // blind spot (only ever checking the first pair) must not pass these
    // either.

    inline constexpr BandTable<3> GapAtEnd {
        band(0, 1, 10, 1),
        band(10, 1, 20, 1),
        band(25, 1, 35, 1), // gap: band[1]'s high (20) != band[2]'s low (25)
    };

    inline constexpr BandTable<3> OverlapAtEnd {
        band(0, 1, 10, 1),
        band(10, 1, 20, 1),
        band(18, 1, 28, 1), // overlap: band[2]'s low (18) < band[1]'s high (20)
    };

    // ---- an inverted band (its own low is not below its own high) -- fix
    // round 1's finding. Constructed so every adjacent pair still shares its
    // boundary exactly (`bands_are_adjacent` alone would pass every one of
    // these), isolating that only `band_is_well_formed` catches this defect.
    // One inversion per position -- first, middle, last -- for the same
    // reason gap and overlap each got a middle case and an end case: a check
    // exercised at only one position can silently be one that only works
    // there.

    inline constexpr BandTable<3> InvertedFirstBand {
        band(10, 1, 0, 1), // inverted: low (10) is not below high (0)
        band(0, 1, 20, 1),
        band(20, 1, 30, 1),
    };

    inline constexpr BandTable<3> InvertedMiddleBand {
        band(0, 1, 10, 1),
        band(10, 1, 5, 1), // inverted: low (10) is not below high (5)
        band(5, 1, 15, 1),
    };

    inline constexpr BandTable<3> InvertedLastBand {
        band(0, 1, 10, 1),
        band(10, 1, 20, 1),
        band(20, 1, 15, 1), // inverted: low (20) is not below high (15)
    };

    // ---- bands declared out of order, each individually well-formed --
    // caught by the EXISTING adjacency check, with no separate ordering
    // check needed (see band_table_is_well_formed's comment for the proof).

    inline constexpr BandTable<3> DeclaredOutOfOrder {
        band(20, 1, 30, 1), // well-formed on its own, but declared before band 0's range
        band(10, 1, 20, 1),
        band(0, 1, 10, 1),
    };
} // namespace

// ---- the two predicates, on a single band or a single pair ----

TEST_CASE("bands_are_adjacent is true exactly when the boundary is shared", "[band]")
{
    STATIC_REQUIRE(bands_are_adjacent(band(0, 1, 10, 1), band(10, 1, 20, 1)));
    // The same boundary, stated in different terms -- 10/1 and 20/2 are the
    // same rational -- must still compare adjacent.
    STATIC_REQUIRE(bands_are_adjacent(band(0, 1, 10, 1), band(20, 2, 20, 1)));
    STATIC_REQUIRE(!bands_are_adjacent(band(0, 1, 10, 1), band(25, 1, 35, 1))); // gap
    STATIC_REQUIRE(!bands_are_adjacent(band(0, 1, 20, 1), band(18, 1, 28, 1))); // overlap

    // A malformed bound (zero denominator) is not adjacent to anything --
    // waved through silently is exactly what this predicate must not do.
    STATIC_REQUIRE(!bands_are_adjacent(band(0, 1, 10, 0), band(10, 1, 20, 1)));
    STATIC_REQUIRE(!bands_are_adjacent(band(0, 1, 10, 1), band(10, 0, 20, 1)));
}

TEST_CASE("band_is_well_formed is true exactly when low is strictly below high", "[band]")
{
    STATIC_REQUIRE(band_is_well_formed(band(0, 1, 10, 1)));
    // The same value stated in different terms -- 0/1 and 0/5 are both zero.
    STATIC_REQUIRE(band_is_well_formed(band(0, 5, 10, 1)));
    STATIC_REQUIRE(!band_is_well_formed(band(10, 1, 0, 1))); // inverted
    STATIC_REQUIRE(!band_is_well_formed(band(10, 1, 10, 1))); // low == high: empty, not a band

    // A malformed bound (zero denominator) is not well-formed either --
    // waved through silently is exactly what this predicate must not do.
    STATIC_REQUIRE(!band_is_well_formed(band(0, 0, 10, 1)));
    STATIC_REQUIRE(!band_is_well_formed(band(0, 1, 10, 0)));
}

// ---- whole-table validation ----

TEST_CASE("a well-formed band table validates", "[band]")
{
    STATIC_REQUIRE(band_table_is_well_formed(WellFormedThreeBands));
    REQUIRE(band_table_is_well_formed(WellFormedThreeBands));
}

TEST_CASE("a single-band table validates", "[band]")
{
    STATIC_REQUIRE(band_table_is_well_formed(SingleBand));
}

TEST_CASE("an empty table validates -- it is always missing, not malformed", "[band]")
{
    STATIC_REQUIRE(band_table_is_well_formed(EmptyTable));
}

TEST_CASE("adjacent bands sharing an endpoint validate -- the half-open case working", "[band]")
{
    // WellFormedThreeBands' own boundaries (10 and 20) are exactly this case:
    // band[0]'s high is band[1]'s low, and band[1]'s high is band[2]'s low.
    STATIC_REQUIRE(bands_are_adjacent(WellFormedThreeBands[0], WellFormedThreeBands[1]));
    STATIC_REQUIRE(bands_are_adjacent(WellFormedThreeBands[1], WellFormedThreeBands[2]));
}

TEST_CASE("a gap between the middle two bands is caught, not only a gap at the end", "[band]")
{
    STATIC_REQUIRE(!band_table_is_well_formed(GapInMiddle));
    REQUIRE(!band_table_is_well_formed(GapInMiddle));

    // Pinned precisely: the defect is pair (1,2), and pairs (0,1) and (2,3)
    // either side of it are themselves fine -- a validator that stopped at
    // the first mismatch and a validator that only checked the last pair
    // would both be exercised differently by this, and both must still
    // report the table as a whole as invalid.
    STATIC_REQUIRE(bands_are_adjacent(GapInMiddle[0], GapInMiddle[1]));
    STATIC_REQUIRE(!bands_are_adjacent(GapInMiddle[1], GapInMiddle[2]));
    STATIC_REQUIRE(bands_are_adjacent(GapInMiddle[2], GapInMiddle[3]));
}

TEST_CASE("a gap between the last two bands is also caught", "[band]")
{
    STATIC_REQUIRE(!band_table_is_well_formed(GapAtEnd));
    REQUIRE(!band_table_is_well_formed(GapAtEnd));
}

TEST_CASE("an overlap between the middle two bands is caught, not only an overlap at the end", "[band]")
{
    STATIC_REQUIRE(!band_table_is_well_formed(OverlapInMiddle));
    REQUIRE(!band_table_is_well_formed(OverlapInMiddle));

    STATIC_REQUIRE(bands_are_adjacent(OverlapInMiddle[0], OverlapInMiddle[1]));
    STATIC_REQUIRE(!bands_are_adjacent(OverlapInMiddle[1], OverlapInMiddle[2]));
    STATIC_REQUIRE(bands_are_adjacent(OverlapInMiddle[2], OverlapInMiddle[3]));
}

TEST_CASE("an overlap between the last two bands is also caught", "[band]")
{
    STATIC_REQUIRE(!band_table_is_well_formed(OverlapAtEnd));
    REQUIRE(!band_table_is_well_formed(OverlapAtEnd));
}

TEST_CASE("an inverted band is caught regardless of its position in the table", "[band]")
{
    // Each of these three tables has every adjacent pair sharing its
    // boundary exactly -- bands_are_adjacent alone would pass all of them --
    // so only band_is_well_formed can be catching the defect below.
    STATIC_REQUIRE(bands_are_adjacent(InvertedFirstBand[0], InvertedFirstBand[1]));
    STATIC_REQUIRE(bands_are_adjacent(InvertedFirstBand[1], InvertedFirstBand[2]));
    STATIC_REQUIRE(!band_is_well_formed(InvertedFirstBand[0]));
    STATIC_REQUIRE(!band_table_is_well_formed(InvertedFirstBand));
    REQUIRE(!band_table_is_well_formed(InvertedFirstBand));

    STATIC_REQUIRE(bands_are_adjacent(InvertedMiddleBand[0], InvertedMiddleBand[1]));
    STATIC_REQUIRE(bands_are_adjacent(InvertedMiddleBand[1], InvertedMiddleBand[2]));
    STATIC_REQUIRE(!band_is_well_formed(InvertedMiddleBand[1]));
    STATIC_REQUIRE(!band_table_is_well_formed(InvertedMiddleBand));
    REQUIRE(!band_table_is_well_formed(InvertedMiddleBand));

    STATIC_REQUIRE(bands_are_adjacent(InvertedLastBand[0], InvertedLastBand[1]));
    STATIC_REQUIRE(bands_are_adjacent(InvertedLastBand[1], InvertedLastBand[2]));
    STATIC_REQUIRE(!band_is_well_formed(InvertedLastBand[2]));
    STATIC_REQUIRE(!band_table_is_well_formed(InvertedLastBand));
    REQUIRE(!band_table_is_well_formed(InvertedLastBand));
}

TEST_CASE("bands declared out of order are caught by the existing adjacency check", "[band]")
{
    // Every band here is individually well-formed -- this is not an
    // inverted-band defect -- but declaring them out of order breaks
    // adjacency between every consecutive pair, which is what actually
    // catches it. No separate ordering check exists or is needed; see
    // band_table_is_well_formed's comment for the proof.
    STATIC_REQUIRE(band_is_well_formed(DeclaredOutOfOrder[0]));
    STATIC_REQUIRE(band_is_well_formed(DeclaredOutOfOrder[1]));
    STATIC_REQUIRE(band_is_well_formed(DeclaredOutOfOrder[2]));
    STATIC_REQUIRE(!bands_are_adjacent(DeclaredOutOfOrder[0], DeclaredOutOfOrder[1]));
    STATIC_REQUIRE(!band_table_is_well_formed(DeclaredOutOfOrder));
}

TEST_CASE("a well-formed, adjacent table has its bands in ascending order -- implied, not "
          "separately checked",
          "[band]")
{
    // The proof (see band_table_is_well_formed's comment): well-formedness
    // gives low_i < high_i, adjacency gives high_i == low_(i+1), and chaining
    // those forces low_0 < low_1 < low_2 < ... This asserts the concrete
    // instance rather than only the general argument.
    STATIC_REQUIRE(band_table_is_well_formed(WellFormedThreeBands));
    STATIC_REQUIRE(WellFormedThreeBands[0].lowNumerator < WellFormedThreeBands[1].lowNumerator);
    STATIC_REQUIRE(WellFormedThreeBands[1].lowNumerator < WellFormedThreeBands[2].lowNumerator);
}

// ---- the static_assert wiring, exercised on its success path ----
//
// The failure path (a malformed table refusing to compile) is exactly what
// must NOT be exercised here -- that would break this test binary. It is
// pinned instead by test/negative/band_gap.cpp, test/negative/band_overlap.cpp
// and test/negative/band_inverted.cpp, which assert that each fails to
// compile and that the failure names the offending band or pair. This test
// only proves the success path is reachable the way phase 10 tasks 2-4 will
// reach it.

TEST_CASE("RequireValidBandTable accepts a well-formed table", "[band]")
{
    STATIC_REQUIRE(formula::RequireValidBandTable<WellFormedThreeBands>::value);
    STATIC_REQUIRE(formula::RequireValidBandTable<SingleBand>::value);
    STATIC_REQUIRE(formula::RequireValidBandTable<EmptyTable>::value);
}

TEST_CASE("RequireBandWellFormed accepts a well-formed band", "[band]")
{
    STATIC_REQUIRE(formula::RequireBandWellFormed<band(0, 1, 10, 1)>::value);
}
