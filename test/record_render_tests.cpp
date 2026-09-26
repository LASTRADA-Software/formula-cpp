// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Reference
{
};
/// A role whose published name holds characters every dialect treats
/// specially: a space, a colon, a percent sign and an underscore.
struct OddlyNamed
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
    static constexpr std::string_view of() noexcept { return "reference 1:2 %_B"; }
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
    // LaTeX: in math mode, inside \mathrm, with the math-mode escaper (X11's
    // ruling): spaces as "\ ", % and _ backslashed.
    // Markdown: the author-words escaping lookup keys use.
    constexpr auto odd = formula::from_record<OddlyNamed>(var<Strength>);
    CHECK(formula::render<formula::Dialect::LaTeX>(odd) == "f_c\\ \\text{of }\\mathrm{reference\\ 1:2\\ \\%\\_B}");
    CHECK(formula::render<formula::Dialect::Markdown>(odd) == "`f_c` of reference 1:2 %\\_B");
    CHECK(formula::render(odd) == "f_c of reference 1:2 %_B");
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
    // The lead's ruling: a single quantity read from another record reads in
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
