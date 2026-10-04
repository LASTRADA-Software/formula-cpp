# Changelog

Notable changes to formula-cpp, in the form [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
describes. The public API may still change before 1.0: a minor release may break it, and each such
change is recorded here.

## [Unreleased]

### Added

- **`formula::Int128`** (`int128.hpp`), a signed 128-bit integer with one API on every compiler: the compiler's own
  128-bit integer computes where it has one (GCC, Clang), portable `constexpr` code everywhere else (cl, clang-cl).
  It converts to no built-in integer implicitly or explicitly; `to_int64()` and `to_uint64()` say when a value does
  not fit. `std::format` writes it in decimal, and `std::numeric_limits<formula::Int128>` states its limits as a
  built-in signed integer's are.

### Changed

- **Every computed value in a trace shows a unit.** A value scaled by a pure number, and a sum or difference of
  values shown in one unit, read in that unit: the outlier-rejection limit `#2 * #3 = 1239/500000` is now
  `#2 * #3 = 1239/500 g`. A negation and an absolute value read in their operand's unit, and a conditional and a
  precision limit in the unit of the step they restate. An offset unit is never borrowed for a sum, difference,
  scaling or negation: the difference of two Celsius readings reads in `K`. Any other dimensioned value is shown in
  the coherent unit, followed by its spelling from the base units (`427/125000000 kg^2`, `60000000 kg/(m s^2)`), and
  so is a value in a unit that has no symbol. A dimensionless value is still a bare number. A trace text pinned in
  a test changes wherever it showed a dimensioned value bare. `Step::unit` of a scaled, summed, negated,
  absolute-value, conditional or precision-limit step now holds the unit it borrowed, so code that reads steps sees the unit the
  trace text names.
- A snap's permitted values, a binning's classes, a lookup's bands and rows and a curve's rows, declared in a unit
  that has no symbol, are written in the coherent unit with its spelling, as the value beside them is, rather than
  as numbers in a scale the line does not name.
- **`Rational` stores its numerator and denominator in `formula::Int128`**, so `Rational::Int` is `Int128` and a
  `Rational` is 32 bytes. Realistic laboratory statistics that overflowed 64 bits now answer: the sample variance of
  masses read to 6 decimal places of a gram, rejection by standard deviations at that resolution, and a cylinder's
  strength at every diameter measured (`docs/numeric-headroom.md`). Code that stored `numerator()` or `denominator()`
  in a built-in integer must narrow with `to_int64()`. The `_r` literal, `Rational::from_decimal`'s exponents and
  rounding's decimal places keep their limits of 18.
- `rounded_sqrt` computes in 128 bits, and answers at more places before it reports `Overflow`.
- `NumberTextCapacity` is 128, so that a 39-digit numerator over a 39-digit denominator fits a `NumberText`.
- `band(Rational, Rational)` and `breakpoint(Rational)` refuse a bound or key that does not fit their 64-bit fields:
  in a constant expression it fails to compile, naming `formula_band_bound_out_of_range` or
  `formula_breakpoint_key_out_of_range`; reached at run time, it ends the program, because a `Band` or a
  `Breakpoint` is a template argument, built at compile time, and has no way to carry a failure.
- `Step` gains `lookupKeyHigh`, bits 64 to 127 of the count a sample-size lookup selected with, so that a miss on a
  count above 2^64 - 1 names the whole count. Code that reads `lookupKey` for such a step reads both.
- `Rational`'s converting constructor takes every built-in integer type of at most 64 bits except `bool`, exactly,
  `std::uint64_t` now among them; a wider built-in integer is refused. A constructor from `Int128` is added.
- **Breaking: a dimensionless unit with a scale or an offset and no symbol no longer compiles**, wherever it is
  written: as a quantity's unit, a constant's, a rounding's, or a table's key or result. A trace showed a value in
  such a unit as a bare number in a scale nothing named (one half in hundredths read `50`), and no spelling of the
  unit could name it. Give the unit a symbol (`%`, `ppm`, or the author's own), or declare the quantity in scale 1.
  The refusal reads `formula: a dimensionless unit with a scale must have a symbol`. A dimensioned unit with no
  symbol is still accepted, and shown in the coherent unit.
- A coherent unit with no positive exponent is spelt with negative exponents in a trace: `kg^-1`, `s^-1`,
  `m^-1 s^-1`, `JPY^-1`, where it was `1/kg`, `1/s`, `1/(m s)`, `1/JPY`. After a number in the fraction style,
  `20000/413 1/kg` read as a fraction divided again. A unit with a numerator keeps its slash: `m/s`, `EUR/JPY`.
  A trace text pinned in a test changes where it showed such a unit.

## [0.3.0] - 2026-10-01

The third release. It gives a shorter spelling to everything the examples repeated, and takes no
spelling away. `_r` writes an exact decimal, `27.3_r` being the `Rational` 273/10; a series takes
plain numbers and `not_measured`; `number_of` reads the number a result holds; `describe` and
`std::format` write results, units, dimensions and enumerations; `traced`, the `explain_*` twins and
`trace_of` return a trace in one call; `DecimalRounding` names a rounding once; a `Measured` has
throwing twins for its checked conversion, rounding and bounds check; and `yields<Q>` binds a
formula to its result quantity. Every example is rewritten with these spellings and prints with
`std::println`. GCC 14 is now the oldest supported GCC. Some changes break code written for 0.2.0;
each is listed under Changed.

### Added

- `convert_to<R>`, `round_to_declared` and `within_bounds` for a `Measured<Q>`: the throwing twins
  of `checked_convert_to`, `checked_round_to_declared` and `checked_within_bounds`, for callers who
  would only rethrow. They throw `ArithmeticException` where the `checked_` form returns an error.
  Every earlier spelling stays.
- `_r`, an exact decimal literal: `27.3_r` is the `Rational` 273/10, where `27.3` is the double
  nearest it. It reads integers, fractions, a leading or trailing point (`.5_r`, `5._r`), an
  exponent (`1.5e-3_r` is 3/2000) and digit separators, and a minus sign is `Rational`'s own
  negation. A spelling `Rational` cannot hold, and one that is not a decimal (`0x1F_r`, and `017_r`,
  which C++ reads as octal), fails to compile.
- `measured_series<Q>` takes plain numbers and `not_measured` beside `Measured<Q>`:
  `measured_series<Retained>(127, 10.3_r, not_measured)`. An element that is none of these draws
  one message. `band(low, high)` takes its bounds, and `breakpoint(key)` its key, as exact numbers:
  `band(83.7_r, 97.3_r)`, `breakpoint(12.7_r)`. Every earlier spelling stays.
- `number_of(x)`, the number a result holds or nothing, as a `std::optional<Rational>`. It reads a
  `Measured`, an `Outcome`, a `checked_evaluate` result, an `Evaluated<Rational>`, a
  `RetryOutcome` and a `RejectionOutcome`, and is empty for an absent number, an error, a verdict
  and an invalid result, so `number_of(checked_evaluate<Q>(...)) == 0.5_r` is a complete check.
- `describe` of a `ConstraintOutcomeKind`, a `RetryEnd`, a `ValueSource`, an `OutcomeKind` and a
  `FailureSite`: a lowercase phrase with no trailing punctuation, as `describe` of an
  `ArithmeticError` already gave -- `satisfied`, `not checked`, `manually entered`, `verdict`,
  `result element`.
- `std::format` writes an `Outcome<Q>`, a `Unit`, a `Dimension` and every enumeration that has a
  `describe()`, with `<formula-cpp/format.hpp>` included. An `Outcome` takes a `Measured`'s spec and
  writes a value as it does, an empty one as `(not measured)`, and a verdict or an invalid one as
  its label, padded by the spec's fill, alignment and width. A `Unit` is its symbol (`kJ`); a
  `Dimension` its exponents (`L^2 M^-3`, `L^(1/2)`, `(dimensionless)`); an enumeration its words
  (`overflow in exact arithmetic`), aligned as a string is.
- `symbol_of<Q>()` without a vocabulary is `Describe<Q>::symbol`, and `render(x, options)`,
  `render<D>(x, options)`, `document(x, options)` and `document<D>(x, options)` take
  `RenderOptions` without a vocabulary that renames nothing.
- `traced(evaluation)`, which runs any evaluation that takes a sink with a `RecordingSink` and
  returns `{ outcome, trace }`: what it returned, failure included, and every step it recorded.
  `explain_method<Tag>`, `explain_check_method`, `explain_curve<DomainQ, ValueQ>`,
  `explain_rejection<Q>`, `explain_check`, `explain_check_all` and `explain_conformity` are the
  traced twins of `evaluate_method`, `check_method`, `checked_evaluate_curve`,
  `checked_evaluate_rejection`, `check`, `check_all` and `check_conformity`, each with the
  vocabulary as an optional last argument. `explain_series` and `explain_retry` return the same
  shape as before.
- `trace_of<Q>(expression, env)`, `trace_of(boundFormula, env)` and `trace_of_si(expression, env)`:
  the trace of an evaluation in one call, whether it succeeds or fails, for code that only shows how
  a number was reached. They return no outcome; read that with `checked_evaluate` or
  `checked_explain` where it is used. Each takes the vocabulary as an optional last argument.
- `DecimalRounding` and `SignificantRounding` name a rounding once -- a unit, how many places or
  digits, and a `RoundingMode` -- where the three arguments were repeated at every use:
  `constexpr DecimalRounding tenthMpa { unit::Megapascal, DecimalPlaces { 1 }, RoundingMode::HalfAwayFromZero };`
  then `rounded<tenthMpa>(x)`. `rounded`, `rounded_to_digits`, `rounding_rule`, `with_rounding`,
  `rounded_output`, `rounded_sqrt` and `rounded_elementwise` each take one, and build the same
  type as the three arguments do. `rounded_elementwise<R>(series)` rounds every element to the same
  places. Every earlier spelling stays.
- `declared_rounding(unit, mode)`, a `DecimalRounding` in the places `unit` declares.
- `yields<Q>(expression)` names a formula's result quantity once, where the formula is written:
  `constexpr auto ratio = yields<WaterCementRatio>(var<WaterVolume> / var<CementVolume>);` then
  `evaluate(ratio, environment)`. `evaluate`, `checked_evaluate`, `checked_evaluate_series`,
  `checked_evaluate_rejection`, `explain`, `checked_explain`, `explain_series`, `explain_rejection`
  and `define` take it, and return what they return for the formula it holds and `Q`; `render` and
  `document` write the formula. The result is still never deduced from the expression: `Q` is
  checked against the dimension the expression computes where it is written, with
  `checked_evaluate`'s message, and a result named at the call as well is accepted only when it is
  `Q`. Nest `documented()` inside it, and reuse the formula in another through `.expression`; a
  `yields` around a bound formula is refused where it is written. A bound series, rejection of
  outliers, retry or whole opaque call handed to a verb that answers with one value is refused in
  words that name the verbs that take it, and a bound formula used as an operand or compared is
  refused in favour of its `.expression`. Every earlier spelling stays.

### Changed

- A floating-point key given to `breakpoint`, or a floating-point bound given to the
  four-argument `band`, used to be truncated silently (`breakpoint(1.5)` was `breakpoint(1)`). It is
  now refused at compile time, with a message that names the exact spelling for that call:
  `12.7_r` or `Rational { 127, 10 }` for `breakpoint`, `breakpoint(127, 10)`, `band(12.7_r, 17.3_r)`
  or `band(127, 10, 173, 10)`.
- GCC 14 is the oldest supported GCC; older GCC is not supported. The install-and-consume check
  now builds with it on Linux.
- `<formula-cpp/format.hpp>` now specialises `std::formatter` for `formula::Outcome<Q>`,
  `formula::Unit`, `formula::Dimension` and every enumeration that has a `describe()`, as it
  already did for `Rational` and `Measured<Q>`. A program that defines its own `std::formatter`
  for one of these types now defines it twice, and a generic `std::formatter` for every
  enumeration is ambiguous for them; remove it and use the library's.
- `checked_convert_to` refuses a conversion between measured quantities of different dimensions
  where the call is written, with or without a value present. Code that converted, say, a volume
  into a mass, or euros into yen, used to compile and get `ArithmeticError::DomainError` at run
  time; it no longer compiles, and the message names the two quantities. A conversion between
  quantities of one dimension is unchanged.
- An unqualified call with arguments of this library's types now also finds, by argument-dependent
  lookup, the functions this release adds, among them: `describe` of a `ConstraintOutcomeKind`, a
  `RetryEnd`, a `ValueSource`, an `OutcomeKind` or a `FailureSite`; `number_of`, `convert_to`,
  `round_to_declared` and `within_bounds`; `traced`, `trace_of` and `trace_of_si`; the `explain_*`
  twins above; `yields`, given an expression; and `declared_rounding`, given a `Unit`. A consumer's
  own function of one of these names, visible where the call is written, meets the library's in one
  of three ways. A function template of the same name and shape -- a
  `template <typename R, typename Q> Measured<R> convert_to(Measured<Q>)` helper, say -- is
  displaced **silently**: the library's is more constrained, so it is chosen, the helper no longer
  runs, and where the helper returned an absent value on failure the library's `convert_to` throws
  `ArithmeticException`. A function of another shape -- a non-template beside the library's
  non-template `describe`, such as a `describe(ConstraintOutcomeKind)` helper, or a template taking
  its argument by value, such as `template <typename Q> Rational number_of(Measured<Q>)` -- makes
  the call ambiguous. A non-template taking exactly a library template's parameter types is
  preferred over it and keeps working. Where a helper is displaced or a call is ambiguous, rename
  the helper, remove it where the library's does the same (as `examples/constraints.cpp`'s
  `describe` was removed), or call it by a qualified name such as `::convert_to`. And since `_r` is
  declared in an inline namespace of `formula`, `using namespace formula;` now brings it into scope,
  where a consumer's own `_r` is ambiguous with it.
- The refusal of `with_rounding` with no citation now opens
  `formula: with_rounding<...>() was given no citation`, for either spelling, the three arguments or
  a `DecimalRounding`; it opened `formula: with_rounding<U, Places, Mode>() was given no citation`.

## [0.2.0] - 2026-09-30

The second release. It adds calculations -- quantities defined by formulas, with a dependency graph
known at compile time -- and worksheets that recalculate only what a change reaches; decimals in
traces, rendered formulas and `std::format`; base dimensions an application declares, such as a
currency; and power, energy and Fahrenheit units. A value the exact layer cannot hold is now written
only as the correctly rounded decimal its formula declares, proved with integer arithmetic: an
opaque operation's output through `rounded_output`, and the new logarithms and exponentials, which
are exact where the value is rational. Least squares reaches raw observations: a line with R² and
the number of points, and a regression on up to eight regressors that refuses a singular design.
Some changes break code written for 0.1.0; each is listed under Changed.

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
  an override. On a worksheet about to be discarded, `set` and `clear_override` each answer it
  changed, so `worksheet(...).set(...)` can be kept or asked at once. `with(...)` answers a changed
  copy and leaves the worksheet as it was. A value a change reaches is reused when nothing it reads
  has changed, and a value calculated again to the same answer from the same source counts as
  unchanged, so that what reads it can be reused in turn; `recomputed()` and `reused()` count both.
  The calculation's queries take a worksheet as well. A calculated value is kept in its quantity's
  declared unit, so a conversion the inlined formula never makes can overflow. Refused where it is
  written, each with one message: an environment with no entry for an input, with an entry the
  calculation neither reads nor defines, with a series or raw observations, or with a calculated
  quantity given as a measurement rather than `entered`; `set()` naming one quantity twice, one the
  calculation neither reads nor defines, a series, or a calculated quantity given as a measurement;
  asking about a quantity the calculation neither defines nor reads; and `clear_override` or
  `is_overridden` of an input, which is set again rather than overridden -- whether its value was
  typed in is the `source()` of what `calculate` answers for it.
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
  `18.8822`), as a decimal rounded under a named rounding mode (`118.26`), or as a fraction (`3/5`),
  into a `NumberText` with a fixed 64-byte buffer -- without allocating, and at compile time as well
  as at run time. A `NumberStyle` chooses the notation. A decimal is shown only when it is the exact
  value, so `1/3` stays `1/3` under `NumberStyle::exact_decimal()`; an approximation is opt-in
  through `NumberStyle::approximate_decimal(mode)`, rounds at the unit's declared decimals and
  always starts with `≈` (`≈0.333`). `number_text` of a `Measured` value adds its unit's symbol
  (`5.2 kJ`), or reads `(not measured)` when it is absent. `decimal_text` spells exactly what
  `checked_round` rounds to, and at 0 to 18 places still does where `checked_round`'s own arithmetic
  overflows. `exact_decimal_text` gives the exact decimal alone, as a `std::optional<NumberText>`
  empty where the value has none of at most 18 places, and `has_exact_decimal` says whether it has
  one. `fraction_text` gives the fraction alone, in lowest terms, and `checked_number_text` and
  `checked_decimal_text` answer `std::expected` where `number_text` and `decimal_text` throw
  `ArithmeticException`. `ApproximationMarker` is the `≈` an approximation starts with, and
  `NotMeasuredText` the `(not measured)` an absent value reads.
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
  is never padded with zeros, and where its default 3 places would round a value other than zero to
  `≈0` they are extended to its first significant digit, up to 18: a price in euros per joule
  reads `≈0.0000001`, not `≈0`. A step's value the style cannot spell in its unit reads `(not
  shown: ...)`. `render_trace(trace, { .numbers = ... })` without `.maxSteps` still does not
  compile. `render_derivation` spells a worksheet's derivation in the same style: its steps and
  inputs as a trace's lines, each block's header value as the line that reads it -- rounded,
  padded or exact alike -- and the header's definition as `render` writes it under
  `RenderOptions`. A typed value is exact wherever the derivation states it, on the line of another
  block that reads it as well: the new `WorksheetEntry::readSlots` records, for each step, which of
  the calculation's quantities it read from the worksheet, so that the line is matched to that
  value's block even where a vocabulary writes two quantities alike.
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
  writes a value with an exact decimal of at most 18 places as it is, and throws `std::format_error`
  when written if the value has none and exact arithmetic cannot round it there, as
  `from_double_exact(0.1)` at -3 decimals cannot. The library owns these `std::formatter`
  specialisations, so the header belongs in every translation unit that formats these types or asks
  `std::formattable` about them; only `char` is supported.
- `RenderOptions` for `render()` and `document()`: `render(f, vocabulary, { .numbers =
  NumberStyle::exact_decimal() })` writes a constant holding 863/1000 as `0.863`, and so every
  number a formula states -- a table's bounds and rows, a snap's permitted values, a domain's
  points, a per-element constant's values, an envelope's limits -- and `document()` its formula, a
  derived quantity's derivation and a rejection's limit; for a calculation, `render` each definition
  and `document` its formula and each `calculatedAs`. These numbers were typed by the formula's
  author, so they are written exactly whatever the style: `1/3` stays `1/3` under an approximating
  style, and none is padded, so `number(Rational { 1, 2 })` reads `0.5`. Without options nothing
  changes. The style travels with the vocabulary, the one argument every `render_node` already
  receives, so a consumer's own two-argument `render_node` hands it on unchanged and can read it
  with `number_style_of(vocabulary)`. `typed_number_style(vocabulary)` is the style every number a
  formula states is written in -- the vocabulary's, exact only and never padded -- for a consumer's
  own node that writes a number its author typed.
- `docs/display.md`, a guide to displaying numbers, and its example `examples/display.cpp`: decimals
  in a trace, in a rendered formula and in its documentation, `number_text()` and `decimal_text()`,
  and a reference for `std::format` of a `Rational` and a `Measured` -- every form of the spec with
  the text it writes, the rounding modes, the width in code points, and what a bad spec does. Each
  output block on the page is checked against the example's output, and each code block against
  its source.
- A guide, *Calculations and worksheets* (`docs/calculations.md`), with `examples/electricity_bill.cpp`:
  a household's monthly electricity bill as a calculation and a worksheet -- its graph and its
  documentation page, a first run, changes and what each recalculates, the derivation of a value
  that was reused, a what-if copy, a value typed in by hand, a failure reaching what reads it, an
  input nobody measured, and what is refused.
- `rounded_output<"name", U, Places, Mode>(call)`: one output of an opaque call, rounded to `Places`
  decimal places of the unit `U` under `Mode`, exactly -- the decimal the operation's true output
  rounds to, even where that output is a fraction too wide for `Rational`, for an operation that
  states its outputs in wider integers; any other operation's output is computed in `Rational`,
  rounded exactly, and fails with `Overflow` where `opaque_output` would. It rounds in `U`, never in
  the coherent unit, and its dimension is the output's. It is refused, in the library's words, for an
  output the operation does not declare, for a unit that does not measure the output's dimension and
  for a unit with an offset, and it does not compile under `Rep = double`, as no rounding node does.
  `rounded<>(opaque_output<>(...))` is unchanged: the exact output rounded afterwards, or `Overflow`
  where the exact output overflows. It renders as the rounding it states,
  `round(linear least squares(t(i), L(i)).slope, to 4 dp of mm/s)`, a page lists its call once
  however the call's outputs are used, an overlay's constant reaches inside its call as it does inside
  `opaque_output`'s, and a calculation may define a quantity by one.
- `OpaqueValues`, and `OpaqueCallInfo::values`: a sink hearing an opaque call is told `Exact`, as
  before, or `RoundedWhereUsed` for a call evaluated for a `rounded_output`, whose `opaque_produced`
  is then handed an `OpaqueEvaluated<Rational, 0>` -- whether the call answered, was absent or
  failed, and no value.
- A trace of a `rounded_output`: its call's line names the outputs without values, since none
  exists until an output is rounded -- `linear least squares(#3) = intercept, slope: rounded where
  used [inside not shown] [...]` -- and the output's own line states the rounding:
  `round(slope of #4, to 4 dp of mm/s) = 3393/5000 mm/s [nearest, ties to even]`. The rounded
  decimal is the step's exact value, so no number style marks it `≈`. A failure the call carried
  reads as the call's on the output's line too. `OpaqueStepData` records `values`, and in `answer`, an
  `OpaqueAnswer`, whether the call answered.
- `rounded_output<"slope", ...>(linear_least_squares(...))` answers where the exact route overflows:
  the fit is computed in 256-bit integers for it, so that on readings at 3 decimal places of a few
  thousand newtons it answers at every size from 2 to 128 points, where `opaque_output<"slope">`
  overflows at 57 of those sizes, the first at 34. A different denominator on every point outgrows
  the 256 bits from 58 points, and the answer is `Overflow` (`docs/numeric-headroom.md`).
- The guides explain values the exact layer cannot hold: `docs/display.md` gains *Values the exact
  layer cannot hold* -- such a value, a root, a logarithm, an exponential or the output of a fit
  computed in wider integers, is written only as the rounding its formula declares, exact and
  without `≈`, and refused without one -- and `docs/opaque-and-retry.md` a section on
  `rounded_output`, with the fit's trace and the fifteen-point fit it answers.
- `ln(x)`, `log10(x)` and `exp(x)` (`function.hpp`) take the natural logarithm, the
  decimal logarithm and the exponential of a formula. The argument must be dimensionless -- a quantity
  divided by a reference value of its own dimension, or a number read with `numeric_value_of` -- and a
  dimensioned one does not compile. A percentage is dimensionless and read as a fraction, so `log10` of
  1000 % is 1. Evaluated exactly, each answers where its value is rational -- ln 1 = 0, exp 0 = 1,
  log10 10^k = k for k from -18 to 18 -- and is `ArithmeticError::Inexact` elsewhere; the logarithm of
  zero or of a negative value is `DomainError`. `checked_evaluate_si<double>` answers with `std::log`,
  `std::log10` and `std::exp`. `RepFunctions` gains `natural_log`, `decimal_log` and `exponential`,
  which a representation of a consumer's own needs only to evaluate these.
  They render as `ln(x)`, `log10(x)` and `exp(x)`, in LaTeX as `\ln\left(x\right)`,
  `\log_{10}\left(x\right)` and `\exp\left(x\right)`, and a trace writes each as a step of its own,
  `ln(#1) = ...`.
- `rounded_ln<Places, Mode>(x)`, `rounded_log10<Places, Mode>(x)` and `rounded_exp<Places, Mode>(x)`, in the
  new header `rounded_transcendental.hpp` (included by `formula.hpp`), answer the logarithm or exponential of a
  dimensionless expression rounded to `Places` decimal places under `Mode`: the decimal the true value rounds
  to, computed with integer arithmetic, so no floating-point mode enters and the same inputs give the same
  digits at compile time and at run time. Only ln 1, log10 10^k and exp 0 can tie, and the mode breaks the tie
  as `checked_round` does. `Overflow` answers a result too large for a `Rational` at the declared places,
  places outside -18 to 18, and the rare rounding the computation cannot decide: one whose value lies within
  the computation's width -- under 2^-118, relative for the exponential -- of a rounding boundary.
  Evaluated with `Rep = double`, each is refused at compile time, as `rounded_sqrt` is.
  `rounded<...>(ln(x))` still means an exact logarithm, which fails where there is none.
  They render as `round(ln(x), to 4 dp)`, in LaTeX `\operatorname{round}_{4}(\ln\left(x\right))`, and a trace
  writes each as one step whose value is the rounded decimal and whose bracket names the mode:
  `round(ln(#1), to 4 dp) = 6931/10000 [nearest, ties away from zero]`.
- A section, *Logarithms and exponentials*, in *Expressions and evaluation* (`docs/expressions.md`), and a
  gallery entry, a logarithmic reduction rounded exactly to 0.01.
- `observations.hpp`, holding raw observations -- `observations<Q, Capacity>`, `ObservationsVarNode`,
  `ObservationsNode`, `ObservationsValue` and `EvaluatedObservations` -- which `binning.hpp` declared before and
  still includes. Code that reads observations no longer needs a binning's classes, lookups and bands.
- `InputShape::Observations`: an opaque operation may take raw observations, `observations<Q, Capacity>`, as an
  input. `compute` receives one `std::span<Rep const>` over the observations made -- as many as were made, not
  the capacity, and empty when none were -- in any `Rep`, `double` included. The library compares no counts: an
  operation over two independent samples takes two counts, and one that pairs its inputs row by row checks its
  counts itself.
- `linear_least_squares(observations<X, C>, observations<Y, C2>, citation)` fits a straight line through raw
  observations, paired row by row, whose number is data. Its outputs are `intercept`, `slope`, `r squared` -- the
  coefficient of determination, dimensionless -- and `points`, the number of observations fitted. Decided before
  any sum, in every representation, and each the fit's own `DomainError`: both inputs hold as many observations, at
  least two, the points are not all equal, and the values are not all equal, so a flat response never passes an R²
  acceptance. Exact through `opaque_output`, where the four outputs answer or all fail with `Overflow` when one
  does not fit a `Rational`; correctly rounded through `rounded_output`, which rounds the kernel's wide result and so
  answers where the exact route overflows -- within the kernel's width, and beyond it `Overflow`; approximately, and
  untraced, through `checked_evaluate_si<double>`. One quantity read as both points and values, or observations without a
  citation, is refused where it is written.
- `multiple_least_squares(regressors(x1, ..., xK), y, citation)` fits y = constant + coefficient 1 x1 + ... +
  coefficient K xK through raw observations paired by row, for K from 1 to 8; K = 1 is the line. Its outputs are
  `constant`, `coefficient 1` to `coefficient K`, `r squared` and `points`, each coefficient in the values'
  dimension over its regressor's. A singular design is the fit's own `DomainError`, never a number: decided exactly
  in `Rational` and by `rounded_output`, and in `double` when a pivot of the centred normal equations is at or below
  10⁻⁹ of its diagonal, so a design within 10⁻⁹ of singular, but not exactly singular, is answered exactly and refused
  in `double`.
  Refused where written: no citation, no regressor, more than eight, anything but raw observations, and one quantity
  read twice. `MultipleLeastSquares<K>`, `Regressors` and `regressors` are the operation and its holder.
- Two sections of *Opaque operations and bounded retry* (`docs/opaque-and-retry.md`): *A line through
  observations* -- the fit rounded where its exact fractions do not fit, R² as an acceptance, and the line in
  `double` and against a temperature in degrees Celsius -- and *Several regressors*, with the designs refused as
  singular. `examples/opaque_and_retry.cpp` fits both, and the census (`docs/numeric-headroom.md`, *Regression
  over observations*) counts the sizes, up to 128 points, at which each route overflows.

### Changed

- An unqualified call of `fraction_text`, `number_text`, `decimal_text`, `exact_decimal_text`,
  `define`, `calculation` or `worksheet` now also finds the library's function by argument-dependent
  lookup, since each takes an argument of a type in namespace `formula`. A consumer's own function
  of the same name that accepts the same arguments -- a `fraction_text(Rational)` helper, say -- now
  makes such a call ambiguous, and has to be renamed, as `examples/statistics.cpp`'s was, or called
  by a qualified name such as `::fraction_text`.
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
- `OpaqueCallInfo` gains `values` after `dimensions`, defaulted to `OpaqueValues::Exact`: code that
  builds one with designated initialisers is unaffected; a structured binding over one now has five
  members, not four.
- `OpaqueStepData` gains `values` and `answer`, after `inputsNotEvaluated`: a structured binding
  over one now has seven members, not five.
- `StepKind` gains `RoundedOpaqueOutput`, `NaturalLogarithm`, `DecimalLogarithm`, `Exponential`,
  `RoundedNaturalLogarithm`, `RoundedDecimalLogarithm` and `RoundedExponential`, appended after
  `AttemptInput` in that order: a `switch` over `StepKind` that names every enumerator and has no
  `default` now misses seven, which g++ 14 reports under `-Wswitch` (part of `-Wall`).
- An unqualified call of `ln`, `log10` or `exp` whose argument is a formula node now finds the library's function
  by argument-dependent lookup; one whose argument is a number still finds only the standard library's function;
  the library's own takes a formula node only. A consumer's own function of one of these names that accepts a
  formula node now makes such a call ambiguous.
- `linear_least_squares` given anything but a curve or two sets of observations says "formula:
  linear_least_squares fits a curve, or points and values read as observations; pair a domain series and a value
  series with curve(domain, values), or read both with observations<Q, Capacity>", which names both ways to call it.
- `LinearLeastSquares::compute<double>`, the curve fit in `double`, refuses one point that is NaN with the fit's own
  `DomainError`, as it refuses any single point; it answered NaN for the intercept and the slope before.
- `OpaqueCallFailure` has a last member, `site` (`FailureSite`, default `FailureSite::ResultElement`), so an
  aggregate initialisation naming the members before it is unchanged; a structured binding over one now has five
  members, not four. Raw observations that fail to convert at observation k relay
  `site == FailureSite::InputObservation`, and the call's trace line says `[carried up from #n, at observation k]`.
  `InputShape` has a fourth enumerator, so a consumer's exhaustive `switch` over it warns under `-Wswitch`.
- `statistics.hpp` includes `observations.hpp` instead of `binning.hpp`, and so no longer brings in
  `binning.hpp`, `band.hpp` or `lookup.hpp`: code that used a binning, a band table or a lookup through
  `statistics.hpp` alone includes `binning.hpp` or `lookup.hpp`.
- An overlay's refusal of a constant or a derivation for a quantity the method reads as a series or as raw
  observations names both: "formula: this overlay fixes a quantity the method reads as a series or as raw
  observations; one constant cannot stand for many values", with "derives" and "one definition" for a
  derivation. It said "reads as a series" and "cannot stand for a series", of raw observations too.

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

[Unreleased]: https://github.com/LASTRADA-Software/formula-cpp/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/LASTRADA-Software/formula-cpp/releases/tag/v0.3.0
[0.2.0]: https://github.com/LASTRADA-Software/formula-cpp/releases/tag/v0.2.0
[0.1.0]: https://github.com/LASTRADA-Software/formula-cpp/releases/tag/v0.1.0
