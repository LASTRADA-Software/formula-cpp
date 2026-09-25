// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Lookup tables: a method publishes rows, and something about the specimen
/// selects one of them. Three kinds live here, and they share one vocabulary
/// for everything they have in common:
///
///  - **Banded lookup** (`BandedLookupNode`): a measured value falls in an
///    interval, and that interval selects a correction.
///  - **Exact lookup** (`ExactLookupNode`): a category key -- a discriminator
///    such as a specimen shape or an apparatus variant, which is *not* a
///    quantity -- names a row directly.
///  - **Interpolating lookup** (`InterpolatingLookupNode`): a measured value
///    sits between two rows, and the answer is the value those two rows imply
///    at that point -- a number that appears in no row of the table.
///
/// All three report a miss identically, split structure from contents
/// identically, and share one `Corrections<N>` wrapper and one key-unit guard.
/// Everything below about a miss, about `documented()` carrying a table's
/// identity, and about what is left to a later task is written once and binds
/// all three; the exact-lookup and interpolating-lookup sections near the end
/// of this comment add only what is genuinely particular to each.
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
/// falls in no band -- or an `ExactLookupNode` whose key names no row of its
/// table -- has found nothing: not zero, not the nearest band, not the
/// first row. There is no default-value parameter and no fallback of any
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
///      `::%value(...)` itself, and an `ArithmeticError` returned by any node
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
///
/// ===========================================================================
///
/// **Exact lookup: a category key names a row.** The second table kind in
/// this file. Not every published table buckets a measurement: a method just
/// as often publishes one row per *variant* -- a specimen shape, an apparatus
/// type, a curing regime -- and the row is chosen by which variant is in
/// front of you, not by how large anything is. That key is a discriminator,
/// not a quantity: it has no dimension, no unit and no order, so a band table
/// cannot express it and `find_band`'s comparison means nothing for it.
///
/// **How a key is spelled, and why.** A key is an enumerator of a *scoped*
/// enumeration the method's author declares, and a table is
/// `KeyTable<Shape, 3>` -- `std::array<Shape, 3>` -- as a non-type template
/// parameter. The alternative considered, and rejected, was
/// `detail::FixedString` (phase 4), which is equally usable as an NTTP. The
/// deciding question is the one a method author will actually hit: **what
/// happens when a key is absent.**
///
///   1. With a string key, the author's realistic mistake -- writing
///      `"cylindr"` where the table says `"cylinder"` -- compiles cleanly and
///      becomes a runtime miss. A miss carries `ArithmeticError::DomainError`
///      and nothing else (see below), so what the author gets back is a
///      formula that reports a domain error for every specimen, with no
///      statement anywhere of which key was wrong. With a scoped
///      enumeration, `Shape::Cylindr` is a compile error from the language
///      itself, at the offending token, quoting the misspelling and listing
///      the enumerators that do exist. No library diagnostic can beat that,
///      and none is needed to get it.
///   2. A key's *type* names the categorisation. Two tables keyed on `Shape`
///      and on `Apparatus` cannot be crossed: passing the wrong one is a type
///      error naming both types. Two tables keyed on strings both accept
///      `"a"`, and neither can tell it was meant for the other.
///   3. Duplicate-key detection (below) is exact and cheap on enumerators.
///
/// What the enumeration route costs, stated rather than hidden: the
/// enumerator's spelling is the author's, not the published table's, so an
/// enumerator can drift from the row label it stands for. That is a
/// *documentation* fact, and this library already has one place for
/// documentation facts -- `documented()`/`Citation` (`citation.hpp`) -- which
/// is where a table's identity belongs for the exact lookup for exactly the
/// same reason it does for the banded one. Per-row labels are not modelled
/// here and are not smuggled into the node.
///
/// `std::is_scoped_enum_v` is required -- not enumerations generally, and not
/// `int`. An unscoped enumeration and an integer both convert to and from
/// arithmetic silently, which gives back the one property point 1 is built
/// on: with `KeyTable<int, 3> { 1, 2, 3 }` the author's typo is `3` where `2`
/// was meant, and nothing catches it. The refusal is `RequireScopedEnumKey`,
/// in the node's own body so it fires whether or not the factory's result is
/// used.
///
/// **Give each translation unit's key enumeration a name of its own.** A key
/// enumeration declared in an anonymous namespace -- the ordinary way to write
/// one in a `.cpp` -- is an internal-linkage type, so `KeyTable<Shape, N>` in
/// two such files names two *different* specialisations. Clang spells an
/// anonymous namespace `_GLOBAL__N_1` with no per-translation-unit
/// discriminator, so if two files' enumerations share a name **and** their
/// tables share their element values, both template parameter objects mangle to
/// one name and land in a COMDAT group keyed by it; the linker keeps one group
/// and the other file's reference points into a discarded section. Measured on
/// clang 20.1.8, with `nm` over the objects: each such object is emitted as a
/// *local* symbol yet still carries that shared group name, and the mangling
/// encodes the element values, which is why tables differing in any value link
/// cleanly. cl 19.51, clang-cl 22 and g++ 14.2 all link it without complaint.
///
/// This is worth a paragraph because of how it fails: a link error with a
/// dangling relocation, naming a mangled symbol and not the key type, from a
/// program that compiled without a single diagnostic. Distinct names cost
/// nothing and end it; an enumeration declared in a *header* has external
/// linkage instead, in which case both files name one type and one
/// specialisation and the question does not arise.
///
/// **A table's own well-formedness is that no key repeats.** That is the
/// whole of it: an exact table has no order to violate, no boundary to share
/// and no coverage to leave a gap in. A repeated key is a real typo a
/// published table can contain, and it is not harmless -- the second row
/// becomes unreachable, so a correction the author entered is silently never
/// selected, and which of the two wins depends on nothing but scan direction.
/// `RequireValidKeyTable` refuses it at compile time through
/// `RequireKeysDistinct`, which names both offending keys as its template
/// arguments exactly as `RequireBandsAdjacent` names both offending bands.
/// `key_table_is_well_formed` is the same question for a table that only
/// arrives at runtime, built on the same `keys_match` predicate, so the two
/// cannot drift -- the arrangement `band.hpp` uses, for the same reason.
///
/// An empty key table validates and always misses, for the identical reason
/// `BandTable<0>` does (`band.hpp`'s file comment): there is no pair that
/// could repeat, and a table naming no rows leaves the whole domain
/// undefined, which is a thing this library can say honestly.
///
/// **Where the key comes from, and what that does not cover.** An
/// `ExactLookupNode` has no operand. A category is not a quantity, and this
/// library's `Environment` carries `Measured<Q>`/`Entered<Q>` -- numbers --
/// and nothing else, so there is no existing channel through which a
/// discriminator could arrive at evaluation time. The key is therefore
/// ordinary runtime state on the node, arriving through the factory, for the
/// same reason `ConstantNode::number` is runtime state: the *structure*
/// (which rows exist, and the unit they are stated in) is the method, and
/// lives in the type; the *selection* and the *contents* are facts about this
/// specimen and this customer's registered table, and both arrive late.
/// Concretely, a formula whose key varies per specimen is a function of the
/// key -- `auto f(Shape s) { return var<Force> / var<Area> *
/// exact_lookup<Shapes, unit::One>(s, { ... }); }` -- and a node is a cheap
/// aggregate, so that is one build per specimen, not one evaluation per
/// specimen. **Giving `Environment` a categorical entry, so that a key could
/// be supplied alongside the measurements, is deliberately not done here**:
/// it is a change to `environment.hpp`, outside this task's files. Nothing
/// here forecloses it -- `Environment`'s `detail::EntryTraits` is an open
/// specialisation point, and this node's `checked_evaluate_si` already takes
/// the environment -- but it is **a second node kind, not a field swap on
/// this one**, and a later task should plan for that rather than the easier
/// version. The reason is `key`'s own comment below: `KeyOf<Keys>{}` is a
/// legitimate key that hits a row, so `ExactLookupNode` has no spelling for
/// "no key yet, take it from the environment". Whatever reads a key from an
/// environment has to say so in its *type*, exactly as everything else
/// structural in this file does.
///
/// **A missing key is a miss, in the one vocabulary this file already has.**
/// `std::unexpected { ArithmeticError::DomainError }`, through
/// `Evaluated<Rep>`'s existing error channel -- the identical mechanism a
/// value falling in no band uses, for the identical reason: `DomainError` is
/// documented (`error.hpp`) as "an argument was outside the domain of the
/// operation", and an exact table's domain **is** its set of keys, exactly
/// and literally. No second enumerator, no `Outcome::invalid`, no
/// `InvalidReason`, no composed sentence. The reasoning is given in full
/// under "How the miss is actually reported" above and is deliberately not
/// restated here, because restating it is how two surfaces that must agree
/// begin to drift.
///
/// **Exact-key selection is `Rational`-only by construction** -- as the
/// banded node is, and for a different reason, which is worth stating rather
/// than copying across a justification that does not transfer. Deciding which
/// band a value falls in is arithmetic, and binary floating point is
/// unreliable at it. Deciding whether two enumerators are the same is not
/// arithmetic at all, and would be exact in any representation. So what
/// closes `Rep` here is not floating point but the lookup family speaking
/// with one voice: a formula containing a lookup of either kind evaluates in
/// `Rational`, full stop, rather than in whichever `Rep` happens to be legal
/// for the kind that got used. As with the banded node the guard says "this
/// representation" and leaves the instantiation backtrace to name the
/// caller's actual type, and **no `RepExactSelection` seam is built** --
/// mirroring the decision not to build `RepBandSelection`.
/// `checked_evaluate<Result>` always computes in `Rational`, so this is never
/// reached from the entry point every test here uses.
///
/// **What a later task is owed, stated because the error channel cannot say
/// it.** A miss carries `DomainError` and nothing more, so "which key missed
/// which table" has to be rendered from the trace -- and for the exact lookup
/// that is a harder obligation than for the banded one. A banded miss still
/// leaves its evidence in the tree: the value that missed is the operand's
/// own evaluated result, and once a lookup node is taught to a
/// `RecordingSink` the operand contributes a step of its own carrying that
/// value. **An exact lookup has no operand**, so the key that missed appears
/// in no step at all unless `ExactLookupNode`'s own step records it. Nothing
/// here loses the key -- it is a plain data member of the node the sink is
/// handed, readable as `node.key`, and `Keys` is a compile-time property of
/// the node's type -- but recovering it *does* require the later task to add
/// a field for it, where the banded case can lean on a step that already
/// exists. `detail::StepKindOf` (`trace.hpp`) has a specialisation for
/// neither lookup node today, so both are equally untraceable right now; the
/// asymmetry is written down here so the later task does not discover it
/// after designing for the banded case alone.
///
/// ===========================================================================
///
/// **Interpolating lookup: a value between two rows.** The third table kind in
/// this file, and the only one whose answer is a number the table does not
/// contain. A published curve states a value at a handful of key points --
/// 0, 10, 20, 30 mm, say -- and a specimen measured at 15 mm is not "in the
/// 10-20 row": the method means the value the two surrounding rows imply at
/// 15 mm. A band table cannot express that, because a band selects one stored
/// correction for its whole interval; this node computes.
///
/// **The table is points, not intervals, and that is why `BandTable` is not
/// reused.** A band carries a low and a high bound and the correction belongs
/// to the interval between them. An interpolating row carries one key and the
/// value stated *at* that key, and the interval between two rows is implied by
/// the rows rather than declared. So `Breakpoint` is a single
/// numerator/denominator pair -- `std::int64_t` rather than `Rational`, for
/// exactly the reason `Band` is (`band.hpp`: `Rational` keeps its members
/// private, so it is not structural and cannot be a non-type template
/// parameter) -- and `BreakpointTable<N>` is an alias over `std::array`, for
/// the reason `BandTable` and `KeyTable` are.
///
/// The structure/contents split is `BandedLookupNode`'s, unchanged: `KeyUnit`,
/// `Points` and `ResultUnit` are the table's *structure* and live in the type;
/// `corrections` -- the value stated at each breakpoint -- are its *contents*
/// and arrive at runtime, through the same `Corrections<N>` wrapper, closing
/// the same short-braced-list hole.
///
/// **A table's own well-formedness is that its breakpoints strictly ascend.**
/// Two rows at the same key state two different values at one key and leave a
/// segment of zero width to divide by; two rows out of order make "between
/// these two rows" meaningless. One rule -- `first < second` -- and therefore
/// one diagnostic, the same judgement `band.hpp` makes in treating an inverted
/// band and a zero-width band as one failure of one rule.
/// `RequireBreakpointsAscend` refuses it at compile time and names both
/// offending breakpoints; `breakpoint_table_is_well_formed` is the same
/// question for a curve that only arrives at runtime, built on the same
/// `breakpoints_ascend` predicate, so the two cannot drift -- the arrangement
/// `band.hpp` and the exact lookup both use.
///
/// **Only neighbouring pairs are compared, and that is a proof rather than an
/// optimisation.** If every adjacent pair ascends then the whole table ascends
/// by transitivity, so no two rows anywhere in the table can share a key or sit
/// out of order -- the same chaining argument `band_table_is_well_formed`'s own
/// comment sets out. This is the one place the interpolating table differs from
/// the *exact* table, which must compare every pair because its keys have no
/// order for transitivity to run along.
///
/// Alongside that, `breakpoint_is_well_formed` asks the separate question of
/// whether a row's key is a rational number at all -- a zero denominator, or a
/// pair that cannot be reduced without overflow. It is separate for the reason
/// `band_is_well_formed` is separate from `bands_are_adjacent`: a one-row table
/// has no pair, so the ordering sweep never runs and only a per-row check can
/// catch a malformed key there; and where both apply, an author reading "this
/// breakpoint is not a number" is better served than one reading "these
/// breakpoints are not in ascending order".
///
/// An empty table validates and always misses, and a **one-row** table
/// validates and answers at exactly its own key and nowhere else -- both for
/// the reason `BandTable<0>` validates (`band.hpp`'s file comment). Neither is
/// malformed; both are merely narrow, and a table that leaves part of the
/// domain undefined is a thing this library can say honestly. A one-row table
/// is explicitly **not** refused for "having nothing to interpolate between":
/// it states a value at one key, and reporting that value at that key is not
/// interpolation, but it is not a lie either.
///
/// **The domain is closed at BOTH ends -- `first` through `last`, both
/// included -- and this is a deliberate difference from a band table, not an
/// inconsistency.** A band's high bound is exclusive because it is shared with
/// the next band's low bound and a value sitting on it must belong to exactly
/// one of them. A breakpoint is not a boundary between rows, it *is* a row: the
/// table states a value there. So the last breakpoint is a hit, and excluding
/// it would make the table's own final row unreachable -- the table would
/// answer for every input except the one it states most directly.
/// `lookup_tests.cpp` asserts the two kinds' top ends against each other in one
/// test, so that "harmonising" them in either direction fails there rather than
/// in a consumer.
///
/// **A value exactly on a row returns that row, and is not interpolated.**
/// Stated as a rule because it is one, even though it is observable in exactly
/// one place: interpolating across the segment a row begins would return that
/// row's own value anyway, since the weight is exactly zero. The place it is
/// observable is the table's **last** row, which begins no segment at all.
///
/// It also means the **interpolation** does no arithmetic on a row hit, so no
/// row can be reported as an overflow *of the interpolation*. That is the whole
/// of the guarantee, and an earlier revision of this comment claimed more: that
/// a row whose value is representable can never come back as an `Overflow` at
/// all. It can. `checked_evaluate_si` still hands the answer to
/// `detail::in_si`, which converts it out of `ResultUnit` into the coherent SI
/// unit, and **a unit conversion is arithmetic** -- a row stating `2^62`
/// kilometres is a perfectly representable `Rational` that overflows on the way
/// to metres. That path is shared with the banded and the exact lookup, which
/// have it for exactly the same reason, and nothing about it is particular to
/// interpolation. `lookup_tests.cpp` pins both halves: a row hit that overflows
/// in the conversion, and an interpolation that overflows in the interpolation.
///
/// **There is no extrapolation.** A value below the first row or above the last
/// one is a miss -- `ArithmeticError::DomainError` through `Evaluated<Rep>`,
/// the identical mechanism and the identical reason as every other miss in this
/// file, and deliberately not a third spelling for "found nothing".
/// Interpolation between two rows yields a value the table's author *implied*;
/// running the final segment's slope onwards, or clamping to the final row,
/// yields one the document never defined -- invented from the slope of the last
/// segment, which is an artefact of where the table happened to stop. That is
/// the same lie this file refuses in having no default-value parameter, no
/// nearest-band fallback and no first-row fallback. **A method that genuinely
/// says "hold the last value beyond the final row" is a clamp its author writes
/// explicitly, in the formula, where a reader can see it** -- never a default
/// this library applies silently.
///
/// **The result is exact, and where it cannot be, it says so.** Every step is
/// `Rational`'s own checked arithmetic -- `checked_sub` for the span and the
/// rise, `checked_mul` and `checked_div` for the share of the rise, and
/// `checked_add` onto the lower row -- so
/// `y0 + (x - x0)(y1 - y0)/(x1 - x0)` is computed with no rounding anywhere,
/// and a result no finite decimal can hold (14/15, say) comes back as exactly
/// 14/15. Nothing here reaches for phase 8's rounding, and nothing here loses
/// precision silently: the *only* way the answer is not the exact rational the
/// two rows imply is that some intermediate lies outside `Rational`'s
/// representable range, and that is reported as `ArithmeticError::Overflow`
/// through the same channel a miss uses, never approximated away -- asserted
/// on a table whose exact answer genuinely does not fit, rather than only
/// reasoned about. `x1 - x0` cannot be zero -- strictly ascending breakpoints
/// are enforced at compile time -- so the division is guarded by the table's
/// own validation rather than by a runtime test.
///
/// **`Rep` is closed to `Rational`, and here the arithmetic reason is the true
/// one.** The banded node's guard gives an arithmetic reason (band selection
/// really is arithmetic) and the exact node's deliberately does not (comparing
/// two enumerators is exact in every representation). This node has the banded
/// node's ground and one of its own that is stronger: locating the segment is
/// the same comparison band selection is, and the answer is then *computed*, so
/// a representation that rounds would hand back a number that is not the one
/// the table's own rows imply -- the precise defect this task exists to avoid.
/// As with both other kinds the message says "this representation" rather than
/// naming a type the instantiation backtrace already names, and no
/// `RepInterpolation<Rep>` seam is built, mirroring the decision not to build
/// `RepBandSelection`.
///
/// **What a later task is owed.** Everything the banded lookup's own note above
/// says applies unchanged: a miss carries `DomainError` and nothing else, the
/// value that missed is the operand's own evaluated result, and the table's
/// identity comes from `documented()`. One thing is new, and belongs to task 6
/// rather than here: this node can produce `ArithmeticError::Overflow` *of its
/// own*, from the interpolation, where the other two kinds only ever propagate
/// one they were handed. A trace that wants to say "the interpolation
/// overflowed" rather than "something below this overflowed" needs this node's
/// own step to say so; nothing here loses the information, and nothing here
/// composes a sentence that would make saying it harder.

#include <formula-cpp/band.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <type_traits>
#include <utility>

namespace formula
{

namespace detail
{
    /// Fails to compile when a lookup's key unit does not measure the
    /// dimension of the expression whose value selects a row -- the same
    /// shape and the same reason as `RequireRoundingUnitMatches`
    /// (`rounding_node.hpp`): looking a mass up in a table of length rows is
    /// not a lookup miss, it is a category error, and it is caught here
    /// rather than surfacing as a confusing `checked_convert` failure at
    /// evaluation time.
    ///
    /// **Worded for every table kind that has a key unit at all**, which is
    /// the banded and the interpolating lookup -- for the same reason
    /// `RequireCorrectionCountMatches` just below is: the rule, the mistake
    /// and the mechanism are one and the same, and only the word for a row
    /// differs. A second guard saying this in the interpolating lookup's own
    /// words is precisely how two surfaces that must agree start to disagree.
    /// (The exact lookup has no key unit: its key is a discriminator, not a
    /// quantity, so there is nothing to compare a dimension against.)
    template <Unit KeyUnit, typename Operand>
    struct RequireLookupKeyMatches
    {
        static_assert(KeyUnit.dimension == Operand::dimension,
                      "formula: this lookup table's key unit does not measure the dimension of the "
                      "expression whose value selects a row; the unit's dimension and the operand "
                      "appear in this diagnostic as the template arguments of RequireLookupKeyMatches");

        static constexpr bool value = true;
    };

    /// Fails to compile when a lookup is given a different number of
    /// corrections than its table has rows -- the same shape as
    /// `RequireBandsAdjacent` (`band.hpp`): instantiating a named template on
    /// the two counts makes the compiler print both as template arguments, so
    /// the diagnostic names the mismatch rather than falling through to the
    /// compiler's own "no matching constructor". See `Corrections`, just
    /// below, for why this exists: a short braced list handed to
    /// `std::array`'s own aggregate initialisation silently zero-fills the
    /// rest, and `Rational{} == 0/1` is a perfectly legitimate correction --
    /// indistinguishable from a forgotten one.
    ///
    /// **Worded for every table kind on purpose.** A banded lookup's rows are
    /// its bands, an exact lookup's rows are its keys and an interpolating
    /// lookup's rows are its breakpoints, but the hole, the mechanism that
    /// closes it and the mistake an author makes are one and the same, so
    /// there is one guard and one sentence. A second guard saying the same
    /// thing in another table kind's own words is precisely how two surfaces
    /// that must agree start to disagree.
    template <std::size_t Given, std::size_t Expected>
    struct RequireCorrectionCountMatches
    {
        static_assert(Given == Expected,
                      "formula: this lookup table was given a different number of corrections than it "
                      "has rows; the two counts appear in this diagnostic as the template arguments "
                      "Given and Expected of RequireCorrectionCountMatches -- make the corrections list "
                      "exactly as long as the table it belongs to, one correction per row");

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

/// Exactly `N` corrections, one per row of a lookup table -- no more and no
/// fewer. Shared by every table kind in this file: `N` is the band count for
/// `banded_lookup`, the key count for `exact_lookup` and the breakpoint count
/// for `interpolating_lookup`, because the hole being closed is the same
/// hole. It stands in for a bare `std::array<Rational, N>`, whose own
/// aggregate initialisation from a short braced list is exactly the "every
/// answer is a lie" failure the rest of this file refuses on the *miss* side,
/// reappearing on the *hit* side: the unwritten elements value-initialise to
/// `Rational{} == 0/1`, and a band, a key or a breakpoint whose correction the
/// author forgot to type then answers `0`, confidently, as a value,
/// indistinguishable from a deliberate zero -- and for the interpolating
/// table, a forgotten row drags the whole segment either side of it down with
/// it.
///
/// **It is every node's own member type, not only the factories' parameter
/// type, and the difference was measured rather than argued.** With a raw
/// array on the node and this wrapper only on the factory, the guard covered
/// every route *except the one that needs no factory*: every lookup node is a
/// public aggregate with public members, so
///
///     inline constexpr ExactLookupNode<ThreeKeys, unit::One> node {
///         {}, { rat(7, 10) }, Shape::Prism };
///
/// compiled, linked, and evaluated the two rows nobody typed as `0` -- checked
/// against the installed package on all three node kinds, all three of which
/// did it. The factory's parameter type cannot see that call, because there is
/// no call. Making the member itself a `Corrections<N>` is what closes it: the
/// braced list now initialises this type, a short one selects the
/// arity-mismatch constructor below, and its `static_assert` names both counts
/// at the offending line. `lookup_short_corrections_no_factory.cpp` and its
/// two siblings pin exactly that, one per node kind, and reverting any one
/// member to a raw array fails that kind's case alone.
///
/// Nodes therefore declare `Corrections<N> corrections;` with **no default
/// member initialiser**, and that omission is load bearing: `{}` for a table
/// of three rows is a count of zero, which is the very mistake being refused,
/// so a node cannot be default-constructed and must state its contents. No
/// consumer noticed the change -- `operator[]` below keeps
/// `node.corrections[index]` meaning what it always meant in the renderer, the
/// tracer and the evaluator alike.
///
/// **Neither the trait nor the concept will tell you a node is not
/// default-constructible, and they fail differently.** The refusal is a
/// `static_assert` in a constructor *body*, so
/// `std::is_default_constructible_v<SomeLookupNode>` answers **`true`** -- a
/// trait only asks whether a constructor is viable, and never instantiates one
/// to find out. `std::default_initializable` and `std::semiregular` do
/// instantiate it, and so do not answer at all: they **hard-error**. Measured
/// on cl 19.51 and clang-cl 22, with a deleted-default-constructor control
/// that both traits and both concepts answer `false` for cleanly, so the
/// instruments are shown able to say it. The error text is this library's own
/// sentence naming both counts (`0` and `3`), which is the good message -- but
/// it arrives from inside `<concepts>`, at a line the caller never wrote, and
/// **a concept that hard-errors cannot be used as a predicate**: it will not
/// constrain an overload, pick a branch or SFINAE anything. Generic code over
/// nodes must not ask. The honest check is whether a factory was called.
///
/// **`corrections` is no longer a range, and `operator[]` is const and returns
/// by value.** So `for (auto& correction: node.corrections)`,
/// `auto& correction = node.corrections[index]` and
/// `node.corrections[index] = ...` no longer compile, where they did while the
/// member was a `std::array`. `values` stays public and is the route for all
/// three -- this is a transparent aggregate of a table's contents, not an
/// encapsulation. Written down because **no in-tree consumer needed changing**,
/// which is exactly why nothing in this repository will remind anybody.
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

    /// One correction per row, in the table's own declared order -- per band
    /// for a banded lookup, per key for an exact one, per breakpoint for an
    /// interpolating one.
    std::array<Rational, N> values {};

    /// The correction at @p index, so that every consumer reads a table's
    /// contents the way it read them when this was a bare `std::array`.
    ///
    /// Present so that becoming a node's member type costs the rest of the
    /// library nothing: `node.corrections[index]` means what it always meant,
    /// in the renderer, the tracer and the evaluator alike. `values` stays
    /// public alongside it -- this is a transparent aggregate of the table's
    /// contents, not an encapsulation.
    [[nodiscard]] constexpr Rational operator[](std::size_t index) const noexcept
    {
        return values[index];
    }

    /// How many corrections there are -- `N`, and a row count is a thing a
    /// consumer can reasonably ask a table's contents for.
    [[nodiscard]] static constexpr std::size_t size() noexcept
    {
        return N;
    }
};

/// A measured value falls in an interval, and that interval selects a
/// correction -- see the file comment for the join this node is built on and
/// for exactly how a miss is reported.
template <Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
struct BandedLookupNode: NodeBase
{
    static_assert(RequireValidBandTable<Bands>::value);
    static_assert(detail::RequireLookupKeyMatches<KeyUnit, Operand>::value);

    /// One correction per band, stated in `unit` -- the table's *contents*,
    /// runtime state for the same reason `ConstantNode::number` is. See the
    /// file comment.
    ///
    /// **`Corrections<N>` and not `std::array<Rational, N>`, and that choice
    /// is what closes the hole rather than the factory's parameter type.** See
    /// `Corrections` for the measurement: with a raw array here, a caller who
    /// declares this aggregate directly -- which its public members invite --
    /// could hand it a short braced list, and the rows they never typed would
    /// answer `0`. There is deliberately no default member initialiser: `{}`
    /// for a table of three rows is a count of zero, which is exactly the
    /// mistake being refused.
    Corrections<Bands.size()> corrections;

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
    return BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand> { {}, corrections, operand };
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

// ---------------------------------------------------------------------------
// Exact lookup: a category key names a row. See the file comment's
// "Exact lookup" section for why a key is a scoped enumerator, where the key
// comes from, and why a missing key is reported the same way a value in no
// band is.
// ---------------------------------------------------------------------------

/// A table of category keys, in the order their corrections are declared. An
/// alias template over `std::array`, for the same reason `BandTable` is one
/// (`band.hpp`): a spike compiled `template <KeyTable Keys>` with both `Key`
/// and `N` deduced from the template argument, on cl, clang-cl, clang++ and
/// g++, so a wrapping struct would add a name to unwrap and nothing else.
///
/// `Key` is a scoped enumeration -- enforced by `RequireScopedEnumKey` in
/// `ExactLookupNode`'s own body rather than by a constraint here, so that the
/// diagnostic is this library's sentence and not "constraints not satisfied".
template <typename Key, std::size_t N>
using KeyTable = std::array<Key, N>;

/// The key type of a `KeyTable` given as a template argument -- `Shape` for a
/// `KeyTable<Shape, 3>`. Written once here because it is needed in three
/// places (the node's member, the factory's parameter, `find_key`'s
/// parameter) and spelling `typename decltype(Keys)::%value_type` in each is
/// how one of them ends up subtly different from the others.
///
/// The `%` is Doxygen's "do not try to link this" prefix, and it is load
/// bearing rather than a typo: **Doxygen 1.9.8 -- the version `pages.yml`
/// installs -- reads a leading `::` as an explicit link request even inside a
/// code span**, and fails the build under `WARN_AS_ERROR = FAIL_ON_WARNINGS`
/// when it cannot resolve the name. Newer Doxygen does not, which is exactly
/// how this reached the branch: it was verified against 1.18.0 on a
/// contributor's machine and was red for the one CI actually runs. The `%` is
/// stripped from the generated HTML, so nothing leaks onto the page. Same
/// treatment, same reason, on `Outcome`'s `::%value(...)` in this file's
/// comment above.
template <KeyTable Keys>
using KeyOf = typename std::remove_cvref_t<decltype(Keys)>::value_type;

namespace detail
{
    /// Fails to compile when an exact lookup's keys are not enumerators of a
    /// scoped enumeration -- see the file comment for why `int` and an
    /// unscoped enumeration are refused rather than merely discouraged: both
    /// convert to and from arithmetic silently, which destroys the one
    /// property the choice of an enumerated key was made for, that a
    /// misspelled key is a compile error at the offending token.
    ///
    /// Declared here, ahead of the predicates, because **every** public entry
    /// point that takes a key enforces it, not only the node. A validator that
    /// accepted an `std::array<int, N>` no node would ever take is two
    /// surfaces disagreeing about the same question -- the defect this phase
    /// keeps finding -- and a runtime loader (phase 10 task 4) reaching for
    /// `key_table_is_well_formed` is exactly where it would bite.
    template <typename Key>
    struct RequireScopedEnumKey
    {
        static_assert(std::is_scoped_enum_v<Key>,
                      "formula: an exact lookup's keys must be enumerators of a scoped enumeration "
                      "(enum class); the offending key type appears in this diagnostic as the template "
                      "argument Key of RequireScopedEnumKey -- an int or a plain enum converts to and "
                      "from arithmetic silently, so a mistyped key would be a value rather than a "
                      "compile error");

        static constexpr bool value = true;
    };
} // namespace detail

/// The single predicate every "is this the same key" question in this file is
/// built on: exact equality of two enumerators, with no ordering and no
/// conversion. Used by `key_table_is_well_formed` for a table that arrives at
/// runtime, by `RequireKeysDistinct` for one fixed at compile time, and by
/// `detail::find_key` at evaluation time -- so a change to what "the same
/// key" means cannot reach one of those three without reaching all of them.
/// The same arrangement `bands_are_adjacent` has in `band.hpp`, and for the
/// same reason: this project has had two checks of one fact drift apart
/// before.
///
/// Refuses the same key types `ExactLookupNode` refuses, and says so with the
/// same sentence. "Cannot drift" has to cover *which key types are accepted*
/// as well as *what the same key means*: a predicate that answered for an
/// `std::array<int, N>` would be validating a table no node could ever be
/// built from.
template <typename Key>
[[nodiscard]] constexpr bool keys_match(Key first, Key second) noexcept
{
    static_assert(detail::RequireScopedEnumKey<Key>::value);
    return first == second;
}

/// True when `table` is well-formed: no key appears twice. That is the whole
/// of an exact table's well-formedness -- see the file comment -- and it is
/// checked for every pair, not merely for neighbours, because an exact table
/// has no declared order for a duplicate to hide behind: the same key in rows
/// 0 and 7 is exactly as unreachable as the same key in rows 3 and 4.
///
/// An empty table and a single-key table are both well-formed: neither has a
/// pair that could repeat. See `band.hpp`'s file comment for why an empty
/// table is treated as valid rather than refused -- an exact table naming no
/// rows always misses, which is a thing this library can say honestly. A
/// **two-row** table is the smallest one that can be malformed at all, and is
/// asserted on directly in `lookup_tests.cpp` rather than left to follow from
/// the general case.
///
/// Refuses the same key types `ExactLookupNode` refuses -- see `keys_match`
/// just above for why a validator that accepted more than the node does would
/// be a defect rather than a convenience. It carries no guard of its own,
/// deliberately: the call to `keys_match` in its body is instantiated whenever
/// this function is, **including for `N == 0` and `N == 1` where the loops
/// never run** (a call expression in a template's body is instantiated with
/// the template, not when control reaches it -- measured on all four
/// compilers, and `exact_lookup_int_key_table.cpp` pins it). A second
/// `static_assert` here would therefore be one no test could ever distinguish
/// from this one, which is an assertion nothing can keep honest.
template <typename Key, std::size_t N>
[[nodiscard]] constexpr bool key_table_is_well_formed(KeyTable<Key, N> const& table) noexcept
{
    for (std::size_t first = 0; first + 1 < N; ++first)
        for (std::size_t second = first + 1; second < N; ++second)
            if (keys_match(table[first], table[second]))
                return false;
    return true;
}

/// Fails to compile when a table declares the same key twice -- so the second
/// row is unreachable and a correction the author entered is silently never
/// selected.
///
/// Same shape and same reason as `RequireBandsAdjacent` (`band.hpp`):
/// instantiating a named template on the two values makes the compiler print
/// the offending key, and the wording is ours so the negative-compile harness
/// can assert the reason rather than merely the failure. Reached through
/// `::value`, for the same reason `RequireBandsAdjacent` is.
template <auto First, auto Second>
struct RequireKeysDistinct
{
    static_assert(!keys_match(First, Second),
                  "formula: this exact lookup table declares the same key twice; the later row can "
                  "never be selected, so a correction that was entered would silently never be used; "
                  "the offending key appears in this diagnostic as both template arguments First and "
                  "Second of RequireKeysDistinct");

    /// Always `true` once reached -- see `RequireBandsAdjacent::value`.
    static constexpr bool value = true;
};

namespace detail
{
    /// Expands to one `RequireKeysDistinct<Keys[Index], Keys[j]>::value` for
    /// every `j` strictly after `Index`, `&&`-folded together. Every operand
    /// of a fold expression is instantiated to form the expression,
    /// independent of the runtime short-circuit `&&` also performs -- so
    /// every later key is compared against this one and each duplicate
    /// reports on its own. The same reasoning as
    /// `require_all_bands_adjacent` (`band.hpp`).
    template <KeyTable Keys, std::size_t Index, std::size_t... Later>
    [[nodiscard]] constexpr bool require_key_distinct_from_later(std::index_sequence<Later...>) noexcept
    {
        return (RequireKeysDistinct<Keys[Index], Keys[Index + 1 + Later]>::value && ...);
    }

    /// The outer half of the pairwise sweep: one `Index` per row that has a
    /// row after it, so every unordered pair is visited exactly once.
    template <KeyTable Keys, std::size_t... Index>
    [[nodiscard]] constexpr bool require_all_keys_distinct(std::index_sequence<Index...>) noexcept
    {
        return (require_key_distinct_from_later<Keys, Index>(std::make_index_sequence<Keys.size() - 1 - Index> {}) && ...);
    }

    /// Split out of `RequireValidKeyTable` so that `Keys.size() - 1` -- which
    /// underflows for an empty table -- sits behind `if constexpr` and is
    /// therefore never instantiated for `N < 2`. Guarding with `||` instead
    /// would not be enough, for the reason `band_table_is_valid`'s own
    /// comment gives: that operator's short circuit applies to *evaluation*,
    /// not to forming the type of its right-hand operand, and
    /// `std::make_index_sequence<Keys.size() - 1>` for an empty table would
    /// still have to name a sequence of length `SIZE_MAX`.
    template <KeyTable Keys>
    [[nodiscard]] constexpr bool key_table_is_valid() noexcept
    {
        if constexpr (Keys.size() < 2)
            return true;
        else
            return require_all_keys_distinct<Keys>(std::make_index_sequence<Keys.size() - 1> {});
    }

    /// Finds the index of the row in @p Keys whose key is @p key, or nothing
    /// when no row has it.
    ///
    /// A linear scan, for the reason `find_band` is one: a method's own
    /// published table is rows, not big data, and an obviously-correct O(N)
    /// scan is worth more here than any cleverer search. There is nothing to
    /// convert and nothing to compare inexactly -- `keys_match` is equality
    /// of two enumerators -- which is the whole difference between this
    /// function and `find_band`.
    template <KeyTable Keys>
    [[nodiscard]] constexpr std::optional<std::size_t> find_key(KeyOf<Keys> key) noexcept
    {
        for (std::size_t index = 0; index < Keys.size(); ++index)
            if (keys_match(Keys[index], key))
                return index;
        return std::nullopt;
    }
} // namespace detail

/// The static_assert wiring for an exact table: instantiating this with a
/// `KeyTable` that is a compile-time constant enforces, right there, that no
/// key repeats -- reusing `keys_match`, the same predicate
/// `key_table_is_well_formed` uses for a table that only arrives at runtime,
/// through `RequireKeysDistinct` above. Reached through `::value`, for the
/// same reason `RequireValidBandTable` is.
template <KeyTable Keys>
struct RequireValidKeyTable
{
    /// Always `true` once reached -- every `static_assert` this instantiates
    /// has already failed compilation otherwise. Present so `::value` is the
    /// spelling that instantiates the class template, exactly as
    /// `RequireValidBandTable::value` (`band.hpp`) is.
    static constexpr bool value = detail::key_table_is_valid<Keys>();
};

/// A category key names a row, and that row selects a correction -- see the
/// file comment's "Exact lookup" section for how a key is spelled, where it
/// comes from and how a miss is reported.
///
/// The same split as `BandedLookupNode`, not a parallel one invented for this
/// node: `Keys` and `ResultUnit` are the table's *structure* and live in the
/// type, `corrections` are its *contents* and arrive at runtime. `key` is the
/// one member with no counterpart there, and it is runtime state for the
/// reason the file comment gives: a discriminator is not a quantity, so it
/// cannot reach the node through an operand or through the `Environment`.
///
/// Both `static_assert`s sit in the class body rather than in the factory,
/// and the property that buys is narrower than it first looks -- stated
/// precisely here because an earlier revision of this comment claimed more
/// than it could deliver, and a reviewer measured the difference. Discarding
/// the factory's result is **not** what distinguishes the two placements:
/// `exact_lookup` returns `ExactLookupNode` *by value*, so calling it
/// completes the class whichever placement is chosen, and an assert in the
/// factory body fires on any call, discarded or not. What the class body
/// buys is this: `ExactLookupNode` is a public aggregate with public members,
/// so a caller can declare one **without ever calling the factory** --
///
///     inline constexpr ExactLookupNode<Duplicated, unit::One> node {
///         {}, { rat(1), rat(1), rat(1) }, Shape::Cube };
///
/// -- and only a `static_assert` in the class body refuses that. With the
/// asserts in the factory it compiles, links, and carries a silently
/// unreachable row. `exact_lookup_duplicate_key_no_factory.cpp` is exactly
/// that declaration and exists to pin this; it differs from
/// `exact_lookup_duplicate_key.cpp` in precisely one thing, the absence of the
/// factory call.
template <KeyTable Keys, Unit ResultUnit>
struct ExactLookupNode: NodeBase
{
    static_assert(detail::RequireScopedEnumKey<KeyOf<Keys>>::value);
    static_assert(RequireValidKeyTable<Keys>::value);

    /// One correction per key, stated in `unit`, in the same order `keys`
    /// declares -- the table's *contents*, runtime state for the same reason
    /// `BandedLookupNode::corrections` and `ConstantNode::number` are, and
    /// `Corrections<N>` rather than a raw array for the reason that member
    /// gives.
    Corrections<Keys.size()> corrections;

    /// The key this lookup selects with: which specimen variant, apparatus or
    /// regime is in front of the caller. Runtime state, for the reason the
    /// file comment gives at length; a key that names no row of `keys` is a
    /// miss, reported exactly as a value falling in no band is.
    ///
    /// **There is no unset state, and a later task must not assume one.** The
    /// default member initialiser is `KeyOf<Keys>{}` -- the enumerator whose
    /// value is zero -- which for the ordinary table is a perfectly legitimate
    /// key that hits a row. It does not mean "no key yet" and cannot be made
    /// to: an enumeration has no spare value this library gets to reserve, and
    /// adding a sentinel enumerator would be a rule imposed on the method
    /// author's own type. See the file comment's note on where the key comes
    /// from for what this costs the environment-sourced variant.
    KeyOf<Keys> key {};

    /// The keys themselves, already validated above -- part of the table's
    /// *structure*, so a formula whose table is wrong is a compile error
    /// naming it.
    static constexpr KeyTable<KeyOf<Keys>, Keys.size()> keys = Keys;
    /// The unit each entry of `corrections` is stated in, and this node's own
    /// declared unit -- the same role `ConstantNode::unit` plays.
    static constexpr Unit unit = ResultUnit;
    /// The dimension of `unit`: what this node itself produces. An exact
    /// lookup stands where a number of *this* dimension stands.
    static constexpr Dimension dimension = ResultUnit.dimension;
};

/// Declares an exact lookup: `exact_lookup<Shapes, unit::One>(shape,
/// { rat(1), rat(97, 100), rat(92, 100) })`.
///
/// `Keys` and `ResultUnit` are deliberately not deduced, for the reason
/// `banded_lookup` leaves its structural parameters unstated at the argument
/// list: a table's structure is the author's declared intent, not something
/// inferred from whatever the corrections happen to look like.
///
/// `key`'s type is `KeyOf<Keys>` exactly -- not a deduced parameter with a
/// `static_assert` behind it. Handing this an enumerator of the wrong
/// enumeration is then the compiler's own conversion diagnostic, which names
/// both enumerations; a library guard here could only restate that less well.
///
/// `corrections` is the same `Corrections<N>` a banded lookup takes, and it is
/// also this node's own member type, so a short braced list is refused
/// identically whether it arrives here or is handed straight to the aggregate
/// -- see that type for why a raw `std::array<Rational, N>` closed only the
/// first of those two routes, and for the measurement that showed it.
template <KeyTable Keys, Unit ResultUnit>
[[nodiscard]] constexpr ExactLookupNode<Keys, ResultUnit> exact_lookup(KeyOf<Keys> key,
                                                                       Corrections<Keys.size()> corrections) noexcept
{
    return ExactLookupNode<Keys, ResultUnit> { {}, corrections, key };
}

/// Looks the node's key up in its table. A key that names a row produces that
/// row's correction; a key that names none is reported as
/// `ArithmeticError::DomainError` -- never a default, never the first row --
/// the identical mechanism `BandedLookupNode` reports an out-of-range value
/// with, and see the file comment for why that is the honest answer rather
/// than a euphemism.
///
/// There is no absence case to propagate, and that is not an omission: an
/// exact lookup has no operand and reads nothing from the environment, so it
/// is always either a hit or a miss, exactly as `ConstantNode` is always a
/// value. Absence is a fact about a measurement, and this node has none.
template <typename Rep = Rational, KeyTable Keys, Unit ResultUnit, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(ExactLookupNode<Keys, ResultUnit> const& node,
                                                           Env const&,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);

    if constexpr (!std::is_same_v<Rep, Rational>)
    {
        // See the file comment: the lookup family evaluates in `Rational`
        // only, so both kinds answer in one representation rather than each
        // in whichever one happens to be legal for it. **The message gives
        // that reason and not the banded node's**, because the banded node's
        // reason is not true here -- comparing two enumerators is not
        // arithmetic and is exact in every representation -- and the author
        // who trips this guard reads the message, never this file's comment.
        // It names no concrete type either: the instantiation backtrace
        // already names whatever `Rep` the caller actually asked for.
        // Dependent on `Rep` so this fires only when this function is
        // actually instantiated with a non-`Rational` `Rep`, not merely
        // declared -- the same trick `RepRounding<double>::round_in` uses.
        static_assert(sizeof(Rep) == 0,
                      "formula: an exact lookup node can only be evaluated with Rep = Rational -- not "
                      "because selecting a row by key needs exact arithmetic (comparing two "
                      "enumerators does not), but because every lookup table in this library answers "
                      "in one representation, so that a formula evaluates the same way whichever kind "
                      "of table it contains; evaluate this formula with Rep = Rational instead "
                      "(checked_evaluate<Result> always does)");
        return std::unexpected { ArithmeticError::DomainError };
    }
    else
    {
        std::optional<std::size_t> const index = detail::find_key<Keys>(node.key);
        if (!index.has_value())
        {
            // A miss is not a value: no default, no first-row fallback.
            // `DomainError` is literally true here -- an exact table's domain
            // is its set of keys -- not a euphemism. See the file comment.
            Evaluated<Rep> const missed = std::unexpected { ArithmeticError::DomainError };
            sink.produced(node, missed);
            return missed;
        }

        Evaluated<Rep> const result = detail::in_si<Rep>(node.corrections[*index], ResultUnit);
        sink.produced(node, result);
        return result;
    }
}

// ---------------------------------------------------------------------------
// Interpolating lookup: a value between two rows. See the file comment's
// "Interpolating lookup" section for why the table is points rather than
// intervals, why its domain is closed at both ends where a band table's is
// half-open, why there is no extrapolation, and what stays exact.
// ---------------------------------------------------------------------------

/// One row of an interpolating table: the key at which the table states a
/// value, as an exact rational stated numerator over denominator -- see the
/// file comment for why this is not a `Rational`, which is `band.hpp`'s reason
/// unchanged.
///
/// Only the key lives here. The value stated at this key is runtime state on
/// the node, exactly as a band's correction is: which rows exist is the method,
/// and what each row says is the customer's registered table.
struct Breakpoint
{
    /// Numerator of the declared key.
    std::int64_t numerator = 0;
    /// Denominator of the declared key.
    std::int64_t denominator = 1;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(Breakpoint const&) const noexcept = default;
};

/// Builds a `Breakpoint` from its key as a numerator/denominator pair. The
/// denominator defaults to 1 -- unlike `band()`, which takes both bounds as
/// explicit pairs -- because a breakpoint is one number rather than two, and a
/// curve stated at whole-numbered keys then reads
/// `{ breakpoint(0), breakpoint(10), breakpoint(20) }` rather than carrying a
/// column of `1`s that says nothing.
[[nodiscard]] constexpr Breakpoint breakpoint(std::int64_t numerator, std::int64_t denominator = 1) noexcept
{
    return { numerator, denominator };
}

/// A table of breakpoints, declared in strictly ascending order. An alias
/// template over `std::array`, for the reason `BandTable` (`band.hpp`) and
/// `KeyTable` above are: a spike compiled `template <BandTable Bands>` with
/// both the element type and `N` deduced on all four compilers, so a wrapping
/// struct would add a name to unwrap and nothing else.
template <std::size_t N>
using BreakpointTable = std::array<Breakpoint, N>;

/// Where a value sat in an interpolating table: the two rows its answer came
/// from, as the table **declared** them.
///
/// The declared keys and not their row indices, because the reader this exists
/// for is reconciling a derivation against a published table on paper, where
/// rows are named by the key they state a value at and an array offset appears
/// nowhere. It is also why these are `Breakpoint`s rather than `Rational`s:
/// they are repeated back exactly as typed, and reducing them is the rendering
/// layer's decision, made in the one place that already reduces every other
/// declared bound.
///
/// `low == high` means the value sat exactly **on** a row -- the rule
/// `detail::locate_and_interpolate`'s equality-first test implements, and a
/// state worth distinguishing because at the table's last row it is the only
/// way an answer can be produced at all.
struct Segment
{
    /// The row at or below the value.
    Breakpoint low {};
    /// The row above it, or `low` again when the value sat on a row.
    Breakpoint high {};

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(Segment const&) const noexcept = default;
};

/// One of the two predicates an interpolating table's well-formedness is built
/// on (the other is `breakpoints_ascend` just below), used both by the
/// `static_assert` wiring and by any runtime loader -- so the two checks cannot
/// drift, the arrangement `band.hpp` and `key_table_is_well_formed` both use.
///
/// True when this row's key is a rational number at all: its denominator is
/// non-zero and the pair reduces without overflow. Nothing about a pair of
/// rows -- a single `Breakpoint` either names a number or it does not, and
/// `breakpoints_ascend` alone cannot answer this for a one-row table, where
/// there is no pair for it to look at.
[[nodiscard]] constexpr bool breakpoint_is_well_formed(Breakpoint const& value) noexcept
{
    return Rational::make(value.numerator, value.denominator).has_value();
}

/// The other predicate, alongside `breakpoint_is_well_formed`: true when
/// `first`'s key is **strictly** below `second`'s. Strictly, because two rows
/// at one key state two different values there and leave a segment of zero
/// width for the interpolation to divide by -- see the file comment.
///
/// Compares through `Rational::make`, which reduces to lowest terms and reports
/// overflow, rather than cross-multiplying the raw pairs -- the same overflow
/// `bands_are_adjacent` avoids the same way, and what makes `1/2 < 2/3` answer
/// the question about the numbers rather than about the digits. A key that is
/// not a representable rational is treated as not ascending with anything, in
/// either position: a malformed row is exactly the kind of typo this validation
/// exists to catch, not a case to silently wave through.
[[nodiscard]] constexpr bool breakpoints_ascend(Breakpoint const& first, Breakpoint const& second) noexcept
{
    auto const lower = Rational::make(first.numerator, first.denominator);
    auto const upper = Rational::make(second.numerator, second.denominator);
    if (!lower || !upper)
        return false;
    return *lower < *upper;
}

/// True when `table` is well-formed: every row's key is a rational number, AND
/// every adjacent pair strictly ascends. The runtime half of the same question
/// `RequireValidBreakpointTable` asks at compile time, on the same two
/// predicates.
///
/// **Only neighbouring pairs are compared, and that is a proof rather than an
/// optimisation** -- unlike the exact table just above, which must compare
/// every pair. `key_i < key_(i+1)` for every consecutive pair chains into
/// `key_0 < key_1 < ... < key_(N-1)`, so no two rows anywhere in the table can
/// share a key or sit out of order. An exact table has no such chain available:
/// its keys are discriminators with no ordering, so transitivity has nothing to
/// run along and every pair really must be visited.
///
/// An empty table and a one-row table both validate -- neither has a pair that
/// could fail, and a one-row table has only its own key's well-formedness left
/// to check. See the file comment for why a table that answers nowhere, or
/// almost nowhere, is treated as narrow rather than malformed.
template <std::size_t N>
[[nodiscard]] constexpr bool breakpoint_table_is_well_formed(BreakpointTable<N> const& table) noexcept
{
    for (std::size_t index = 0; index < N; ++index)
        if (!breakpoint_is_well_formed(table[index]))
            return false;
    for (std::size_t index = 0; index + 1 < N; ++index)
        if (!breakpoints_ascend(table[index], table[index + 1]))
            return false;
    return true;
}

/// Fails to compile when a row's key is not a rational number -- its
/// denominator is zero, or its numerator and denominator cannot be reduced
/// without overflow -- so the row names no point on the curve at all.
///
/// Same shape and same reason as `RequireBandWellFormed` (`band.hpp`):
/// instantiating a named template on the value makes the compiler print the
/// offending breakpoint, and the wording is ours so the negative-compile
/// harness can assert the reason rather than merely the failure. Reached
/// through `::value`, for the same reason `RequireBandsAdjacent` is.
template <Breakpoint B>
struct RequireBreakpointWellFormed
{
    static_assert(breakpoint_is_well_formed(B),
                  "formula: this interpolating lookup table has a row whose key is not a rational "
                  "number; its denominator is zero, or its numerator and denominator cannot be "
                  "reduced without overflow; the offending Breakpoint value appears in this "
                  "diagnostic as the template argument B of RequireBreakpointWellFormed");

    /// Always `true` once reached -- see `RequireBandsAdjacent::value`.
    static constexpr bool value = true;
};

/// Fails to compile when two adjacent rows do not strictly ascend -- they
/// either state the same key twice, so the table claims two different values at
/// one key and leaves a segment of zero width to interpolate across, or they
/// are declared out of order, so "between these two rows" names nothing.
///
/// One diagnostic for one rule, deliberately, exactly as `band.hpp` gives an
/// inverted band and a zero-width band one message: both are `first < second`
/// failing, and the message names the two offending keys, so an author can see
/// at a glance which case they have. Reached through `::value`, for the same
/// reason `RequireBandsAdjacent` is.
template <Breakpoint First, Breakpoint Second>
struct RequireBreakpointsAscend
{
    static_assert(breakpoints_ascend(First, Second),
                  "formula: this interpolating lookup table's breakpoints do not strictly ascend; two "
                  "adjacent rows either state the same key twice or are declared out of order, and the "
                  "two offending Breakpoint values appear in this diagnostic as the template arguments "
                  "First and Second of RequireBreakpointsAscend");

    /// Always `true` once reached -- see `RequireBandsAdjacent::value`.
    static constexpr bool value = true;
};

namespace detail
{
    /// Expands to one `RequireBreakpointWellFormed<Points[i]>::value` per row,
    /// `&&`-folded together. Every operand of a fold expression is instantiated
    /// to form the expression, independent of the runtime short-circuit `&&`
    /// also performs -- so every row is checked and each bad one reports on its
    /// own. The same reasoning as `require_all_bands_well_formed` (`band.hpp`).
    template <BreakpointTable Points, std::size_t... Index>
    [[nodiscard]] constexpr bool require_all_breakpoints_well_formed(std::index_sequence<Index...>) noexcept
    {
        return (RequireBreakpointWellFormed<Points[Index]>::value && ...);
    }

    /// Same idea, one index per adjacent pair rather than per row, so every
    /// pair is checked and reported independently of every other.
    template <BreakpointTable Points, std::size_t... Index>
    [[nodiscard]] constexpr bool require_all_breakpoints_ascend(std::index_sequence<Index...>) noexcept
    {
        return (RequireBreakpointsAscend<Points[Index], Points[Index + 1]>::value && ...);
    }

    /// Split out of `RequireValidBreakpointTable` so that `Points.size() - 1` --
    /// which underflows for an empty table -- sits behind `if constexpr` and is
    /// therefore never instantiated for `N < 2`. Guarding with `&&` instead
    /// would not be enough, for the reason `band_table_is_valid`'s own comment
    /// gives: that operator's short circuit applies to *evaluation*, not to
    /// forming the type of its right-hand operand. The per-row fold has no such
    /// hazard (it indexes 0..N-1, not 0..N-2) and always runs, so a one-row
    /// table with a malformed key -- no pair to speak of -- is still caught.
    template <BreakpointTable Points>
    [[nodiscard]] constexpr bool breakpoint_table_is_valid() noexcept
    {
        bool const wellFormed = require_all_breakpoints_well_formed<Points>(std::make_index_sequence<Points.size()> {});
        if constexpr (Points.size() < 2)
            return wellFormed;
        else
            return wellFormed && require_all_breakpoints_ascend<Points>(std::make_index_sequence<Points.size() - 1> {});
    }
} // namespace detail

/// The static_assert wiring for an interpolating table: instantiating this with
/// a `BreakpointTable` that is a compile-time constant enforces, right there,
/// that every row names a number and that the rows strictly ascend -- reusing
/// `breakpoint_is_well_formed` and `breakpoints_ascend`, the same two
/// predicates `breakpoint_table_is_well_formed` uses for a curve that only
/// arrives at runtime. Reached through `::value`, for the same reason
/// `RequireValidBandTable` is.
template <BreakpointTable Points>
struct RequireValidBreakpointTable
{
    /// Always `true` once reached -- every `static_assert` this instantiates
    /// has already failed compilation otherwise. Present so `::value` is the
    /// spelling that instantiates the class template, exactly as
    /// `RequireValidBandTable::value` (`band.hpp`) is.
    static constexpr bool value = detail::breakpoint_table_is_valid<Points>();
};

namespace detail
{
    /// The interpolation itself, on two rows already reduced to plain
    /// rationals: `lowValue + ((key - lowKey) / (highKey - lowKey)) *
    /// (highValue - lowValue)`.
    ///
    /// Every step is `Rational`'s own checked arithmetic, so the result is the
    /// **exact** rational the two rows imply and nothing is rounded anywhere on
    /// the way to it; the only failure is an intermediate outside `Rational`'s
    /// representable range, reported as `ArithmeticError::Overflow` rather than
    /// approximated away. See the file comment.
    ///
    /// **The span is divided out first, and the rise multiplied in after.** In
    /// exact arithmetic the answer is identical either way -- there is no
    /// rounding for an order of operations to change -- so the only thing this
    /// choice affects is how large the intermediates get, which is to say how
    /// far the computation gets before it has to report `Overflow`.
    ///
    /// **Neither order dominates**, and the comment that used to stand here
    /// claimed one did. Measured, both directions:
    ///
    ///  - keys `{0, 10}` with values `{0, 2^62}`, asked at 5: dividing first
    ///    answers `2^61` exactly; multiplying first reports `Overflow`.
    ///  - keys `{0, 4e9}` with values `{0, 4e9}`, asked at `1/4e9`: multiplying
    ///    first answers `1/4e9` exactly; dividing first reports `Overflow`.
    ///
    /// Dividing first is chosen because it is the better order for the tables
    /// this library is actually for. A published curve states its rows on a
    /// common grid -- 0, 10, 20 mm -- so `offset` and `span` share their
    /// denominator, `offset / span` cancels to a weight strictly below 1, and
    /// the key scale never meets the value scale. Multiplying first forms the
    /// one product in the whole computation that mixes key magnitude with value
    /// magnitude, and nothing cancels it. The shape dividing first is worse for
    /// is the opposite one: a key so fine that `offset / span` cannot cancel,
    /// against a value that would have cancelled against the offset instead.
    ///
    /// Both of those tables are in `lookup_tests.cpp`, asserted as behaviour --
    /// one showing the chosen order returning a value the rejected order could
    /// not, one showing the chosen order refusing a table the rejected order
    /// could have answered. The order was previously pinned by nothing at all:
    /// the entire suite compiled unchanged under either.
    ///
    /// `highKey - lowKey` cannot be zero: `RequireValidBreakpointTable` has
    /// already refused a table whose rows do not strictly ascend, so there is
    /// no runtime test for it here. `checked_div` would report
    /// `DivisionByZero` rather than trap if that guarantee were ever broken.
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> interpolate_between(
        Rational lowKey, Rational lowValue, Rational highKey, Rational highValue, Rational key) noexcept
    {
        std::expected<Rational, ArithmeticError> const span = checked_sub(highKey, lowKey);
        if (!span.has_value())
            return span;
        std::expected<Rational, ArithmeticError> const rise = checked_sub(highValue, lowValue);
        if (!rise.has_value())
            return rise;
        std::expected<Rational, ArithmeticError> const offset = checked_sub(key, lowKey);
        if (!offset.has_value())
            return offset;
        std::expected<Rational, ArithmeticError> const weight = checked_div(*offset, *span);
        if (!weight.has_value())
            return weight;
        std::expected<Rational, ArithmeticError> const share = checked_mul(*weight, *rise);
        if (!share.has_value())
            return share;
        return checked_add(lowValue, *share);
    }

    /// Answers @p key against @p Points and @p corrections **and says where the
    /// answer came from**: the row's own value when the key sits exactly on a
    /// row, the interpolation of the two surrounding rows when it sits between
    /// them, and `ArithmeticError::DomainError` when it sits outside the table
    /// altogether -- below the first row, above the last, or anywhere at all
    /// for an empty table.
    ///
    /// **The location is returned rather than discarded because this scan
    /// already computes it.** It has exactly three exits -- on row `i`,
    /// between `i - 1` and `i`, or a miss -- and a derivation that wants to
    /// say which two rows an answer came from would otherwise have to find
    /// them again with a second scan somewhere else. Two scans of one table
    /// against one rule is the pair-of-surfaces-that-must-agree defect this
    /// phase keeps refusing; returning what is already known costs one
    /// `std::pair` and cannot drift from itself. `interpolate` just below
    /// drops the location for the evaluation path, which has no use for it.
    ///
    /// `DomainError` is this function's spelling for a **miss**, and is
    /// unambiguous here: the only other errors it can return come from
    /// `interpolate_between`, which reports `Overflow` (or, unreachably,
    /// `DivisionByZero`) and never `DomainError`.
    ///
    /// A single forward scan, not a binary search, for the reason `find_band`
    /// is one: a method's own published curve is rows, not big data. It leans
    /// on the ascending order `RequireValidBreakpointTable` has already
    /// enforced -- the first row whose key is **not** below @p key is where the
    /// answer is, and everything after it can be ignored.
    ///
    /// The equality test comes first, so that a key sitting exactly on a row
    /// returns that row rather than interpolating a segment to it. That is
    /// observable at the table's **last** row, which begins no segment; at
    /// every other row interpolating would give the same number, because the
    /// weight is exactly zero. See the file comment. It is also what makes the
    /// returned `Segment`'s `low == high` mean "on a row" rather than "a
    /// segment of zero width", which a well-formed table cannot contain.
    template <BreakpointTable Points>
    [[nodiscard]] constexpr std::expected<std::pair<Rational, Segment>, ArithmeticError> locate_and_interpolate(
        Rational key, Corrections<Points.size()> const& corrections) noexcept
    {
        for (std::size_t index = 0; index < Points.size(); ++index)
        {
            std::expected<Rational, ArithmeticError> const here =
                Rational::make(Points[index].numerator, Points[index].denominator);
            // Unreachable for a `Points` that reached this point: every
            // `InterpolatingLookupNode` instantiates
            // `RequireValidBreakpointTable<Points>`, which already refuses a
            // malformed key at compile time. Guarded anyway, for the same
            // reason `find_band` guards its own: reporting nothing is better
            // than interpolating against a number that was never there.
            if (!here.has_value())
                return std::unexpected { ArithmeticError::DomainError };

            if (*here == key)
                return std::pair<Rational, Segment> { corrections[index], Segment { Points[index], Points[index] } };

            if (key < *here)
            {
                // Below the table's first row: a miss, never an extrapolation
                // backwards along the first segment's slope.
                if (index == 0)
                    return std::unexpected { ArithmeticError::DomainError };

                std::expected<Rational, ArithmeticError> const previous =
                    Rational::make(Points[index - 1].numerator, Points[index - 1].denominator);
                if (!previous.has_value())
                    return std::unexpected { ArithmeticError::DomainError };

                std::expected<Rational, ArithmeticError> const answered =
                    interpolate_between(*previous, corrections[index - 1], *here, corrections[index], key);
                if (!answered.has_value())
                    return std::unexpected { answered.error() };

                return std::pair<Rational, Segment> { *answered,
                                                      Segment { Points[index - 1], Points[index] } };
            }
        }
        // Past the table's last row -- or an empty table, which is past its
        // last row vacuously. A miss, never a clamp to the final row and never
        // an extrapolation onwards along the final segment's slope.
        return std::unexpected { ArithmeticError::DomainError };
    }

    /// The answer alone, for the evaluation path, which has no use for the
    /// two rows it came from. A wrapper and not a second scan, deliberately:
    /// see `locate_and_interpolate` above for why there is exactly one.
    template <BreakpointTable Points>
    [[nodiscard]] constexpr std::expected<Rational, ArithmeticError> interpolate(
        Rational key, Corrections<Points.size()> const& corrections) noexcept
    {
        std::expected<std::pair<Rational, Segment>, ArithmeticError> const answered =
            locate_and_interpolate<Points>(key, corrections);
        if (!answered.has_value())
            return std::unexpected { answered.error() };
        return answered->first;
    }
} // namespace detail

/// A measured value sits between two rows, and the answer is the value those
/// two rows imply at that point -- see the file comment's "Interpolating
/// lookup" section for the domain, the refusal to extrapolate, and what stays
/// exact.
///
/// The same structure/contents split as `BandedLookupNode`, not a parallel one
/// invented here: `KeyUnit`, `Points` and `ResultUnit` live in the type,
/// `corrections` and `operand` are runtime state.
///
/// Both `static_assert`s sit in the class body rather than in the factory, for
/// the reason `ExactLookupNode`'s comment sets out at length:
/// `InterpolatingLookupNode` is a public aggregate with public members, so a
/// caller can declare one **without ever calling the factory**, and only a
/// class-body assert refuses that.
/// `interpolating_lookup_descending_breakpoint_no_factory.cpp` is exactly that
/// declaration and exists to pin this.
template <Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
struct InterpolatingLookupNode: NodeBase
{
    static_assert(RequireValidBreakpointTable<Points>::value);
    static_assert(detail::RequireLookupKeyMatches<KeyUnit, Operand>::value);

    /// The value this table states at each breakpoint, in `unit`, in the same
    /// order `breakpoints` declares -- the table's *contents*, runtime state
    /// for the same reason `BandedLookupNode::corrections` and
    /// `ConstantNode::number` are, and `Corrections<N>` rather than a raw
    /// array for the reason that member gives.
    Corrections<Points.size()> corrections;

    /// The expression whose evaluated value is located against `breakpoints`.
    Operand operand {};

    /// The unit breakpoints are declared in, and the unit `operand`'s value is
    /// compared against them in -- part of the table's *structure*. The
    /// interpolation is carried out in this unit, which is the unit the
    /// published curve is stated in.
    static constexpr Unit keyUnit = KeyUnit;
    /// The breakpoints themselves, already validated above.
    static constexpr BreakpointTable<Points.size()> breakpoints = Points;
    /// The unit each entry of `corrections` is stated in, and this node's own
    /// declared unit -- the same role `ConstantNode::unit` plays. An
    /// interpolated value is a weighted combination of two entries of
    /// `corrections`, so it is stated in their unit too.
    static constexpr Unit unit = ResultUnit;
    /// The dimension of `unit`: what this node itself produces. Independent of
    /// `keyUnit`'s dimension on purpose, exactly as a banded lookup's is.
    static constexpr Dimension dimension = ResultUnit.dimension;
};

/// Declares an interpolating lookup: `interpolating_lookup<unit::Millimetre,
/// Points, unit::One>(var<Diameter>, { rat(9, 10), rat(1), rat(12, 10) })`.
///
/// `KeyUnit`, `Points` and `ResultUnit` are deliberately not deduced, for the
/// reason `banded_lookup` leaves its structural parameters unstated at the
/// argument list: a table's structure is the author's declared intent, not
/// something inferred from whatever the values happen to look like.
///
/// `corrections` is the same `Corrections<N>` both other kinds take, and it is
/// also this node's own member type, so a short braced list is refused
/// identically whether it arrives here or is handed straight to the aggregate
/// -- see that type for why a raw `std::array<Rational, N>` closed only the
/// first of those two routes, and for the measurement that showed it.
template <Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
[[nodiscard]] constexpr InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand> interpolating_lookup(
    Operand operand, Corrections<Points.size()> corrections) noexcept
{
    return InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand> { {}, corrections, operand };
}

/// Evaluates the operand, converts its value into `KeyUnit`, and answers from
/// the table: the row's own value when the value sits exactly on a row, the
/// interpolation of the two surrounding rows when it sits between them.
/// Absence propagates, same as every other node; a value outside the table's
/// own first and last row is reported as `ArithmeticError::DomainError` --
/// never an extrapolation, never a clamp to the nearest row -- and an
/// interpolation whose exact answer is not representable is reported as
/// `ArithmeticError::Overflow` rather than rounded. See the file comment.
template <typename Rep = Rational, Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand,
          typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(
    InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand> const& node,
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
        // See the file comment: this node has the banded node's ground for
        // refusing an inexact representation and one of its own that is
        // stronger -- the answer is *computed*, not selected, so a
        // representation that rounds hands back a number the table's own rows
        // do not imply. The message says "this representation" rather than
        // naming a type the instantiation backtrace already names. Dependent on
        // `Rep` so this fires only when this function is actually instantiated
        // with a non-`Rational` `Rep`, not merely declared -- the same trick
        // `RepRounding<double>::round_in` uses.
        static_assert(sizeof(Rep) == 0,
                      "formula: an interpolating lookup node can only be evaluated with Rep = Rational -- "
                      "locating the two rows a value sits between needs exact comparison, and the value "
                      "between them is then computed rather than selected, so a representation that "
                      "rounds would answer with a number this table's own rows do not imply; evaluate "
                      "this formula with Rep = Rational instead (checked_evaluate<Result> always does)");
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

        std::expected<Rational, ArithmeticError> const interpolated =
            detail::interpolate<Points>(*valueInKey, node.corrections);
        if (!interpolated.has_value())
        {
            // Either a miss (`DomainError` -- no default, no clamp, no
            // extrapolation) or an interpolation whose exact answer is not
            // representable (`Overflow` -- reported, never rounded). Both are
            // already the right enumerator; nothing is rewritten here.
            Evaluated<Rep> const failed = std::unexpected { interpolated.error() };
            sink.produced(node, failed);
            return failed;
        }

        Evaluated<Rep> const result = detail::in_si<Rep>(*interpolated, ResultUnit);
        sink.produced(node, result);
        return result;
    }
}

} // namespace formula
