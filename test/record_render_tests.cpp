// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Reference
{
};
/// A role whose published name holds the two characters of an
/// identifier-like name a dialect treats specially: a space and an
/// underscore.
struct OddlyNamed
{
};
/// A second record, for rows read from two different ones.
struct Prior
{
};

struct Force: formula::Quantity<Force, "F", "applied force", unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", unit::Millimetre>
{
};
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", unit::Megapascal>
{
};
struct Factor: formula::Quantity<Factor, "k", "a factor", unit::One>
{
};
struct Cube
{
};

constexpr auto ratio = var<Strength> / formula::from_record<Reference>(var<Strength>);
constexpr auto compound = formula::from_record<Reference>(var<Force> / var<EdgeX>);

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 83'400 } },
                                           formula::Measured<EdgeX> { formula::Rational { 139 } },
                                           formula::Measured<Strength> { formula::Rational { 6 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 55'600 } },
                                            formula::Measured<EdgeX> { formula::Rational { 139 } },
                                            formula::Measured<Strength> { formula::Rational { 4 } });
constexpr auto ctx = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
} // namespace

template <>
struct formula::TagName<OddlyNamed>
{
    static constexpr std::string_view of() noexcept { return "reference specimen_B"; }
};

TEST_CASE("a read from another record renders as words, in every dialect", "[record-render]")
{
    // The scope takes the lowest precedence, so as an operand it is always
    // bracketed; LaTeX's fraction groups both sides itself.
    CHECK(formula::render(ratio) == "f_c / (f_c of Reference)");
    CHECK(formula::render<formula::Dialect::Markdown>(ratio) == "`f_c` / (`f_c` of Reference)");
    CHECK(formula::render<formula::Dialect::LaTeX>(ratio) == "\\frac{f_c}{f_c\\ \\text{of }\\mathrm{Reference}}");
}

TEST_CASE("a compound read from another record keeps its brackets", "[record-render]")
{
    // A single symbol reads unbracketed; anything more is bracketed, or
    // "F / x_m of Reference" could be read as "F / (x_m of Reference)".
    CHECK(formula::render(compound) == "(F / x_m) of Reference");
    CHECK(formula::render<formula::Dialect::Markdown>(compound) == "(`F` / `x_m`) of Reference");
    CHECK(formula::render<formula::Dialect::LaTeX>(compound)
          == "\\left(\\frac{F}{x_m}\\right)\\ \\text{of }\\mathrm{Reference}");
}

TEST_CASE("a role's name is escaped for the dialect it is written in", "[record-render]")
{
    // LaTeX: in math mode, inside \mathrm, with the math-mode escaper:
    // spaces as "\ ", _ backslashed. Markdown: the author-words
    // escaping lookup keys use. Plain: as spelt.
    constexpr auto odd = formula::from_record<OddlyNamed>(var<Strength>);
    CHECK(formula::render<formula::Dialect::LaTeX>(odd) == "f_c\\ \\text{of }\\mathrm{reference\\ specimen\\_B}");
    CHECK(formula::render<formula::Dialect::Markdown>(odd) == "`f_c` of reference specimen\\_B");
    CHECK(formula::render(odd) == "f_c of reference specimen_B");
    // The escaper alone: letters and digits as they are.
    CHECK(formula::detail::latex_math_words("Batch 2_b") == "Batch\\ 2\\_b");
}

TEST_CASE("a role's name must be identifier-like", "[record-render]")
{
    // ASCII letters,
    // digits, underscores and single spaces between words. Each refusal below
    // is a character class the rule removes; each negative test
    // (`record_role_name_*`) is the same through a real role.
    STATIC_REQUIRE(formula::detail::is_identifier_like_role_name("Reference"));
    STATIC_REQUIRE(formula::detail::is_identifier_like_role_name("reference specimen_B 2"));
    STATIC_REQUIRE(formula::detail::is_identifier_like_role_name("Reference_2"));
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name("_private")); // not a letter first
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name("2nd reference"));
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name("9"));
    STATIC_REQUIRE(formula::detail::is_identifier_like_role_name("")); // unreadable signature: the library's fault
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name("Reference-B"));
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name("Batch<2>"));
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name("O'Brien"));
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name("a*b"));
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name("Ref\nB"));
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name("Ref\tB"));
    // "Réf" in UTF-8, split so that the f is not read as a hex digit of the escape.
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name("R\xC3\xA9"
                                                                       "f"));
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name("two  spaces"));
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name(" leading"));
    STATIC_REQUIRE_FALSE(formula::detail::is_identifier_like_role_name("trailing "));
}

TEST_CASE("a role may not be displayed as this record", "[record-render]")
{
    // "from record this record" would name the record being evaluated. Any
    // mix of case -- and with or without the space or with an underscore for it:
    // `ThisRecord` is how a lineage check names the record being evaluated.
    STATIC_REQUIRE(formula::detail::reads_as_this_record("this record"));
    STATIC_REQUIRE(formula::detail::reads_as_this_record("This Record"));
    STATIC_REQUIRE(formula::detail::reads_as_this_record("THIS RECORD"));
    STATIC_REQUIRE(formula::detail::reads_as_this_record("ThisRecord"));
    STATIC_REQUIRE(formula::detail::reads_as_this_record("this_record"));
    STATIC_REQUIRE_FALSE(formula::detail::reads_as_this_record("this record 2"));
    STATIC_REQUIRE_FALSE(formula::detail::reads_as_this_record("ThisRecords"));
    STATIC_REQUIRE_FALSE(formula::detail::reads_as_this_record("this"));
    STATIC_REQUIRE_FALSE(formula::detail::reads_as_this_record("Reference"));
    STATIC_REQUIRE_FALSE(formula::detail::reads_as_this_record(""));
}

TEST_CASE("a quantity read here and from the reference has two rows", "[record-render]")
{
    auto const page =
        formula::document<formula::Dialect::Plain>(var<Strength> / formula::from_record<Reference>(var<Strength>));
    REQUIRE(page.symbols.size() == 2); // deduplicating by quantity alone would give 1
    CHECK(page.symbols[0].record.empty());
    CHECK(page.symbols[1].record == "Reference");
    CHECK(page.symbols[0].symbol == "f_c");
    CHECK(page.symbols[1].symbol == "f_c");
}

TEST_CASE("a row read after a scope is this record's again", "[record-render]")
{
    // The scope first, then the local read: a walk that did not restore this
    // record's role after the scope would file the second row under
    // Reference too, and merge it into the first.
    auto const page = formula::document(formula::from_record<Reference>(var<Strength>) / var<Strength>);
    REQUIRE(page.symbols.size() == 2);
    CHECK(page.symbols[0].record == "Reference");
    CHECK(page.symbols[1].record.empty());
}

TEST_CASE("a quantity read twice from the same record has one row", "[record-render]")
{
    // The other half of the identity: (role, quantity), not every read.
    auto const page = formula::document(formula::from_record<Reference>(var<Strength>)
                                        / formula::from_record<Reference>(var<Strength> * var<Strength>));
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].record == "Reference");
}

TEST_CASE("a vocabulary renames a quantity read from another record, on the page and in the trace", "[record-render]")
{
    constexpr auto north = formula::vocabulary(formula::renames<Strength>("S"));
    CHECK(formula::render(ratio, north) == "S / (S of Reference)");
    auto const page = formula::document(ratio, north);
    REQUIRE(page.symbols.size() == 2);
    CHECK(page.symbols[0].symbol == "S");
    CHECK(page.symbols[1].symbol == "S");

    formula::Trace<> trace {};
    (void) formula::checked_evaluate_si<formula::Rational>(ratio, ctx, formula::RecordingSink { trace, north });
    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });
    INFO(text);
    CHECK(text.find("1. S = 6 MPa\n") != std::string::npos);
    CHECK(text.find("2. S = 4 MPa, from record Reference (sample 23, test 3)\n") != std::string::npos);
    CHECK(text.find("f_c") == std::string::npos);
}

TEST_CASE("a scope's trace line shows its value in the quantity's unit", "[record-render]")
{
    // A single quantity read from another record reads in
    // that quantity's unit, 4 MPa, not bare coherent SI (4000000). A compound
    // operand names no single quantity, and stays in coherent SI.
    formula::Trace<> trace {};
    (void) formula::checked_evaluate_si<formula::Rational>(formula::from_record<Reference>(var<Strength>), ctx,
                                                           formula::RecordingSink { trace });
    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });
    INFO(text);
    CHECK(text == "1. f_c = 4 MPa, from record Reference (sample 23, test 3)\n"
                  "2. #1 from record Reference (sample 23, test 3) = 4 MPa\n");
}

TEST_CASE("a quantity read from two different records has a row for each, each labelled", "[record-render]")
{
    // One identity shared by every role would merge
    // the two foreign reads into one row labelled Reference, and tell a
    // reader to supply one value where the formula reads two.
    auto const page = formula::document(
        var<Strength> + formula::from_record<Reference>(var<Strength>) - formula::from_record<Prior>(var<Strength>));
    REQUIRE(page.symbols.size() == 3);
    CHECK(page.symbols[0].record.empty());
    CHECK(page.symbols[1].record == "Reference");
    CHECK(page.symbols[2].record == "Prior");
}

TEST_CASE("a read from another record as a conditional's else branch is bracketed", "[record-render]")
{
    // "of Reference" trails, so unbracketed it could be read as applying to
    // the whole conditional -- the second formula below, a different one.
    constexpr auto overFifty = var<Strength> > formula::constant<unit::Megapascal>(formula::Rational { 50 });
    constexpr auto elseForeign = formula::when(overFifty, var<Strength>, formula::from_record<Reference>(var<Strength>));
    constexpr auto foreignWhen = formula::from_record<Reference>(formula::when(overFifty, var<Strength>, var<Strength>));
    CHECK(formula::render(elseForeign) == "if f_c > 50 MPa then f_c else (f_c of Reference)");
    CHECK(formula::render<formula::Dialect::Markdown>(elseForeign)
          == "if `f_c` > 50 MPa then `f_c` else (`f_c` of Reference)");
    CHECK(formula::render(foreignWhen) == "(if f_c > 50 MPa then f_c else f_c) of Reference");
    CHECK(formula::render<formula::Dialect::Markdown>(foreignWhen)
          == "(if `f_c` > 50 MPa then `f_c` else `f_c`) of Reference");
}

TEST_CASE("a scope's trace line shows the unit its operand's line does", "[record-render]")
{
    // A scope is its operand's value unchanged, so it
    // takes its operand step's unit. A rounded read shows MPa as the rounding
    // does; a compound one shows what the computation's line shows: 400000,
    // coherent N/m, which has no symbol.
    formula::Trace<> rounded {};
    (void) formula::checked_evaluate_si<formula::Rational>(
        formula::from_record<Reference>(
            formula::rounded<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
                var<Strength>)),
        ctx, formula::RecordingSink { rounded });
    std::string const roundedText = formula::render_trace(rounded, { .maxSteps = 10 });
    INFO(roundedText);
    CHECK(roundedText.find("3. #2 from record Reference (sample 23, test 3) = 4 MPa\n") != std::string::npos);

    formula::Trace<> computed {};
    (void) formula::checked_evaluate_si<formula::Rational>(compound, ctx, formula::RecordingSink { computed });
    std::string const computedText = formula::render_trace(computed, { .maxSteps = 10 });
    INFO(computedText);
    CHECK(computedText == "1. F = 55600 N, from record Reference (sample 23, test 3)\n"
                          "2. x_m = 139 mm, from record Reference (sample 23, test 3)\n"
                          "3. #1 / #2 = 400000\n"
                          "4. #3 from record Reference (sample 23, test 3) = 400000\n");
}

TEST_CASE("an overlay's constant used here and inside a scope has one row, of no record", "[record-render]")
{
    // The fixed value is the overlay's, read from no
    // record, so it is not repeated under the scope's role. The strength it
    // multiplies is read from both records, and has a row for each.
    constexpr auto both = formula::method(
        formula::variants(formula::variant<Cube>(
            var<Factor> * var<Strength> / formula::from_record<Reference>(var<Factor> * var<Strength>))),
        formula::rounding_rule<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());
    constexpr auto fixedBoth =
        formula::apply(formula::overlay(formula::with_constant<Factor>(formula::Rational { 97, 100 }, formula::Citation { .reference = "Example Standard 14:2022 NA" })), both);
    auto const fixedPage = formula::document(std::get<0>(fixedBoth.variantSet.cases).expression);
    REQUIRE(fixedPage.symbols.size() == 3);
    CHECK(fixedPage.symbols[0].symbol == "k");
    CHECK(fixedPage.symbols[0].record.empty());
    CHECK(fixedPage.symbols[0].fixedValue == formula::Rational { 97, 100 });
    CHECK(fixedPage.symbols[1].record.empty());
    CHECK(fixedPage.symbols[2].record == "Reference");

    // The same for a derived quantity: one definition; its inputs, read in
    // each record, a row each.
    constexpr auto derivedBoth =
        formula::apply(formula::overlay(formula::add_derived<Factor>(var<EdgeX> / var<EdgeX>, formula::Citation { .reference = "Example Standard 14:2022 NA" })), both);
    auto const derivedPage = formula::document(std::get<0>(derivedBoth.variantSet.cases).expression);
    std::size_t factorRows = 0;
    std::size_t edgeRows = 0;
    for (formula::SymbolEntry const& row: derivedPage.symbols)
    {
        if (row.symbol == "k")
        {
            ++factorRows;
            CHECK(row.record.empty());
        }
        if (row.symbol == "x_m")
            ++edgeRows;
    }
    CHECK(factorRows == 1);
    CHECK(edgeRows == 2);
}

namespace
{
struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", unit::Gram>
{
};

constexpr auto retainedRatio =
    formula::sum(formula::series<Retained, 5>) / formula::from_record<Reference>(formula::sum(formula::series<Retained, 5>));
} // namespace

TEST_CASE("a series read from another record has its own row, marked as a series, on the page", "[record-render]")
{
    // The series marker and the scope's words combine, and
    // the symbol table keeps a row per record, each with the series' shape
    // and length. The LaTeX typesets clean under MathJax 3.2.2, with the
    // site's configuration and strictly.
    CHECK(formula::render(retainedRatio) == "sum(m_r(i)) / (sum(m_r(i)) of Reference)");
    CHECK(formula::render<formula::Dialect::Markdown>(retainedRatio) == "sum(`m_r(i)`) / (sum(`m_r(i)`) of Reference)");
    CHECK(formula::render<formula::Dialect::LaTeX>(retainedRatio)
          == "\\frac{\\sum {m_r}_{i}}{\\sum {m_r}_{i}\\ \\text{of }\\mathrm{Reference}}");

    auto const page = formula::document(retainedRatio);
    REQUIRE(page.symbols.size() == 2);
    CHECK(page.symbols[0].record.empty());
    CHECK(page.symbols[1].record == "Reference");
    for (formula::SymbolEntry const& row: page.symbols)
    {
        CHECK(row.symbol == "m_r");
        CHECK(row.shape == formula::ValueShape::Series);
        CHECK(row.length == 5);
    }
}
