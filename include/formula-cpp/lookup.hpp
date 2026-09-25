// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Banded lookup: a measured value falls in an interval, and that interval
/// selects a correction.
///
/// A method's own algebra sometimes needs a coefficient no formula computes --
/// a size-correction factor for a specimen's diameter, say -- that the
/// method's procedure instead publishes as a table of intervals. `§16.1`
/// bounds what this phase may contain: the table's *structure* (its bands)
/// is part of the method and belongs in a formula's type; the table's
/// *contents* (the correction each band selects) are master data that may be
/// registered per customer, per region, per contract, and must be allowed to
/// arrive at runtime. This file is the node that lets one formula hold both
/// at once.
///
/// **The join, stated explicitly, because two things separately verified do
/// not verify their join.** A prior spike verified, separately, that
/// `std::array<Band, N>` works as a non-type template parameter on all four
/// compilers, and that a node can hold runtime state behind a compile-time
/// shape (`ConstantNode` holds a runtime `Rational number` while `unit` and
/// `dimension` live in its type). It explicitly did not build a node
/// combining both. `BandedLookupNode` is that combination:
///
///  - **In the type** (compile-time shape, part of what a formula *is*):
///    `KeyUnit` (the unit band boundaries are stated in and the operand's
///    value is compared against them in), `Bands` (the boundaries
///    themselves, an NTTP reusing task 1's `BandTable<N>` exactly as
///    declared there), `ResultUnit` (the unit each band's correction is
///    stated in, playing the same role `ConstantNode`'s `unit` plays), and
///    `Operand` (the child expression whose value selects a band). These are
///    the table's *structure*: a method publishes them once, and a formula
///    containing the wrong one is a compile error naming it, exactly as a
///    dimension mismatch already is everywhere else in this library.
///  - **Runtime state** (an ordinary data member): `corrections`, one
///    `Rational` per band, in `ResultUnit`. This is the table's *contents* --
///    the numbers a real customer's registered table supplies -- and it is
///    runtime state for the same reason `ConstantNode::number` is: a
///    constant's *value* may arrive late even though its *unit* cannot.
///
/// Validating `Bands` is entirely task 1's job, reused rather than
/// reimplemented: `RequireValidBandTable` (`band.hpp`) is instantiated in
/// this node's own body, so a gap, an overlap, an inverted band or a
/// zero-width band anywhere in `Bands` is a compile error here, at the
/// earliest moment the table's structure is known, before `corrections` is
/// ever populated.
///
/// **A miss is not a value.** A `BandedLookupNode` whose operand's value
/// falls in no band has found nothing -- not zero, not the nearest band, not
/// the first. There is no default-value parameter and no fallback of any
/// kind: adding one would be phase 9's forbidden `bool satisfied()` in a new
/// costume, an API that must answer *something* for the unresolved case,
/// where every answer is a lie.
///
/// **How the miss is actually reported, and where this departs from the
/// phase's own design-decisions note.** That note proposed reusing
/// `Outcome`'s `Invalid` alternative and its `InvalidReason`. Reading the
/// tree as it stands today (not as the note anticipated it) rules that out
/// on two independent grounds, and the tree wins:
///
///   1. `checked_evaluate_si` -- the machinery this node rides, per this
///      phase's own settled decision that a lookup is a `Node` needing none
///      of `Constraint`'s separate entry points -- returns `Evaluated<Rep>`,
///      an alias for `std::expected<std::optional<Rep>, ArithmeticError>`.
///      There is no path from there to `Outcome<Result>::invalid(...)`:
///      `checked_evaluate` only ever builds `Outcome<Result>::empty()` or
///      `::value(...)` itself, and an `ArithmeticError` returned by any node
///      propagates out of `Outcome` entirely, as the outer `std::expected`'s
///      error. Reaching `Outcome::invalid` from inside a node's own
///      `checked_evaluate_si` would mean widening `Evaluated<Rep>` for every
///      existing node kind to serve this one new caller -- exactly the kind
///      of change task 2's file list (this header, its tests, and the
///      umbrella) does not include, and a much larger one than "add a node".
///   2. A prior spike proved `InvalidReason::label` is a non-owning
///      `std::string_view` that dangles the moment it is built from anything
///      but a string literal -- it printed the pointer inside the
///      reason-building function and the one the caller received, identical,
///      with the string already destroyed. "Value 42 falls in no band" is
///      exactly the generated-at-the-point-of-failure sentence that proof
///      condemns.
///
/// So the miss is reported the way `Evaluated<Rep>`'s existing error channel
/// already reports every other kind of failure: `std::unexpected {
/// ArithmeticError::DomainError }`. This is not a euphemism borrowed to fit a
/// enum that does not really mean it -- `DomainError` is documented
/// (`error.hpp`) as "an argument was outside the domain of the operation",
/// and a banded lookup's domain **is** the union of its bands, exactly and
/// literally. Nothing is composed into a sentence anywhere in this file: the
/// enum tag is the entire report, structurally, and it threads through
/// `checked_evaluate`, tracing and rendering exactly as every other
/// `ArithmeticError` already does, with no new plumbing.
///
/// **What "structured fields, never a composed sentence" means concretely
/// here, and what is deliberately left to a later task.** The value that
/// missed and the unit it is stated in are never lost -- they are the
/// operand's own evaluated result, sitting with whichever caller dispatched
/// it (and, once a lookup node is taught to a `RecordingSink`, recoverable
/// from the operand's own step exactly as any other value is). The table's
/// identity is available the same way every other node's provenance is
/// available in this library: wrap the lookup in `documented(...)`
/// (`citation.hpp`), which already carries a title, a reference, a section
/// and a full text as separate fields -- never a composed sentence -- and
/// already composes with any `Node`, lookups included, with no change needed
/// here. Actually *rendering* a miss's structured fields into prose --
/// giving `Step` a `StepKind::BandedLookup` and reading `KeyUnit`, `bands`
/// and a wrapping `Citation` back out the way `trace_render.hpp` already
/// does for every other kind -- is `trace.hpp`/`trace_render.hpp` work, and
/// is deliberately outside this task's own file list; nothing here forecloses
/// it, and nothing here composes a sentence that would make it harder.
///
/// **Bands are half-open, `[low, high)`, exactly as task 1 declared them --
/// see `band.hpp`'s file comment.** A value sitting exactly on a shared
/// boundary belongs to the band whose *low* bound it is, never the band
/// whose *high* bound it is; `lookup_tests.cpp` exercises this concretely
/// rather than only asserting it.
///
/// **Band selection is `Rational`-only by construction, not a
/// `double`-specific refusal.** Deciding which band a value falls in is
/// exactly the operation binary floating point is unreliable at: a value a
/// few ULPs off an intended exact boundary would pick the wrong band,
/// silently, with no `ArithmeticError` to report. So this node's own
/// `checked_evaluate_si` refuses to compile for any `Rep` other than
/// `Rational` -- not only `double` -- and says so as "this representation",
/// leaving the instantiation backtrace to name whatever type the caller
/// actually asked for, rather than asserting a specific one that may be
/// wrong. This is unlike `rounding_node.hpp`'s `RepRounding<double>`, which
/// refuses *by specialisation* and leaves `RepRounding<MyRep>` open to any
/// representation a consumer teaches it: `RepTraits` is a documented public
/// extension point (`evaluate.hpp`), and a `RepBandSelection<Rep>` seam
/// mirroring `RepRounding` would be the way to open the same door here.
/// **Deliberately not built in this task** -- it is additive and this task
/// should not absorb it -- so today every representation but `Rational` is
/// closed, full stop, until that seam exists. `checked_evaluate<Result>` --
/// the entry point every test in this file uses -- always computes in
/// `Rational` internally, so this restriction is never reached from there.

#include <formula-cpp/band.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <optional>
#include <type_traits>

namespace formula
{

namespace detail
{
    /// Fails to compile when a banded lookup's key unit does not measure the
    /// dimension of the expression whose value selects a band -- the same
    /// shape and the same reason as `RequireRoundingUnitMatches`
    /// (`rounding_node.hpp`): looking a mass up in a table of length bands is
    /// not a lookup miss, it is a category error, and it is caught here
    /// rather than surfacing as a confusing `checked_convert` failure at
    /// evaluation time.
    template <Unit KeyUnit, typename Operand>
    struct RequireBandedLookupKeyMatches
    {
        static_assert(KeyUnit.dimension == Operand::dimension,
                      "formula: this banded lookup's key unit does not measure the dimension of the "
                      "expression whose value selects a band; the unit's dimension and the operand "
                      "appear in this diagnostic as the template arguments of "
                      "RequireBandedLookupKeyMatches");

        static constexpr bool value = true;
    };

    /// Fails to compile when a banded lookup is given a different number of
    /// corrections than it has bands -- the same shape as `RequireBandsAdjacent`
    /// (`band.hpp`): instantiating a named template on the two counts makes
    /// the compiler print both as template arguments, so the diagnostic
    /// names the mismatch rather than falling through to the compiler's own
    /// "no matching constructor". See `Corrections`, just below, for why this
    /// exists: a short braced list handed to `std::array`'s own aggregate
    /// initialisation silently zero-fills the rest, and `Rational{} == 0/1`
    /// is a perfectly legitimate correction -- indistinguishable from a
    /// forgotten one.
    template <std::size_t Given, std::size_t Expected>
    struct RequireCorrectionCountMatches
    {
        static_assert(Given == Expected,
                      "formula: this banded lookup was given a different number of corrections than it "
                      "has bands; the two counts appear in this diagnostic as the template arguments "
                      "Given and Expected of RequireCorrectionCountMatches -- make the corrections list "
                      "exactly as long as the band table");

        static constexpr bool value = true;
    };

    /// Finds the index of the band in @p Bands containing @p value (already
    /// converted into the table's own key unit), or nothing when @p value
    /// falls in none of them.
    ///
    /// A linear scan, not a binary search, even though `band_table_is_well_
    /// formed`'s own proof (`band.hpp`) shows a well-formed table's bands are
    /// strictly ascending, which would make a binary search valid. Phase 10
    /// is the first thing in this codebase to need an interval search at
    /// all; a method's own published table is rows, not big data, and an
    /// obviously-correct O(N) scan is worth more here than O(log N) --
    /// especially for the half-open, exactly-on-a-boundary case this type
    /// exists to get right. `Bands` is compile-time state (see the file
    /// comment); this is the one place its `int64` pairs are turned into
    /// `Rational` for an exact comparison.
    template <BandTable Bands>
    [[nodiscard]] constexpr std::optional<std::size_t> find_band(Rational value) noexcept
    {
        for (std::size_t index = 0; index < Bands.size(); ++index)
        {
            std::expected<Rational, ArithmeticError> const low =
                Rational::make(Bands[index].lowNumerator, Bands[index].lowDenominator);
            std::expected<Rational, ArithmeticError> const high =
                Rational::make(Bands[index].highNumerator, Bands[index].highDenominator);
            // Unreachable for a `Bands` that reached this point: every
            // `BandedLookupNode` instantiates `RequireValidBandTable<Bands>`,
            // which already refuses a malformed bound at compile time. Guarded
            // anyway, for the same reason `band_is_well_formed` itself is: a
            // silent wrong bucket is worse than one skipped comparison.
            if (!low.has_value() || !high.has_value())
                continue;
            if (*low <= value && value < *high)
                return index;
        }
        return std::nullopt;
    }
} // namespace detail

/// A measured value falls in an interval, and that interval selects a
/// correction -- see the file comment for the join this node is built on and
/// for exactly how a miss is reported.
template <Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
struct BandedLookupNode: NodeBase
{
    static_assert(RequireValidBandTable<Bands>::value);
    static_assert(detail::RequireBandedLookupKeyMatches<KeyUnit, Operand>::value);

    /// One correction per band, stated in `unit` -- the table's *contents*,
    /// runtime state for the same reason `ConstantNode::number` is. See the
    /// file comment.
    std::array<Rational, Bands.size()> corrections {};

    /// The expression whose evaluated value selects a band.
    Operand operand {};

    /// The unit band boundaries are declared in, and the unit `operand`'s
    /// value is compared against them in -- part of the table's *structure*.
    static constexpr Unit keyUnit = KeyUnit;
    /// The band boundaries themselves, already validated above -- reused from
    /// task 1, never reimplemented here.
    static constexpr BandTable<Bands.size()> bands = Bands;
    /// The unit each entry of `corrections` is stated in, and this node's own
    /// declared unit -- the same role `ConstantNode::unit` plays.
    static constexpr Unit unit = ResultUnit;
    /// The dimension of `unit`: what this node itself produces. Independent
    /// of `keyUnit`'s dimension on purpose -- a banded lookup may select a
    /// pressure correction from a measured length, say -- a banded lookup
    /// node stands where a number of *this* dimension stands.
    static constexpr Dimension dimension = ResultUnit.dimension;
};

/// Exactly `N` corrections, one per band -- no more and no fewer. Handed to
/// `banded_lookup` in place of a bare `std::array<Rational, N>`, whose own
/// aggregate initialisation from a short braced list is exactly the "every
/// answer is a lie" failure the rest of this file refuses on the *miss*
/// side, reappearing on the *hit* side: the unwritten elements
/// value-initialise to `Rational{} == 0/1`, and a band whose correction the
/// author forgot to type then answers `0` -- confidently, as a value,
/// indistinguishable from a deliberate zero.
///
/// A named type with two arity-disjoint constructor templates rather than
/// one constrained by `requires` alone: the *matching*-arity constructor
/// does the real construction, and the *every-other*-arity constructor is
/// the only one left viable when the count is wrong, so its body is reached
/// and its `static_assert` (through `RequireCorrectionCountMatches`) names
/// both counts -- rather than the compiler's own generic "no matching
/// constructor for call", which names neither.
///
/// Each element is constrained by `std::convertible_to<Rational>`, not
/// `std::same_as<Rational>`: the arity check is what closes the actual hole
/// (see the class comment above), and over-constraining the element type on
/// top of it would only take away what `Rational` already does correctly.
/// `Rational`'s converting constructor from an integral type is implicit *by
/// design* (`rational.hpp`) -- `same_as` would silently stop `{1, 1, 1}`,
/// the common case of a table whose rows are all unity, from compiling at
/// all, a surprise this library exists to remove. And `Rational`'s
/// floating-point constructor is deliberately poisoned with its own
/// diagnostic pointing at `from_decimal`/`rational_from_double`; `same_as`
/// would reject a stray `{0.45}` with a generic constraint failure instead
/// of letting it reach that better message. `convertible_to` lets both
/// through to `Rational` itself, which is exactly where each is already
/// handled correctly.
template <std::size_t N>
struct Corrections
{
    /// The `N`-correction case: the one path that actually builds `values`.
    template <typename... Rs>
        requires(sizeof...(Rs) == N) && (std::convertible_to<Rs, Rational> && ...)
    constexpr Corrections(Rs... rs) noexcept:
        values { rs... }
    {
    }

    /// Every other count: fails to compile, naming both counts through
    /// `RequireCorrectionCountMatches`'s template arguments.
    template <typename... Rs>
        requires(sizeof...(Rs) != N) && (std::convertible_to<Rs, Rational> && ...)
    constexpr Corrections(Rs...) noexcept
    {
        static_assert(detail::RequireCorrectionCountMatches<sizeof...(Rs), N>::value);
    }

    /// One correction per band, in the table's own declared order.
    std::array<Rational, N> values {};
};

/// Declares a banded lookup: `banded_lookup<unit::Millimetre, Bands,
/// unit::One>(var<Diameter>, { rat(95, 100), rat(1), rat(105, 100) })`.
///
/// `KeyUnit`, `Bands` and `ResultUnit` are deliberately not deduced -- the
/// same reason `rounded<U, Places, Mode>` (`rounding_node.hpp`) leaves its
/// three non-operand parameters unstated at the call site's argument list:
/// a table's structure is the author's declared intent, not something
/// inferred from whatever `corrections` happens to look like. The braced
/// list at the call site still reads exactly as it did before `Corrections`
/// existed -- only its target type changed, from `std::array<Rational, N>`
/// to `Corrections<N>` -- because the call site's target type is already
/// known from the explicit template arguments, so list-initialisation finds
/// `Corrections`'s constructor the same way it found `std::array`'s
/// aggregate initialisation before.
template <Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
[[nodiscard]] constexpr BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand> banded_lookup(
    Operand operand, Corrections<Bands.size()> corrections) noexcept
{
    return BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand> { {}, corrections.values, operand };
}

/// Evaluates the operand, converts its value into `KeyUnit`, and looks up the
/// band it falls in. Absence propagates, same as every other node; a value
/// that falls in no band is reported as `ArithmeticError::DomainError` --
/// never a default, never the nearest band, never the first -- see the file
/// comment for why that is the honest answer rather than a euphemism.
template <typename Rep = Rational, Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand, typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const operand = detail::dispatch<Rep>(node.operand, environment, sink);
    if (!operand.has_value())
    {
        Evaluated<Rep> const failed = std::unexpected { operand.error() };
        sink.produced(node, failed);
        return failed;
    }
    if (!operand->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    if constexpr (!std::is_same_v<Rep, Rational>)
    {
        // See the file comment: band selection is Rational-only by
        // construction, refusing every representation but Rational rather
        // than only double, so the message says "this representation"
        // rather than naming a specific type it might be wrong about --
        // the instantiation backtrace already names whatever `Rep` the
        // caller actually asked for. Dependent on `Rep` so this fires only
        // when this function is actually instantiated with a non-`Rational`
        // `Rep`, not merely declared -- the same trick
        // `RepRounding<double>::round_in` uses.
        static_assert(sizeof(Rep) == 0,
                      "formula: a banded lookup node can only be evaluated with Rep = Rational -- "
                      "deciding which band a value falls in needs exact arithmetic this representation "
                      "may not give; evaluate this formula with Rep = Rational instead "
                      "(checked_evaluate<Result> always does)");
        return std::unexpected { ArithmeticError::DomainError };
    }
    else
    {
        std::expected<Rational, ArithmeticError> const valueInKey =
            checked_convert(**operand, coherent(KeyUnit.dimension), KeyUnit);
        if (!valueInKey.has_value())
        {
            Evaluated<Rep> const failed = std::unexpected { valueInKey.error() };
            sink.produced(node, failed);
            return failed;
        }

        std::optional<std::size_t> const index = detail::find_band<Bands>(*valueInKey);
        if (!index.has_value())
        {
            // A miss is not a value: no default, no nearest-band, no
            // first-band fallback. `DomainError` is literally true here, not
            // a euphemism -- see the file comment.
            Evaluated<Rep> const missed = std::unexpected { ArithmeticError::DomainError };
            sink.produced(node, missed);
            return missed;
        }

        Evaluated<Rep> const result = detail::in_si<Rep>(node.corrections[*index], ResultUnit);
        sink.produced(node, result);
        return result;
    }
}

} // namespace formula
