// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// One overlaid method and one vocabulary, `inline constexpr` in a header and
/// used from two translation units: `join_tests.cpp` and
/// `method_cross_tu_b.cpp`. Each unit renders, documents and traces the method
/// through its own copy of the helpers at the bottom, and `join_tests.cpp`
/// compares the texts, and the method crosses between them in both
/// directions.
///
/// **What this catches, measured** by a deliberate split in
/// `method_cross_tu_b.cpp` alone, on cl 19.51, g++ 13.3 and clang++ 20.1.8
/// (see `evaluate_joined_in_other_tu` for the details):
///
///  - **A split in the method's type** -- the overlay's rounding at 3 dp in
///    one unit and 2 in the other -- fails to link on all three.
///  - **A split in a value that is not part of the type** is not caught. A
///    different citation text in one unit passed on all three; the Cube and
///    Cylinder swapped in one unit, which changes the published layout,
///    failed only on g++ in release and passed on cl and clang++. Such a
///    split is an ODR violation no diagnostic is required for: `joined` is an
///    `inline` variable, the linker keeps one definition for both units, and
///    whether a unit reads that one or its own constant-folded copy is the
///    optimiser's choice. The comparison of texts is no guard against it.
///
/// Two quantities' symbol-table identities folded into one by the linker
/// would merge two rows of the page `join_tests.cpp` pins in full; whether
/// any preset's linker folds identical data is not established here, so that
/// half is a guard, not a measurement.
///
/// **Everything shared lives in a named namespace**, never an anonymous one:
/// an anonymous namespace in a header is a different namespace in every unit
/// that includes it, so its tags and quantities would be different types per
/// unit and the method would not be one method.

#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <string>
#include <type_traits>

namespace join_cross_tu
{
namespace unit = formula::unit;
using formula::var;

struct Cube
{
};
struct Cylinder
{
};
struct Prism
{
};

// Every declared symbol ends `_decl` and the vocabularies below rename every
// one, so any text written past a vocabulary shows `_decl`.
struct Strength: formula::Quantity<Strength, "A_decl", "compressive strength", unit::Megapascal>
{
};
struct Modulus: formula::Quantity<Modulus, "B_decl", "elastic modulus", unit::Megapascal>
{
};
struct Diameter: formula::Quantity<Diameter, "D_decl", "specimen diameter", unit::Millimetre>
{
};
struct SizeFactor: formula::Quantity<SizeFactor, "K_decl", "size factor", unit::One>
{
};
struct NationalFactor: formula::Quantity<NationalFactor, "X_decl", "national factor", unit::One>
{
};

inline constexpr formula::BandTable<2> JoinBands { formula::band(100, 1, 150, 1), formula::band(150, 1, 300, 1) };

/// Three variants, reporting a dimensionless ratio rounded in percent. Only
/// the Cylinder survives the overlays below, so a trace that says "2nd of 3"
/// is counting the method as published, not as overlaid.
inline constexpr auto baseMethod = formula::method(
    formula::variants(formula::variant<Cube>(var<Strength> / var<Modulus>),
                      formula::variant<Cylinder>(var<Modulus> / var<Strength>),
                      formula::variant<Prism>(var<Strength> / var<Strength>)),
    formula::rounding_rule<unit::Percent, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());

inline constexpr formula::Citation constantAnnex { .reference = "Example Standard 12:2021 NA", .section = "NA.2.1" };
inline constexpr formula::Citation derivedAnnex { .reference = "Example Standard 12:2021 NA", .section = "NA.2.2" };
inline constexpr formula::Citation replacementAnnex { .reference = "Example Standard 12:2021 NA", .section = "NA.3" };
inline constexpr formula::Citation roundingAnnex { .reference = "Example Standard 12:2021 NA", .section = "NA.4" };

/// Every overlay operation, in one jurisdiction. The replacement is listed
/// first, so that the fixed constant and the derived quantity listed after it
/// are substituted inside it: operations apply in the order listed.
inline constexpr auto southernOverlay = formula::overlay(
    formula::replace_variant<Cylinder>(var<NationalFactor> * var<SizeFactor> * var<Strength>
                                           / (var<Modulus>
                                              * formula::banded_lookup<unit::Millimetre, JoinBands, unit::One>(
                                                  var<Diameter>, { formula::Rational { 1 }, formula::Rational { 2 } })),
                                       replacementAnnex),
    formula::with_constant<NationalFactor>(formula::Rational { 3, 2 }, constantAnnex),
    formula::add_derived<SizeFactor>(var<Diameter> / formula::constant<unit::Millimetre>(formula::Rational { 100 }),
                                     derivedAnnex),
    formula::prune_variant<Prism>(formula::Citation { .reference = "Example Standard 12:2021 NA", .section = "NA.1.2" }),
    formula::with_rounding<unit::Percent, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
        roundingAnnex));

/// A second overlay pinning what the first left: a pin and a prune may not
/// share one overlay, so the pin comes from another.
inline constexpr auto pinCylinder = formula::overlay(
    formula::pin_variant<Cylinder>(formula::Citation { .reference = "Example Standard 12:2024 NA", .section = "NA.1.1" }));

inline constexpr auto joined = formula::apply(pinCylinder, formula::apply(southernOverlay, baseMethod));

/// The two jurisdictions' words for the same two quantities, crossed over.
inline constexpr auto south = formula::vocabulary(formula::renames<Strength>("E"),
                                                  formula::renames<Modulus>("R"),
                                                  formula::renames<Diameter>("D"),
                                                  formula::renames<SizeFactor>("k_n"),
                                                  formula::renames<NationalFactor>("x_n"));
inline constexpr auto north = formula::vocabulary(formula::renames<Strength>("R"),
                                                  formula::renames<Modulus>("E"),
                                                  formula::renames<Diameter>("D"),
                                                  formula::renames<SizeFactor>("k_n"),
                                                  formula::renames<NationalFactor>("x_n"));

// 30 MPa, 11 MPa and 200 mm, distinct from each other and from every table row.
// The Cylinder then reports 90/22 = 4.0909...: 409.09 % under the overlay's two
// decimal places and 409.1 % under the base method's one, so which rule
// rounded shows in the number and not only in the trace's words.
inline constexpr auto inputs = formula::environment(formula::Measured<Strength> { formula::Rational { 30 } },
                                                    formula::Measured<Modulus> { formula::Rational { 11 } },
                                                    formula::Measured<Diameter> { formula::Rational { 200 } });

// The helpers are `static`, and deliberately so: each translation unit gets its
// own copy, compiled against its own view of the shared declarations above.
// Were they `inline`, the linker would keep one copy for both units and the
// comparison in `join_tests.cpp` would compare a function with itself. What
// they read, `joined`, is one `inline` variable all the same -- see the file
// comment for what that leaves the comparison unable to see.

/// The selected variant's page under @p vocabulary: the rendered formula, then
/// one line per symbol-table row, then how many replacements it records.
template <typename V>
[[nodiscard]] static std::string page(V const& vocabulary)
{
    auto const& expression = std::get<0>(joined.variantSet.cases).expression;
    formula::Documentation const documentation = formula::document(expression, vocabulary);
    std::string text = formula::render(expression, vocabulary) + "\n";
    for (formula::SymbolEntry const& row: documentation.symbols)
    {
        text += std::string { row.symbol } + ": " + std::string { row.description };
        if (row.fixedValue.has_value())
            text += " (fixed)";
        if (row.derivedAs.has_value())
            text += " (derived as " + *row.derivedAs + ")";
        text += "\n";
    }
    text += "replacements: " + std::to_string(documentation.replacedBy.size()) + "\n";
    return text;
}

/// The derivation of the pinned Cylinder under @p vocabulary, rendered.
template <typename V>
[[nodiscard]] static std::string trace(V const& vocabulary)
{
    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Cylinder>(joined, inputs, formula::RecordingSink { recorded, vocabulary });
    return formula::render_trace(recorded, { .maxSteps = 30 });
}
} // namespace join_cross_tu

/// Defined in `method_cross_tu_b.cpp`: the overlaid method as that
/// translation unit holds it, for `join_tests.cpp` to evaluate. It carries
/// the method's type only in its return type, and a return type is part of a
/// function's linker name on cl but not under the Itanium ABI g++ and clang++
/// use -- so on those two, units that disagree about the method's type still
/// link through this function. It is not the check that they agree;
/// `evaluate_joined_in_other_tu` below is.
[[nodiscard]] std::remove_cvref_t<decltype(join_cross_tu::joined)> joined_in_other_tu();

/// Defined in `method_cross_tu_b.cpp`: @p handed, evaluated there for the
/// Cylinder against `inputs`. **The check that both units hold the method
/// as one type.** The method's type is a parameter type, and a parameter type
/// is part of the linker name on every ABI, so units that disagree about it
/// declare two different functions: the one `join_tests.cpp` calls is never
/// defined, and the test does not link.
///
/// Measured by a deliberate split in `method_cross_tu_b.cpp` alone -- the
/// overlay's `with_rounding` at 3 decimal places there and 2 here, which
/// changes the method's type -- on cl 19.51, g++ 13.3 and clang++ 20.1.8:
/// each fails to link, naming this function as an unresolved or undefined
/// symbol. With this call removed and only `joined_in_other_tu` left, the
/// same split failed to link on cl alone; g++ and clang++ linked it, and the
/// test failed at run time only because that split happens to change the
/// number. A split in a value that is not part of the type leaves both units
/// declaring the same function, and is not caught -- see the file comment.
[[nodiscard]] formula::Evaluated<formula::Rational> evaluate_joined_in_other_tu(
    std::remove_cvref_t<decltype(join_cross_tu::joined)> const& handed);

/// Defined in `method_cross_tu_b.cpp`: `join_cross_tu::page(south)` and
/// `join_cross_tu::trace(south)` as that translation unit computes them.
[[nodiscard]] std::string join_page_in_other_tu();
[[nodiscard]] std::string join_trace_in_other_tu();
