// SPDX-License-Identifier: Apache-2.0
//
// The join: an overlaid method, its selected variant, and a jurisdiction's
// vocabulary, together. Each has tests of its own; this file checks them
// combined, in one method that uses every overlay operation, and across two
// translation units (`method_cross_tu.hpp`).
#include "method_cross_tu.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <tuple>

namespace
{
using namespace join_cross_tu;

[[nodiscard]] bool declares_no_symbol(std::string_view text)
{
    return text.find("_decl") == std::string_view::npos;
}

constexpr auto selectedFormula = std::get<0>(joined.variantSet.cases).expression;
} // namespace

TEST_CASE("an overlaid method's selected variant renders in the jurisdiction's vocabulary", "[join]")
{
    // Crossed over: the strength is `E` in the south and `R` in the north, and
    // the modulus the other way round -- the two pages swap exactly those two
    // letters, and the fixed and derived quantities stand where the overlay
    // substituted them into the replacement formula.
    CHECK(formula::render(selectedFormula, south)
          == "x_n * k_n * E / (R * lookup(D, 103 to under 163 mm gives 1127/1000, 163 to under 331 mm gives 1973/1000))");
    CHECK(formula::render(selectedFormula, north)
          == "x_n * k_n * R / (E * lookup(D, 103 to under 163 mm gives 1127/1000, 163 to under 331 mm gives 1973/1000))");
    for (std::string const& text: { formula::render<formula::Dialect::Markdown>(selectedFormula, south),
                                    formula::render<formula::Dialect::LaTeX>(selectedFormula, south) })
        CHECK(declares_no_symbol(text));
}

TEST_CASE("an overlaid method's selected variant documents in the jurisdiction's vocabulary", "[join]")
{
    // The symbol table keeps each quantity's meaning under either vocabulary:
    // `E` is the strength in the south, and `R` in the north.
    CHECK(page(south)
          == "x_n * k_n * E / (R * lookup(D, 103 to under 163 mm gives 1127/1000, 163 to under 331 mm gives 1973/1000))\n"
             "x_n: national factor (fixed)\n"
             "k_n: size factor (derived as D / 127 mm)\n"
             "D: specimen diameter\n"
             "E: compressive strength\n"
             "R: elastic modulus\n"
             "replacements: 1\n");
    CHECK(page(north)
          == "x_n * k_n * R / (E * lookup(D, 103 to under 163 mm gives 1127/1000, 163 to under 331 mm gives 1973/1000))\n"
             "x_n: national factor (fixed)\n"
             "k_n: size factor (derived as D / 127 mm)\n"
             "D: specimen diameter\n"
             "R: compressive strength\n"
             "E: elastic modulus\n"
             "replacements: 1\n");

    formula::Documentation const documentation = formula::document(selectedFormula, south);
    REQUIRE(documentation.replacedBy.size() == 1);
    CHECK(documentation.replacedBy[0] == replacementAnnex);
    for (auto const& dialect: { formula::document<formula::Dialect::Markdown>(selectedFormula, south),
                                formula::document<formula::Dialect::LaTeX>(selectedFormula, south) })
    {
        CHECK(declares_no_symbol(dialect.formula));
        for (formula::SymbolEntry const& row: dialect.symbols)
        {
            CHECK(declares_no_symbol(row.symbol));
            if (row.derivedAs.has_value())
                CHECK(declares_no_symbol(*row.derivedAs));
        }
    }
}

TEST_CASE("an overlaid method's selected variant traces in the jurisdiction's vocabulary", "[join]")
{
    // Every overlay operation says itself: the fixed constant (1), the derived
    // quantity (5), the replacement (14), the jurisdiction's rounding rule with
    // its citation (15) -- 390.05 %, where the base method's one decimal would
    // give 390.1 % -- and the selection, counted in the method as published:
    // the Cylinder is 2nd of 3 although the prune and the pin left it alone.
    // Every quantity is in the vocabulary's words, and the strength (30 MPa)
    // and the modulus (11 MPa) swap letters between the two traces.
    CHECK(trace(south)
          == "1. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA, NA.2.1]\n"
             "2. D = 241 mm\n"
             "3. 127 mm\n"
             "4. #2 / #3 = 241/127\n"
             "5. k_n = #4 = 241/127 [derived by jurisdiction overlay: Example Standard 12:2021 NA, NA.2.2]\n"
             "6. #1 * #5 = 358367/127000\n"
             "7. E = 30 MPa\n"
             "8. #6 * #7 = 1075101/12700 MPa\n"
             "9. R = 11 MPa\n"
             "10. D = 241 mm\n"
             "11. lookup(#10) = 1973/1000 [163 to under 331 mm]\n"
             "12. #9 * #11 = 21703/1000 MPa\n"
             "13. #8 / #12 = 10751010/2756281\n"
             "14. #13 = 10751010/2756281 [replaced by jurisdiction overlay: Example Standard 12:2021 NA, NA.3]\n"
             "15. round(#14, in %) = 7801/20 % [rounded to 2 dp (jurisdiction overlay: Example Standard 12:2021 NA, "
             "NA.4); nearest, ties away from zero]\n"
             "16. #15 = 7801/20 % [variant Cylinder (2nd of 3), selected by tag; 1 of 3 pruned by jurisdiction overlay: "
             "Example Standard 12:2021 NA, NA.1.2; pinned by jurisdiction overlay: Example Standard 12:2024 NA, "
             "NA.1.1]\n");

    std::string const northern = trace(north);
    CHECK(northern.find("7. R = 30 MPa\n") != std::string::npos);
    CHECK(northern.find("9. E = 11 MPa\n") != std::string::npos);
    CHECK(declares_no_symbol(northern));
}

TEST_CASE("an overlaid method shared across translation units renders and traces identically", "[join][cross-tu]")
{
    std::string const here = page(south);
    std::string const there = join_page_in_other_tu();
    CHECK(here == there);
    CHECK(trace(south) == join_trace_in_other_tu());
    CHECK(declares_no_symbol(there));
    CHECK(declares_no_symbol(join_trace_in_other_tu()));

    // The method the other unit built, evaluated here, gives the number the
    // trace above ends in.
    auto const theirs = formula::evaluate_method<Cylinder>(joined_in_other_tu(), inputs);
    REQUIRE(theirs.has_value());
    REQUIRE(theirs->has_value());
    CHECK(**theirs == formula::Rational { 39005, 10000 });

    // And the method this unit built, evaluated there. The parameter's type
    // is the method's, so the two units must agree on it to link at all --
    // see `evaluate_joined_in_other_tu`.
    auto const ours = evaluate_joined_in_other_tu(joined);
    REQUIRE(ours.has_value());
    REQUIRE(ours->has_value());
    CHECK(**ours == formula::Rational { 39005, 10000 });
}
