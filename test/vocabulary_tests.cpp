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
#include <tuple>
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
             "\\{R/\\mathrm{MPa}\\} \\cdot \\operatorname{lookup}(D,\\allowbreak \\mathrm{100\\ to\\ under\\ 150\\ mm\\ "
             "gives\\ 1\\ MPa},\\allowbreak \\mathrm{150\\ to\\ under\\ 300\\ mm\\ gives\\ 2\\ MPa}) \\cdot "
             "\\operatorname{interpolate}(D,\\allowbreak \\mathrm{at\\ 100\\ mm\\ gives\\ 1},\\allowbreak \\mathrm{at\\ "
             "300\\ mm\\ gives\\ 3}) \\cdot E^{3} & \\text{otherwise} "
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

inline constexpr auto fixedFactor = formula::overlay(
    formula::with_constant<Factor>(rat(97, 100), formula::Citation { .reference = "Example Standard 12:2021 NA" }));

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
          == "1. k_s = 97/100 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
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
          == "1. k = 97/100 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
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

TEST_CASE("the documentation of an overlaid formula agrees with its formula", "[vocabulary][document]")
{
    // The fixed row is written by its own `collect` overload, not the plain
    // variable's; a formula reading `k_s` beside a fixed row labelled `k`
    // would leave a reader unable to find the fixed value in the formula.
    constexpr auto cube = std::get<0>(formula::apply(fixedFactor, crossedMethod).variantSet.cases).expression;
    formula::Documentation const southern = formula::document(cube, everyNamedQuantity);

    CHECK(southern.formula == "k_s * E * lookup(D, 100 to under 150 mm gives 1, 150 to under 300 mm gives 2) - R");
    REQUIRE(southern.symbols.size() == 4);
    CHECK(southern.symbols[0].symbol == "k_s");
    CHECK(southern.symbols[0].description == "shape factor");
    REQUIRE(southern.symbols[0].fixedValue.has_value());
    CHECK(*southern.symbols[0].fixedValue == rat(97, 100));
    CHECK(southern.symbols[1].symbol == "E");
    CHECK(southern.symbols[1].description == "compressive strength");
    CHECK(!southern.symbols[1].fixedValue.has_value());
    CHECK(southern.symbols[2].symbol == "D");
    CHECK(southern.symbols[3].symbol == "R");
    CHECK(southern.symbols[3].description == "elastic modulus");

    // A plain read of the same quantity shares the fixed row, in the
    // vocabulary's word -- met first, so the row is the plain overload's.
    // Only an overlay builds an overridden constant, so the formula holding
    // both is assembled from the one `apply` put in `cube`: `k_s * E * lookup
    // - R` parses as `((k_s * E) * lookup) - R`.
    constexpr auto fixed = cube.lhs.lhs.lhs;
    formula::Documentation const both = formula::document(var<Factor> + fixed, everyNamedQuantity);
    CHECK(both.formula == "k_s + k_s");
    REQUIRE(both.symbols.size() == 1);
    CHECK(both.symbols[0].symbol == "k_s");
    CHECK(both.symbols[0].fixedValue.has_value());
}

TEST_CASE("a vocabulary looks through const on the quantity it names", "[vocabulary]")
{
    // `renames<Strength const>` applies to `var<Strength>`, and a lookup of
    // `Strength const` finds `renames<Strength>`: in `symbol_of`, and so in
    // every surface that asks it.
    constexpr auto constEntry = formula::vocabulary(formula::renames<Strength const>("R"));
    STATIC_REQUIRE(formula::symbol_of<Strength>(constEntry) == "R");
    STATIC_REQUIRE(formula::symbol_of<Strength const>(north) == "R");
    STATIC_REQUIRE(formula::symbol_of<Strength const>(south) == "E");

    CHECK(formula::render(f, constEntry) == "R / E_m");

    formula::Documentation const documentation = formula::document(f, constEntry);
    REQUIRE(documentation.symbols.size() == 2);
    CHECK(documentation.symbols[0].symbol == "R");
    CHECK(documentation.symbols[1].symbol == "E_m");

    formula::Trace<> trace {};
    (void) formula::check(formula::constraint(var<Strength> >= var<Modulus>, formula::Verdict { "reject the specimen" }),
                          crossedInputs,
                          formula::RecordingSink { trace, constEntry });
    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
          == "1. R = 30 MPa\n"
             "2. E_m = 12 MPa\n"
             "3. require #1 >= #2 [satisfied]\n");
}

namespace
{
/// A sink built from a vocabulary local to the function that built it, and
/// returned: the vocabulary is gone before the sink is used. The sink holds a
/// copy, so nothing dangles.
[[nodiscard]] formula::RecordingSink<formula::Rational, std::remove_cvref_t<decltype(south)>> southernSink(
    formula::Trace<>& trace)
{
    auto const local = formula::vocabulary(formula::renames<Strength>("E"), formula::renames<Modulus>("R"));
    return formula::RecordingSink { trace, local };
}
} // namespace

TEST_CASE("a sink keeps its own copy of the vocabulary", "[vocabulary][trace]")
{
    constexpr auto limit = formula::constraint(var<Strength> >= var<Modulus>, formula::Verdict { "reject the specimen" });

    formula::Trace<> returned {};
    (void) formula::check(limit, crossedInputs, southernSink(returned));
    CHECK(formula::render_trace(returned, { .maxSteps = 10 })
          == "1. E = 30 MPa\n"
             "2. R = 12 MPa\n"
             "3. require #1 >= #2 [satisfied]\n");

    formula::Trace<> temporary {};
    (void) formula::check(
        limit, crossedInputs, formula::RecordingSink { temporary, formula::vocabulary(formula::renames<Modulus>("M")) });
    CHECK(formula::render_trace(temporary, { .maxSteps = 10 })
          == "1. f_c = 30 MPa\n"
             "2. M = 12 MPa\n"
             "3. require #1 >= #2 [satisfied]\n");

    // The default vocabulary is empty and takes no space: a sink that names
    // none is the one pointer it was before vocabularies existed.
    STATIC_REQUIRE(sizeof(formula::RecordingSink<>) == sizeof(formula::Trace<>*));
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

namespace
{
/// A consumer's node deriving from one of the library's, with a one-argument
/// `render_node` of its own -- which it keeps, rather than being rendered as
/// the `VarNode` it derives from (`detail::render_in_vocabulary`).
struct Labelled: formula::VarNode<Strength>
{
};

template <formula::Dialect D>
[[nodiscard]] std::string render_node(Labelled const&)
{
    return "labelled";
}
} // namespace

TEST_CASE("a consumer's node derived from a library node keeps its own render_node", "[vocabulary][render]")
{
    CHECK(formula::render(Labelled {}) == "labelled");
    CHECK(formula::render(Labelled {}, north) == "labelled");
    CHECK(formula::render(Labelled {} / var<Modulus>, north) == "labelled / E");
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

// ------------------------------------------------- every node kind, crossed
//
// One method holding every node kind this library renders, documents and
// traces -- task 7's derived quantity and replaced variant included -- put
// through `render`, `document` and the trace under one vocabulary. Every
// quantity here declares a symbol ending `_decl`, and the vocabulary names
// every one of them, so a surface that drops the vocabulary anywhere writes
// `_decl` where it drops it: the absence checks below cover every dialect and
// every document field, and the pinned strings cover where each renamed symbol
// stands. The vocabulary crosses the two pressures over -- strength is `E`,
// modulus is `R` -- so a symbol resolved for the wrong quantity sits beside
// the wrong number in the trace.

namespace
{
struct EveryCube
{
};
struct EveryCylinder
{
};

struct EveryStrength: formula::Quantity<EveryStrength, "A_decl", "compressive strength", unit::Megapascal>
{
};
struct EveryModulus: formula::Quantity<EveryModulus, "B_decl", "elastic modulus", unit::Megapascal>
{
};
struct EveryDiameter: formula::Quantity<EveryDiameter, "D_decl", "specimen diameter", unit::Millimetre>
{
};
struct EveryDerived: formula::Quantity<EveryDerived, "K_decl", "size factor", unit::One>
{
};
struct EveryFixed: formula::Quantity<EveryFixed, "X_decl", "national factor", unit::One>
{
};

enum class EveryFinish : std::uint8_t
{
    Smooth,
    Rough,
};

inline constexpr formula::KeyTable<EveryFinish, 2> EveryFinishKeys { EveryFinish::Smooth, EveryFinish::Rough };

inline constexpr auto everyVocabulary = formula::vocabulary(formula::renames<EveryStrength>("E"),
                                                            formula::renames<EveryModulus>("R"),
                                                            formula::renames<EveryDiameter>("D"),
                                                            formula::renames<EveryDerived>("k_n"),
                                                            formula::renames<EveryFixed>("x_n"));

inline constexpr formula::Citation everyCited { .reference = "Example Standard 1:2020", .section = "3.1" };

[[nodiscard]] constexpr auto everyNodeKind()
{
    constexpr auto a = var<EveryStrength>;
    constexpr auto b = var<EveryModulus>;
    constexpr auto d = var<EveryDiameter>;
    constexpr auto r = a / b;
    return formula::when(
               a >= b,
               formula::documented(-formula::pow<2>(r) + formula::pow<3>(formula::root<3>(r * r * r)), everyCited)
                   * formula::rounded<unit::Percent, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
                       r)
                   * formula::rounded_to_digits<unit::Percent,
                                                formula::SignificantDigits { 2 },
                                                formula::RoundingMode::HalfAwayFromZero>(r),
               formula::numeric_value_of<unit::Megapascal, "a table stated in megapascals">(a)
                   * formula::banded_lookup<unit::Millimetre, VocabularyDiameterBands, unit::One>(d, { rat(1), rat(2) })
                   * formula::interpolating_lookup<unit::Millimetre, VocabularyDiameterPoints, unit::One>(
                       d, { rat(1), rat(3) }))
           * var<EveryDerived> * var<EveryFixed> * formula::pi * formula::constant<unit::One>(rat(2))
           * formula::exact_lookup<EveryFinishKeys, unit::One>(EveryFinish::Rough, { rat(1), rat(5, 4) });
}

inline constexpr auto everyMethod = formula::method(
    formula::variants(formula::variant<EveryCube>(everyNodeKind()),
                      formula::variant<EveryCylinder>(var<EveryStrength> / var<EveryModulus>)),
    formula::rounding_rule<unit::Percent, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());

inline constexpr auto everyOverlay = formula::overlay(
    formula::with_constant<EveryFixed>(rat(3, 2), formula::Citation { .reference = "Example Standard 12:2021 NA" }),
    formula::add_derived<EveryDerived>(var<EveryDiameter> / formula::constant<unit::Millimetre>(rat(100)),
                                       formula::Citation { .reference = "Example Standard 12:2021 NA" }),
    formula::replace_variant<EveryCylinder>(var<EveryModulus> / var<EveryStrength>,
                                            formula::Citation { .reference = "Example Standard 12:2021 NA" }));

inline constexpr auto everyOverlaid = formula::apply(everyOverlay, everyMethod);

// 30 MPa, 12 MPa and 200 mm, distinct from each other and from every table row.
inline constexpr auto everyInputs = formula::environment(formula::Measured<EveryStrength> { rat(30) },
                                                         formula::Measured<EveryModulus> { rat(12) },
                                                         formula::Measured<EveryDiameter> { rat(200) });

template <typename Tag>
[[nodiscard]] std::string everyTraceOf()
{
    formula::Trace<> trace {};
    (void) formula::evaluate_method<Tag>(everyOverlaid, everyInputs, formula::RecordingSink { trace, everyVocabulary });
    return formula::render_trace(trace, { .maxSteps = 60 });
}

[[nodiscard]] bool declares_no_symbol(std::string_view text)
{
    return text.find("_decl") == std::string_view::npos;
}
} // namespace

TEST_CASE("every node kind renders in the vocabulary, in every dialect", "[vocabulary][render]")
{
    constexpr auto cube = std::get<0>(everyOverlaid.variantSet.cases).expression;
    constexpr auto cylinder = std::get<1>(everyOverlaid.variantSet.cases).expression;

    CHECK(formula::render(cube, everyVocabulary)
          == "(if E >= R then (-(E / R)^2 + root3(E / R * E / R * E / R)^3) * round(E / R, to 1 dp of %) "
             "* round(E / R, to 2 sf of %) else numeric(E, in MPa) * lookup(D, 100 to under 150 mm gives 1, "
             "150 to under 300 mm gives 2) * interpolate(D, at 100 mm gives 1, at 300 mm gives 3)) * k_n * x_n "
             "* pi * 2 * lookup(key Rough, key Smooth gives 1, key Rough gives 5/4)");
    CHECK(formula::render(cylinder, everyVocabulary) == "R / E");
    for (std::string const& text: { formula::render<formula::Dialect::Markdown>(cube, everyVocabulary),
                                    formula::render<formula::Dialect::LaTeX>(cube, everyVocabulary),
                                    formula::render<formula::Dialect::Markdown>(cylinder, everyVocabulary),
                                    formula::render<formula::Dialect::LaTeX>(cylinder, everyVocabulary) })
        CHECK(declares_no_symbol(text));
}

TEST_CASE("every node kind documents in the vocabulary, in every dialect", "[vocabulary][document]")
{
    constexpr auto cube = std::get<0>(everyOverlaid.variantSet.cases).expression;

    formula::Documentation const plain = formula::document(cube, everyVocabulary);
    CHECK(plain.formula == formula::render(cube, everyVocabulary));
    REQUIRE(plain.symbols.size() == 5);
    CHECK(plain.symbols[0].symbol == "E");
    CHECK(plain.symbols[0].description == "compressive strength");
    CHECK(plain.symbols[1].symbol == "R");
    CHECK(plain.symbols[2].symbol == "D");
    CHECK(plain.symbols[3].symbol == "k_n");
    REQUIRE(plain.symbols[3].derivedAs.has_value());
    CHECK(*plain.symbols[3].derivedAs == "D / 100 mm");
    CHECK(plain.symbols[4].symbol == "x_n");
    CHECK(plain.symbols[4].fixedValue.has_value());

    // The replaced variant: marked replaced though uncited
    // (`Documentation::replacedBy`), its replacement in the page's words, and
    // its rows the vocabulary's -- modulus first, as the replacement reads it.
    constexpr auto cylinder = std::get<1>(everyOverlaid.variantSet.cases).expression;
    formula::Documentation const replaced = formula::document(cylinder, everyVocabulary);
    CHECK(replaced.formula == "R / E");
    CHECK(replaced.replacedBy.size() == 1);
    REQUIRE(replaced.symbols.size() == 2);
    CHECK(replaced.symbols[0].symbol == "R");
    CHECK(replaced.symbols[0].description == "elastic modulus");
    CHECK(replaced.symbols[1].symbol == "E");

    for (auto const& documentation: { formula::document<formula::Dialect::Markdown>(cube, everyVocabulary),
                                      formula::document<formula::Dialect::LaTeX>(cube, everyVocabulary),
                                      formula::document<formula::Dialect::Markdown>(cylinder, everyVocabulary),
                                      formula::document<formula::Dialect::LaTeX>(cylinder, everyVocabulary) })
    {
        CHECK(declares_no_symbol(documentation.formula));
        for (formula::SymbolEntry const& row: documentation.symbols)
        {
            CHECK(declares_no_symbol(row.symbol));
            if (row.derivedAs.has_value())
                CHECK(declares_no_symbol(*row.derivedAs));
        }
    }
}

TEST_CASE("every node kind traces in the vocabulary", "[vocabulary][trace]")
{
    // The lines that name a quantity, found by what they say rather than
    // pinned whole: the rest of this 48-step derivation is arithmetic, and pi's
    // rational approximation would pin nothing about vocabularies.
    std::string const cube = everyTraceOf<EveryCube>();
    CHECK(declares_no_symbol(cube));
    CHECK(cube.starts_with("1. E = 30 MPa\n"
                           "2. R = 12 MPa\n"));
    CHECK(cube.find("34. D = 200 mm\n") != std::string::npos);
    CHECK(cube.find("37. k_n = #36 = 2 [derived by jurisdiction overlay: Example Standard 12:2021 NA]\n")
          != std::string::npos);
    CHECK(cube.find("39. x_n = 3/2 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n") != std::string::npos);

    CHECK(everyTraceOf<EveryCylinder>()
          == "1. R = 12 MPa\n"
             "2. E = 30 MPa\n"
             "3. #1 / #2 = 2/5\n"
             "4. #3 = 2/5 [replaced by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "5. round(#4, in %) = 40 % [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "6. #5 = 40 % [variant EveryCylinder (2nd of 2), selected by tag]\n");
}

TEST_CASE("a constraint over the overlaid quantities traces and documents in the vocabulary",
          "[vocabulary][trace][document]")
{
    constexpr auto limit =
        formula::constraint(var<EveryStrength> >= var<EveryModulus>, formula::Verdict { "reject the specimen" });
    formula::Trace<> trace {};
    (void) formula::check(limit, everyInputs, formula::RecordingSink { trace, everyVocabulary });
    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
          == "1. E = 30 MPa\n"
             "2. R = 12 MPa\n"
             "3. require #1 >= #2 [satisfied]\n");
    CHECK(formula::document<formula::Dialect::LaTeX>(limit, everyVocabulary).formula == "\\text{require } E \\geq R");
}
