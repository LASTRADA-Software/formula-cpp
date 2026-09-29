# Changelog

Notable changes to formula-cpp, in the form [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
describes. 0.1.0 is the first release. The public API may still change before 1.0: a minor release
may break it, and each such change is recorded here.

## [Unreleased]

### Added

- `dim::Power`, energy per time, and the units `unit::Watt`, `unit::Kilowatt`, `unit::WattHour` and
  `unit::KilowattHour`. A kilowatt-hour is exactly 3600000 joules, so a power times a time converts
  into kilowatt-hours without a rounded factor.
- `unit::Fahrenheit`, a second affine temperature unit beside `unit::Celsius`. No conversion to or
  from kelvin or Celsius rounds, so 98.6 degrees Fahrenheit is exactly 37 degrees Celsius and 100
  degrees Fahrenheit is exactly 340/9 degrees Celsius.
- Base dimensions an application declares itself, such as money: `base_dimension("EUR")` makes a
  dimension of its own, so euros can no longer be added to a bare ratio, euros and yen never convert
  into each other, and energy times euros per energy is euros. `NamedBase` is one such base and its
  exponent; a `Dimension` holds up to `NamedBaseCapacity` (four) of them, and a product that would
  need a fifth does not compile. A name is an ASCII letter followed by letters or digits, shorter
  than 16 bytes and not the symbol of an SI base unit; two uses of one name are one dimension. A
  trace spells a named base by its name in a coherent unit, ahead of the SI units: `EUR s^2/(m^2 kg)`.
- An environment can fail a read: the variable evaluator reads it through `checked_get<Q>()` when it
  has one returning exactly `std::expected<Measured<Q>, ArithmeticError>`, and a failed read is that
  variable's failure, which travels up the formula as any operand's does. A member of that name with
  any other return type is not taken for it. The environments a precision limit, a rejection and a
  retry evaluate in forward it.
- `define<Q>(expression)`, in the new header `calculation.hpp` (included by `formula.hpp`), binds a
  quantity to the expression that calculates it and returns a `Definition<Q, Expr>`. The quantities
  the expression reads are known at compile time: each once, in the order first read, a `when()`
  listing its condition and both branches, an overlay's derived quantity what its definition reads
  and a fixed constant nothing. A definition is refused where it is written when its expression
  measures a dimension other than `Q`'s, is a series, reads a quantity as a series or as raw
  observations, reads from another record, or holds a node kind of a consumer's own that cannot be
  seen inside.
- `calculation(define<A>(...), ...)` builds a `Calculation` whose dependency graph is worked out and
  checked at compile time. Its inputs are what its definitions read and none of them defines, in the
  order first read, and `inputs_of` lists them. Its defined quantities are calculated in dependency
  order, each once everything it reads is known, keeping the order given wherever it can, and
  `calculation_order` lists them -- the defined quantities only -- in that order. `dependencies_of`,
  `dependents_of`, `upstream_of` and `affected_by` answer the quantities concerned in dependency
  order, inputs first. Each of these queries answers symbols as a `std::array<std::string_view, N>`,
  in a vocabulary's words, and `depends_on` whether one quantity depends on another. A calculation is
  refused where it is written when it is given something other than a definition, or nothing;
  defines a quantity twice; holds more than 64 quantities; or holds a definition that reads what it
  defines, or definitions that read one another in a cycle. A query about a quantity the calculation
  neither defines nor reads is refused.
- `worksheet(calculation, environment(...))` builds a `Worksheet`: the calculation's inputs, taken
  from the environment, and its calculated values, each calculated when first asked for and
  recalculated only when a change reaches it. `calculate<Q>()` answers an `Outcome<Q>`, throwing
  `ArithmeticException` for a failed calculation, and `checked_calculate<Q>()` answers
  `std::expected<Outcome<Q>, ArithmeticError>`; each takes several quantities too, answering a
  `std::tuple`, or the quantities' variables, as in
  `auto [total, vat] = sheet.calculate(var<Total>, var<Vat>)`. Each definition is evaluated by
  `checked_evaluate` against the worksheet's values of just the quantities it reads: a failed value
  fails what reads it as a failed operand would, except on a `when()` branch not taken, and an absent
  input leaves what reads it empty. `set(...)` changes inputs, or overrides a calculated value by hand
  with `entered(Measured<Q> { ... })`, and marks what the change reaches; `clear_override<Q>()` drops
  an override; `with(...)` answers a changed copy and leaves the worksheet as it was. A value a change
  reaches is reused when nothing it reads has changed, and a value calculated again to the same
  answer from the same source counts as unchanged, so that what reads it can be reused in turn;
  `recomputed()` and `reused()` count both. The calculation's queries take a worksheet as well. A
  calculated value is kept in its quantity's declared unit, so a conversion the inlined formula never
  makes can overflow. Refused where it is written, each with one message: an environment with no
  entry for an input, with an entry the calculation neither reads nor defines, with a series or raw
  observations, or with a calculated quantity given as a measurement rather than `entered`; `set()`
  naming one quantity twice, one the calculation neither reads nor defines, a series, or a calculated
  quantity given as a measurement; asking about a quantity the calculation neither defines nor reads;
  and `clear_override` or `is_overridden` of an input, which is set again rather than overridden --
  whether its value was typed in is the `source()` of what `calculate` answers for it.
- `explain_worksheet<Q>(sheet, vocabulary)`, in `trace.hpp`, asks a worksheet for `Q` and records
  how the answer was reached, as an `ExplainedWorksheet`: the answer, failure included, and one
  `WorksheetEntry` per named value -- `Q`'s first, then each calculated value it was reached through,
  each before the values it reads, and last the inputs read. A calculated value's block is its
  definition's derivation, recorded afresh from the values the worksheet holds, so it describes them
  even where a value was reused rather than calculated again; a calculated value it reads is one
  step, its source `Derived`, and has a block of its own. An override or an input is one step. A
  value the evaluation never reached -- read only in a `when()` branch not taken, or to the right of
  an operand that failed -- gets no block, and a failed value is a block like any other. Recording
  the blocks calculates nothing again.
- A calculation as text, in `render.hpp`: `render(calculation, vocabulary)` writes one `symbol =
  expression` line per definition, in the order the definitions are calculated, in any dialect;
  `describe_graph` lists the inputs and then what each calculated value reads, the arrows aligned;
  and `to_dot` writes the graph in Graphviz's DOT language, the inputs as boxes and the calculated
  values as ellipses, each node labelled with its symbol, a `"` or `\` in it escaped.
- `render_derivation(explained, { .maxSteps = n })`, in `trace_render.hpp`, renders what
  `explain_worksheet` recorded: each block under a header `symbol = definition = value` -- or
  `symbol = value, entered by hand in place of definition` for an override -- with its steps
  numbered within it as `render_trace` writes them, and the inputs last. One step limit bounds every
  header, step and input line, and one last line says how many were left out;
  `render_derivation(explained, {})` does not compile.
- `document(calculation, vocabulary)`, in `document.hpp`, documents a calculation: its `formula` is
  the calculation as `render` writes it; its symbol table opens with a row per calculated quantity,
  in the order they are calculated in, each with its definition in the page's dialect in the new
  `SymbolEntry::calculatedAs`, and then lists the inputs, each once, in the order the definitions
  first read them; and its citations are every one the definitions hold. A definition reading
  `attempt_input` is refused there, as a formula documented on its own is.
- `number_text.hpp`, included by `formula.hpp`: a `Rational` spelled as an exact decimal (`0.6`,
  `18.8822`), as a decimal rounded under a named rounding mode (`118.26`), or as a fraction
  (`3/5`), into a `NumberText` with a fixed 64-byte buffer -- without allocating, and at compile
  time as well as at run time. A `NumberStyle` chooses the notation. A decimal is shown only when it is the exact
  value, so `1/3` stays `1/3` under `NumberStyle::exact_decimal()`; an approximation is opt-in
  through `NumberStyle::approximate_decimal(mode)`, rounds at the unit's declared decimals and
  always starts with `≈` (`≈0.333`). `number_text` of a `Measured` value adds its unit's symbol
  (`5.2 kJ`), or reads `(not measured)` when it is absent. `decimal_text` spells exactly what
  `checked_round` rounds to, and at 0 to 18 places still does where `checked_round`'s own
  arithmetic overflows. These functions take a `Rational`, so an unqualified call also finds them
  by argument-dependent lookup: a consumer's own function of the same name and parameters -- a
  `fraction_text(Rational)` helper, say -- now makes such a call ambiguous, and has to be renamed,
  as `examples/statistics.cpp`'s was, or called by a qualified name such as `::fraction_text`.
- `TraceRenderOptions::numbers`: a trace's numbers as exact decimals (`0.6`), or as rounded
  decimals marked `≈` (`≈0.333`) when the caller names a rounding mode. Fractions stay the
  default, so a trace rendered without it reads exactly as before. A decimal is shown only where
  it is the exact value -- `1/3` stays `1/3` under `NumberStyle::exact_decimal()` -- and whatever
  the style, a number typed rather than computed is shown exact: a constant or a per-element
  constant, `pi`, a declared domain, a constant an overlay fixed or derived, a table's row or bound,
  a permitted value, a limit, and whatever passes one on unchanged: a documented, selected, replaced
  or conditional step, a record's scope, a precision limit's level and limit, and a curve. So is
  either side of a comparison a line states beside its verdict. A value in a unit nobody declared
  -- a computed product or ratio, or a quantity declared in `unit::One`, which is the same unit --
  is never padded with zeros. A step's value the style cannot spell in its unit reads `(not
  shown: ...)`. `render_trace(trace, { .numbers = ... })` without `.maxSteps` still does not
  compile.
- `format.hpp`, opt-in and not included by `formula.hpp`, because it needs `<format>`:
  `std::format` writes a `Rational` and a `Measured` value in the spellings `number_text` gives.
  `{}` is the exact decimal, or the fraction where there is none (`0.6`, `1/3`, `5.2 kJ`); `{:/}`
  the fraction; `{:.2HalfEven}` rounds to two places, padded, without a marker (`118.26`);
  `{:~.3HalfEven}` rounds only an inexact value, and marks it `≈` (`≈0.333`); `{:~HalfEven}` does
  the same at the decimals a measured value's unit declares. A rounding always names its
  `RoundingMode`: there is no default. Fill, alignment and width work as for other types, with
  the width counted in code points, so `°C` and `≈` take one column each. A spec the grammar does
  not allow is a compile error naming what is wrong in a literal format string, and a
  `std::format_error` starting `formula: ` under `std::vformat`. The fill must be one Unicode scalar
  value in well-formed UTF-8. `{:~Mode}` of a measured value in a unit declaring negative decimals
  throws `std::format_error` when written if exact arithmetic cannot round the value there, as
  `from_double_exact(0.1)` at -3 decimals cannot. The library owns these `std::formatter`
  specialisations, so the header belongs in every translation unit that formats these types or asks
  `std::formattable` about them; only `char` is supported.
- `RenderOptions` for `render()` and `document()`: `render(f, vocabulary, { .numbers =
  NumberStyle::exact_decimal() })` writes a constant holding 863/1000 as `0.863`, and so every
  number a formula states -- a table's bounds and rows, a snap's permitted values, a domain's
  points, a per-element constant's values, an envelope's limits -- and `document()` its formula, a
  derived quantity's derivation and a rejection's limit. These numbers were typed by the formula's
  author, so they are written exactly whatever the style: `1/3` stays `1/3` under an approximating
  style, and none is padded, so `number(Rational { 1, 2 })` reads `0.5`. Without options nothing
  changes. The style travels with the vocabulary, the one argument every `render_node` already
  receives, so a consumer's own two-argument `render_node` hands it on unchanged and can read it
  with `number_style_of(vocabulary)`.

### Changed

- `Dimension` gains `namedBases`, after the seven SI exponents, so a designated initialiser of SI
  exponents still compiles; a structured binding over a `Dimension` now has eight members, not
  seven. `Dimension` grows from 56 to 152 bytes, `Unit` from 152 to 248, and a trace's
  `Step<Rational>` from 1008 to 1296, on 64-bit builds.
- `Symbol`, `SymbolCapacity`, `symbol()` and `view()` are now declared in `dimension.hpp`, which
  `unit.hpp` includes, so code including either header still finds them.
- The message of `RequireSameDimension` now ends "..., luminosity, then the named base dimensions by
  name"; its opening, "formula: these two dimensions are not the same", is unchanged.
- A currency declared as `dim::Scalar` until now should be declared with `base_dimension` instead,
  so that the dimension system tells it from a bare ratio.
- An environment's run-time `source_of<Q>()`, when it has one returning `ValueSource`, now decides
  the source a trace records for a variable, and for the entry an overlay's constant or derived
  quantity replaced; `is_entered<Q>` decides only for an environment without one. For an
  `Environment` the two always agree, so its traces read as before. A variable's step can now
  record `ValueSource::Derived`, a value its environment calculated: its line ends `, calculated`,
  and one with no value reads `(no value), calculated` rather than `(not measured)`.

## [0.1.0] - 2026-09-28

The first release of formula-cpp, a header-only C++23 library for traceable formulas: formulas
written once in C++, with dimensions checked at compile time and units converted exactly, evaluated
in exact rational arithmetic, documented from the same source, and traced step by step into an audit
trail that states every number with its unit and its source. 0.1.0 contains exact numbers and
rounding, dimensions, units and quantities, expressions and evaluation, citations and generated
documentation, tracing, conditionals and constraints, lookup tables, methods with jurisdiction
overlays, series and grading curves, statistics with outlier rejection, cross-record context and
lineage, opaque operations such as least squares, and bounded retry.

### Added

**Phase 1: a consumable project.** Apache-2.0 licensing, an `INTERFACE` CMake target with install
and export, presets for `cl`, `clang-cl`, Clang and GCC, Catch2 through a pinned CPM bootstrap, a
must-not-compile harness that asserts both that a build fails and that it fails with the library's
own message, and CI that installs the library and builds a consumer against it.

**Phase 2: exact numbers.** An exact rational type with checked arithmetic that reports overflow
rather than wrapping, and rounding to decimal places, significant digits and multiples under named
rounding modes.

**Phase 3: dimensions and units.** Dimensions with rational exponents, units with exact conversion
between them, and each unit's decimals and bounds.

**Phase 4: quantities.** Quantities declared once with their symbol, description and unit, read
through `Describe<T>`, and measurements that may be absent without being an error.

**Phase 5: formulas.** An expression layer with the arithmetic operators, powers and roots,
environments of measured values, and evaluation into an `Outcome` that holds a value, a verdict,
an invalid result or nothing, never a bare number that hides which.

**Phase 6: citations and documentation.** `documented()` citations on any part of a formula,
rendering in plain text, Markdown and LaTeX, generated documentation with a symbol table, a
MkDocs and Doxygen site, and a formula gallery produced by running the library.

**Phase 7: tracing.** Composable sinks, a trace that records every step of an evaluation, and
bounded rendering of it as an audit trail a person can check.

**Phase 8: rounding, conditionals and an escape hatch.** Rounding as a node of a formula,
`when()` conditionals, and `numeric_value_of`, which takes a number out of its unit only with a
stated justification that the trace records.

**Phase 9: constraints.** Constraints as peers of formulas, with verdicts recorded as trace steps,
and checking a whole set without stopping at the first failure.

**Phase 10: lookup tables.** Exact, banded and interpolating lookups over tables validated at
compile time for gaps, overlaps and order, each traced with the row it used or the reason it found
none. Lookup keys are shown by their enumerator names, or by an author's own spelling through
`EnumeratorName`.

**Phase 11: methods and jurisdiction overlays.** A method holds variants chosen by tag, a rounding
rule and constraints; `evaluate_method` and `check_method` evaluate and check it, and the trace
names the variant that ran and its position in the method as published. An overlay pins or prunes
variants, fixes a quantity with `with_constant`, defines one with `add_derived`, replaces a
variant's formula, changes the rounding rule and replaces the constraints. Every operation takes a
citation argument, and an empty one is shown as `(no citation given)`; each change is recorded in
the trace with what the overlay cited, a fixed, derived or replaced part is marked as such in the
generated documentation, and an overlay that would silently do nothing is refused. Overlays stack,
the later one holding. A vocabulary renders a formula and its trace in a jurisdiction's own symbols,
and `TagName` spells a variant's tag.

**Phase 12: series.** A quantity measured at every point of a method's domain, `series<Q, N>`,
with each element absent or present on its own; elementwise arithmetic with a broadcast scalar,
per-element constants, running totals from either end, `sum`, and per-element rounding. A failure
names its element. Conformity judges each element against its own row of a limit envelope, closed
at both ends and either side explicitly unbounded, and the trace records each element's value, in
the check's unit, and the row it was judged against.
`snapped` replaces a value with the nearest permitted one under a stated tie rule. Curves pair a
domain with values, `interpolate_at` reads one between two points, and `splice` joins two curves
into one whose values run in a required direction; a curve that fails names the point and the
rule it broke there. Raw observations, as many as were made up to a stated capacity, are binned
into declared half-open classes; an observation in no class fails at its position, never dropped.
A series renders with an index marker, `m_r(i)`, and its trace spends the same step budget, one
unit per element. A series is evaluated with `checked_evaluate_series` and explained with
`explain_series`, a curve with `checked_evaluate_curve`. A failure is a `SeriesFailure` naming its
position, and its `FailureSite` says whether that is an element of the result or, for a binning,
an observation of the input. `envelope_from` builds an envelope from rows read at run time, and
`MeasuredObservations::from` a set of observations; each refuses a wrong count with both counts,
never padding or truncating. Everything that compares -- a curve, a splice, a snap, a binning and
a conformity check -- is evaluated with `Rational` only, and refuses any other `Rep` at compile
time. A new guide, *Series and grading curves*, works a screen analysis through
all of it, and the gallery gains a series, a grading curve and a binning.

**Phase 13: statistics.** A sample of determinations reduced to one value: `sample_count`,
`sample_mean`, `sample_variance` (over n - 1, in two passes) and `sample_range`, strict about
absence, exact, and naming the determination at which an overflow happened. `rounded_sqrt` rounds a
square root exactly to a declared granularity, so a standard deviation is the correctly rounded
decimal and never a rounded floating-point one. `critical_value` reads an author's table by sample
size and misses rather than guessing a neighbouring row. `precision_limit` evaluates a
repeatability or reproducibility limit at the level of the results it checks, in two declared
passes, with `precision_level` read only inside it. `without_outliers` rejects outliers from a
sample by a criterion (a deviation from the mean or in standard deviations, or the gap from an
extreme to its neighbour over the range), whose limit may read a critical value at each pass's
sample size, re-running the mean until nothing more is rejected; a declared bound turns one rejection too many, too few left, or too few made to begin with,
into the author's verdict. Every pass and every rejected value is its own step in the trace.
A sample is a series or raw observations, `observations<Q, Capacity>`, whose count is known only
at run time: `Capacity` is a bound, and every statistic, rejection and critical value reads the
observations actually made. None made count 0 and have no mean.
`docs/statistics.md` is the guide to all of it, with `examples/statistics.cpp` and gallery entries.
`docs/numeric-headroom.md` measures how many of `Rational`'s 64 bits real formulas use: most
leave 30 bits or more, but a sample variance of masses read to 0.01 mg leaves 4, and read to
1 µg it overflows on 423 of 1,000 samples -- which recommends 128-bit intermediates.

**Phase 14: other samples and other tests.** A formula reads from a record other than the one
being evaluated through `from_record<Role>(expression)`: one value, or a computation over the
other specimen's own measurements. A role is a type the author declares and a record is data --
a sample and test key, an environment and lineage keys -- held by role in a `record_context`,
which is this record's environment and so goes wherever an environment goes. Every step inside
a read from another record carries which record it was read from, with both keys, and each value
read from it names the record in its trace line; the record is kept once in the trace
(`Trace::origins`, read by `origin_of`), so a step costs what it did. Every input, a series
included, says whether it was measured or typed in, and an entry typed in empty says so.
`same_lineage<Attrs...>()` gates a read on the two records sharing the author's lineage
attributes: the value; a refusal (`DomainError`) when any attribute differs, with the trace
naming it, the record compared with, and whose key is whose (`lineage_of`); or no answer when a
key is unknown and none differs. A record not yet made gives no answer, never zero, whatever its
environment holds. `checked_explain` traces a refused read without throwing. On the page a read
reads `f_c of Reference`, and `(F / A) of Reference` for a computation, with a row per record in
the symbol table. A role's name must be identifier-like -- starting with a letter, never read as
`this record`, and never as another bound role's -- and `TagName` spells it otherwise. An
overlay's constant or derived quantity reaches inside a read from another record, as it reaches
the rest of the method, and says when it replaced a value a person typed in. A series or raw
observations read inside a read from another record are traced with that record, and have their
own row on the page; a read whose value would be a whole series is refused, and reduced inside
instead: `from_record<Role>(sum(series<Q, N>))`.

**Phase 15: opaque operations and bounded retry.** An opaque operation is a named computation a
method relies on but does not spell out: a type the author declares with its name, the shape of
each input, a name for each output and what each measures, and a `compute` that receives the
inputs' values -- never the environment -- and returns the outputs. `opaque<Op>(citation, inputs...)`
calls it, and each output, `opaque_output<"name">(call)`, is a node a formula uses like any other.
Its trace line lists every output it produced and its citation and says `[inside not shown]`; the
page lists the operation with its outputs. Using two outputs runs the operation twice, and the
trace shows both runs. `linear_least_squares(curve, citation)` ships as one: exact in `Rational`,
its own `DomainError` for fewer than two distinct points, and `Overflow`, never a wrong line, when
its exact sums leave `Rational`'s range -- `docs/numeric-headroom.md` measures when, over 2 to 128
points. A citation is required, and an empty one reads `(no citation given)`. An overlay rewrites
through an operation's inputs, which are all it reads. `retry<R, Max, FirstJudged>(start, attempt,
acceptance, verdict, citation)` repeats one attempt expression at most `Max` times, at most 64,
until the acceptance holds; `previous_attempt<R>`, `this_attempt<R>`, `attempt_number` and
`attempt_input<Q>` -- a new recorded determination at each attempt -- say where it is. It ends in
exactly one of six ways (`RetryEnd`): accepted with that attempt's value; exhausted with the
method's verdict, never a missing value; not judgeable; not recorded; failed, naming the attempt;
or entered by a person, which no attempt replaces. Every attempt is traced with its judgement, then
how the retry ended; the page marks the result as iterated. An opaque call read inside a read from
another record carries its origin, and a retry's attempt can read from another record. An acceptance
can compare with a `precision_limit` whose level is `this_attempt`. A retry whose starting value reads a
context node is refused where it is written, and so is one whose attempt reads `this_attempt`. A
concept asked of a retry, such as `std::equality_comparable`, answers no; comparing or adding one is
refused. A failing starting value ends at `RetryFailure::atStartingValue`, never at the first
attempt's position. An opaque call stopped at a failing input writes each input after it as
`(not evaluated)`, and an output never borrows a unit that has no symbol. A precision limit can
read a level inside an opaque output. `OpaqueOperation` joins the customisation points: its
`compute` does its arithmetic through `RepTraits` and never throws.
`docs/opaque-and-retry.md` is the guide, with `examples/opaque_and_retry.cpp` and gallery entries.

**Quantities declared by alias.** `using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w",
"effective water content", unit::Litre>;` is supported beside the struct form, everywhere a
quantity is named, and the two mix in one formula; the guides, the examples and the gallery now
lead with it. An alias cannot be forward-declared, and two aliases with all four arguments equal
are one type; `docs/quantities.md` sets out what each spelling costs.

### Changed

- Invented example numbers replaced so none resembles a published table: the band edges, lookup
  corrections, shape and national factors, test ages and code-like thresholds in the guides,
  examples, gallery, tests and header comments are now uneven invented values.

### Fixed

- A `documented()` step, and a jurisdiction's replaced-variant step, printed its value in coherent
  SI with no unit: a citation over `16 g` read `#2 = 2/125`. Each now states its value in the unit
  of the step it wraps, as that step does: `#2 = 16 g`. Over a consumer's node, which records no
  step of its own, it stays in coherent SI.
- A local in `evaluate.hpp` made GCC 13.3 report `-Wmaybe-uninitialized` at `-O2`, a false
  positive, in a consumer's own build: under `-Werror` a program including the header failed to
  build. CI's GCC 14 does not report it. `rounded()` and `rounded_to_digits()` had the same local,
  reported under `documented()` at `-O3`. The same local relayed an operand's failure in a
  conditional, `numeric_value_of()`, a unary operator, a power or root, and the three lookups, and
  reported a lookup's miss, key conversion failure or interpolation failure. Each of those now goes
  through one helper that holds no such local.
- `check_all(constraints())`, over an empty set, made GCC 13 report
  `-Wunused-but-set-parameter` on the sink, in a consumer's own build under `-Werror`.
- Locals and parameters in the public headers shared names with ordinary globals such as
  `result`, `value`, `index`, `here` and `origin`, so cl at `/W4 /WX` reported C4459 in a consumer
  that declared one, and failed to build. They are renamed, and a test declares some 270 such globals
  before every header -- `points`, `slope`, `intercept`, `previous` and `verdict` among them, which
  the least-squares example's own names found in the curve, lookup, snap and least-squares headers.
- cl names an enumerator that is not one as a cast, `(enum Flag)true`; it was shown as a name, and
  is refused now.
- The opt-in header include check was not portable across the compilers it claimed to run on.
- `render<Dialect::LaTeX>` wrote a unit's symbol unescaped, so `unit::Percent`'s `%`, TeX's comment
  character, commented out the rest of the formula: a percent-valued lookup row or constant did
  not typeset. Every unit symbol that enters LaTeX -- a constant, a rounding, a numeric value, a
  lookup row -- is now escaped and set as `\mathrm{...}`, as is an author's `#`, `&`, `_`, `$`, `{`
  or `}`. A constant reads `150\,\mathrm{mm}` rather than `150 mm`.
- A lookup's rows and key names were set in LaTeX as `\text{...}`, whose escapes the site's MathJax
  shows literally: `\text{key fit\_2}` read `key fit\_2`. They are now set as `\mathrm{...}`, with a
  space as `\ ` (`detail::latex_math_words`).
- A band table with a gap, an exact lookup table with a repeated key, or a breakpoint table out of
  order drew up to five errors on clang++, one the library's message and the rest the compiler
  reading the failed check. Each now draws the one message on cl, clang-cl, clang++ and g++, the
  compilers the library is built and tested with.
- A trace stated a sum, a range or a running total of readings in a unit with an offset in that
  unit, as if it were a reading: the sum of 23.7, 41.3 and 37.9 °C read `3246/5 °C` and their
  range `-5111/20 °C`, each off by the offset. A rejection's deviation from the mean did the same,
  `abs(x - mean) = -26891/100 °C` for a deviation of 4.24 K. Each now reads in the coherent unit,
  `18447/20`, `88/5` and `106/25` kelvin, as a difference of two readings does; a mean, which is a
  reading, stays in degrees Celsius. A sum or a range no longer borrows a unit without a symbol
  either, and the mean and a rejection's lines -- a pass's mean, a rejected value, a deviation --
  follow the same rule: in a unit with no symbol they read in the coherent unit.
- A binary step whose left operand failed named that operand alone, in prefix form: `/ #2` read as
  something unseen divided by #2, where #2 was the dividend that failed and the divisor was never
  evaluated. The line now reads `#2 / (not evaluated)`, and a side computed by a consumer's node that
  records no step of its own reads `(untraced)`. `Step` gains `leftOperand` and `rightOperand`
  (`OperandSide`), which say so.
- A series multiplied or divided by a pure number was stated in coherent SI with no unit, `411/2000`
  for 205.5 g, and so were a curve over it and a value read off that curve. Each now reads in the
  series' unit, `411/2 g`, when that unit has a symbol and no offset.

[Unreleased]: https://github.com/LASTRADA-Software/formula-cpp/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/LASTRADA-Software/formula-cpp/releases/tag/v0.1.0
