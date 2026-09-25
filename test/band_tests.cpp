// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/band.hpp>

#include <catch2/catch_test_macros.hpp>

using formula::Band;
using formula::BandTable;
using formula::band;
using formula::band_table_has_no_gap_or_overlap;
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
} // namespace

// ---- the one predicate, on a single pair ----

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

// ---- whole-table validation ----

TEST_CASE("a well-formed band table validates", "[band]")
{
    STATIC_REQUIRE(band_table_has_no_gap_or_overlap(WellFormedThreeBands));
    REQUIRE(band_table_has_no_gap_or_overlap(WellFormedThreeBands));
}

TEST_CASE("a single-band table validates", "[band]")
{
    STATIC_REQUIRE(band_table_has_no_gap_or_overlap(SingleBand));
}

TEST_CASE("an empty table validates -- it is always missing, not malformed", "[band]")
{
    STATIC_REQUIRE(band_table_has_no_gap_or_overlap(EmptyTable));
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
    STATIC_REQUIRE(!band_table_has_no_gap_or_overlap(GapInMiddle));
    REQUIRE(!band_table_has_no_gap_or_overlap(GapInMiddle));

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
    STATIC_REQUIRE(!band_table_has_no_gap_or_overlap(GapAtEnd));
    REQUIRE(!band_table_has_no_gap_or_overlap(GapAtEnd));
}

TEST_CASE("an overlap between the middle two bands is caught, not only an overlap at the end", "[band]")
{
    STATIC_REQUIRE(!band_table_has_no_gap_or_overlap(OverlapInMiddle));
    REQUIRE(!band_table_has_no_gap_or_overlap(OverlapInMiddle));

    STATIC_REQUIRE(bands_are_adjacent(OverlapInMiddle[0], OverlapInMiddle[1]));
    STATIC_REQUIRE(!bands_are_adjacent(OverlapInMiddle[1], OverlapInMiddle[2]));
    STATIC_REQUIRE(bands_are_adjacent(OverlapInMiddle[2], OverlapInMiddle[3]));
}

TEST_CASE("an overlap between the last two bands is also caught", "[band]")
{
    STATIC_REQUIRE(!band_table_has_no_gap_or_overlap(OverlapAtEnd));
    REQUIRE(!band_table_has_no_gap_or_overlap(OverlapAtEnd));
}

// ---- the static_assert wiring, exercised on its success path ----
//
// The failure path (a gapped or overlapping table refusing to compile) is
// exactly what must NOT be exercised here -- that would break this test
// binary. It is pinned instead by test/negative/band_gap.cpp and
// test/negative/band_overlap.cpp, which assert that it fails to compile and
// that the failure names the offending pair. This test only proves the
// success path is reachable the way phase 10 tasks 2-4 will reach it.

TEST_CASE("RequireValidBandTable accepts a well-formed table", "[band]")
{
    STATIC_REQUIRE(formula::RequireValidBandTable<WellFormedThreeBands>::value);
    STATIC_REQUIRE(formula::RequireValidBandTable<SingleBand>::value);
    STATIC_REQUIRE(formula::RequireValidBandTable<EmptyTable>::value);
}
