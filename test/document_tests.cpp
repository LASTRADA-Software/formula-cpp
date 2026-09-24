// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", formula::unit::Millimetre>
{
};
struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
// Shares Diameter's symbol on purpose, with a different description and a
// different unit, so a formula using both proves deduplication keys on the
// quantity, not on the rendered letter.
struct ExcavationDepth: formula::Quantity<ExcavationDepth, "d", "excavation depth", formula::unit::Metre>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

// Invented, as every citation in this repository must be.
constexpr auto ratio =
    formula::documented(var<WaterVolume> / var<CementVolume>,
                        { .title = "Water/cement ratio", .reference = "Example Standard 1:2020", .section = "5.4.2" });

constexpr auto perCent = formula::documented(
    ratio * rat(100),
    { .title = "Water/cement ratio, per cent", .reference = "Example Standard 1:2020", .section = "5.4.3" });

} // namespace

TEST_CASE("document: the documentation carries the rendered formula", "[document]")
{
    formula::Documentation const documentation = formula::document(ratio);

    CHECK(documentation.formula == "V_w / V_c");
}

TEST_CASE("document: the rendered formula follows the requested dialect", "[document]")
{
    formula::Documentation const latex = formula::document<formula::Dialect::LaTeX>(ratio);

    CHECK(latex.formula == "\\frac{V_w}{V_c}");
}

TEST_CASE("document: citations come back outermost first", "[document]")
{
    formula::Documentation const documentation = formula::document(perCent);

    REQUIRE(documentation.citations.size() == 2);
    CHECK(documentation.citations[0].title == std::string_view { "Water/cement ratio, per cent" });
    CHECK(documentation.citations[0].section == std::string_view { "5.4.3" });
    CHECK(documentation.citations[1].title == std::string_view { "Water/cement ratio" });
    CHECK(documentation.citations[1].section == std::string_view { "5.4.2" });
}

TEST_CASE("document: an undocumented formula yields an empty citation list", "[document]")
{
    formula::Documentation const documentation = formula::document(var<WaterVolume> + var<CementVolume>);

    CHECK(documentation.citations.empty());
    CHECK(documentation.formula == "V_w + V_c");
}

TEST_CASE("document: the symbol table carries each variable's symbol, description and unit", "[document]")
{
    formula::Documentation const documentation = formula::document(ratio);

    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_w" });
    CHECK(documentation.symbols[0].description == std::string_view { "effective water content" });
    CHECK(documentation.symbols[0].unit == formula::unit::Litre);
}

TEST_CASE("document: symbols come back in first-appearance order", "[document]")
{
    // Alphabetical order would put A before d in a formula that reads
    // pi * d^2 / 4, which is not how anyone reads it.
    formula::Documentation const documentation =
        formula::document(formula::pi * formula::pow<2>(var<Diameter>) / var<WaterVolume>);

    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "d" });
    CHECK(documentation.symbols[1].symbol == std::string_view { "V_w" });
}

TEST_CASE("document: a quantity used twice appears once", "[document]")
{
    formula::Documentation const documentation = formula::document(var<WaterVolume> + var<WaterVolume> * rat(2));

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_w" });
}

TEST_CASE("document: two entries for the same quantity compare equal", "[document]")
{
    constexpr formula::SymbolEntry left { .symbol = "d",
                                          .description = "specimen diameter",
                                          .unit = formula::unit::Millimetre };
    constexpr formula::SymbolEntry right { .symbol = "d",
                                           .description = "specimen diameter",
                                           .unit = formula::unit::Millimetre };
    constexpr formula::SymbolEntry other { .symbol = "D",
                                           .description = "specimen diameter",
                                           .unit = formula::unit::Millimetre };

    STATIC_REQUIRE(left == right);
    STATIC_REQUIRE(left != other);
}

TEST_CASE("document: a constant contributes no symbol", "[document]")
{
    formula::Documentation const documentation =
        formula::document(var<Diameter> * formula::constant<formula::unit::Millimetre>(rat(2)));

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "d" });
}

TEST_CASE("document: two quantities that share a symbol both get a row", "[document]")
{
    // Diameter is millimetres, ExcavationDepth is metres: two unrelated
    // quantities that happen to render the same letter. Collapsing them would
    // silently attribute one's description and unit to the other's uses.
    formula::Documentation const documentation = formula::document(var<Diameter> + var<ExcavationDepth>);

    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == std::string_view { "d" });
    CHECK(documentation.symbols[0].description == std::string_view { "specimen diameter" });
    CHECK(documentation.symbols[0].unit == formula::unit::Millimetre);
    CHECK(documentation.symbols[1].symbol == std::string_view { "d" });
    CHECK(documentation.symbols[1].description == std::string_view { "excavation depth" });
    CHECK(documentation.symbols[1].unit == formula::unit::Metre);
}

TEST_CASE("document: a negated variable still appears in the symbol table", "[document]")
{
    formula::Documentation const documentation = formula::document(-var<Diameter>);

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "d" });
}

TEST_CASE("document: a variable under a root still appears in the symbol table", "[document]")
{
    formula::Documentation const documentation = formula::document(formula::sqrt(var<WaterVolume>));

    REQUIRE(documentation.symbols.size() == 1);
    CHECK(documentation.symbols[0].symbol == std::string_view { "V_w" });
}
