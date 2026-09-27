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
// warnings as errors, like every test here.
//
// **What it guards is what it instantiates, and no more.** cl reports a
// template's local only once that template is instantiated, and never
// reports a function template's parameter at all; a non-template function's
// locals and parameters it reports where the function is defined, so
// including the header is enough for those. The probe below instantiates:
// evaluation of every node kind -- arithmetic with a bare number on either
// side, negation, powers and every root, pi, rounding both ways, a
// conditional, the escape hatch and the three lookups -- untraced and traced,
// with `explain`; `render` and `document` in all three dialects, with and
// without a vocabulary, of that formula, of a constraint and its predicate,
// and of formulas an overlay fixed, derived and replaced; `render_trace`;
// `check` and `check_all`; `evaluate_method` of an original and of a
// replaced variant, and `check_method`, with `RecordingSink` and with a sink
// of its own; `apply` with every overlay operation; `Outcome`'s factories;
// `checked_convert_to`, `checked_within_bounds`, `checked_round_to_declared`,
// `transform` and `combine`; `entered`, `Environment::get` and `source_of`;
// and the three table validators. A template it does not reach is not
// guarded by it. `consumer_globals_run_tests.cpp` checks that each of these
// computed what it should.
//
// Measured against the headers before their names were changed: cl 19.51
// reported 292 declarations across 30 headers with the probe's first
// version, and 18 more in 9 headers once it reached the templates listed
// above that it had not; g++ 13.3 at -Wshadow reported four of the first
// 292 -- constructor parameters, with the globals declared before the headers
// as here; clang++ 20.1.8 at -Wshadow and clang-cl 22.1.3 at /W4 reported
// nothing. So this guards cl builds, and g++ builds for those four, for the
// templates above.
//
// The list began with 47 names and was widened to 258 after a consumer's
// `here` and `origin` were found hidden. Against the headers before that
// round's renames, the widened guard made cl 19.51 report 97 declarations;
// the other 19 renamed then are in templates the probe does not reach, or
// hid a namespace (`formula::unit`) rather than a global. g++ 13.3 at
// -Wshadow reported one more, which cl does not: a generic lambda's
// parameter pack named `items`; g++ 14.2 and clang++ 20.1.8 at -Wshadow
// reported nothing.
//
// The globals come after the standard headers and this test's own, whose
// names are theirs. `index` is declared for cl-compatible compilers only:
// glibc's <cstring> declares a function of that name at global scope. Left
// out: `time`, which the C library declares as a function at global scope
// (refused as a redeclaration by cl 19.51, clang-cl 22.1.3, g++ 13.3 and
// 14.2, and clang++ 20.1.8 with libstdc++ and with libc++); and `min` and
// `max`, the standard library's own algorithm names and a common pair of
// macros -- both built as globals on those toolchains, and are left out
// anyway.
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
    environment, sink, a, acc, accumulator, amount, area, arg, args, array, average, b, begin, block, bound, bounds,
    buffer, c, capacity, cell, cells, character, chars, child, children, chunk, column, columns, comment, config,
    context, copy, counter, ctx, cur, current, d, data, date, deadline, delay, delimiter, depth, description, digit,
    done, duration, e, elapsed, element, elements, empty, end, env, epsilon, extent, f, factor, failure, field,
    fields, file, flag, flags, fmt, force, format, g, h, head, height, here, hi, high, how, id, in, info, input,
    interval, invalid, is, item, items, j, k, kind, l, label, length, level, limit, line, lines, list, lo, lookup,
    low, lower, map, mass, maximum, mean, measurement, median, member, members, message, minimum, mode, next, node,
    note, notes, number, numbers, offset, ok, option, options, origin, os, other, out, output, p, pair, param, params,
    parent, part, parts, path, pattern, percent, period, piece, precision, prefix, pressure, prev, q, quantity, r,
    range, rate, ratio, reading, readings, ready, record, records, rest, root, rows, s, sample, scale, section,
    segment, separator, set, source, span, start, state, status, stop, str, stream, string, success, suffix, sum, t,
    tail, target, temp, temperature, temporary, threshold, timeout, title, tmp, token, tokens, tolerance, tree, tuple,
    type, types, u, unit, unitName, upper, v, valid, values, vector, view, volume, w, weight, what, when, where, who,
    why, word, words, x, y, z;
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
inline constexpr formula::BandTable<2> Bands { formula::band(0, 1, 277, 100), formula::band(277, 100, 613, 100) };
inline constexpr formula::BreakpointTable<2> Points { formula::breakpoint(0), formula::breakpoint(831, 100) };

/// A formula touching every node kind the evaluator, renderer and trace know:
/// arithmetic, a power and a root, a documented citation, rounding both
/// ways, a conditional, the escape hatch, and all three lookups.
inline constexpr auto everything = formula::documented(
    formula::rounded<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
        var<Force> / formula::pow<2>(var<EdgeX>)
        * formula::when(
            var<Factor> > formula::number(formula::Rational { 0 }), var<Factor>, formula::number(formula::Rational { 1 }))
        * formula::sqrt(formula::pow<2>(var<Factor>))
        * formula::exact_lookup<SpecimenFormKeys, unit::One>(
            SpecimenForm::Round, { formula::Rational { 1'087, 1'000 }, formula::Rational { 1'249, 1'000 } })
        * formula::banded_lookup<unit::One, Bands, unit::One>(
            var<Factor>, { formula::Rational { 1'127, 1'000 }, formula::Rational { 1'973, 1'000 } })
        * formula::interpolating_lookup<unit::One, Points, unit::One>(
            var<Factor>, { formula::Rational { 1'043, 1'000 }, formula::Rational { 2'917, 1'000 } })
        * formula::rounded_to_digits<unit::One, formula::SignificantDigits { 3 }, formula::RoundingMode::HalfAwayFromZero>(
            formula::numeric_value_of<unit::One, "Example Standard 1 states it bare">(var<Factor>))),
    formula::Citation { .title = "Everything", .reference = "Example Standard 1:2020", .section = "1" });

inline constexpr auto forceLimit = formula::constraint(
    var<Force> >= formula::constant<unit::Newton>(formula::Rational { 1 }), formula::Verdict { "no load" });

inline constexpr auto baseMethod = formula::method(
    formula::variants(formula::variant<Cube>(var<Factor> * var<Force> / (var<EdgeX> * var<EdgeX>) ),
                      formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
    formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints(forceLimit));

/// Every overlay operation, so that `apply` instantiates each one's
/// `apply_operation` and every result check.
inline constexpr auto everyOperation = formula::overlay(
    formula::with_constraints(formula::constraints(forceLimit), formula::Citation { .reference = "Example Standard 2" }),
    formula::replace_variant<Cylinder>(formula::number(formula::Rational { 2 }) * var<Force> / (var<EdgeX> * var<EdgeX>),
                                       formula::Citation { .reference = "Example Standard 12:2021 NA" }),
    formula::with_constant<Factor>(formula::Rational { 1'043, 1'000 },
                                   formula::Citation { .reference = "Example Standard 12:2021 NA" }),
    formula::with_rounding<unit::Megapascal, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
        formula::Citation { .reference = "Example Standard 12:2021 NA" }));

inline constexpr auto overlaid = formula::apply(everyOperation, baseMethod);
inline constexpr auto pinned = formula::apply(
    formula::overlay(formula::pin_variant<Cube>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
    baseMethod);
inline constexpr auto pruned = formula::apply(
    formula::overlay(formula::prune_variant<Cylinder>(formula::Citation { .reference = "Example Standard 12:2021 NA" })),
    baseMethod);
inline constexpr auto derived =
    formula::apply(formula::overlay(formula::add_derived<Factor>(
                       var<EdgeX> / var<EdgeX>, formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                   baseMethod);

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
                        + formula::render<formula::Dialect::LaTeX>(everything) + formula::render(forceLimit);
    auto const documentation = formula::document(everything, north);
    auto const markdownDocumentation = formula::document<formula::Dialect::Markdown>(everything);
    auto const latexDocumentation = formula::document<formula::Dialect::LaTeX>(everything);
    probe.checks.push_back(!pages.empty());
    probe.checks.push_back(!documentation.symbols.empty());
    probe.checks.push_back(markdownDocumentation.symbols.size() == latexDocumentation.symbols.size());

    // Constraints, on their own and in methods, and every overlay operation.
    formula::Trace<> trace {};
    auto const setOutcomes =
        formula::check_all(formula::constraints(forceLimit), specimen, formula::RecordingSink { trace, north });
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

    // Arithmetic the formula above does not spell: negation, pi, a cube root
    // and the other roots, and every operator with a bare number on either
    // side -- each its own template.
    auto const negated = formula::evaluate<Strength>(-(var<Force> / formula::pow<2>(var<EdgeX>)), specimen);
    auto const withPi = formula::evaluate<Strength>(formula::pi * var<Force> / formula::pow<2>(var<EdgeX>), specimen);
    auto const rooted = formula::evaluate<Factor>(
        formula::cbrt(formula::pow<3>(var<Factor>)) * formula::root<4>(formula::pow<4>(var<Factor>)), specimen);
    auto const mixed = formula::evaluate<Factor>(
        ((var<Factor> + formula::Rational { 1 }) - formula::Rational { 1 }) * formula::Rational { 2 }
                / formula::Rational { 2 }
            + (formula::Rational { 1 } + var<Factor>) -(formula::Rational { 1 } - var<Factor>) +formula::Rational { 2 }
                  * var<Factor> / (formula::Rational { 2 } / var<Factor>),
        specimen);
    probe.checks.push_back(negated.is_value());
    probe.checks.push_back(withPi.is_value());
    probe.checks.push_back(rooted.is_value());
    probe.checks.push_back(mixed.is_value());
    pages += formula::render(-var<Force>) + formula::render(formula::pi * var<Force>)
             + formula::render<formula::Dialect::LaTeX>(formula::cbrt(var<Force>));

    // The variant an overlay replaced, and the documentation of formulas an
    // overlay fixed, derived and replaced.
    auto const replaced = formula::evaluate_method<Cylinder>(overlaid, specimen, formula::RecordingSink { trace, north });
    probe.checks.push_back(replaced.has_value());
    auto const fixedDocumentation = formula::document(std::get<0>(overlaid.variantSet.cases).expression, north);
    auto const replacedDocumentation =
        formula::document<formula::Dialect::Markdown>(std::get<1>(overlaid.variantSet.cases).expression);
    auto const derivedDocumentation = formula::document(std::get<0>(derived.variantSet.cases).expression);
    probe.checks.push_back(!fixedDocumentation.symbols.empty());
    probe.checks.push_back(!replacedDocumentation.formula.empty());
    probe.checks.push_back(!derivedDocumentation.symbols.empty());
    pages += formula::render(std::get<1>(overlaid.variantSet.cases).expression, north);

    // Constraints and predicates on every surface of their own.
    auto const constraintDocumentation = formula::document(forceLimit, north);
    auto const latexConstraintDocumentation = formula::document<formula::Dialect::LaTeX>(forceLimit);
    pages += formula::render(forceLimit, north) + formula::render<formula::Dialect::Markdown>(forceLimit.predicate, north)
             + formula::render(forceLimit.predicate);
    probe.checks.push_back(!constraintDocumentation.formula.empty());
    probe.checks.push_back(!latexConstraintDocumentation.formula.empty());
    auto const checkedLimit = formula::check(forceLimit, specimen, formula::RecordingSink { trace, north });
    probe.checks.push_back(checkedLimit.is_satisfied());

    // Outcomes, measured values and environments, through their templates.
    auto const verdictOutcome = formula::Outcome<Strength>::verdict(formula::Verdict { "rejected" });
    auto const invalidOutcome = formula::Outcome<Strength>::invalid(formula::InvalidReason { "not usable" });
    auto const emptyOutcome = formula::Outcome<Strength>::empty();
    probe.checks.push_back(verdictOutcome.is_verdict() && invalidOutcome.is_invalid() && emptyOutcome.is_empty());
    formula::Measured<EdgeX> const edge { formula::Rational { 150 } };
    auto const inMetres = formula::checked_convert_to<EdgeX>(edge);
    auto const withinBounds = formula::checked_within_bounds(edge);
    auto const declared = formula::checked_round_to_declared(edge, formula::RoundingMode::HalfAwayFromZero);
    auto const doubled =
        formula::transform(edge, [](formula::Rational measured) { return measured * formula::Rational { 2 }; });
    auto const summed = formula::combine<EdgeX>(
        edge, edge, [](formula::Rational augend, formula::Rational addend) { return augend + addend; });
    probe.checks.push_back(inMetres.has_value() && withinBounds.has_value() && declared.has_value());
    probe.checks.push_back(doubled.value() == formula::Rational { 300 } && summed.value() == formula::Rational { 300 });
    auto const enteredForce = formula::entered(formula::Measured<Force> { formula::Rational { 1 } });
    auto const enteredEnvironment = formula::environment(enteredForce);
    probe.checks.push_back(specimen.get<Force>().value() == formula::Rational { 90'000 });
    probe.checks.push_back(enteredEnvironment.source_of<Force>() == formula::ValueSource::ManuallyEntered);

    // The tables' own validators.
    probe.checks.push_back(formula::band_table_is_well_formed(Bands) && formula::key_table_is_well_formed(SpecimenFormKeys)
                           && formula::breakpoint_table_is_well_formed(Points));
    return probe;
}
