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
// side, negation, powers and every root, pi, rounding both ways, a rounded
// square root, a logarithm, a decimal logarithm and an exponential, exactly
// and in double, each rounded exactly to declared places, a conditional,
// the escape hatch, the three lookups, a critical value, an absolute value
// and a two-pass precision limit --
// untraced and traced, with `explain`; `render` and `document` in all three
// dialects, with and without a vocabulary, of that formula, of a constraint
// and its predicate, and of formulas an overlay fixed, derived and replaced;
// `render_trace`; `traced`, `trace_of`, `trace_of_si`, `explain_conformity`,
// `explain_method`, `explain_check_method`, `explain_check` and
// `explain_check_all`;
// `check` and `check_all`; `evaluate_method` of an original
// and of a replaced variant, and `check_method`, with `RecordingSink` and
// with a sink of its own; `apply` with every overlay operation; `Outcome`'s
// factories; `checked_convert_to`, `checked_within_bounds`,
// `checked_round_to_declared`, their throwing twins `convert_to`,
// `within_bounds` and `round_to_declared`, `transform` and `combine`; `entered`,
// `Environment::get` and `source_of`;
// `measured_series`, `entered` of a series, `Environment::get_series` and
// `checked_evaluate_series` of a series variable, derived and entered;
// `render` and `document` of a series variable, and `explain_series` with
// its trace rendered; elementwise arithmetic with a broadcast scalar,
// negation and a per-element constant, on the same surfaces; running totals
// from either end, a per-element rounding and `sum`, inside a method an
// overlay's constant rewrote, evaluated, rendered, documented and traced; a
// conformity check against a limit envelope, a snap, and curves -- a declared
// domain, a pairing, a splice and an interpolation -- on the same surfaces,
// with `explain_curve`;
// raw observations, `from` and `get_observations`, binned into classes and
// divided by their sum, on the same surfaces;
// a sample's count, mean, variance and range, and a rounded root of the
// variance, on the same surfaces; a rejection of outliers, evaluated alone,
// with `explain_rejection` and under a mean, on the same surfaces, and one by
// gap to range; a mean
// and a rejection of raw observations, on the same surfaces; a consumer's
// opaque operation's output, evaluated exactly and in double, and rounded
// where it is used, traced, rendered and documented; a least-squares fit,
// exact and rounded where it is used, and a line through raw observations,
// exactly, rounded and in double, and `multiple_least_squares` of
// `regressors(...)` of two through them; a retry over recorded determinations,
// evaluated, traced, rendered and documented; and the four table validators;
// and `record_key`, `sample_id`, `test_id`, `record`, `Record::unbound`,
// `record_context`, its `this_record`,
// `record<Role>()` and `binds`, with `checked_evaluate`, `evaluate_method`
// and `explain` through a context, and `from_record`, over a bound and an
// unbound record, untraced and traced into `render_trace`, gated on
// `same_lineage` through `checked_explain`, rendered and documented, with
// `lineage_of` and `origin_of` reading the trace's side tables, and under an
// overlay's constant and derived quantity, traced; `define` of the formula
// above and of a variant an overlay derived a quantity in, with what each
// reads, and a `calculation` of two with every query over its graph; a
// `worksheet` of it, from an environment with and without an override,
// asked for one value and for two, by type and by variable, set, set on a
// worksheet about to be discarded, copied with an override by `with`, the
// override cleared, with its counters and every query, and its derivation
// by `explain_worksheet`, rendered by `render_derivation`; the calculation
// rendered and documented, with and without a number style, and its graph
// by `describe_graph` and `to_dot`; a worksheet over a quantity in a
// currency of the consumer's own, asked, derived in exact decimals, and
// with an override cleared on a worksheet about to be discarded;
// a measured value spelled by `number_text` in each notation, with
// `checked_number_text`, `decimal_text`, `fraction_text` and
// `exact_decimal_text`; a `Rational` and a measured value written by
// `std::format`, aligned and rounded, and an `Outcome`, a `Unit`, a
// `Dimension` and an enumeration written the same way; an `Int128` through
// every operator, and written by `std::format`; `symbol_of` with no
// vocabulary, and `render` and `document` given `RenderOptions` and none;
// `yields` of the formula touching every node kind, and `evaluate`,
// `checked_evaluate`, `explain`, `checked_explain`, `trace_of`, `render`, `document` and
// `define` of what it binds, and `checked_evaluate_series`,
// `explain_series`, `checked_evaluate_rejection` and `explain_rejection` of
// a series and a rejection bound the same way;
// and a quantity declared by alias at
// global scope, so that its tag is one more global. A template it does
// not reach is not guarded by it.
// `consumer_globals_run_tests.cpp` checks that each of these computed what
// it should.
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
// `applied` was added after it: cl 19.51 reported the parameter of that
// name in the constructor `evaluate_method` builds a `RoundingRuleNode`
// (`method.hpp`) with. The refused public constructor's parameter of the
// same name, which nothing instantiates, was renamed with it.
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

#include <array>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

// clang-format off
int result, value, text, step, mark, first, last, count, size, name, key, left, right, lhs, rhs, operand, outcome,
    error, symbol, position, converted, selected, selectedTag, expression, selection, outcomes, entry, rendered, row,
    band, keys, found, digits, scaled, numerator, denominator, quotient, sign, width, total, table, i, n, m, applied,
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
    why, word, words, x, y, z, variance, spread, deviation, deviations, gap, statistic, survivors, rejected, sampled,
    counted, squares, dispersion, extreme, lowest, highest, determinations, determination, smallest, largest, degrees,
    statistics, batch, lineage, role, gated, there, reference, scope, attribute, comparand, subject, points, slope,
    intercept, fit, attempt, attempts, verdict, previous, judgement, accepted, exhausted, observation, observations,
    pivot, design, coefficient, coefficients, regressor, regressors;
#if defined(_MSC_VER)
int index;
#endif
// clang-format on

#include <formula-cpp/band.hpp>
#include <formula-cpp/binning.hpp>
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/citation.hpp>
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/conformity.hpp>
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/critical_value.hpp>
#include <formula-cpp/curve.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/document.hpp>
#include <formula-cpp/enumerator.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/int128.hpp>
#include <formula-cpp/least_squares.hpp>
#include <formula-cpp/lineage.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/number_text.hpp>
#include <formula-cpp/observations.hpp>
#include <formula-cpp/opaque.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/record.hpp>
#include <formula-cpp/rejection.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/retry.hpp>
#include <formula-cpp/rounded_root.hpp>
#include <formula-cpp/rounded_transcendental.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/snap.hpp>
#include <formula-cpp/statistics.hpp>
#include <formula-cpp/tag.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>
#include <formula-cpp/unit.hpp>
#include <formula-cpp/version.hpp>
#include <formula-cpp/vocabulary.hpp>
#include <formula-cpp/yields.hpp>

// A quantity declared by alias at global scope, as a consumer would: the
// elaborated type specifier declares its tag, `AliasEdgeTag`, as one more
// global of the consumer's.
using AliasEdge = formula::Quantity<struct AliasEdgeTag, "x_g", "an edge declared by alias", formula::unit::Millimetre>;

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
struct Reference
{
};
struct MaterialBatch
{
};
struct TestMethod
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
struct AgreedEdge: formula::Quantity<AgreedEdge, "x_a", "agreed edge", unit::Millimetre>
{
};

// A currency of the consumer's own, a base dimension the SI does not have,
// and a fee in it before and after tax.
inline constexpr formula::Unit ProbeEuro { .dimension = formula::base_dimension("EUR"),
                                           .symbolText = formula::symbol("EUR"),
                                           .decimals = 2 };
struct NetFee: formula::Quantity<NetFee, "fee_n", "a fee before tax", ProbeEuro>
{
};
struct GrossFee: formula::Quantity<GrossFee, "fee_g", "a fee after tax", ProbeEuro>
{
};

inline constexpr formula::KeyTable<SpecimenForm, 2> SpecimenFormKeys { SpecimenForm::Square, SpecimenForm::Round };
inline constexpr formula::BandTable<2> Bands { formula::band(0, 1, 277, 100), formula::band(277, 100, 613, 100) };
inline constexpr formula::BreakpointTable<2> Points { formula::breakpoint(0), formula::breakpoint(831, 100) };
inline constexpr formula::SampleSizeTable<2> Sizes { 1, 2 };

/// A formula touching every node kind the evaluator, renderer and trace know:
/// arithmetic, a power and a root, a documented citation, rounding both
/// ways, a rounded square root, a conditional, the escape hatch, all three
/// lookups, a critical value, an absolute value, a precision limit, a
/// logarithm, a decimal logarithm and an exponential, each also rounded to
/// declared places.
inline constexpr auto everything = formula::documented(
    formula::rounded<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(
        var<Force> / formula::pow<2>(var<EdgeX>)
        * formula::when(
            var<Factor> > formula::number(formula::Rational { 0 }), var<Factor>, formula::number(formula::Rational { 1 }))
        * formula::sqrt(formula::pow<2>(var<Factor>))
        * formula::rounded_sqrt<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Factor> * formula::Rational { 2 })
        * formula::critical_value<Sizes, unit::One>(var<Factor>, { formula::Rational { 70 }, formula::Rational { 20 } })
        * formula::abs(var<Factor>)
        * formula::precision_limit<formula::PrecisionKind::Repeatability>(var<Factor>, formula::precision_level<Factor>)
        * formula::exact_lookup<SpecimenFormKeys, unit::One>(
            SpecimenForm::Round, { formula::Rational { 1'087, 1'000 }, formula::Rational { 1'249, 1'000 } })
        * formula::banded_lookup<unit::One, Bands, unit::One>(
            var<Factor>, { formula::Rational { 1'127, 1'000 }, formula::Rational { 1'973, 1'000 } })
        * formula::interpolating_lookup<unit::One, Points, unit::One>(
            var<Factor>, { formula::Rational { 1'043, 1'000 }, formula::Rational { 2'917, 1'000 } })
        * formula::rounded_to_digits<unit::One, formula::SignificantDigits { 3 }, formula::RoundingMode::HalfAwayFromZero>(
            formula::numeric_value_of<unit::One, "Example Standard 1 states it bare">(var<Factor>))
        * formula::exp(formula::ln(var<Factor>)) * formula::log10(var<Factor> * formula::Rational { 10 })
        * formula::rounded_exp<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
            formula::rounded_ln<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(var<Factor>))
        * formula::rounded_log10<formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Factor> * formula::Rational { 10 })),
    formula::Citation { .title = "Everything", .reference = "Example Standard 1:2020", .section = "1" });

/// A consumer's opaque operation: the span of a series, its highest element
/// less its lowest.
struct EdgeSpan
{
    static constexpr std::string_view name = "edge span";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 1> outputs { "span" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(std::span<Rep const> edges) noexcept
    {
        Rep least = edges[0];
        Rep most = edges[0];
        for (Rep const& each: edges)
        {
            if (each < least)
                least = each;
            if (most < each)
                most = each;
        }
        std::expected<Rep, formula::ArithmeticError> const edgeSpread = formula::RepTraits<Rep>::subtract(most, least);
        if (!edgeSpread.has_value())
            return std::unexpected { edgeSpread.error() };
        return std::array { *edgeSpread };
    }
};
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

inline constexpr auto elsewhere = formula::environment(formula::Measured<Force> { formula::Rational { 60'000 } },
                                                       formula::Measured<EdgeX> { formula::Rational { 139 } },
                                                       formula::Measured<Factor> { formula::Rational { 1 } });

/// Records declaring lineage: the same batch, and a different method.
inline constexpr auto lineageRecords = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), specimen,
                                         formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12)),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), elsewhere,
                               formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(13)));

inline constexpr auto boundRecords = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), specimen),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), elsewhere));

inline constexpr auto north = formula::vocabulary(formula::renames<Force>("P"));

inline constexpr formula::BreakpointTable<3> EdgeSnapSet { formula::breakpoint(137),
                                                           formula::breakpoint(149),
                                                           formula::breakpoint(151) };

inline constexpr formula::BreakpointTable<2> EdgeCurvePoints { formula::breakpoint(139), formula::breakpoint(161) };
inline constexpr formula::BreakpointTable<1> EdgeCurveTail { formula::breakpoint(307) };

inline constexpr formula::BandTable<2> EdgeClasses { formula::band(0, 1, 163, 1), formula::band(163, 1, 331, 1) };

inline constexpr formula::PlacesTable<2> edgePlaces { formula::DecimalPlaces { 0 }, formula::DecimalPlaces { 1 } };

/// Every series node kind that reaches a method: a sum over a running total
/// from each end, with the factor an overlay fixes inside the elementwise
/// product, rounded element by element.
inline constexpr auto seriesMethod = formula::method(
    formula::variants(formula::variant<Cube>(
        formula::sum(formula::cumulative<formula::CumulativeDirection::FromLast>(
            formula::rounded_elementwise<unit::Millimetre, edgePlaces, formula::RoundingMode::HalfAwayFromZero>(
                formula::series<EdgeX, 2> * var<Factor>)))
        / formula::sum(formula::cumulative<formula::CumulativeDirection::FromFirst>(formula::series<EdgeX, 2>)))),
    formula::rounding_rule<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
inline constexpr auto seriesOverlaid =
    formula::apply(formula::overlay(formula::with_constant<Factor>(
                       formula::Rational { 3 }, formula::Citation { .reference = "Example Standard 12:2021 NA" })),
                   seriesMethod);

/// A method reading its factor only from another record, so that an overlay
/// must reach inside the scope, fixed and derived.
inline constexpr auto acrossMethod = formula::method(
    formula::variants(formula::variant<Cube>(var<Force> / formula::from_record<Reference>(var<Factor> * var<Force>))),
    formula::rounding_rule<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
inline constexpr auto fixedAcross =
    formula::apply(formula::overlay(formula::with_constant<Factor>(formula::Rational { 1, 2 },
                                                                   formula::Citation { .reference = "Example Standard 3" })),
                   acrossMethod);
inline constexpr auto derivedAcross =
    formula::apply(formula::overlay(formula::add_derived<Factor>(var<EdgeX> / var<EdgeX>, formula::Citation { .reference = "Example Standard 3" })), acrossMethod);

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
    // The irrational path of the rounded root: sqrt(2) to 0.01 is 1.41.
    auto const spreadNode = formula::evaluate<Factor>(
        formula::rounded_sqrt<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Factor> * formula::Rational { 2 }),
        specimen);
    probe.checks.push_back(spreadNode.is_value() && spreadNode.measurement().value() == formula::Rational { 141, 100 });
    // Logarithms and exponentials where each is exact -- exp(ln 1) is 1 and log10 1000 is 3 -- and the
    // double route where none is: ln 2.
    auto const logarithmic = formula::evaluate<Factor>(
        formula::exp(formula::ln(var<Factor>)) * formula::log10(var<Factor> * formula::Rational { 1000 }), specimen);
    auto const approximateLogarithm =
        formula::checked_evaluate_si<double>(formula::ln(var<Factor> * formula::Rational { 2 }), specimen);
    probe.checks.push_back(logarithmic.is_value() && logarithmic.measurement().value() == formula::Rational { 3 });
    probe.checks.push_back(approximateLogarithm.has_value() && approximateLogarithm->has_value()
                           && **approximateLogarithm > 0.69 && **approximateLogarithm < 0.70);
    // The rounded forms through the kernel: ln 2 to 4 places is 0.6931, log10 2 to 3 is 0.301 and exp 1
    // to 4 is 2.7183, 3.7124 together.
    auto const roundedLogarithms = formula::evaluate<Factor>(
        formula::rounded_ln<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfAwayFromZero>(var<Factor>
                                                                                                   * formula::Rational { 2 })
            + formula::rounded_log10<formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(
                var<Factor> * formula::Rational { 2 })
            + formula::rounded_exp<formula::DecimalPlaces { 4 }, formula::RoundingMode::HalfAwayFromZero>(var<Factor>),
        specimen);
    probe.checks.push_back(roundedLogarithms.is_value()
                           && roundedLogarithms.measurement().value() == formula::Rational { 37124, 10000 });
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
    auto const convertedEdge = formula::convert_to<EdgeX>(edge);
    auto const roundedEdge = formula::round_to_declared(edge, formula::RoundingMode::HalfAwayFromZero);
    probe.checks.push_back(convertedEdge == *inMetres && roundedEdge == *declared
                           && formula::within_bounds(edge) == *withinBounds);
    probe.checks.push_back(doubled.value() == formula::Rational { 300 } && summed.value() == formula::Rational { 300 });

    // A series: built, entered, read from an environment and evaluated both
    // ways, derived and entered.
    auto const screens = formula::measured_series<EdgeX>(edge, formula::Measured<EdgeX>::absent());
    auto const seriesInputs = formula::environment(screens);
    auto const typedInSeries = formula::environment(formula::entered(screens));
    auto const readSeries = formula::checked_evaluate_series<EdgeX>(formula::series<EdgeX, 2>, seriesInputs);
    auto const overriddenSeries = formula::checked_evaluate_series<EdgeX>(formula::series<EdgeX, 2>, typedInSeries);
    probe.checks.push_back(readSeries.has_value() && readSeries->element(0).value() == formula::Rational { 150 }
                           && readSeries->element(1).is_absent());
    probe.checks.push_back(overriddenSeries.has_value() && overriddenSeries->is_overridden()
                           && seriesInputs.get_series<EdgeX, 2>() == screens);

    // A series on every surface: rendered in the three dialects, documented,
    // and explained with its trace rendered.
    std::string const seriesPages = formula::render(formula::series<EdgeX, 2>, north)
                                    + formula::render<formula::Dialect::Markdown>(formula::series<EdgeX, 2>)
                                    + formula::render<formula::Dialect::LaTeX>(formula::series<EdgeX, 2>);
    auto const seriesDocumentation = formula::document(formula::series<EdgeX, 2>, north);
    auto const explainedSeries = formula::explain_series<EdgeX>(formula::series<EdgeX, 2>, seriesInputs, north);
    probe.checks.push_back(seriesPages == "x_m(i)`x_m(i)`{x_m}_{i}"
                           && seriesDocumentation.symbols[0].shape == formula::ValueShape::Series);
    // Elementwise arithmetic with a broadcast scalar, negation and a
    // per-element constant, through evaluation, render, document and trace.
    auto const weighted =
        -(formula::series<EdgeX, 2>
          * formula::series_constant<unit::One>(formula::Rational { 2 }, formula::Rational { 3 }) / var<Factor>);
    auto const weightedInputs = formula::environment(screens, formula::Measured<Factor> { formula::Rational { 2 } });
    auto const explainedWeighted = formula::explain_series<EdgeX>(weighted, weightedInputs, north);
    probe.checks.push_back(explainedWeighted.outcome.has_value()
                           && explainedWeighted.outcome->element(0).value() == formula::Rational { -150 }
                           && explainedWeighted.outcome->element(1).is_absent()
                           && formula::render<formula::Dialect::LaTeX>(weighted).find("values") != std::string::npos
                           && formula::document(weighted).symbols.size() == 2
                           && !formula::render_trace(explainedWeighted.trace, { .maxSteps = 20 }).empty());
    probe.checks.push_back(explainedSeries.outcome.has_value()
                           && formula::render_trace(explainedSeries.trace, { .maxSteps = 4 })
                                  == "1. x_m = 150 mm; (not measured)\n");
    // Running totals and sums inside an overlaid method: 150 and 103 mm with
    // the fixed factor 3 give totals 759 and 309 mm from the last, 150 and
    // 253 mm from the first, and a quotient of sums of 1068/403 = 2.6501..., or
    // 2.65 under the method's rule. Either direction swapped gives 3.
    auto const bothScreens =
        formula::environment(formula::measured_series<EdgeX>(edge, formula::Measured<EdgeX> { formula::Rational { 103 } }));
    formula::Trace<> seriesMethodTrace {};
    auto const seriesShare =
        formula::evaluate_method<Cube>(seriesOverlaid, bothScreens, formula::RecordingSink { seriesMethodTrace, north });
    constexpr auto seriesVariant = std::get<0>(seriesOverlaid.variantSet.cases).expression;
    probe.checks.push_back(
        seriesShare.has_value() && *seriesShare == formula::Rational { 53, 20 }
        && formula::render(seriesVariant, north).find("cumulative(x_m(i), from first)") != std::string::npos
        && formula::render(seriesVariant, north).find("to 0/1 dp of mm") != std::string::npos
        && formula::render<formula::Dialect::Markdown>(seriesVariant).find("sum(") != std::string::npos
        && formula::render<formula::Dialect::LaTeX>(seriesVariant).find("\\sum") != std::string::npos
        && formula::document(seriesVariant, north).symbols.size() == 2
        && formula::render_trace(seriesMethodTrace, { .maxSteps = 40 }).find("sum(#") != std::string::npos);
    // A conformity check: 150 mm within 139 to 163 mm, 103 mm below its
    // least of 127 mm.
    auto const edgeCheck = formula::conformity<unit::Millimetre>(
        formula::series<EdgeX, 2>,
        { formula::LimitRow { formula::limit(formula::Rational { 139 }), formula::limit(formula::Rational { 163 }) },
          formula::LimitRow { formula::limit(formula::Rational { 127 }), formula::unbounded } },
        formula::Verdict { "reject the edge" },
        formula::Citation { .reference = "Example Standard 3" });
    formula::Trace<> conformityTrace {};
    auto const edgeOutcomes =
        formula::check_conformity(edgeCheck, bothScreens, formula::RecordingSink { conformityTrace, north });
    probe.checks.push_back(edgeOutcomes[0].is_satisfied() && edgeOutcomes[1].is_violated()
                           && formula::render(edgeCheck, north) == "conform(x_m(i), from 139 to 163 mm, at least 127 mm)"
                           && formula::render<formula::Dialect::LaTeX>(edgeCheck).find("conform") != std::string::npos
                           && formula::document(edgeCheck, north).citations.size() == 1
                           && formula::render_trace(conformityTrace, { .maxSteps = 20 })
                                      .find("[1 satisfied, 150 mm (from 139 to 163 mm); "
                                            "2 violated, 103 mm (at least 127 mm): reject the edge]")
                                  != std::string::npos);
    // Tracing any evaluation, and two of the explain twins: the same outcome
    // and the same steps as the hand-built sink above.
    auto const tracedEdgeCheck = formula::traced(
        [&](auto recordingSink) { return formula::check_conformity(edgeCheck, bothScreens, recordingSink); }, north);
    auto const explainedEdgeCheck = formula::explain_conformity(edgeCheck, bothScreens, north);
    auto const explainedStrength = formula::explain_method<Cube>(overlaid, specimen, north);
    probe.checks.push_back(tracedEdgeCheck.outcome == edgeOutcomes && !tracedEdgeCheck.trace.empty());
    probe.checks.push_back(explainedEdgeCheck.outcome == edgeOutcomes
                           && formula::render_trace(explainedEdgeCheck.trace, { .maxSteps = 20 })
                                  == formula::render_trace(conformityTrace, { .maxSteps = 20 }));
    probe.checks.push_back(explainedStrength.outcome == strength && !explainedStrength.trace.empty());
    // The constraint twins and the method's: each the outcome of its untraced
    // verb above, with the steps it recorded.
    auto const explainedLimit = formula::explain_check(forceLimit, specimen, north);
    auto const explainedLimits = formula::explain_check_all(formula::constraints(forceLimit), specimen, north);
    auto const explainedVerdicts = formula::explain_check_method(overlaid, specimen, north);
    probe.checks.push_back(explainedLimit.outcome == checkedLimit && !explainedLimit.trace.empty());
    probe.checks.push_back(explainedLimits.outcome == setOutcomes && !explainedLimits.trace.empty());
    probe.checks.push_back(explainedVerdicts.outcome == verdicts && !explainedVerdicts.trace.empty());
    // A snap: 150 mm among 137, 149 and 151 mm is a tie, decided toward the
    // higher.
    auto const snappedEdge = formula::snapped<unit::Millimetre, EdgeSnapSet, formula::SnapTie::TowardHigher>(var<EdgeX>);
    formula::Trace<> snapTrace {};
    auto const snappedValue =
        formula::checked_evaluate<EdgeX>(snappedEdge, specimen, formula::RecordingSink { snapTrace, north });
    probe.checks.push_back(
        snappedValue.has_value() && snappedValue->measurement().value() == formula::Rational { 151 }
        && formula::render(snappedEdge, north) == "snap(x_m, to 137, 149, 151 mm)"
        && formula::document<formula::Dialect::LaTeX>(snappedEdge).formula.find("snap") != std::string::npos
        && formula::render_trace(snapTrace, { .maxSteps = 10 }).find("tie, toward higher") != std::string::npos);
    // Curves: two declared domains spliced, and read at the specimen's 150 mm,
    // halfway from 139 mm (10 mm) to 161 mm (30 mm).
    auto const edgeCurve = formula::splice<formula::Monotone::NonDecreasing>(
        formula::curve(formula::domain<unit::Millimetre, EdgeCurvePoints>,
                       formula::series_constant<unit::Millimetre>(formula::Rational { 10 }, formula::Rational { 30 })),
        formula::curve(formula::domain<unit::Millimetre, EdgeCurveTail>,
                       formula::series_constant<unit::Millimetre>(formula::Rational { 40 })));
    auto const readEdge = formula::interpolate_at(edgeCurve, var<EdgeX>);
    formula::Trace<> curveTrace {};
    auto const readValue =
        formula::checked_evaluate<EdgeX>(readEdge, specimen, formula::RecordingSink { curveTrace, north });
    auto const splicedEdge = formula::checked_evaluate_curve<EdgeX, EdgeX>(edgeCurve, specimen);
    probe.checks.push_back(
        readValue.has_value() && readValue->measurement().value() == formula::Rational { 20 } && splicedEdge.has_value()
        && splicedEdge->domain()[2].value() == formula::Rational { 307 }
        && formula::render(readEdge, north)
               == "interpolate(splice(curve(domain(139, 161 mm), values(10 mm, 30 mm)), "
                  "curve(domain(307 mm), values(40 mm)), non-decreasing), at x_m)"
        && formula::render(edgeCurve, north).starts_with("splice(")
        && formula::document<formula::Dialect::LaTeX>(readEdge).formula.find("interpolate") != std::string::npos
        && formula::document(edgeCurve, north).symbols.empty()
        && formula::render_trace(curveTrace, { .maxSteps = 40 }).find("[between 139 and 161 mm]") != std::string::npos);
    auto const explainedCurve = formula::explain_curve<EdgeX, EdgeX>(edgeCurve, specimen, north);
    probe.checks.push_back(explainedCurve.outcome == splicedEdge && !explainedCurve.trace.empty());
    // Raw observations, from a span, binned into two classes: 163 mm sits on
    // the boundary and is counted in the upper class.
    std::array<formula::Rational, 3> const edgeReadings { formula::Rational { 103 },
                                                          formula::Rational { 163 },
                                                          formula::Rational { 241 } };
    auto const edgeObserved = formula::MeasuredObservations<EdgeX, 4>::from(edgeReadings);
    auto const edgeSample = formula::environment(*edgeObserved);
    auto const binnedEdges = formula::binned<unit::Millimetre, EdgeClasses>(formula::observations<EdgeX, 4>);
    auto const edgeShares = binnedEdges / formula::sum(binnedEdges);
    formula::Trace<> binningTrace {};
    auto const sharesValue =
        formula::checked_evaluate_series<Factor>(edgeShares, edgeSample, formula::RecordingSink { binningTrace, north });
    probe.checks.push_back(
        edgeObserved.has_value() && edgeSample.get_observations<EdgeX, 4>().size() == 3 && sharesValue.has_value()
        && sharesValue->elements()[1].value() == formula::Rational { 2, 3 }
        && formula::render(binnedEdges, north) == "bin(x_m(i), 0 to under 163 mm, 163 to under 331 mm)"
        && formula::document<formula::Dialect::LaTeX>(edgeShares).formula.find("bin") != std::string::npos
        && formula::document(binnedEdges, north).symbols.front().shape == formula::ValueShape::Observations
        && formula::render_trace(binningTrace, { .maxSteps = 20 }).find("bin(#1) = 1; 2") != std::string::npos);

    // The count and the mean of a sample, evaluated, rendered, documented
    // and traced: 150 and 103 mm give 253/2 mm from 2 determinations.
    auto const sampleMean = formula::sample_mean(formula::series<EdgeX, 2>);
    auto const sampleCount = formula::sample_count(formula::series<EdgeX, 2>);
    formula::Trace<> sampleTrace {};
    auto const meanEdge =
        formula::checked_evaluate<EdgeX>(sampleMean, bothScreens, formula::RecordingSink { sampleTrace, north });
    auto const countedEdges = formula::checked_evaluate<Factor>(sampleCount, bothScreens);
    probe.checks.push_back(meanEdge.has_value() && meanEdge->measurement().value() == formula::Rational { 253, 2 }
                           && countedEdges.has_value() && countedEdges->measurement().value() == formula::Rational { 2 }
                           && formula::render(sampleMean, north) == "sample_mean(x_m(i))"
                           && formula::render<formula::Dialect::LaTeX>(sampleCount) == "n({x_m}_{i})"
                           && formula::document(sampleMean * sampleCount, north).symbols.size() == 1
                           && formula::render_trace(sampleTrace, { .maxSteps = 4 }).find("sample_mean(#1) = 253/2 mm")
                                  != std::string::npos);
    // The range and the variance of the same sample, and the variance's
    // exact root: 150 and 103 mm give a range of 47 mm and a variance of
    // 2209/2 mm^2, whose root is 33.23 mm to 2 dp.
    auto const sampleRange = formula::sample_range(formula::series<EdgeX, 2>);
    auto const sampleSpread =
        formula::rounded_sqrt<unit::Millimetre, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(
            formula::sample_variance(formula::series<EdgeX, 2>));
    formula::Trace<> spreadTrace {};
    auto const rangedEdges = formula::checked_evaluate<EdgeX>(sampleRange, bothScreens);
    auto const spreadEdges =
        formula::checked_evaluate<EdgeX>(sampleSpread, bothScreens, formula::RecordingSink { spreadTrace, north });
    probe.checks.push_back(
        rangedEdges.has_value() && rangedEdges->measurement().value() == formula::Rational { 47 } && spreadEdges.has_value()
        && spreadEdges->measurement().value() == formula::Rational { 3323, 100 }
        && formula::render(sampleSpread, north) == "round(sqrt(sample_variance(x_m(i))), to 2 dp of mm)"
        && formula::render<formula::Dialect::LaTeX>(sampleRange) == "\\operatorname{range}({x_m}_{i})"
        && formula::document(sampleSpread + sampleRange, north).symbols.size() == 1
        && formula::render_trace(spreadTrace, { .maxSteps = 4 }).find("sample_variance(#1)") != std::string::npos);
    // A rejection of outliers at 30 % of the pass's mean: 150 and 103 mm
    // deviate 23.5 mm from 126.5 mm, inside 37.95 mm, so it settles in its
    // first pass with both kept.
    auto const trimmed = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<1>, formula::KeepAtLeast<1>>(
            formula::series<EdgeX, 2>,
            formula::deviation_from_mean(formula::Rational { 3, 10 } * formula::pass_mean<EdgeX>),
            formula::Verdict { "repeat the test" });
    formula::Trace<> trimmedTrace {};
    auto const trimmedOutcome = formula::checked_evaluate_rejection<EdgeX>(trimmed, bothScreens);
    auto const trimmedMean = formula::checked_evaluate<EdgeX>(
        formula::sample_mean(trimmed), bothScreens, formula::RecordingSink { trimmedTrace, north });
    probe.checks.push_back(trimmedOutcome.has_value()
                           && trimmedOutcome->outcome().measurement().value() == formula::Rational { 253, 2 }
                           && trimmedOutcome->rejected().empty() && trimmedMean.has_value()
                           && trimmedMean->measurement().value() == formula::Rational { 253, 2 }
                           && formula::render(trimmed, north).find("without outliers(x_m(i)") != std::string::npos
                           && formula::render<formula::Dialect::LaTeX>(trimmed).find("\\bar{x}") != std::string::npos
                           && formula::document(formula::sample_mean(trimmed), north).rejections.size() == 1
                           && formula::render_trace(trimmedTrace, { .maxSteps = 20 }).find("settled: 0 rejected, 2 remain")
                                  != std::string::npos);
    auto const explainedRejection = formula::explain_rejection<EdgeX>(trimmed, bothScreens, north);
    probe.checks.push_back(
        explainedRejection.outcome.has_value() && trimmedOutcome.has_value()
        && explainedRejection.outcome->outcome() == trimmedOutcome->outcome()
        && formula::render_trace(explainedRejection.trace, { .maxSteps = 20 }).find("settled: 0 rejected, 2 remain")
               != std::string::npos);
    // Gap to range at 3/4: 150 and 103 mm are each other's neighbour, a gap
    // of the whole range, so both are past it -- and rejecting both would
    // leave none of at least 1, so it aborts with the verdict.
    auto const gapped = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<2>, formula::KeepAtLeast<1>>(
            formula::series<EdgeX, 2>,
            formula::gap_to_range(formula::number(formula::Rational { 3, 4 })),
            formula::Verdict { "repeat the test" });
    formula::Trace<> gappedTrace {};
    auto const gappedOutcome =
        formula::checked_evaluate_rejection<EdgeX>(gapped, bothScreens, formula::RecordingSink { gappedTrace, north });
    probe.checks.push_back(gappedOutcome.has_value() && gappedOutcome->outcome().is_verdict()
                           && formula::render(gapped, north).find("gap to range > 3/4") != std::string::npos
                           && formula::render_trace(gappedTrace, { .maxSteps = 20 }).find("would leave 0 of at least 1")
                                  != std::string::npos);
    // Statistics of the raw observations: 103, 163 and 241 mm made in room for
    // four, a mean of 169 mm from 3, and a rejection within 197 mm of it that
    // keeps all three.
    auto const observedMean = formula::sample_mean(formula::observations<EdgeX, 4>);
    formula::Trace<> observedTrace {};
    auto const observedEdge =
        formula::checked_evaluate<EdgeX>(observedMean, edgeSample, formula::RecordingSink { observedTrace, north });
    auto const keptObserved = formula::checked_evaluate<Factor>(
        formula::sample_count(formula::without_outliers<formula::PerPass::MostExtreme,
                                                        formula::OnLimit::Keep,
                                                        formula::AtMost<1>,
                                                        formula::KeepAtLeast<3>>(
            formula::observations<EdgeX, 4>,
            formula::deviation_from_mean(formula::constant<unit::Millimetre>(formula::Rational { 197 })),
            formula::Verdict { "repeat the test" })),
        edgeSample);
    probe.checks.push_back(
        observedEdge.has_value() && observedEdge->measurement().value() == formula::Rational { 169 }
        && keptObserved.has_value() && keptObserved->measurement().value() == formula::Rational { 3 }
        && formula::render(observedMean, north) == "sample_mean(x_m(i))"
        && formula::document(observedMean, north).symbols.front().shape == formula::ValueShape::Observations
        && formula::render_trace(observedTrace, { .maxSteps = 20 }).find("sample_mean(#1) = 169 mm") != std::string::npos);
    // An opaque operation's output, evaluated exactly and in double: the span
    // of 163 and 127 mm is 36 mm.
    auto const edgeSpan = formula::opaque_output<"span">(
        formula::opaque<EdgeSpan>({ .reference = "Example Standard 3" }, formula::series<EdgeX, 2>));
    auto const spanEdges = formula::environment(formula::measured_series<EdgeX>(
        formula::Measured<EdgeX> { formula::Rational { 163 } }, formula::Measured<EdgeX> { formula::Rational { 127 } }));
    auto const spanValue = formula::checked_evaluate<EdgeX>(edgeSpan, spanEdges);
    auto const spanInDouble = formula::checked_evaluate_si<double>(edgeSpan, spanEdges);
    probe.checks.push_back(spanValue.has_value() && spanValue->measurement().value() == formula::Rational { 36 }
                           && spanInDouble.has_value() && spanInDouble->has_value());
    // ... and on every surface: traced, rendered in the three dialects and
    // documented.
    auto const explainedSpan = formula::explain<EdgeX>(edgeSpan, spanEdges, north);
    probe.checks.push_back(
        formula::render_trace(explainedSpan.trace, { .maxSteps = 20 }).find("[inside not shown]") != std::string::npos
        && formula::render(edgeSpan, north) == "edge span(x_m(i)).span"
        && formula::render<formula::Dialect::Markdown>(edgeSpan).find("edge span") != std::string::npos
        && formula::render<formula::Dialect::LaTeX>(edgeSpan).find("\\text{edge span}") != std::string::npos
        && formula::document(edgeSpan, north).opaqueOperations.size() == 1);
    // The span rounded where it is used, 36 mm to 1 dp of mm: evaluated,
    // traced, rendered and documented.
    auto const roundedSpan =
        formula::rounded_output<"span", unit::Millimetre, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfEven>(
            formula::opaque<EdgeSpan>({ .reference = "Example Standard 3" }, formula::series<EdgeX, 2>));
    auto const roundedSpanValue = formula::checked_evaluate<EdgeX>(roundedSpan, spanEdges);
    auto const explainedRoundedSpan = formula::explain<EdgeX>(roundedSpan, spanEdges, north);
    probe.checks.push_back(
        roundedSpanValue.has_value() && roundedSpanValue->measurement().value() == formula::Rational { 36 }
        && formula::render_trace(explainedRoundedSpan.trace, { .maxSteps = 20 }).find("span: rounded where used")
               != std::string::npos
        && formula::render(roundedSpan, north) == "round(edge span(x_m(i)).span, to 1 dp of mm)"
        && formula::document(roundedSpan, north).opaqueOperations.size() == 1);
    // A straight line fitted through the declared curve points 139 and
    // 161 mm, at 13.7 and 28.3 mm: a slope of 14.6/22 = 73/110.
    auto const edgeFit =
        formula::linear_least_squares(formula::curve(formula::domain<unit::Millimetre, EdgeCurvePoints>,
                                                     formula::series_constant<unit::Millimetre>(
                                                         formula::Rational { 137, 10 }, formula::Rational { 283, 10 })),
                                      { .reference = "Example Standard 3" });
    auto const fitSlope = formula::checked_evaluate<Factor>(formula::opaque_output<"slope">(edgeFit), specimen);
    probe.checks.push_back(fitSlope.has_value() && fitSlope->measurement().value() == formula::Rational { 73, 110 });
    // The same slope, 73/110, rounded where it is used through the fit's exact
    // hook: 0.664 at 3 dp.
    auto const roundedFitSlope = formula::checked_evaluate<Factor>(
        formula::rounded_output<"slope", unit::One, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfEven>(edgeFit),
        specimen);
    probe.checks.push_back(roundedFitSlope.has_value()
                           && roundedFitSlope->measurement().value() == formula::Rational { 83, 125 });
    // A line through raw observations: edges of 103, 163 and 241 mm against
    // twice each plus 1 mm, 207, 327 and 483 mm -- slope 2, R^2 1, exactly,
    // rounded and in double.
    std::array<formula::Rational, 3> const agreedReadings { formula::Rational { 207 },
                                                            formula::Rational { 327 },
                                                            formula::Rational { 483 } };
    auto const lineSample =
        formula::environment(*edgeObserved, *formula::MeasuredObservations<AgreedEdge, 4>::from(agreedReadings));
    constexpr auto edgeLine = formula::linear_least_squares(
        formula::observations<EdgeX, 4>, formula::observations<AgreedEdge, 4>, { .reference = "Example Standard 3" });
    auto const lineSlope = formula::checked_evaluate<Factor>(formula::opaque_output<"slope">(edgeLine), lineSample);
    auto const lineFit = formula::checked_evaluate<Factor>(
        formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 4 }, formula::RoundingMode::Floor>(
            edgeLine),
        lineSample);
    auto const lineInDouble = formula::checked_evaluate_si<double>(formula::opaque_output<"slope">(edgeLine), lineSample);
    probe.checks.push_back(lineSlope.has_value() && lineSlope->measurement().value() == formula::Rational { 2 }
                           && lineFit.has_value() && lineFit->measurement().value() == formula::Rational { 1 }
                           && lineInDouble.has_value() && lineInDouble->has_value() && **lineInDouble > 1.999
                           && **lineInDouble < 2.001);
    // Two regressors over three rows: y = 1 mm + 2 x + 5 mm * k, exactly, so
    // coefficient 1 is 2 and R^2 is 1.
    std::array<formula::Rational, 3> const factorReadings { formula::Rational { 1 },
                                                            formula::Rational { 3 },
                                                            formula::Rational { 2 } };
    std::array<formula::Rational, 3> const combined { formula::Rational { 212 },
                                                      formula::Rational { 342 },
                                                      formula::Rational { 493 } };
    auto const regressionSample = formula::environment(*edgeObserved,
                                                       *formula::MeasuredObservations<Factor, 4>::from(factorReadings),
                                                       *formula::MeasuredObservations<AgreedEdge, 4>::from(combined));
    constexpr auto edgeRegression = formula::multiple_least_squares(
        formula::regressors(formula::observations<EdgeX, 4>, formula::observations<Factor, 4>),
        formula::observations<AgreedEdge, 4>,
        { .reference = "Example Standard 3" });
    auto const firstCoefficient =
        formula::checked_evaluate<Factor>(formula::opaque_output<"coefficient 1">(edgeRegression), regressionSample);
    auto const regressionInDouble =
        formula::checked_evaluate_si<double>(formula::opaque_output<"r squared">(edgeRegression), regressionSample);
    probe.checks.push_back(firstCoefficient.has_value() && firstCoefficient->measurement().value() == formula::Rational { 2 }
                           && regressionInDouble.has_value() && regressionInDouble->has_value()
                           && **regressionInDouble > 0.999);
    // A retry over the two recorded edges, evaluated, traced, rendered and
    // documented in all three dialects.
    constexpr auto edgesAgree = formula::when(formula::this_attempt<AgreedEdge> >= formula::previous_attempt<AgreedEdge>,
                                              formula::this_attempt<AgreedEdge> - formula::previous_attempt<AgreedEdge>,
                                              formula::previous_attempt<AgreedEdge> - formula::this_attempt<AgreedEdge>)
                                <= formula::constant<unit::Millimetre>(formula::Rational { 53 });
    // With a starting value, so that the starting branch and its environment
    // are reached too; judged from the first attempt, whose previous value is
    // the starting 139 mm: the span's first edge, 163 mm, is 24 mm from it, so the
    // first is accepted.
    constexpr auto recordedEdges = formula::retry<AgreedEdge, 2, formula::FirstJudged::AtFirstAttempt>(
        formula::starting_from(formula::constant<unit::Millimetre>(formula::Rational { 139 })),
        formula::attempt_input<EdgeX>,
        edgesAgree,
        formula::Verdict { "measure the edge again" },
        { .reference = "Example Standard 3" });
    auto const edgesRetried = formula::checked_evaluate_retry(recordedEdges, spanEdges);
    auto const explainedEdges = formula::explain_retry(recordedEdges, spanEdges, north);
    probe.checks.push_back(
        edgesRetried.has_value() && edgesRetried->end() == formula::RetryEnd::Accepted
        && edgesRetried->outcome().measurement().value() == formula::Rational { 163 }
        && formula::render_trace(explainedEdges.trace, { .maxSteps = 20 }).find("accepted at attempt 1 of 2")
               != std::string::npos
        && formula::render(recordedEdges, north).find("x_m(k)") != std::string::npos
        && formula::render<formula::Dialect::Markdown>(recordedEdges, north).find("`x_m(k)`") != std::string::npos
        && formula::render<formula::Dialect::LaTeX>(recordedEdges, north).find("{x_m}_{k}") != std::string::npos
        && formula::document(recordedEdges, north).symbols.size() == 2
        && formula::document<formula::Dialect::Markdown>(recordedEdges, north).symbols.size() == 2
        && formula::document<formula::Dialect::LaTeX>(recordedEdges, north).symbols.size() == 2);
    auto const enteredForce = formula::entered(formula::Measured<Force> { formula::Rational { 1 } });
    auto const enteredEnvironment = formula::environment(enteredForce);
    probe.checks.push_back(specimen.get<Force>().value() == formula::Rational { 90'000 });
    probe.checks.push_back(enteredEnvironment.source_of<Force>() == formula::ValueSource::ManuallyEntered);

    // The tables' own validators.
    probe.checks.push_back(formula::band_table_is_well_formed(Bands) && formula::key_table_is_well_formed(SpecimenFormKeys)
                           && formula::breakpoint_table_is_well_formed(Points)
                           && formula::sample_size_table_is_well_formed(Sizes));

    // Records and a context, which is this record's environment.
    auto const throughContext = formula::checked_evaluate<Strength>(everything, boundRecords);
    auto const methodThroughContext = formula::evaluate_method<Cube>(baseMethod, boundRecords);
    auto const explainedThroughContext = formula::explain<Strength>(everything, boundRecords, north);
    probe.checks.push_back(throughContext == checked
                           && methodThroughContext == formula::evaluate_method<Cube>(baseMethod, specimen));
    probe.checks.push_back(explainedThroughContext.outcome == explained.outcome);
    probe.checks.push_back(
        boundRecords.this_record().key() == formula::record_key(formula::sample_id(17), formula::test_id(5))
        && boundRecords.record<Reference>().key() == formula::record_key(formula::sample_id(23), formula::test_id(3)));
    auto const unboundReference = formula::Record<Reference, std::remove_cvref_t<decltype(elsewhere)>>::unbound();
    probe.checks.push_back(!unboundReference.is_bound() && !unboundReference.key().has_value()
                           && unboundReference.environment().get<Force>().is_absent()
                           && boundRecords.record<Reference>().is_bound());
    probe.checks.push_back(decltype(boundRecords)::binds<Reference> && !decltype(boundRecords)::binds<Cube>);

    // Reading from another record: 90 000 N here over 60 000 N there, and
    // absent over a record not yet made.
    auto const acrossRecords = formula::checked_evaluate_si<formula::Rational>(
        var<Force> / formula::from_record<Reference>(var<Force>), boundRecords);
    auto const notYetMade = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), specimen),
        unboundReference);
    auto const fromNothing =
        formula::checked_evaluate_si<formula::Rational>(formula::from_record<Reference>(var<Force>), notYetMade);
    probe.checks.push_back(acrossRecords.has_value() && **acrossRecords == formula::Rational { 3, 2 }
                           && fromNothing.has_value() && !fromNothing->has_value());

    // The same read, traced and rendered: each step inside the scope says
    // which record it was read from.
    formula::Trace<> recordTrace {};
    (void) formula::checked_evaluate_si<formula::Rational>(var<Force> / formula::from_record<Reference>(var<Force>),
                                                           boundRecords, formula::RecordingSink { recordTrace, north });
    probe.checks.push_back(formula::render_trace(recordTrace, { .maxSteps = 20 }).find(
                               "from record Reference (sample 23, test 3)")
                           != std::string::npos);

    // A read gated on lineage: the batch agrees and the method does not, so
    // the read is refused, and checked_explain keeps the trace that says why.
    auto const gatedRead = formula::from_record<Reference>(var<Force>, formula::same_lineage<MaterialBatch, TestMethod>());
    auto const refusedRead = formula::checked_explain<Force>(gatedRead, lineageRecords, north);
    probe.checks.push_back(!refusedRead.has_value() && refusedRead.error().error == formula::ArithmeticError::DomainError);
    auto const agreedRead = formula::checked_evaluate_si<formula::Rational>(
        formula::from_record<Reference>(var<Force>, formula::same_lineage<MaterialBatch>()), lineageRecords);
    probe.checks.push_back(!refusedRead.has_value()
                           && formula::render_trace(refusedRead.error().trace, { .maxSteps = 20 }).find(
                                  "same TestMethod as this record: 12 for this record, 13 for Reference, violated")
                                  != std::string::npos
                           && agreedRead.has_value() && **agreedRead == formula::Rational { 60'000 }
                           && lineageRecords.record<Reference>().lineage_of<MaterialBatch>() == std::uint64_t { 4411 });
    // The trace's side tables, through their accessors: the comparison the
    // refusing attribute recorded, and the record its step was read from.
    probe.checks.push_back(!refusedRead.has_value() && refusedRead.error().trace.steps.size() > 1
                           && formula::lineage_of(refusedRead.error().trace, 1).has_value()
                           && formula::lineage_of(refusedRead.error().trace, 1)->attribute() == "TestMethod"
                           && formula::origin_of(refusedRead.error().trace, refusedRead.error().trace.steps[1])
                                      .has_value()
                           && formula::origin_of(refusedRead.error().trace, refusedRead.error().trace.steps[1])->role()
                                  == "Reference");

    // The page of a formula that reads from another record, in every dialect:
    // its words, and a row per (record, quantity).
    auto const acrossPage = formula::document<formula::Dialect::LaTeX>(
        var<Force> / formula::from_record<Reference>(var<Force>), north);
    pages += formula::render<formula::Dialect::Markdown>(formula::from_record<Reference>(var<Force> / var<EdgeX>), north)
             + formula::render(formula::from_record<Reference>(var<Force>));
    probe.checks.push_back(acrossPage.symbols.size() == 2 && acrossPage.symbols[1].record == "Reference"
                           && acrossPage.formula.find("\\mathrm{Reference}") != std::string::npos);

    // An overlay reaching inside a scope: 90 000 N over 1/2 of 60 000 N is 3,
    // traced with the fixed factor's record; and over a derived factor of 1,
    // 1.5.
    formula::Trace<> fixedAcrossTrace {};
    auto const fixedAcrossValue =
        formula::evaluate_method<Cube>(fixedAcross, boundRecords, formula::RecordingSink { fixedAcrossTrace, north });
    probe.checks.push_back(fixedAcrossValue.has_value() && **fixedAcrossValue == formula::Rational { 3 }
                           && formula::render_trace(fixedAcrossTrace, { .maxSteps = 20 })
                                      .find("k = 1/2, from record Reference (sample 23, test 3) [fixed by")
                                  != std::string::npos);
    auto const derivedAcrossValue = formula::evaluate_method<Cube>(derivedAcross, boundRecords);
    probe.checks.push_back(derivedAcrossValue.has_value() && **derivedAcrossValue == formula::Rational { 3, 2 });

    // A series read from another record, reduced inside the scope: 150 mm
    // and 103 mm there, 253 mm, and the series' own line names the record.
    auto const seriesRecords = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), bothScreens),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), bothScreens));
    formula::Trace<> seriesAcrossTrace {};
    auto const seriesAcross = formula::checked_evaluate_si<formula::Rational>(
        formula::from_record<Reference>(formula::sum(formula::series<EdgeX, 2>)), seriesRecords,
        formula::RecordingSink { seriesAcrossTrace, north });
    probe.checks.push_back(seriesAcross.has_value() && **seriesAcross == formula::Rational { 253, 1000 }
                           && formula::render_trace(seriesAcrossTrace, { .maxSteps = 20 })
                                      .find("103 mm, from record Reference (sample 23, test 3)")
                                  != std::string::npos);

    // The same series typed in on the reference: the series variable's
    // evaluator tells the sink (`series_input_source`), and the line says so
    // after the record.
    auto const typedScreens = formula::environment(
        formula::entered(formula::measured_series<EdgeX>(edge, formula::Measured<EdgeX> { formula::Rational { 103 } })));
    auto const typedSeriesRecords = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), bothScreens),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), typedScreens));
    formula::Trace<> typedSeriesTrace {};
    auto const typedSeries = formula::checked_evaluate_si<formula::Rational>(
        formula::from_record<Reference>(formula::sum(formula::series<EdgeX, 2>)), typedSeriesRecords,
        formula::RecordingSink { typedSeriesTrace, north });
    probe.checks.push_back(typedSeries.has_value() && **typedSeries == formula::Rational { 253, 1000 }
                           && formula::render_trace(typedSeriesTrace, { .maxSteps = 20 })
                                      .find("103 mm, from record Reference (sample 23, test 3), entered by hand\n")
                                  != std::string::npos);

    // Definitions of the formula touching every node kind and of the
    // variant an overlay derived the factor in: what each reads, and the
    // formula it holds, evaluated.
    constexpr auto definedStrength = formula::define<Strength>(everything);
    constexpr auto definedDerived = formula::define<Strength>(std::get<0>(derived.variantSet.cases).expression);
    auto const fromDefinition = formula::checked_evaluate<Strength>(definedStrength.expression, specimen);
    probe.checks.push_back(
        std::is_same_v<std::remove_cv_t<decltype(definedStrength)>::reads,
                       formula::detail::QuantityList<Force, EdgeX, Factor>>
        && std::is_same_v<std::remove_cv_t<decltype(definedDerived)>::reads, formula::detail::QuantityList<EdgeX, Force>>
        && fromDefinition.has_value() && checked.has_value() && *fromDefinition == *checked);

    // A calculation of two definitions, and every query over its graph, in
    // a vocabulary: the edge and the force are its inputs, the factor is
    // calculated before the strength that reads it.
    constexpr auto strengthCalculation =
        formula::calculation(formula::define<Strength>(var<Factor> * var<Force> / (var<EdgeX> * var<EdgeX>)),
                             formula::define<Factor>(var<EdgeX> / var<EdgeX>));
    probe.checks.push_back(
        formula::inputs_of(strengthCalculation, north) == std::array<std::string_view, 2> { "P", "x_m" }
        && formula::calculation_order(strengthCalculation) == std::array<std::string_view, 2> { "k", "f_c" }
        && formula::dependencies_of<Strength>(strengthCalculation, north)
               == std::array<std::string_view, 3> { "P", "x_m", "k" }
        && formula::dependents_of<EdgeX>(strengthCalculation) == std::array<std::string_view, 2> { "k", "f_c" }
        && formula::upstream_of<Strength>(strengthCalculation).size() == 3
        && formula::affected_by<Force>(strengthCalculation, north) == std::array<std::string_view, 1> { "f_c" }
        && formula::depends_on<Strength, EdgeX>(strengthCalculation)
        && !formula::depends_on<Factor, Force>(strengthCalculation)
        // Nothing to name: an input reads nothing, and nothing reads the
        // strength.
        && formula::dependencies_of<Force>(strengthCalculation).empty()
        && formula::dependents_of<Strength>(strengthCalculation, north).empty());

    // A worksheet of that calculation: 100 kN on a 100 mm edge is 10 MPa,
    // 200 kN 20 MPa, and half the factor, typed in, halves it again.
    auto strengthSheet =
        formula::worksheet(strengthCalculation,
                           formula::environment(formula::Measured<Force> { formula::Rational { 100000 } },
                                                formula::Measured<EdgeX> { formula::Rational { 100 } }));
    auto const firstStrength = strengthSheet.checked_calculate<Strength>();
    strengthSheet.set(formula::Measured<Force> { formula::Rational { 200000 } });
    auto const [setStrength, setFactor] = strengthSheet.calculate<Strength, Factor>();
    auto const [checkedStrength, checkedForce] = strengthSheet.checked_calculate(var<Strength>, var<Force>);
    auto halvedCopy = strengthSheet.with(formula::entered(formula::Measured<Factor> { formula::Rational { 1, 2 } }));
    auto const halvedStrength = halvedCopy.calculate(var<Strength>);
    bool const halvedOverridden = halvedCopy.is_overridden<Factor>();
    halvedCopy.clear_override<Factor>();
    auto const restoredStrength = halvedCopy.calculate<Strength>();
    auto const widerStrength =
        formula::worksheet(strengthCalculation,
                           formula::environment(formula::Measured<Force> { formula::Rational { 100000 } },
                                                formula::Measured<EdgeX> { formula::Rational { 100 } },
                                                formula::entered(formula::Measured<Factor> { formula::Rational { 1 } })))
            .set(formula::Measured<EdgeX> { formula::Rational { 200 } })
            .checked_calculate<Strength>();
    probe.checks.push_back(
        firstStrength.has_value() && firstStrength->measurement().value() == formula::Rational { 10 }
        && setStrength.measurement().value() == formula::Rational { 20 }
        && setFactor.measurement().value() == formula::Rational { 1 } && checkedStrength.has_value()
        && checkedStrength->measurement().value() == formula::Rational { 20 } && checkedForce.has_value()
        && checkedForce->source() == formula::ValueSource::Measured
        && halvedStrength.measurement().value() == formula::Rational { 10 } && halvedOverridden
        && !halvedCopy.is_overridden<Factor>() && restoredStrength.measurement().value() == formula::Rational { 20 }
        && widerStrength.has_value() && widerStrength->measurement().value() == formula::Rational { 5, 2 }
        && strengthSheet.recomputed() == 3 && strengthSheet.reused() == 0 && halvedCopy.recomputed() == 6);
    probe.checks.push_back(
        formula::inputs_of(strengthSheet, north) == std::array<std::string_view, 2> { "P", "x_m" }
        && formula::calculation_order(strengthSheet) == std::array<std::string_view, 2> { "k", "f_c" }
        && formula::dependencies_of<Strength>(strengthSheet, north)
               == std::array<std::string_view, 3> { "P", "x_m", "k" }
        && formula::dependents_of<EdgeX>(strengthSheet) == std::array<std::string_view, 2> { "k", "f_c" }
        && formula::upstream_of<Strength>(strengthSheet).size() == 3
        && formula::affected_by<Force>(strengthSheet, north) == std::array<std::string_view, 1> { "f_c" }
        && formula::depends_on<Strength, EdgeX>(strengthSheet) && !formula::depends_on<Factor, Force>(strengthSheet)
        && formula::dependencies_of<Force>(strengthSheet).empty());

    // The strength's derivation: its block, the factor's it reads, and the
    // two inputs, in the vocabulary.
    auto const strengthDerivation = formula::explain_worksheet<Strength>(strengthSheet, north);
    probe.checks.push_back(
        strengthDerivation.outcome.has_value() && strengthDerivation.entries.size() == 4
        && strengthDerivation.entries[0].symbol == "f_c" && strengthDerivation.entries[1].symbol == "k"
        && strengthDerivation.entries[2].symbol == "P" && strengthDerivation.entries[3].symbol == "x_m"
        && strengthDerivation.entries[0].kind == formula::WorksheetEntryKind::Calculated
        && strengthDerivation.entries[0].value == formula::Rational { 20 }
        && strengthDerivation.entries[2].kind == formula::WorksheetEntryKind::Input
        && !strengthDerivation.entries[0].trace.empty());

    // The calculation as text: its definitions, its graph described and in
    // DOT, and the strength's derivation rendered.
    std::string const strengthRendered = formula::render<formula::Dialect::Markdown>(strengthCalculation, north);
    std::string const strengthGraph = formula::describe_graph(strengthCalculation, north);
    std::string const strengthDot = formula::to_dot(strengthCalculation, north);
    std::string const strengthDerivationText = formula::render_derivation(strengthDerivation, { .maxSteps = 3 });
    probe.checks.push_back(strengthRendered.starts_with("`k` = ") && strengthGraph.starts_with("inputs: P, x_m\n")
                           && strengthDot.starts_with("digraph calculation {\n")
                           && strengthDerivationText.starts_with("f_c = ")
                           && strengthDerivationText.ends_with(" further steps not shown\n"));

    // The calculation documented: the calculated rows first, each with its
    // definition, then the inputs in the order the definitions read them.
    formula::Documentation const strengthPage =
        formula::document<formula::Dialect::Markdown>(strengthCalculation, north);
    probe.checks.push_back(strengthPage.formula == strengthRendered && strengthPage.symbols.size() == 4
                           && strengthPage.symbols[0].symbol == "k" && strengthPage.symbols[0].calculatedAs.has_value()
                           && strengthPage.symbols[1].symbol == "f_c" && strengthPage.symbols[2].symbol == "x_m"
                           && strengthPage.symbols[3].symbol == "P" && !strengthPage.symbols[3].calculatedAs.has_value());

    // The same, with the numbers in a style: it has none to spell, so the
    // text is the same.
    formula::RenderOptions const exactDecimals { .numbers = formula::NumberStyle::exact_decimal() };
    std::string const styledStrength = formula::render(strengthCalculation, north, exactDecimals);
    formula::Documentation const styledPage = formula::document(strengthCalculation, north, exactDecimals);
    probe.checks.push_back(styledStrength == formula::render(strengthCalculation, north)
                           && styledPage.formula == styledStrength && styledPage.symbols.size() == 4);

    // A worksheet over a fee in a currency of the consumer's own: 12.50 EUR
    // with 19 % tax is 14.875 EUR, its derivation in exact decimals; and the
    // fee with tax typed in, cleared on a worksheet about to be discarded.
    constexpr auto feeCalculation =
        formula::calculation(formula::define<GrossFee>(var<NetFee> * formula::Rational { 119, 100 }));
    auto feeSheet = formula::worksheet(
        feeCalculation, formula::environment(formula::Measured<NetFee> { formula::Rational { 25, 2 } }));
    auto const grossFee = feeSheet.calculate<GrossFee>();
    std::string const feeDerivation = formula::render_derivation(
        formula::explain_worksheet<GrossFee>(feeSheet), { .maxSteps = 10, .numbers = exactDecimals.numbers });
    auto const clearedFee =
        formula::worksheet(feeCalculation,
                           formula::environment(formula::Measured<NetFee> { formula::Rational { 25, 2 } }))
            .set(formula::entered(formula::Measured<GrossFee> { formula::Rational { 20 } }))
            .clear_override<GrossFee>()
            .calculate<GrossFee>();
    probe.checks.push_back(grossFee.measurement().value() == formula::Rational { 119, 8 }
                           && feeDerivation.starts_with("fee_g = fee_n * 1.19 = 14.875 EUR\n")
                           && clearedFee.measurement() == grossFee.measurement()
                           && std::format("{}", grossFee.measurement()) == "14.875 EUR");

    // A quantity declared by alias, evaluated, traced and rendered.
    formula::Trace<> aliasTrace {};
    auto const doubledEdge = formula::checked_evaluate<AliasEdge>(
        var<AliasEdge> * formula::Rational { 2 },
        formula::environment(formula::Measured<AliasEdge> { formula::Rational { 139 } }),
        formula::RecordingSink { aliasTrace, north });
    probe.checks.push_back(doubledEdge.has_value() && doubledEdge->measurement().value() == formula::Rational { 278 }
                           && formula::render_trace(aliasTrace, { .maxSteps = 5 }).starts_with("1. x_g = 139 mm\n")
                           && formula::render(var<AliasEdge> * formula::Rational { 2 }) == "x_g * 2");

    // A measured value spelled as text in the edge's millimetres, which
    // declare one decimal: 150 mm padded, 452/3 mm as a fraction -- asked
    // for, and as the exact decimal it has none of -- and rounded to 150.7,
    // and an absent edge; and the functions they are made of.
    formula::Measured<EdgeX> const thirdEdge { formula::Rational { 452, 3 } };
    probe.checks.push_back(
        formula::number_text(edge, formula::NumberStyle::exact_decimal(formula::DecimalPadding::Padded)) == "150.0 mm"
        && formula::number_text(thirdEdge, formula::NumberStyle::fraction()) == "452/3 mm"
        && formula::number_text(thirdEdge, formula::NumberStyle::exact_decimal()) == "452/3 mm"
        && formula::number_text(thirdEdge, formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven))
               == "\xe2\x89\x88" "150.7 mm"
        && formula::checked_number_text(formula::Measured<EdgeX>::absent(), formula::NumberStyle {}).value()
               == formula::NotMeasuredText
        && formula::decimal_text(formula::Rational { 23653, 200 }, formula::DecimalPlaces { 2 },
                                 formula::RoundingMode::HalfEven, formula::DecimalPadding::Padded)
               == "118.26"
        && formula::fraction_text(formula::Rational { -1, 3 }) == "-1/3"
        && formula::has_exact_decimal(formula::Rational { 3, 5 })
        && *formula::exact_decimal_text(formula::Rational { 3, 5 }) == "0.6");

    // The same values written by std::format: a Rational as its exact
    // decimal, and the third of an edge right-aligned in ten code points,
    // rounded to the edge's one decimal with the approximation marker, and
    // as a fraction.
    probe.checks.push_back(std::format("{}", formula::Rational { 3, 5 }) == "0.6"
                           && std::format("{:>10~HalfEven}", thirdEdge) == " \xe2\x89\x88" "150.7 mm"
                           && std::format("{:/}", thirdEdge) == "452/3 mm");

    // An outcome, a unit, a dimension and an enumeration written by
    // std::format, and the default vocabulary and render options taken
    // without a vocabulary.
    probe.checks.push_back(
        std::format("{}", formula::Outcome<EdgeX>::value(thirdEdge, formula::ValueSource::Derived)) == "452/3 mm"
        && std::format("{:>17}", formula::Outcome<EdgeX>::verdict({ "repeat the test" })) == "  repeat the test"
        && std::format("{}", formula::unit::Millimetre) == "mm"
        && std::format("{}", formula::dim::Density) == "L^-3 M^1"
        && std::format("{}", formula::ArithmeticError::Overflow) == "overflow in exact arithmetic");
    // A 128-bit integer through its constructor template, every operator and
    // std::format: ((-7 * 3 + 1 - 4) / 2 % 5 << 3 >> 1), stepped up and down,
    // is -8.
    formula::Int128 wideInteger { std::int16_t { -7 } };
    wideInteger = +(wideInteger * formula::Int128 { 3U } + 1 - 4);
    wideInteger += 2;
    wideInteger -= 2;
    wideInteger *= 1;
    wideInteger = wideInteger / 2 % 5;
    wideInteger /= 1;
    wideInteger %= 100;
    wideInteger = (wideInteger << 3) >> 1;
    wideInteger <<= 1;
    wideInteger >>= 1;
    ++wideInteger;
    --wideInteger;
    (void) wideInteger++;
    (void) wideInteger--;
    probe.checks.push_back(wideInteger == -wideInteger * -1 && wideInteger < formula::Int128 {}
                           && wideInteger.to_int64() == std::optional<std::int64_t> { -8 }
                           && std::format("{}", wideInteger) == "-8");
    probe.checks.push_back(
        formula::symbol_of<EdgeX>() == formula::Describe<EdgeX>::symbol
        && formula::render(formula::var<EdgeX> * formula::Rational { 3, 5 },
                           formula::RenderOptions { .numbers = formula::NumberStyle::exact_decimal() })
               == formula::render(formula::var<EdgeX> * formula::Rational { 3, 5 },
                                  formula::DefaultVocabulary {},
                                  formula::RenderOptions { .numbers = formula::NumberStyle::exact_decimal() }));
    probe.checks.push_back(
        formula::document(formula::var<EdgeX> * formula::Rational { 3, 5 },
                          formula::RenderOptions { .numbers = formula::NumberStyle::exact_decimal() })
            .formula
        == formula::document(formula::var<EdgeX> * formula::Rational { 3, 5 },
                             formula::DefaultVocabulary {},
                             formula::RenderOptions { .numbers = formula::NumberStyle::exact_decimal() })
               .formula);

    // The words of the enumerations a constraint, a retry and a series
    // failure report, called qualified: this checks that they answer, and
    // that the header declaring them compiles beside the consumer's globals.
    probe.checks.push_back(formula::describe(formula::ConstraintOutcomeKind::Violated) == "violated"
                           && !formula::describe(formula::RetryEnd::Accepted).empty()
                           && !formula::describe(formula::FailureSite::ResultElement).empty());

    // The exact decimal literal: 27.3 is 273/10, not the double nearest it.
    {
        using namespace formula::literals;
        probe.checks.push_back(27.3_r == formula::Rational { 273, 10 });
    }

    // Plain numbers and not_measured in a series, and exact numbers for a
    // band's bounds and a breakpoint's key.
    {
        using namespace formula::literals;
        auto const suppliedSeries = formula::measured_series<EdgeX>(127, 10.3_r, formula::not_measured);
        probe.checks.push_back(suppliedSeries.size() == 3 && suppliedSeries.element(0).value() == formula::Rational { 127 }
                               && suppliedSeries.element(1).value() == formula::Rational { 103, 10 }
                               && suppliedSeries.element(2).is_absent()
                               && formula::band(83.7_r, 97.3_r) == formula::band(837, 10, 973, 10)
                               && formula::breakpoint(12.7_r) == formula::breakpoint(127, 10));
    }
    // The number a result holds, or nothing: a value's, a retry's accepted
    // value, and an error's nothing.
    probe.checks.push_back(formula::number_of(plain).has_value()
                           && formula::number_of(plain) == plain.measurement().value()
                           && formula::number_of(checked) == formula::number_of(plain)
                           && formula::number_of(edgesRetried) == formula::Rational { 163 }
                           && !formula::number_of(std::expected<formula::Outcome<Strength>, formula::ArithmeticError> {
                                   std::unexpected { formula::ArithmeticError::Overflow } })
                                   .has_value());
    // A rounding named once, and one that takes the places a unit declares:
    // 12.36 mm is 12.4 to the tenth of a millimetre a millimetre declares.
    {
        constexpr formula::DecimalRounding declaredMillimetre =
            formula::declared_rounding(unit::Millimetre, formula::RoundingMode::HalfAwayFromZero);
        auto const declaredEdge = formula::evaluate<EdgeX>(
            formula::rounded<declaredMillimetre>(var<EdgeX>),
            formula::environment(formula::Measured<EdgeX> { formula::Rational { 1'236, 100 } }));
        probe.checks.push_back(declaredEdge.is_value() && declaredEdge.measurement().value() == formula::Rational { 62, 5 }
                               && declaredMillimetre.places == formula::DecimalPlaces { 1 });
    }
    // Every other spelling that takes a rounding named once, beside its
    // three-argument form.
    {
        constexpr formula::DecimalRounding tenthEdge { unit::Millimetre,
                                                       formula::DecimalPlaces { 1 },
                                                       formula::RoundingMode::HalfAwayFromZero };
        constexpr formula::SignificantRounding twoFigureEdge { unit::Millimetre,
                                                               formula::SignificantDigits { 2 },
                                                               formula::RoundingMode::HalfAwayFromZero };
        constexpr formula::DecimalRounding hundredthPlain { unit::One,
                                                            formula::DecimalPlaces { 2 },
                                                            formula::RoundingMode::HalfAwayFromZero };
        auto const namedEnvironment = formula::environment(formula::Measured<EdgeX> { formula::Rational { 1'236, 100 } });
        // 12.36 mm to two significant digits is 12 mm.
        auto const inFigures = formula::evaluate<EdgeX>(formula::rounded_to_digits<twoFigureEdge>(var<EdgeX>), namedEnvironment);
        probe.checks.push_back(inFigures.is_value() && inFigures.measurement().value() == formula::Rational { 12 });
        probe.checks.push_back(
            std::is_same_v<decltype(formula::rounding_rule<tenthEdge>()),
                           decltype(formula::rounding_rule<unit::Millimetre,
                                                           formula::DecimalPlaces { 1 },
                                                           formula::RoundingMode::HalfAwayFromZero>())>
            && std::is_same_v<decltype(formula::with_rounding<tenthEdge>(formula::Citation { .reference = "Example Standard 3" })),
                              decltype(formula::with_rounding<unit::Millimetre,
                                                              formula::DecimalPlaces { 1 },
                                                              formula::RoundingMode::HalfAwayFromZero>(
                                  formula::Citation { .reference = "Example Standard 3" }))>);
        // The span of 163 and 127 mm is 36 mm, whole under a tenth's rounding.
        auto const namedSpan = formula::checked_evaluate<EdgeX>(
            formula::rounded_output<"span", tenthEdge>(
                formula::opaque<EdgeSpan>({ .reference = "Example Standard 3" }, formula::series<EdgeX, 2>)),
            spanEdges);
        probe.checks.push_back(namedSpan.has_value() && namedSpan->measurement().value() == formula::Rational { 36 });
        // The root of 2 to 0.01 is 1.41.
        auto const namedRoot = formula::evaluate<Factor>(
            formula::rounded_sqrt<hundredthPlain>(var<Factor> * formula::Rational { 2 }), specimen);
        probe.checks.push_back(namedRoot.is_value() && namedRoot.measurement().value() == formula::Rational { 141, 100 });
        auto const everyEdge = formula::rounded_elementwise<tenthEdge>(formula::series<EdgeX, 2>);
        probe.checks.push_back(formula::checked_evaluate_series<EdgeX>(everyEdge, spanEdges).has_value()
                               && std::is_same_v<std::remove_const_t<decltype(everyEdge)>,
                                                 decltype(formula::rounded_elementwise<
                                                          unit::Millimetre,
                                                          formula::PlacesTable<2> { formula::DecimalPlaces { 1 },
                                                                                    formula::DecimalPlaces { 1 } },
                                                          formula::RoundingMode::HalfAwayFromZero>(
                                                     formula::series<EdgeX, 2>))>);
    }
    // The formula touching every node kind, bound to its result quantity
    // once, then evaluated, checked, traced, rendered, documented and defined
    // without naming the quantity again.
    {
        constexpr auto boundStrength = formula::yields<Strength>(everything);
        probe.checks.push_back(formula::evaluate(boundStrength, specimen) == plain
                               && formula::checked_evaluate(boundStrength, specimen) == checked);
        probe.checks.push_back(formula::explain(boundStrength, specimen, north).outcome == explained.outcome
                               && formula::render(boundStrength, north) == formula::render(everything, north)
                               && formula::document(boundStrength).formula == formula::document(everything).formula);
        constexpr auto boundDefinition = formula::define(boundStrength);
        probe.checks.push_back(
            std::is_same_v<std::remove_const_t<decltype(boundDefinition)>, decltype(formula::define<Strength>(everything))>);
        // The other verbs that name a result: the checked trace, and the
        // series and the rejection above, each evaluated and traced.
        auto const checkedBound = formula::checked_explain(boundStrength, specimen, north);
        probe.checks.push_back(checkedBound.has_value() && checkedBound->outcome == explained.outcome);
        // Just the trace: of the formula, of the bound formula, and in SI units.
        probe.checks.push_back(!formula::trace_of<Strength>(everything, specimen, north).empty());
        probe.checks.push_back(!formula::trace_of(boundStrength, specimen, north).empty());
        probe.checks.push_back(!formula::trace_of_si(everything, specimen, north).empty());
        constexpr auto boundScreens = formula::yields<EdgeX>(formula::series<EdgeX, 2>);
        probe.checks.push_back(formula::checked_evaluate_series(boundScreens, seriesInputs) == readSeries
                               && formula::explain_series(boundScreens, seriesInputs, north).outcome
                                      == explainedSeries.outcome);
        auto const boundTrimmed = formula::yields<EdgeX>(trimmed);
        auto const trimmedAgain = formula::checked_evaluate_rejection(boundTrimmed, bothScreens);
        auto const trimmedExplained = formula::explain_rejection(boundTrimmed, bothScreens, north);
        probe.checks.push_back(trimmedAgain.has_value() && trimmedAgain->outcome() == trimmedOutcome->outcome()
                               && trimmedExplained.outcome.has_value()
                               && trimmedExplained.outcome->outcome() == trimmedOutcome->outcome());
    }
    return probe;
}
