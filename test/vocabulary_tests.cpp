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
inline constexpr formula::BandTable<2> VocabularyDiameterBands { formula::band(103, 1, 163, 1),
                                                                 formula::band(163, 1, 331, 1) };
inline constexpr formula::BreakpointTable<2> VocabularyDiameterPoints { formula::breakpoint(103), formula::breakpoint(331) };
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
            * formula::banded_lookup<unit::Millimetre, VocabularyDiameterBands, unit::Megapascal>(
                d, { rat(1127, 1000), rat(1973, 1000) })
            * formula::interpolating_lookup<unit::Millimetre, VocabularyDiameterPoints, unit::One>(
                d, { rat(1043, 1000), rat(2917, 1000) })
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
             "else numeric(R, in MPa) * lookup(D, 103 to under 163 mm gives 1127/1000 MPa, 163 to under 331 mm gives "
             "1973/1000 MPa) * interpolate(D, at 103 mm gives 1043/1000, at 331 mm gives 2917/1000) * E^3");
    CHECK(markdown
          == "if `R` >= `E` then (-`R`^2 + root3(`E` * `E` * `E`)^2) * round(`R`, to 1 dp of MPa) "
             "* round(`E`, to 2 sf of MPa) else numeric(`R`, in MPa) * lookup(`D`, 103 to under 163 mm gives 1127/1000 MPa, "
             "163 to under 331 mm gives 1973/1000 MPa) * interpolate(`D`, at 103 mm gives 1043/1000, at 331 mm gives "
             "2917/1000) * `E`^3");
    CHECK(latex
          == "\\begin{cases} (-R^{2} + \\sqrt[3]{E \\cdot E \\cdot E}^{2}) \\cdot "
             "\\operatorname{round}_{1\\,\\mathrm{MPa}}(R) "
             "\\cdot \\operatorname{round}_{2\\mathrm{sf},\\,\\mathrm{MPa}}(E) & \\text{if } R \\geq E \\\\ "
             "\\{R/\\mathrm{MPa}\\} \\cdot \\operatorname{lookup}(D,\\allowbreak \\mathrm{103\\ to\\ under\\ 163\\ mm\\ "
             "gives\\ 1127/1000\\ MPa},\\allowbreak \\mathrm{163\\ to\\ under\\ 331\\ mm\\ gives\\ 1973/1000\\ MPa}) \\cdot "
             "\\operatorname{interpolate}(D,\\allowbreak \\mathrm{at\\ 103\\ mm\\ gives\\ 1043/1000},\\allowbreak "
             "\\mathrm{at\\ 331\\ mm\\ gives\\ 2917/1000}) \\cdot E^{3} & \\text{otherwise} "
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
inline constexpr auto crossedMethod =
    formula::method(formula::variants(formula::variant<Cube>(
                        var<Factor> * var<Strength>
                            * formula::banded_lookup<unit::Millimetre, VocabularyDiameterBands, unit::One>(
                                var<Diameter>, { rat(1127, 1000), rat(1973, 1000) })
                        - var<Modulus>)),
                    OneDecimalOfMegapascal {},
                    formula::constraints());

inline constexpr auto fixedFactor = formula::overlay(
    formula::with_constant<Factor>(rat(863, 1000), formula::Citation { .reference = "Example Standard 12:2021 NA" }));

// 30 MPa, 12 MPa and 241 mm, each distinct from the others, so a symbol
// attached to the wrong quantity attaches to the wrong number.
inline constexpr auto crossedInputs = formula::environment(formula::Measured<Strength> { rat(30) },
                                                           formula::Measured<Modulus> { rat(12) },
                                                           formula::Measured<Diameter> { rat(241) });

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
          == "1. k_s = 863/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "2. E = 30 MPa\n"
             "3. #1 * #2 = 25890000\n"
             "4. D = 241 mm\n"
             "5. lookup(#4) = 1973/1000 [163 to under 331 mm]\n"
             "6. #3 * #5 = 51080970\n"
             "7. R = 12 MPa\n"
             "8. #6 - #7 = 39080970\n"
             "9. round(#8, in MPa) = 391/10 MPa [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "10. #9 = 391/10 MPa [variant Cube (1st of 1), selected by tag]\n");

    // The same evaluation with no vocabulary, so that the lines above are
    // known to differ from the declared symbols in exactly the four places
    // that name a quantity, and nowhere else.
    CHECK(traceOf(overlaid)
          == "1. k = 863/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "2. f_c = 30 MPa\n"
             "3. #1 * #2 = 25890000\n"
             "4. d = 241 mm\n"
             "5. lookup(#4) = 1973/1000 [163 to under 331 mm]\n"
             "6. #3 * #5 = 51080970\n"
             "7. E_m = 12 MPa\n"
             "8. #6 - #7 = 39080970\n"
             "9. round(#8, in MPa) = 391/10 MPa [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "10. #9 = 391/10 MPa [variant Cube (1st of 1), selected by tag]\n");
}

TEST_CASE("the documentation of an overlaid formula agrees with its formula", "[vocabulary][document]")
{
    // The fixed row is written by its own `collect` overload, not the plain
    // variable's; a formula reading `k_s` beside a fixed row labelled `k`
    // would leave a reader unable to find the fixed value in the formula.
    constexpr auto cube = std::get<0>(formula::apply(fixedFactor, crossedMethod).variantSet.cases).expression;
    formula::Documentation const southern = formula::document(cube, everyNamedQuantity);

    CHECK(southern.formula
          == "k_s * E * lookup(D, 103 to under 163 mm gives 1127/1000, 163 to under 331 mm gives 1973/1000) - R");
    REQUIRE(southern.symbols.size() == 4);
    CHECK(southern.symbols[0].symbol == "k_s");
    CHECK(southern.symbols[0].description == "shape factor");
    REQUIRE(southern.symbols[0].fixedValue.has_value());
    CHECK(*southern.symbols[0].fixedValue == rat(863, 1000));
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
// traces -- an overlay's derived quantity and replaced variant included -- put
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
struct EverySeries
{
};
struct EveryCurve
{
};
struct EveryBinned
{
};

struct EverySample
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
struct EveryRetained: formula::Quantity<EveryRetained, "S_decl", "mass retained on a screen", unit::Gram>
{
};
struct EveryTotal: formula::Quantity<EveryTotal, "T_decl", "total dry mass", unit::Gram>
{
};
struct EveryParticle: formula::Quantity<EveryParticle, "P_decl", "particle size", unit::Millimetre>
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
                                                            formula::renames<EveryFixed>("x_n"),
                                                            formula::renames<EveryRetained>("m_n"),
                                                            formula::renames<EveryTotal>("M_n"),
                                                            formula::renames<EveryParticle>("d_n"));

inline constexpr formula::Citation everyCited { .reference = "Example Standard 1:2020", .section = "3.1" };

// Invented permitted factors for the snap below; x_n (1487/1000) is exactly
// midway between 1437/1000 and 1537/1000, so the tie rule decides, inside the
// overlay's rewrite.
inline constexpr formula::BreakpointTable<3> everySnapSet { formula::breakpoint(1437, 1000),
                                                            formula::breakpoint(1537, 1000),
                                                            formula::breakpoint(1637, 1000) };

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
                   * formula::banded_lookup<unit::Millimetre, VocabularyDiameterBands, unit::One>(
                       d, { rat(1127, 1000), rat(1973, 1000) })
                   * formula::interpolating_lookup<unit::Millimetre, VocabularyDiameterPoints, unit::One>(
                       d, { rat(1043, 1000), rat(2917, 1000) }))
           * var<EveryDerived> * var<EveryFixed> * formula::pi * formula::constant<unit::One>(rat(2))
           * formula::exact_lookup<EveryFinishKeys, unit::One>(EveryFinish::Rough, { rat(1087, 1000), rat(1249, 1000) })
           * formula::snapped<unit::One, everySnapSet, formula::SnapTie::TowardHigher>(var<EveryFixed>)
           // Phase 13's kinds, added rather than multiplied in: the product
           // above leaves too few bits for another factor.
           + formula::rounded_sqrt<unit::Percent, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
               r * var<EveryFixed>)
               * formula::critical_value<formula::SampleSizeTable<2> { 2, 3 }, unit::One>(
                   formula::constant<unit::One>(rat(2)), { rat(60), rat(80) })
               * formula::abs(r)
               * formula::precision_limit<formula::PrecisionKind::Repeatability>(r, formula::precision_level<EveryDerived>);
}

inline constexpr formula::PlacesTable<3> everyPlaces { formula::DecimalPlaces { 0 },
                                                       formula::DecimalPlaces { 0 },
                                                       formula::DecimalPlaces { -1 } };

// Every series node kind, in the one variant a series can stand in: reduced to
// one value by `sum`. The overlay's constant sits inside the elementwise
// product, so it reaches there or the method is refused.
[[nodiscard]] constexpr auto everySeriesKind()
{
    constexpr auto s = formula::series<EveryRetained, 3>;
    return formula::sum(formula::cumulative<formula::CumulativeDirection::FromLast>(
               formula::rounded_elementwise<unit::Gram, everyPlaces, formula::RoundingMode::HalfAwayFromZero>(
                   -s + s * formula::series_constant<unit::One>(rat(1), rat(2), rat(3)) * var<EveryFixed>)))
           / var<EveryTotal>;
}

// Every curve kind, and the join with snapping: two curves over declared
// domains spliced, read at the overlay's fixed factor, and the answer snapped
// to a permitted value. The overlay's constant is the point the curve is read
// at, so it reaches inside the interpolation or the method is refused.
inline constexpr formula::BreakpointTable<3> everyCurvePoints { formula::breakpoint(1),
                                                                formula::breakpoint(2),
                                                                formula::breakpoint(4) };
inline constexpr formula::BreakpointTable<1> everyCurveTail { formula::breakpoint(5) };
inline constexpr formula::BreakpointTable<2> everyCurveSnapSet { formula::breakpoint(1, 200), formula::breakpoint(1, 100) };

[[nodiscard]] constexpr auto everyCurveKind()
{
    constexpr auto s = formula::series<EveryRetained, 3>;
    return formula::snapped<unit::One, everyCurveSnapSet, formula::SnapTie::TowardLower>(formula::interpolate_at(
        formula::splice<formula::Monotone::NonDecreasing>(
            formula::curve(formula::domain<unit::One, everyCurvePoints>, s / var<EveryTotal>),
            formula::curve(formula::domain<unit::One, everyCurveTail>, formula::series_constant<unit::One>(rat(1, 20)))),
        var<EveryFixed>));
}

// Binning: raw particle sizes counted into two classes, the upper
// class's share times the overlay's fixed factor. Invented classes, 0 to
// under 163 and 163 to under 277 mm -- three significant digits, none a
// preferred number; the 163 mm particle is on the boundary, in the upper
// class.
inline constexpr formula::BandTable<2> everyClasses { formula::band(0, 1, 163, 1), formula::band(163, 1, 277, 1) };

[[nodiscard]] constexpr auto everyBinnedKind()
{
    constexpr auto counted = formula::binned<unit::Millimetre, everyClasses>(formula::observations<EveryParticle, 4>);
    return formula::sum(counted * formula::series_constant<unit::One>(rat(0), rat(1))) / formula::sum(counted)
           * var<EveryFixed>;
}

// Every sample statistic, in a variant of its own: the mean of the retained
// masses, with the overlay's constant inside the sample, over the total and
// the count, plus the variance over the squared range, the overlay's
// constant inside every sample. 10, 20 and 40 g times 3/2 are 15, 30 and
// 60 g, whose mean is 35 g; over 2020 g and 3, 7/1212. Their variance is
// 525 g^2 and their range 45 g, so 7/27; the whole is 2891/10908 -- 26.5 %
// at 1 dp. Unscaled (a rewrite that did not reach in) the variance and range
// would read 700/3 g^2 and 30 g in the trace.
//
// And a rejection of outliers, the overlay's constant in its sample and in
// its limit, x_n / 3 = 1/2 of the pass's mean: 15, 30 and 60 g deviate 20, 5
// and 25 g from 35 g, past 17.5 g for 60 g alone; 15 and 30 g then deviate
// 7.5 g from 22.5 g, inside 11.25 g. Two remain, so the count less 2 adds
// nothing to the whole. A limit the rewrite did not reach reads no x_n and
// is refused; a sample it did not reach is 10, 20 and 40 g, which rejects
// 40 g at a limit it cannot read either.
[[nodiscard]] constexpr auto everySampleKind()
{
    constexpr auto s = formula::series<EveryRetained, 3>;
    constexpr auto trimmed = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<2>>(
            s * var<EveryFixed>,
            formula::deviation_from_mean(var<EveryFixed> / formula::number(rat(3)) * formula::pass_mean<EveryRetained>),
            formula::Verdict { "repeat the sieving" },
            everyCited);
    return formula::sample_mean(s * var<EveryFixed>) / var<EveryTotal> / formula::sample_count(s)
           + formula::sample_variance(s * var<EveryFixed>)
                 / (formula::sample_range(s * var<EveryFixed>) * formula::sample_range(s * var<EveryFixed>))
           + (formula::sample_count(trimmed) - formula::number(rat(2)));
}

inline constexpr auto everyMethod = formula::method(
    formula::variants(formula::variant<EveryCube>(everyNodeKind()),
                      formula::variant<EveryCylinder>(var<EveryStrength> / var<EveryModulus>),
                      formula::variant<EverySeries>(everySeriesKind()),
                      formula::variant<EveryCurve>(everyCurveKind()),
                      formula::variant<EveryBinned>(everyBinnedKind()),
                      formula::variant<EverySample>(everySampleKind())),
    formula::rounding_rule<unit::Percent, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());

inline constexpr auto everyOverlay = formula::overlay(
    formula::with_constant<EveryFixed>(rat(1487, 1000), formula::Citation { .reference = "Example Standard 12:2021 NA" }),
    formula::add_derived<EveryDerived>(var<EveryDiameter> / formula::constant<unit::Millimetre>(rat(127)),
                                       formula::Citation { .reference = "Example Standard 12:2021 NA" }),
    formula::replace_variant<EveryCylinder>(var<EveryModulus> / var<EveryStrength>,
                                            formula::Citation { .reference = "Example Standard 12:2021 NA" }));

inline constexpr auto everyOverlaid = formula::apply(everyOverlay, everyMethod);

// 30 MPa, 12 MPa and 241 mm, distinct from each other and from every table row.
// The series 10, 20 and 40 g against 2020 g: with the factors 1, 2, 3 and the
// fixed 1487/1000, the elements are 4.87, 39.48 and 138.44 g; rounded to 0,
// 0 and -1 places they are 5, 39 and 140 g; the totals from the last are 184,
// 179 and 140 g, their sum 503 g, and the share 503/2020, 24.9 % -- unrounded
// it would be 24.7 %, and a direction swapped gives 5, 44 and 184 g, summing
// to 233 g. No element sits on a tie, so this fixture does not tell
// HalfAwayFromZero from the other nearest modes: the signed `Deviation`
// fixture in `series_tests.cpp` separates all seven modes and pins that one.
inline constexpr auto everyInputs =
    formula::environment(formula::Measured<EveryStrength> { rat(30) },
                         formula::Measured<EveryModulus> { rat(12) },
                         formula::Measured<EveryDiameter> { rat(241) },
                         formula::measured_series<EveryRetained>(formula::Measured<EveryRetained> { rat(10) },
                                                                 formula::Measured<EveryRetained> { rat(20) },
                                                                 formula::Measured<EveryRetained> { rat(40) }),
                         formula::Measured<EveryTotal> { rat(2020) },
                         formula::MeasuredObservations<EveryParticle, 4>(rat(103), rat(163), rat(197), rat(127)));

template <typename Tag>
[[nodiscard]] std::string everyTraceOf()
{
    formula::Trace<> trace {};
    (void) formula::evaluate_method<Tag>(everyOverlaid, everyInputs, formula::RecordingSink { trace, everyVocabulary });
    return formula::render_trace(trace, { .maxSteps = 80 });
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
             "* round(E / R, to 2 sf of %) else numeric(E, in MPa) * lookup(D, 103 to under 163 mm gives 1127/1000, "
             "163 to under 331 mm gives 1973/1000) * interpolate(D, at 103 mm gives 1043/1000, at 331 mm gives 2917/1000)) "
             "* k_n * x_n * pi * 2 * lookup(key Rough, key Smooth gives 1087/1000, key Rough gives 1249/1000) "
             "* snap(x_n, to 1437/1000, 1537/1000, 1637/1000) "
             "+ round(sqrt(E / R * x_n), to 1 dp of %) "
             "* critical(2, at 2, 3) * abs(E / R) * r(level; level = E / R)");
    CHECK(formula::render(cylinder, everyVocabulary) == "R / E");

    // Every series kind, the jurisdiction's symbol marked in each dialect.
    constexpr auto seriesVariant = std::get<2>(everyOverlaid.variantSet.cases).expression;
    CHECK(formula::render(seriesVariant, everyVocabulary)
          == "sum(cumulative(round(-m_n(i) + m_n(i) * values(1, 2, 3) * x_n, to 0/0/-1 dp of g), from last)) / M_n");
    CHECK(formula::render<formula::Dialect::Markdown>(seriesVariant, everyVocabulary)
          == "sum(cumulative(round(-`m_n(i)` + `m_n(i)` * values(1, 2, 3) * `x_n`, to 0/0/-1 dp of g), from last)) "
             "/ `M_n`");
    CHECK(formula::render<formula::Dialect::LaTeX>(seriesVariant, everyVocabulary)
          == "\\frac{\\sum \\operatorname{cumulative}_{\\text{from last}}(\\operatorname{round}_{0/0/-1\\,"
             "\\mathrm{g}}(-{m_n}_{i} + {m_n}_{i} \\cdot \\operatorname{values}(1,\\allowbreak 2,\\allowbreak 3) "
             "\\cdot x_n))}{M_n}");

    // Every sample statistic: the sample marked, the statistic not.
    constexpr auto sampleVariant = std::get<5>(everyOverlaid.variantSet.cases).expression;
    CHECK(formula::render(sampleVariant, everyVocabulary)
          == "sample_mean(m_n(i) * x_n) / M_n / sample_count(m_n(i)) + sample_variance(m_n(i) * x_n) / "
             "(sample_range(m_n(i) * x_n) * sample_range(m_n(i) * x_n)) + sample_count(without outliers(m_n(i) * x_n; "
             "abs(x - pass mean) > x_n / 3 * pass mean; most extreme per pass; keep on limit; at most 1; "
             "keep at least 2)) - 2");
    CHECK(formula::render<formula::Dialect::Markdown>(sampleVariant, everyVocabulary)
          == "sample_mean(`m_n(i)` * `x_n`) / `M_n` / sample_count(`m_n(i)`) + sample_variance(`m_n(i)` * `x_n`) "
             "/ (sample_range(`m_n(i)` * `x_n`) * sample_range(`m_n(i)` * `x_n`)) + sample_count(without outliers("
             "`m_n(i)` * `x_n`; abs(x - pass mean) > `x_n` / 3 * pass mean; most extreme per pass; keep on limit; "
             "at most 1; keep at least 2)) - 2");
    CHECK(formula::render<formula::Dialect::LaTeX>(sampleVariant, everyVocabulary)
          == "\\frac{\\frac{\\overline{{m_n}_{i} \\cdot x_n}}{M_n}}{n({m_n}_{i})} + \\frac{s^{2}({m_n}_{i} \\cdot x_n)}"
             "{\\operatorname{range}({m_n}_{i} \\cdot x_n) \\cdot \\operatorname{range}({m_n}_{i} \\cdot x_n)} "
             "+ n(\\operatorname{without\\ outliers}({m_n}_{i} \\cdot x_n;\\allowbreak "
             "\\left\\lvert x - \\bar{x}_{\\text{pass}}\\right\\rvert > \\frac{x_n}{3} \\cdot \\bar{x}_{\\text{pass}};\\allowbreak "
             "\\text{most extreme per pass};\\allowbreak \\text{keep on limit};\\allowbreak "
             "\\text{at most }1;\\allowbreak \\text{keep at least }2)) - 2");

    // Every curve kind, the series marked, the domains listed, the direction
    // stated.
    constexpr auto curveVariant = std::get<3>(everyOverlaid.variantSet.cases).expression;
    CHECK(formula::render(curveVariant, everyVocabulary)
          == "snap(interpolate(splice(curve(domain(1, 2, 4), m_n(i) / M_n), curve(domain(5), values(1/20)), "
             "non-decreasing), at x_n), to 1/200, 1/100)");
    CHECK(formula::render<formula::Dialect::Markdown>(curveVariant, everyVocabulary)
          == "snap(interpolate(splice(curve(domain(1, 2, 4), `m_n(i)` / `M_n`), curve(domain(5), values(1/20)), "
             "non-decreasing), at `x_n`), to 1/200, 1/100)");
    CHECK(formula::render<formula::Dialect::LaTeX>(curveVariant, everyVocabulary)
          == "\\operatorname{snap}(\\operatorname{interpolate}(\\operatorname{splice}(\\operatorname{curve}("
             "\\operatorname{domain}(\\mathrm{1,\\ 2,\\ 4}),\\allowbreak \\frac{{m_n}_{i}}{M_n}),\\allowbreak "
             "\\operatorname{curve}(\\operatorname{domain}(\\mathrm{5}),\\allowbreak "
             "\\operatorname{values}(1/20)),\\allowbreak \\mathrm{non-decreasing}),\\allowbreak \\mathrm{at\\ }x_n),"
             "\\allowbreak \\mathrm{to\\ 1/200,\\ 1/100})");

    // Binning, the observations marked, each class a band.
    constexpr auto binnedVariant = std::get<4>(everyOverlaid.variantSet.cases).expression;
    CHECK(formula::render(binnedVariant, everyVocabulary)
          == "sum(bin(d_n(i), 0 to under 163 mm, 163 to under 277 mm) * values(0, 1)) / sum(bin(d_n(i), 0 to under 163 mm, "
             "163 to under 277 mm)) * x_n");

    for (std::string const& text: { formula::render<formula::Dialect::Markdown>(binnedVariant, everyVocabulary),
                                    formula::render<formula::Dialect::LaTeX>(binnedVariant, everyVocabulary),
                                    formula::render<formula::Dialect::Markdown>(curveVariant, everyVocabulary),
                                    formula::render<formula::Dialect::LaTeX>(curveVariant, everyVocabulary),
                                    formula::render<formula::Dialect::Markdown>(cube, everyVocabulary),
                                    formula::render<formula::Dialect::LaTeX>(cube, everyVocabulary),
                                    formula::render<formula::Dialect::Markdown>(cylinder, everyVocabulary),
                                    formula::render<formula::Dialect::LaTeX>(cylinder, everyVocabulary),
                                    formula::render<formula::Dialect::Markdown>(seriesVariant, everyVocabulary),
                                    formula::render<formula::Dialect::LaTeX>(seriesVariant, everyVocabulary),
                                    formula::render<formula::Dialect::Markdown>(sampleVariant, everyVocabulary),
                                    formula::render<formula::Dialect::LaTeX>(sampleVariant, everyVocabulary) })
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
    CHECK(*plain.symbols[3].derivedAs == "D / 127 mm");
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

    // The series variant: the series read once as a row of its own, marked
    // with its length, the fixed factor, and the total.
    constexpr auto seriesVariant = std::get<2>(everyOverlaid.variantSet.cases).expression;
    formula::Documentation const seriesPage = formula::document(seriesVariant, everyVocabulary);
    CHECK(seriesPage.formula == formula::render(seriesVariant, everyVocabulary));
    REQUIRE(seriesPage.symbols.size() == 3);
    CHECK(seriesPage.symbols[0].symbol == "m_n");
    CHECK(seriesPage.symbols[0].shape == formula::ValueShape::Series);
    CHECK(seriesPage.symbols[0].length == 3);
    CHECK(seriesPage.symbols[1].symbol == "x_n");
    CHECK(seriesPage.symbols[1].fixedValue.has_value());
    CHECK(seriesPage.symbols[2].symbol == "M_n");
    CHECK(seriesPage.symbols[2].shape == formula::ValueShape::Single);

    // The curve variant: the series row, the total, and the fixed factor the
    // curve is read at.
    constexpr auto curveVariant = std::get<3>(everyOverlaid.variantSet.cases).expression;
    formula::Documentation const curvePage = formula::document(curveVariant, everyVocabulary);
    CHECK(curvePage.formula == formula::render(curveVariant, everyVocabulary));
    REQUIRE(curvePage.symbols.size() == 3);
    CHECK(curvePage.symbols[0].symbol == "m_n");
    CHECK(curvePage.symbols[0].shape == formula::ValueShape::Series);
    CHECK(curvePage.symbols[1].symbol == "M_n");
    CHECK(curvePage.symbols[2].symbol == "x_n");
    CHECK(curvePage.symbols[2].fixedValue.has_value());

    // The binning variant: the observations read once, as a row of their
    // capacity, and the fixed factor.
    constexpr auto binnedVariant = std::get<4>(everyOverlaid.variantSet.cases).expression;
    formula::Documentation const binnedPage = formula::document(binnedVariant, everyVocabulary);
    REQUIRE(binnedPage.symbols.size() == 2);
    CHECK(binnedPage.symbols[0].symbol == "d_n");
    CHECK(binnedPage.symbols[0].shape == formula::ValueShape::Observations);
    CHECK(binnedPage.symbols[0].length == 4);
    CHECK(binnedPage.symbols[1].symbol == "x_n");
    CHECK(binnedPage.symbols[1].fixedValue.has_value());

    // The sample variant: its rejection states its limit in the page's
    // words, the fixed factor as the jurisdiction names it.
    constexpr auto sampleVariant = std::get<5>(everyOverlaid.variantSet.cases).expression;
    formula::Documentation const samplePage = formula::document(sampleVariant, everyVocabulary);
    REQUIRE(samplePage.rejections.size() == 1);
    CHECK(samplePage.rejections[0].limit == "x_n / 3 * pass mean");


    for (auto const& documentation: { formula::document<formula::Dialect::Markdown>(binnedVariant, everyVocabulary),
                                      formula::document<formula::Dialect::LaTeX>(binnedVariant, everyVocabulary),
                                      formula::document<formula::Dialect::Markdown>(cube, everyVocabulary),
                                      formula::document<formula::Dialect::LaTeX>(cube, everyVocabulary),
                                      formula::document<formula::Dialect::Markdown>(cylinder, everyVocabulary),
                                      formula::document<formula::Dialect::LaTeX>(cylinder, everyVocabulary),
                                      formula::document<formula::Dialect::Markdown>(seriesVariant, everyVocabulary),
                                      formula::document<formula::Dialect::LaTeX>(seriesVariant, everyVocabulary),
                                      formula::document<formula::Dialect::Markdown>(sampleVariant, everyVocabulary),
                                      formula::document<formula::Dialect::LaTeX>(sampleVariant, everyVocabulary) })
    {
        CHECK(declares_no_symbol(documentation.formula));
        for (formula::RejectionEntry const& rejection: documentation.rejections)
            CHECK(declares_no_symbol(rejection.limit));
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
    // pinned whole: the rest of this derivation, some sixty steps, is
    // arithmetic, and pi's rational approximation would pin nothing about
    // vocabularies.
    std::string const cube = everyTraceOf<EveryCube>();
    CHECK(declares_no_symbol(cube));
    CHECK(cube.starts_with("1. E = 30 MPa\n"
                           "2. R = 12 MPa\n"));
    CHECK(cube.find("34. D = 241 mm\n") != std::string::npos);
    CHECK(cube.find("37. k_n = #36 = 241/127 [derived by jurisdiction overlay: Example Standard 12:2021 NA]\n")
          != std::string::npos);
    CHECK(cube.find("39. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n")
          != std::string::npos);
    // The snap over the fixed factor: the overlay's constant reached inside
    // it, and the tie rule decided.
    CHECK(cube.find("x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n")
          != cube.rfind("x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"));
    CHECK(cube.find("= 1537/1000 [1437/1000 to 1537/1000; tie, toward higher]\n") != std::string::npos);
    // The rounded root reads the fixed factor through its radicand, and its
    // step shows only exact numbers.
    CHECK(cube.find("53. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
                    "54. #52 * #53 = 1487/400\n"
                    "55. round(sqrt(#54), to 1 dp of %) = 964/5 % [nearest, ties away from zero]\n")
          != std::string::npos);
    // The critical value reads a count of 2.
    CHECK(cube.find(") = 60 [critical value at n = 2]\n") != std::string::npos);

    CHECK(everyTraceOf<EveryCylinder>()
          == "1. R = 12 MPa\n"
             "2. E = 30 MPa\n"
             "3. #1 / #2 = 2/5\n"
             "4. #3 = 2/5 [replaced by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "5. round(#4, in %) = 40 % [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "6. #5 = 40 % [variant EveryCylinder (2nd of 6), selected by tag]\n");

    // Every series kind: each series step in the jurisdiction's symbol, the
    // fixed factor broadcast once, the running total from the last screen,
    // and the sum a single value. Computed steps have no declared unit, so
    // they read in kilograms, exactly -- except a series scaled by a pure
    // number, which reads in its series' grams.
    // The statistics read the fixed factor through their sample, and each
    // reads its sample's own step, with every element. The rejection reads
    // the fixed factor in its sample and again in each pass's limit, and its
    // pass means read, as its sample does, in grams.
    CHECK(everyTraceOf<EverySample>()
          == "1. m_n = 10 g; 20 g; 40 g\n"
             "2. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "3. #1 * #2 = 1487/100 g; 1487/50 g; 1487/25 g\n"
             "4. sample_mean(#3) = 10409/300 g\n"
             "5. M_n = 2020 g\n"
             "6. #4 / #5 = 10409/606000\n"
             "7. m_n = 10 g; 20 g; 40 g\n"
             "8. sample_count(#7) = 3\n"
             "9. #6 / #8 = 10409/1818000\n"
             "10. m_n = 10 g; 20 g; 40 g\n"
             "11. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "12. #10 * #11 = 1487/100 g; 1487/50 g; 1487/25 g\n"
             "13. sample_variance(#12) = 15478183/30000000000\n"
             "14. m_n = 10 g; 20 g; 40 g\n"
             "15. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "16. #14 * #15 = 1487/100 g; 1487/50 g; 1487/25 g\n"
             "17. sample_range(#16) = 4461/100 g\n"
             "18. m_n = 10 g; 20 g; 40 g\n"
             "19. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "20. #18 * #19 = 1487/100 g; 1487/50 g; 1487/25 g\n"
             "21. sample_range(#20) = 4461/100 g\n"
             "22. #17 * #21 = 19900521/10000000000\n"
             "23. #13 / #22 = 7/27\n"
             "24. #9 + #23 = 1445227/5454000\n"
             "25. m_n = 10 g; 20 g; 40 g\n"
             "26. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "27. #25 * #26 = 1487/100 g; 1487/50 g; 1487/25 g\n"
             "28. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "29. 3\n"
             "30. #28 / #29 = 1487/3000\n"
             "31. pass mean = 10409/300 g\n"
             "32. #30 * #31 = 15478183/900000000\n"
             "33. pass 1: 3 values, mean 10409/300 g\n"
             "34. rejected element 3 of 3 (1487/25 g) in pass 1: abs(x - mean) = 1487/60 g > 15478183/900000 g (deviation from mean)\n"
             "35. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "36. 3\n"
             "37. #35 / #36 = 1487/3000\n"
             "38. pass mean = 4461/200 g\n"
             "39. #37 * #38 = 2211169/200000000\n"
             "40. pass 2: 2 values, mean 4461/200 g\n"
             "41. settled: 1 rejected, 2 remain\n"
             "42. sample_count(#41) = 2\n"
             "43. 2\n"
             "44. #42 - #43 = 0\n"
             "45. #24 + #44 = 1445227/5454000\n"
             "46. round(#45, in %) = 53/2 % [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "47. #46 = 53/2 % [variant EverySample (6th of 6), selected by tag]\n");
    CHECK(everyTraceOf<EverySeries>()
          == "1. m_n = 10 g; 20 g; 40 g\n"
             "2. -#1 = -1/100; -1/50; -1/25\n"
             "3. m_n = 10 g; 20 g; 40 g\n"
             "4. 1; 2; 3\n"
             "5. #3 * #4 = 10 g; 40 g; 120 g\n"
             "6. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "7. #5 * #6 = 1487/100 g; 1487/25 g; 4461/25 g\n"
             "8. #2 + #7 = 487/100000; 987/25000; 3461/25000\n"
             "9. round(#8, to 0/0/-1 dp of g) = 5 g; 39 g; 140 g [nearest, ties away from zero]\n"
             "10. cumulative(#9, from last) = 184 g; 179 g; 140 g\n"
             "11. sum(#10) = 503 g\n"
             "12. M_n = 2020 g\n"
             "13. #11 / #12 = 503/2020\n"
             "14. round(#13, in %) = 249/10 % [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "15. #14 = 249/10 % [variant EverySeries (3rd of 6), selected by tag]\n");

    // Every curve kind: the retained masses as shares of the total, 1/202,
    // 1/101 and 2/101 at 1, 2 and 4, spliced with 1/20 at 5; read at the
    // fixed 1487/1000, 487/1000 of the way from 1 to 2, that is 1487/202000
    // -- nearer 1/200 than 1/100, whose midpoint 3/400 it falls just short of.
    CHECK(everyTraceOf<EveryCurve>()
          == "1. 1; 2; 4\n"
             "2. m_n = 10 g; 20 g; 40 g\n"
             "3. M_n = 2020 g\n"
             "4. #2 / #3 = 1/202; 1/101; 2/101\n"
             "5. curve(#1, #4) = 1: 1/202; 2: 1/101; 4: 2/101\n"
             "6. 5\n"
             "7. 1/20\n"
             "8. curve(#6, #7) = 5: 1/20\n"
             "9. splice(#5, #8, non-decreasing) = 1: 1/202; 2: 1/101; 4: 2/101; 5: 1/20\n"
             "10. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "11. interpolate(#9, at #10) = 1487/202000 [between 1 and 2]\n"
             "12. snap(#11) = 1/200 [1/200 to 1/100; nearer 1/200]\n"
             "13. round(#12, in %) = 1/2 % [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "14. #13 = 1/2 % [variant EveryCurve (4th of 6), selected by tag]\n");

    // Binning: 103, 163, 197 and 127 mm counted 2 and 2 -- the 163 mm particle
    // in the upper class -- the upper class's share 1/2, times the fixed 1487/1000.
    // Counted closed at the top, it would be 1/4, and 37.2 %.
    CHECK(everyTraceOf<EveryBinned>()
          == "1. d_n = 103 mm; 163 mm; 197 mm; 127 mm\n"
             "2. bin(#1) = 2; 2\n"
             "3. 0; 1\n"
             "4. #2 * #3 = 0; 2\n"
             "5. sum(#4) = 2\n"
             "6. d_n = 103 mm; 163 mm; 197 mm; 127 mm\n"
             "7. bin(#6) = 2; 2\n"
             "8. sum(#7) = 4\n"
             "9. #5 / #8 = 1/2\n"
             "10. x_n = 1487/1000 [fixed by jurisdiction overlay: Example Standard 12:2021 NA]\n"
             "11. #9 * #10 = 1487/2000\n"
             "12. round(#11, in %) = 372/5 % [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "13. #12 = 372/5 % [variant EveryBinned (5th of 6), selected by tag]\n");
}

TEST_CASE("a precision limit's checks see inside every node kind", "[vocabulary][precision]")
{
    // The every-kind method, overlaid -- its series variant holding every
    // series kind -- and the method's rounding around it: every node kind
    // this library ships. One that fell to `LevelChildren`'s
    // primary would hide a placeholder from the level checks without a word,
    // as the overlay's derived quantity once did.
    using Cube = std::remove_cvref_t<decltype(std::get<0>(everyOverlaid.variantSet.cases).expression)>;
    using Cylinder = std::remove_cvref_t<decltype(std::get<1>(everyOverlaid.variantSet.cases).expression)>;
    using Series = std::remove_cvref_t<decltype(std::get<2>(everyOverlaid.variantSet.cases).expression)>;
    using Curve = std::remove_cvref_t<decltype(std::get<3>(everyOverlaid.variantSet.cases).expression)>;
    using Binned = std::remove_cvref_t<decltype(std::get<4>(everyOverlaid.variantSet.cases).expression)>;
    using Sample = std::remove_cvref_t<decltype(std::get<5>(everyOverlaid.variantSet.cases).expression)>;
    using Rounded = formula::
        RoundingRuleNode<unit::Percent, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero, Cube>;
    STATIC_REQUIRE(formula::detail::level_check_sees_every_node<Cube>());
    STATIC_REQUIRE(formula::detail::level_check_sees_every_node<Cylinder>());
    STATIC_REQUIRE(formula::detail::level_check_sees_every_node<Series>());
    STATIC_REQUIRE(formula::detail::level_check_sees_every_node<Curve>());
    STATIC_REQUIRE(formula::detail::level_check_sees_every_node<Binned>());
    STATIC_REQUIRE(formula::detail::level_check_sees_every_node<Sample>());
    STATIC_REQUIRE(formula::detail::level_check_sees_every_node<Rounded>());
    // A consumer's node kind is unseen by design, which is what makes the
    // six above able to fail.
    STATIC_REQUIRE_FALSE(formula::detail::level_check_sees_every_node<decltype(Gauge {} * var<Strength>)>());
    // Told apart by the namespace a kind is declared in, never by its
    // template arguments' -- which is what keeps a consumer's node over a
    // library node walking as unseen, where a library kind without its
    // specialisation is refused.
    STATIC_REQUIRE(formula::detail::declared_in_library<formula::VarNode<Strength>>());
    STATIC_REQUIRE(formula::detail::declared_in_library<Cube>());            // the every-kind expression
    STATIC_REQUIRE_FALSE(formula::detail::declared_in_library<EveryCube>()); // a tag of this file's
    STATIC_REQUIRE(formula::detail::declared_in_library<formula::detail::RefusedSeries<formula::dim::Mass>>());
    STATIC_REQUIRE_FALSE(formula::detail::declared_in_library<Gauge>());
    STATIC_REQUIRE_FALSE(formula::detail::declared_in_library<std::tuple<formula::VarNode<Strength>>>());
    // Nor is one in a limit expression hidden from it, where a placeholder is
    // bound rather than free.
    STATIC_REQUIRE_FALSE(formula::detail::level_check_sees_every_node<
                         decltype(formula::precision_limit<formula::PrecisionKind::Repeatability>(
                             var<Strength>, Gauge {} * var<Strength>))>());
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

// ---- A series in two jurisdictions' words (phase 12) ----

namespace
{
namespace series_vocabulary
{
    struct Retained: formula::Quantity<Retained, "m_r", "mass retained on a screen", unit::Gram>
    {
    };
    struct Sieved: formula::Quantity<Sieved, "m_s", "mass passing a screen", unit::Gram>
    {
    };

    // Crossed over, as `north` and `south` above are.
    inline constexpr auto east = formula::vocabulary(formula::renames<Retained>("R"), formula::renames<Sieved>("S"));
    inline constexpr auto west = formula::vocabulary(formula::renames<Retained>("S"), formula::renames<Sieved>("R"));
} // namespace series_vocabulary
} // namespace

TEST_CASE("a series is written in the page's vocabulary, marked, in every dialect", "[series][vocabulary]")
{
    using series_vocabulary::east;
    using series_vocabulary::Retained;
    using series_vocabulary::Sieved;
    using series_vocabulary::west;
    CHECK(formula::render(formula::series<Retained, 5>, east) == "R(i)");
    CHECK(formula::render(formula::series<Retained, 5>, west) == "S(i)");
    CHECK(formula::render(formula::series<Sieved, 5>, west) == "R(i)");
    CHECK(formula::render<formula::Dialect::Markdown>(formula::series<Retained, 5>, west) == "`S(i)`");
    CHECK(formula::render<formula::Dialect::LaTeX>(formula::series<Retained, 5>, west) == "{S}_{i}");

    auto const page = formula::document(formula::series<Retained, 5>, west);
    CHECK(page.formula == "S(i)");
    REQUIRE(page.symbols.size() == 1);
    CHECK(page.symbols[0].symbol == "S");
    CHECK(page.symbols[0].description == "mass retained on a screen"); // what it is does not change
}

TEST_CASE("elementwise arithmetic is written in the page's vocabulary on every surface", "[series][vocabulary]")
{
    using series_vocabulary::east;
    using series_vocabulary::Retained;
    using series_vocabulary::Sieved;
    using series_vocabulary::west;
    constexpr auto ratio = formula::series<Retained, 3> / formula::series<Sieved, 3>;
    CHECK(formula::render(ratio, east) == "R(i) / S(i)");
    CHECK(formula::render(ratio, west) == "S(i) / R(i)");
    CHECK(formula::render<formula::Dialect::LaTeX>(ratio, west) == "\\frac{{S}_{i}}{{R}_{i}}");

    auto const page = formula::document(ratio, west);
    REQUIRE(page.symbols.size() == 2);
    CHECK(page.symbols[0].symbol == "S");
    CHECK(page.symbols[1].symbol == "R");

    constexpr auto inputs = formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { rat(1) },
                                                                                    formula::Measured<Retained> { rat(2) },
                                                                                    formula::Measured<Retained> { rat(3) }),
                                                 formula::measured_series<Sieved>(formula::Measured<Sieved> { rat(4) },
                                                                                  formula::Measured<Sieved> { rat(5) },
                                                                                  formula::Measured<Sieved> { rat(6) }));
    formula::Trace<> trace {};
    (void) formula::detail::dispatch_series<formula::Rational>(ratio, inputs, formula::RecordingSink { trace, west });
    std::string const text = formula::render_trace(trace, { .maxSteps = 30 });
    CHECK(text.find("1. S = 1 g; 2 g; 3 g") != std::string::npos);
    CHECK(text.find("2. R = 4 g; 5 g; 6 g") != std::string::npos);
    CHECK(text.find("m_r") == std::string::npos);
    CHECK(text.find("m_s") == std::string::npos);
}

// ---- The join: a series inside a method, under an overlay (phase 12) ----
//
// Phase 11's lesson: two separately verified things do not verify their join.
// `sum` is the first series-holding `Node`, so it is where a series first sits
// inside a method's variant; here it is evaluated, rendered, documented and
// traced through a method an overlay rewrote, in a jurisdiction's words.

namespace
{
namespace series_join
{
    using series_vocabulary::Retained;
    struct TotalMass: formula::Quantity<TotalMass, "m_t", "total dry mass", unit::Gram>
    {
    };

    // The total divided out of the sum, and the total divided into every
    // element before summing: the second puts `var<TotalMass>` inside an
    // elementwise node inside `sum`, where the overlay must reach.
    struct OfTheSum
    {
    };
    struct OfEachElement
    {
    };

    inline constexpr auto shareMethod = formula::method(
        formula::variants(
            formula::variant<OfTheSum>(formula::sum(formula::series<Retained, 5>) / formula::var<TotalMass>),
            formula::variant<OfEachElement>(formula::sum(formula::series<Retained, 5> / formula::var<TotalMass>))),
        formula::rounding_rule<unit::Percent, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());

    // The laboratory weighed 1250 g; the jurisdiction fixes 1606 g, so the
    // share is 803/1606 = 1/2 where the overlay reached, and 803/1250 where
    // it did not.
    inline constexpr auto nationally =
        formula::apply(formula::overlay(formula::with_constant<TotalMass>(
                           rat(1606), formula::Citation { .reference = "Example Standard 5" })),
                       shareMethod);

    inline constexpr auto screens =
        formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { rat(130) },
                                                                formula::Measured<Retained> { rat(210) },
                                                                formula::Measured<Retained> { rat(95) },
                                                                formula::Measured<Retained> { rat(340) },
                                                                formula::Measured<Retained> { rat(28) }),
                             formula::Measured<TotalMass> { rat(1250) });

    template <typename Tag>
    [[nodiscard]] std::string traceOf()
    {
        formula::Trace<> trace {};
        (void) formula::evaluate_method<Tag>(nationally, screens, formula::RecordingSink { trace, series_vocabulary::east });
        return formula::render_trace(trace, { .maxSteps = 60 });
    }
} // namespace series_join
} // namespace

TEST_CASE("a sum of a series evaluates inside a method, under an overlay's constant", "[series][vocabulary]")
{
    using series_join::OfEachElement;
    using series_join::OfTheSum;
    // Unoverlaid, both read the laboratory's 1250 g: 803/1250 is 64.24 %,
    // which the method's rule rounds to 64.2 %.
    CHECK(formula::evaluate_method<OfTheSum>(series_join::shareMethod, series_join::screens)
          == formula::detail::present(formula::Rational { 321, 500 }));
    CHECK(formula::evaluate_method<OfEachElement>(series_join::shareMethod, series_join::screens)
          == formula::detail::present(formula::Rational { 321, 500 }));
    // Overlaid, both read the jurisdiction's 1606 g -- the second only if the
    // constant reached inside the elementwise division inside the sum.
    CHECK(formula::evaluate_method<OfTheSum>(series_join::nationally, series_join::screens)
          == formula::detail::present(formula::Rational { 1, 2 }));
    CHECK(formula::evaluate_method<OfEachElement>(series_join::nationally, series_join::screens)
          == formula::detail::present(formula::Rational { 1, 2 }));
}

TEST_CASE("a sum of a series renders and documents inside an overlaid method, in the page's words", "[series][vocabulary]")
{
    constexpr auto ofTheSum = std::get<0>(series_join::nationally.variantSet.cases).expression;
    constexpr auto ofEachElement = std::get<1>(series_join::nationally.variantSet.cases).expression;
    CHECK(formula::render(ofTheSum, series_vocabulary::east) == "sum(R(i)) / m_t");
    CHECK(formula::render(ofEachElement, series_vocabulary::east) == "sum(R(i) / m_t)");
    CHECK(formula::render(ofTheSum, series_vocabulary::west) == "sum(S(i)) / m_t");
    CHECK(formula::render<formula::Dialect::LaTeX>(ofTheSum, series_vocabulary::east) == "\\frac{\\sum {R}_{i}}{m_t}");
    CHECK(formula::render<formula::Dialect::LaTeX>(ofEachElement, series_vocabulary::east) == "\\sum \\frac{{R}_{i}}{m_t}");

    formula::Documentation const page = formula::document(ofEachElement, series_vocabulary::east);
    CHECK(page.formula == "sum(R(i) / m_t)");
    REQUIRE(page.symbols.size() == 2);
    CHECK(page.symbols[0].symbol == "R");
    CHECK(page.symbols[0].shape == formula::ValueShape::Series);
    CHECK(page.symbols[0].length == 5);
    CHECK(page.symbols[1].symbol == "m_t");
    CHECK(page.symbols[1].fixedValue.has_value());
}

TEST_CASE("a sum of a series traces inside an overlaid method, in the sink's words", "[series][vocabulary][trace]")
{
    CHECK(series_join::traceOf<series_join::OfTheSum>()
          == "1. R = 130 g; 210 g; 95 g; 340 g; 28 g\n"
             "2. sum(#1) = 803 g\n"
             "3. m_t = 1606 g [fixed by jurisdiction overlay: Example Standard 5]\n"
             "4. #2 / #3 = 1/2\n"
             "5. round(#4, in %) = 50 % [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "6. #5 = 50 % [variant OfTheSum (1st of 2), selected by tag]\n");
    CHECK(series_join::traceOf<series_join::OfEachElement>()
          == "1. R = 130 g; 210 g; 95 g; 340 g; 28 g\n"
             "2. m_t = 1606 g [fixed by jurisdiction overlay: Example Standard 5]\n"
             "3. #1 / #2 = 65/803; 105/803; 95/1606; 170/803; 14/803\n"
             "4. sum(#3) = 1/2\n"
             "5. round(#4, in %) = 50 % [rounded to 1 dp (method default); nearest, ties away from zero]\n"
             "6. #5 = 50 % [variant OfEachElement (2nd of 2), selected by tag]\n");
}
