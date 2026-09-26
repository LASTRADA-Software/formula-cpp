// SPDX-License-Identifier: Apache-2.0
//
// A consumer's translation unit with ordinary globals named the way code
// commonly names things -- `result`, `value`, `index` -- declared before every
// public header, which it then includes and whose main entry points it uses.
// A local or parameter in a header with one of those names hides the global.
// cl at /W4 reports that as C4459 -- in the header, but in the consumer's
// build -- and g++ at -Wshadow, for some of them, as "shadows a global
// declaration", so under /WX or -Werror a consumer's own global broke the
// build of a library it only included. This translation unit compiles with
// warnings as errors, like every test here, so it fails to build if a header
// declares one of these names again. `consumer_globals_run_tests.cpp` checks
// what it computed.
//
// Measured against the headers before their names were changed: cl 19.51
// reported 292 declarations across 30 headers; g++ 13.3 at -Wshadow reported
// four of them -- constructor parameters, with the globals declared before
// the headers as here; clang++ 20.1.8 at -Wshadow and clang-cl 22.1.3 at /W4
// reported nothing. So this guards cl builds, and g++ builds for those four.
//
// The globals come after the standard headers and this test's own, whose
// names are theirs. `index` is declared for cl-compatible compilers only:
// glibc's <cstring> declares a function of that name at global scope.
//
// Every public header is included by name, and `hygiene.consumer-globals`
// fails when one is missing from the list below.
#include "consumer_globals.hpp"

#include <cstdint>
#include <string>

// clang-format off
int result, value, text, step, mark, first, last, count, size, name, key, left, right, lhs, rhs, operand, outcome,
    error, symbol, position, converted, selected, selectedTag, expression, selection, outcomes, entry, rendered, row,
    band, keys, found, digits, scaled, numerator, denominator, quotient, sign, width, total, table, i, n, m,
    environment, sink;
#if defined(_MSC_VER)
int index;
#endif
// clang-format on

#include <formula-cpp/band.hpp>
#include <formula-cpp/citation.hpp>
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/document.hpp>
#include <formula-cpp/enumerator.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/tag.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>
#include <formula-cpp/unit.hpp>
#include <formula-cpp/version.hpp>
#include <formula-cpp/vocabulary.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Cube
{
};
struct Cylinder
{
};

// A name of its own, not the `Shape` other test files use: two translation
// units' anonymous-namespace key enumerations of one name, with tables of the
// same values, fail to link on clang -- see `lookup.hpp`.
enum class SpecimenForm : std::uint8_t
{
    Square,
    Round,
};

struct Force: formula::Quantity<Force, "F", "applied force", unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", unit::Millimetre>
{
};
struct Factor: formula::Quantity<Factor, "k", "a factor", unit::One>
{
};
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", unit::Megapascal>
{
};

inline constexpr formula::KeyTable<SpecimenForm, 2> SpecimenFormKeys { SpecimenForm::Square, SpecimenForm::Round };
inline constexpr formula::BandTable<2> Bands { formula::band(0, 1, 2, 1), formula::band(2, 1, 6, 1) };
inline constexpr formula::BreakpointTable<2> Points { formula::breakpoint(0), formula::breakpoint(8) };

/// A formula touching every node kind the evaluator, renderer and trace know:
/// arithmetic, a power and a root, a documented citation, rounding both
/// ways, a conditional, the escape hatch, and all three lookups.
inline constexpr auto everything = formula::documented(
    formula::rounded<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
        var<Force> / formula::pow<2>(var<EdgeX>)
        * formula::when(
            var<Factor> > formula::number(formula::Rational { 0 }), var<Factor>, formula::number(formula::Rational { 1 }))
        * formula::sqrt(formula::pow<2>(var<Factor>))
        * formula::exact_lookup<SpecimenFormKeys, unit::One>(SpecimenForm::Round,
                                                             { formula::Rational { 1 }, formula::Rational { 1 } })
        * formula::banded_lookup<unit::One, Bands, unit::One>(var<Factor>,
                                                              { formula::Rational { 1 }, formula::Rational { 1 } })
        * formula::interpolating_lookup<unit::One, Points, unit::One>(var<Factor>,
                                                                      { formula::Rational { 1 }, formula::Rational { 1 } })
        * formula::rounded_to_digits<unit::One, formula::SignificantDigits { 3 }, formula::RoundingMode::HalfAwayFromZero>(
            formula::numeric_value_of<unit::One, "Example Standard 1 states it bare">(var<Factor>))),
    formula::Citation { .title = "Everything", .reference = "Example Standard 1:2020", .section = "1" });

inline constexpr auto limit = formula::constraint(var<Force> >= formula::constant<unit::Newton>(formula::Rational { 1 }),
                                                  formula::Verdict { "no load" });

inline constexpr auto baseMethod = formula::method(
    formula::variants(formula::variant<Cube>(var<Factor> * var<Force> / (var<EdgeX> * var<EdgeX>) ),
                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
    formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints(limit));

/// Every overlay operation, so that `apply` instantiates each one's
/// `apply_operation` and every result check.
inline constexpr auto everyOperation = formula::overlay(
    formula::with_constraints(formula::constraints(limit), formula::Citation { .reference = "Example Standard 2" }),
    formula::replace_variant<Cylinder>(formula::number(formula::Rational { 2 }) * var<Force> / (var<EdgeX> * var<EdgeX>) ),
    formula::with_constant<Factor>(formula::Rational { 1 }),
    formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>());

inline constexpr auto overlaid = formula::apply(everyOperation, baseMethod);
inline constexpr auto pinned = formula::apply(formula::overlay(formula::pin_variant<Cube>()), baseMethod);
inline constexpr auto pruned = formula::apply(formula::overlay(formula::prune_variant<Cylinder>()), baseMethod);
inline constexpr auto derived =
    formula::apply(formula::overlay(formula::add_derived<Factor>(var<EdgeX> / var<EdgeX>)), baseMethod);

inline constexpr auto specimen = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                      formula::Measured<EdgeX> { formula::Rational { 150 } },
                                                      formula::Measured<Factor> { formula::Rational { 1 } });

inline constexpr auto north = formula::vocabulary(formula::renames<Force>("P"));

/// A sink that asks for both pairs of method hooks, and nothing else of its
/// own, so that `evaluate_method`'s and `check_method`'s hooked branches are
/// instantiated for a sink other than `RecordingSink`.
struct MethodHooks: formula::NullSink
{
    std::string* events;

    void variant_entered(formula::VariantSelection const&) const
    {
        *events += "v";
    }
    void variant_produced(formula::VariantSelection const&, formula::Evaluated<formula::Rational> const&) const
    {
        *events += "V";
    }
    void acceptance_entered(formula::ConstraintOrigin const&) const
    {
        *events += "a";
    }
    void acceptance_produced(formula::ConstraintOrigin const&) const
    {
        *events += "A";
    }
};
} // namespace

ConsumerGlobalsProbe probe_consumer_globals()
{
    ConsumerGlobalsProbe probe {};
    // Evaluation, traced and not.
    auto const plain = formula::evaluate<Strength>(everything, specimen);
    auto const checked = formula::checked_evaluate<Strength>(everything, specimen);
    auto const explained = formula::explain<Strength>(everything, specimen, north);
    probe.checks.push_back(plain.is_value());
    probe.checks.push_back(checked.has_value());
    probe.checks.push_back(!explained.trace.empty());

    // Every surface that writes text, in every dialect.
    std::string pages = formula::render(everything) + formula::render(everything, north)
                        + formula::render<formula::Dialect::Markdown>(everything)
                        + formula::render<formula::Dialect::LaTeX>(everything) + formula::render(limit);
    auto const documentation = formula::document(everything, north);
    auto const markdownDocumentation = formula::document<formula::Dialect::Markdown>(everything);
    auto const latexDocumentation = formula::document<formula::Dialect::LaTeX>(everything);
    probe.checks.push_back(!pages.empty());
    probe.checks.push_back(!documentation.symbols.empty());
    probe.checks.push_back(markdownDocumentation.symbols.size() == latexDocumentation.symbols.size());

    // Constraints, on their own and in methods, and every overlay operation.
    formula::Trace<> trace {};
    auto const setOutcomes =
        formula::check_all(formula::constraints(limit), specimen, formula::RecordingSink { trace, north });
    auto const strength = formula::evaluate_method<Cube>(overlaid, specimen, formula::RecordingSink { trace, north });
    auto const verdicts = formula::check_method(overlaid, specimen, formula::RecordingSink { trace, north });
    auto const pinnedStrength = formula::evaluate_method<Cube>(pinned, specimen);
    auto const prunedStrength = formula::evaluate_method<Cube>(pruned, specimen);
    auto const derivedStrength = formula::evaluate_method<Cube>(derived, specimen);
    probe.checks.push_back(setOutcomes[0].is_satisfied());
    probe.checks.push_back(strength.has_value());
    probe.checks.push_back(verdicts[0].is_satisfied());
    probe.checks.push_back(pinnedStrength.has_value());
    probe.checks.push_back(prunedStrength.has_value());
    probe.checks.push_back(derivedStrength.has_value());
    probe.checks.push_back(formula::constraint_origin(overlaid).provenance()
                           == formula::ConstraintProvenance::JurisdictionOverlay);
    probe.checks.push_back(!formula::render_trace(trace, { .maxSteps = 200 }).empty());
    probe.checks.push_back(!formula::render_trace(explained.trace, { .maxSteps = 200 }).empty());

    // The method hooks through a sink of the consumer's own.
    std::string events;
    (void) formula::evaluate_method<Cylinder>(baseMethod, specimen, MethodHooks { {}, &events });
    (void) formula::check_method(baseMethod, specimen, MethodHooks { {}, &events });
    probe.checks.push_back(events == "vVaA");
    return probe;
}
