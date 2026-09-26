// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Cube
{
};

// Declared symbols that no vocabulary below uses, so a rendering that fell
// back to `Describe<Q>::symbol` where it should have renamed shows as the
// wrong text rather than as the right one by coincidence.
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", unit::Megapascal>
{
};
struct Modulus: formula::Quantity<Modulus, "E_m", "elastic modulus", unit::Megapascal>
{
};
struct Factor: formula::Quantity<Factor, "k", "shape factor", unit::One>
{
};
struct Diameter: formula::Quantity<Diameter, "d", "specimen diameter", unit::Millimetre>
{
};

// The crossed-over case: each jurisdiction's word for one quantity is the
// other's word for the other quantity. Two unrelated renames would pass a
// test even if a vocabulary were applied to the wrong quantity; these do not.
inline constexpr auto north = formula::vocabulary(formula::renames<Strength>("R"), formula::renames<Modulus>("E"));
inline constexpr auto south = formula::vocabulary(formula::renames<Strength>("E"), formula::renames<Modulus>("R"));

inline constexpr auto f = var<Strength> / var<Modulus>;

[[nodiscard]] constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// Named for this file alone: see `trace_render_tests.cpp` on what clang does
// to two anonymous-namespace tables of one spelling in two translation units.
inline constexpr formula::BandTable<2> VocabularyDiameterBands { formula::band(100, 1, 150, 1),
                                                                 formula::band(150, 1, 300, 1) };
inline constexpr formula::BreakpointTable<2> VocabularyDiameterPoints { formula::breakpoint(100), formula::breakpoint(300) };
} // namespace

TEST_CASE("two jurisdictions cross over one pair of symbols", "[vocabulary]")
{
    CHECK(formula::render(f, north) == "R / E");
    CHECK(formula::render(f, south) == "E / R");
    // Crossed over, not merely different: each symbol means the other's
    // quantity. A test with two unrelated renames would pass even if the
    // vocabulary were applied to the wrong quantity.
    CHECK(formula::render(f) == "f_c / E_m");
}

TEST_CASE("a vocabulary is spelled in every dialect", "[vocabulary]")
{
    CHECK(formula::render<formula::Dialect::Plain>(f, north) == "R / E");
    CHECK(formula::render<formula::Dialect::Markdown>(f, north) == "`R` / `E`");
    CHECK(formula::render<formula::Dialect::LaTeX>(f, north) == "\\frac{R}{E}");
    CHECK(formula::render<formula::Dialect::LaTeX>(f, south) == "\\frac{E}{R}");
}

TEST_CASE("a quantity a vocabulary does not rename keeps its declared symbol", "[vocabulary]")
{
    constexpr auto onlyStrength = formula::vocabulary(formula::renames<Strength>("R"));
    CHECK(formula::render(f, onlyStrength) == "R / E_m");

    STATIC_REQUIRE(formula::symbol_of<Strength>(onlyStrength) == "R");
    STATIC_REQUIRE(formula::symbol_of<Modulus>(onlyStrength) == "E_m");
    STATIC_REQUIRE(formula::symbol_of<Strength>(north) == "R");
    STATIC_REQUIRE(formula::symbol_of<Strength>(south) == "E");
    STATIC_REQUIRE(formula::symbol_of<Modulus>(north) == "E");
    STATIC_REQUIRE(formula::symbol_of<Modulus>(south) == "R");
}

TEST_CASE("the default vocabulary changes nothing", "[vocabulary]")
{
    STATIC_REQUIRE(formula::symbol_of<Strength>(formula::DefaultVocabulary {}) == formula::Describe<Strength>::symbol);
    STATIC_REQUIRE(formula::symbol_of<Factor>(formula::vocabulary()) == formula::Describe<Factor>::symbol);

    constexpr auto everything =
        formula::when(var<Strength> >= var<Modulus>, formula::sqrt(var<Strength> * var<Modulus>), -var<Modulus>);
    CHECK(formula::render<formula::Dialect::Plain>(everything, formula::DefaultVocabulary {})
          == formula::render<formula::Dialect::Plain>(everything));
    CHECK(formula::render<formula::Dialect::Markdown>(everything, formula::DefaultVocabulary {})
          == formula::render<formula::Dialect::Markdown>(everything));
    CHECK(formula::render<formula::Dialect::LaTeX>(everything, formula::DefaultVocabulary {})
          == formula::render<formula::Dialect::LaTeX>(everything));
}

TEST_CASE("a vocabulary reaches every node kind that holds an operand", "[vocabulary]")
{
    // One node of every kind that renders a sub-expression, each holding a
    // variable, so a kind that rendered its operand without handing the
    // vocabulary on shows the declared symbol `f_c` or `E_m` where it stands.
    constexpr formula::Citation cited { .reference = "Example Standard 1:2020", .section = "3.1" };
    constexpr auto s = var<Strength>;
    constexpr auto m = var<Modulus>;
    constexpr auto d = var<Diameter>;
    constexpr auto everything = formula::when(
        s >= m,
        formula::documented(-formula::pow<2>(s) + formula::pow<2>(formula::root<3>(m * m * m)), cited)
            * formula::rounded<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(s)
            * formula::rounded_to_digits<unit::Megapascal,
                                         formula::SignificantDigits { 2 },
                                         formula::RoundingMode::HalfAwayFromZero>(m),
        formula::numeric_value_of<unit::Megapascal, "a table stated in megapascals">(s)
            * formula::banded_lookup<unit::Millimetre, VocabularyDiameterBands, unit::Megapascal>(d, { rat(1), rat(2) })
            * formula::interpolating_lookup<unit::Millimetre, VocabularyDiameterPoints, unit::One>(d, { rat(1), rat(3) })
            * formula::pow<3>(m));

    constexpr auto everywhere = formula::vocabulary(
        formula::renames<Strength>("R"), formula::renames<Modulus>("E"), formula::renames<Diameter>("D"));

    std::string const plain = formula::render(everything, everywhere);
    std::string const markdown = formula::render<formula::Dialect::Markdown>(everything, everywhere);
    std::string const latex = formula::render<formula::Dialect::LaTeX>(everything, everywhere);

    for (std::string const& text: { plain, markdown, latex })
    {
        CHECK(text.find("f_c") == std::string::npos);
        CHECK(text.find("E_m") == std::string::npos);
    }
    CHECK(plain
          == "if R >= E then (-R^2 + root3(E * E * E)^2) * round(R, to 1 dp of MPa) * round(E, to 2 sf of MPa) "
             "else numeric(R, in MPa) * lookup(D, 100 to under 150 mm gives 1 MPa, 150 to under 300 mm gives 2 MPa) "
             "* interpolate(D, at 100 mm gives 1, at 300 mm gives 3) * E^3");
    CHECK(markdown
          == "if `R` >= `E` then (-`R`^2 + root3(`E` * `E` * `E`)^2) * round(`R`, to 1 dp of MPa) "
             "* round(`E`, to 2 sf of MPa) else numeric(`R`, in MPa) * lookup(`D`, 100 to under 150 mm gives 1 MPa, "
             "150 to under 300 mm gives 2 MPa) * interpolate(`D`, at 100 mm gives 1, at 300 mm gives 3) * `E`^3");
    CHECK(latex
          == "\\begin{cases} (-R^{2} + \\sqrt[3]{E \\cdot E \\cdot E}^{2}) \\cdot "
             "\\operatorname{round}_{1\\,\\mathrm{MPa}}(R) "
             "\\cdot \\operatorname{round}_{2\\mathrm{sf},\\,\\mathrm{MPa}}(E) & \\text{if } R \\geq E \\\\ "
             "\\{R/\\mathrm{MPa}\\} \\cdot \\operatorname{lookup}(D,\\allowbreak \\text{100 to under 150 mm gives 1 MPa},"
             "\\allowbreak \\text{150 to under 300 mm gives 2 MPa}) \\cdot \\operatorname{interpolate}(D,\\allowbreak "
             "\\text{at 100 mm gives 1},\\allowbreak \\text{at 300 mm gives 3}) \\cdot E^{3} & \\text{otherwise} "
             "\\end{cases}");
}

TEST_CASE("a vocabulary renames a predicate and a constraint", "[vocabulary]")
{
    constexpr auto atMost = var<Strength> <= var<Modulus>;
    constexpr auto limit = formula::constraint(atMost, formula::Verdict { "reject the specimen" });

    CHECK(formula::render(atMost, north) == "R <= E");
    CHECK(formula::render(atMost, south) == "E <= R");
    CHECK(formula::render<formula::Dialect::LaTeX>(atMost, north) == "R \\leq E");
    CHECK(formula::render(limit, north) == "require R <= E");
    CHECK(formula::render(limit, south) == "require E <= R");
    CHECK(formula::render<formula::Dialect::Markdown>(limit, north) == "require `R` <= `E`");
}

TEST_CASE("the documentation renames the symbol and keeps the meaning", "[vocabulary][document]")
{
    formula::Documentation const northern = formula::document(f, north);
    formula::Documentation const southern = formula::document(f, south);

    CHECK(northern.formula == "R / E");
    CHECK(southern.formula == "E / R");

    // Same rows in the same order -- the quantities are the same -- with the
    // symbols crossed over and each description still its own quantity's:
    // under the southern vocabulary, `E` is the strength.
    REQUIRE(northern.symbols.size() == 2);
    REQUIRE(southern.symbols.size() == 2);
    CHECK(northern.symbols[0].symbol == "R");
    CHECK(northern.symbols[0].description == "compressive strength");
    CHECK(northern.symbols[1].symbol == "E");
    CHECK(northern.symbols[1].description == "elastic modulus");
    CHECK(southern.symbols[0].symbol == "E");
    CHECK(southern.symbols[0].description == "compressive strength");
    CHECK(southern.symbols[1].symbol == "R");
    CHECK(southern.symbols[1].description == "elastic modulus");
    CHECK(southern.symbols[0].unit == unit::Megapascal);

    formula::Documentation const plain = formula::document(f);
    REQUIRE(plain.symbols.size() == 2);
    CHECK(plain.symbols[0].symbol == "f_c");
    CHECK(plain.symbols[1].symbol == "E_m");
}

TEST_CASE("the documentation of a constraint renames its symbols", "[vocabulary][document]")
{
    constexpr auto limit = formula::constraint(var<Strength> <= var<Modulus>, formula::Verdict { "reject the specimen" });
    formula::Documentation const southern = formula::document<formula::Dialect::Markdown>(limit, south);

    CHECK(southern.formula == "require `E` <= `R`");
    REQUIRE(southern.symbols.size() == 2);
    CHECK(southern.symbols[0].symbol == "E");
    CHECK(southern.symbols[0].description == "compressive strength");
    CHECK(southern.symbols[1].symbol == "R");
}

namespace
{
using OneDecimalOfMegapascal =
    formula::RoundingRule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>;

// Every trace step that names a quantity, beneath every kind of step that
// does not: a variable under a lookup, variables under arithmetic, a constant
// an overlay fixes, all under the method's rounding step and its variant
// selection.
inline constexpr auto crossedMethod = formula::method(
    formula::variants(formula::variant<Cube>(
        var<Factor> * var<Strength>
            * formula::banded_lookup<unit::Millimetre, VocabularyDiameterBands, unit::One>(var<Diameter>, { rat(1), rat(2) })
        - var<Modulus>)),
    OneDecimalOfMegapascal {},
    formula::constraints());

inline constexpr auto fixedFactor = formula::overlay(formula::with_constant<Factor>(rat(97, 100)));

// 30 MPa, 12 MPa and 200 mm, each distinct from the others, so a symbol
// attached to the wrong quantity attaches to the wrong number.
inline constexpr auto crossedInputs = formula::environment(formula::Measured<Strength> { rat(30) },
                                                           formula::Measured<Modulus> { rat(12) },
                                                           formula::Measured<Diameter> { rat(200) });

inline constexpr auto everyNamedQuantity = formula::vocabulary(formula::renames<Strength>("E"),
                                                               formula::renames<Modulus>("R"),
                                                               formula::renames<Factor>("k_s"),
                                                               formula::renames<Diameter>("D"));

template <typename M, typename V>
[[nodiscard]] std::string traceOf(M const& m, V const& vocabulary)
{
    formula::Trace<> trace {};
    (void) formula::evaluate_method<Cube>(m, crossedInputs, formula::RecordingSink { trace, vocabulary });
    return formula::render_trace(trace, { .maxSteps = 20 });
}

template <typename M>
[[nodiscard]] std::string traceOf(M const& m)
{
    formula::Trace<> trace {};
    (void) formula::evaluate_method<Cube>(m, crossedInputs, formula::RecordingSink<> { trace });
    return formula::render_trace(trace, { .maxSteps = 20 });
}
} // namespace

TEST_CASE("the trace names quantities in the sink's vocabulary", "[vocabulary][trace]")
{
    // The trace's symbols are written while the method is evaluated, by the
    // sink -- not when anything is rendered. A vocabulary given only to
    // `render()` would leave every one of these lines in the declared symbols.
    constexpr auto overlaid = formula::apply(fixedFactor, crossedMethod);

    CHECK(traceOf(overlaid, everyNamedQuantity)
          == "1. k_s = 97/100 [fixed by jurisdiction overlay]\n"
             "2. E = 30 MPa\n"
             "3. #1 * #2 = 29100000\n"
             "4. D = 200 mm\n"
             "5. lookup(#4) = 2 [150 to under 300 mm]\n"
             "6. #3 * #5 = 58200000\n"
             "7. R = 12 MPa\n"
             "8. #6 - #7 = 46200000\n"
             "9. round(#8, in MPa) = 231/5 MPa [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "10. #9 = 231/5 MPa [variant Cube (1st of 1), selected by tag]\n");

    // The same evaluation with no vocabulary, so that the lines above are
    // known to differ from the declared symbols in exactly the four places
    // that name a quantity, and nowhere else.
    CHECK(traceOf(overlaid)
          == "1. k = 97/100 [fixed by jurisdiction overlay]\n"
             "2. f_c = 30 MPa\n"
             "3. #1 * #2 = 29100000\n"
             "4. d = 200 mm\n"
             "5. lookup(#4) = 2 [150 to under 300 mm]\n"
             "6. #3 * #5 = 58200000\n"
             "7. E_m = 12 MPa\n"
             "8. #6 - #7 = 46200000\n"
             "9. round(#8, in MPa) = 231/5 MPa [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "10. #9 = 231/5 MPa [variant Cube (1st of 1), selected by tag]\n");
}

TEST_CASE("a constraint's trace names quantities in the sink's vocabulary", "[vocabulary][trace]")
{
    constexpr auto limit = formula::constraint(var<Strength> >= var<Modulus>, formula::Verdict { "reject the specimen" });

    formula::Trace<> northern {};
    (void) formula::check(limit, crossedInputs, formula::RecordingSink { northern, north });
    formula::Trace<> southern {};
    (void) formula::check(limit, crossedInputs, formula::RecordingSink { southern, south });

    CHECK(formula::render_trace(northern, { .maxSteps = 10 })
          == "1. R = 30 MPa\n"
             "2. E = 12 MPa\n"
             "3. require #1 >= #2 [satisfied]\n");
    CHECK(formula::render_trace(southern, { .maxSteps = 10 })
          == "1. E = 30 MPa\n"
             "2. R = 12 MPa\n"
             "3. require #1 >= #2 [satisfied]\n");
}

TEST_CASE("explain records in the vocabulary it is given", "[vocabulary][trace]")
{
    struct Ratio: formula::Quantity<Ratio, "r", "strength over modulus", unit::One>
    {
    };
    auto const explained = formula::explain<Ratio>(f, crossedInputs, south);
    CHECK(formula::render_trace(explained.trace, { .maxSteps = 10 })
          == "1. E = 30 MPa\n"
             "2. R = 12 MPa\n"
             "3. #1 / #2 = 5/2\n");

    auto const plain = formula::explain<Ratio>(f, crossedInputs);
    CHECK(formula::render_trace(plain.trace, { .maxSteps = 10 })
          == "1. f_c = 30 MPa\n"
             "2. E_m = 12 MPa\n"
             "3. #1 / #2 = 5/2\n");
}

namespace
{
/// A consumer's own node, rendered through the one-argument extension point
/// every earlier phase published: it knows nothing of vocabularies.
struct Gauge: formula::NodeBase
{
    // Never read: this node is only rendered, never evaluated.
    [[maybe_unused]] static constexpr formula::Dimension dimension = formula::dim::Scalar;
};

/// Declared here and defined out of line below, after its first use -- the
/// shape a consumer's header and source file give it.
template <formula::Dialect D>
[[nodiscard]] std::string render_node(Gauge const&);

/// A consumer's node that opts in: its two-argument `render_node` hands the
/// vocabulary on to the operand it wraps.
template <formula::Node Inner>
struct Scaled: formula::NodeBase
{
    static constexpr formula::Dimension dimension = Inner::dimension;
    Inner inner;
};

template <formula::Dialect D, formula::Node Inner, formula::Vocabulary V>
[[nodiscard]] std::string render_node(Scaled<Inner> const& node, V const& vocabulary)
{
    return "scaled(" + formula::render<D>(node.inner, vocabulary) + ")";
}
} // namespace

TEST_CASE("a consumer's one-argument render_node is still found", "[vocabulary][render]")
{
    constexpr auto expression = Gauge {} * var<Strength>;

    CHECK(formula::render(Gauge {}) == "gauge");
    CHECK(formula::render(Gauge {}, north) == "gauge");
    CHECK(formula::render(expression) == "gauge * f_c");
    // The consumer's node renders as it always did, and the vocabulary still
    // reaches the library's own nodes around it.
    CHECK(formula::render(expression, north) == "gauge * R");
}

TEST_CASE("a consumer's two-argument render_node receives the vocabulary", "[vocabulary][render]")
{
    constexpr auto expression = Scaled<decltype(var<Strength>)> { {}, var<Strength> };
    CHECK(formula::render(expression) == "scaled(f_c)");
    CHECK(formula::render(expression, south) == "scaled(E)");
}

namespace
{
template <formula::Dialect D>
std::string render_node(Gauge const&)
{
    return "gauge";
}
} // namespace
