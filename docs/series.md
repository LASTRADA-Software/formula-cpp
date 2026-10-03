# Series and grading curves

A screen analysis does not measure one mass. It measures the mass retained on
each screen, and the percentage passing each screen is computed from all of
them. formula-cpp calls one quantity at each point of a method's domain a
**series**, `series<Q, N>`. The length `N` is part of the type, because a
method's domain is part of the method, not data: a screen the laboratory did
not use is an **absent element**, never a shorter series.

The worked example is `examples/series.cpp`. The page holds three kinds of
quoted block:
- **Program output**, copied from that program's actual output.
  `docs.series-output` fails unless each ```` ```text ```` block is a run of
  consecutive lines the program prints (`cmake/CheckGuideOutput.cmake`).
- **Code**, copied from the example's source and checked by
  `docs.series-snippets` (`cmake/CheckGuideSnippets.cmake`).
- **Compiler diagnostics**, the blocks opening `static assertion failed`. These
  are the library's refusals as g++ 14 prints them, captured from this
  repository's negative tests.

Every number on this page is invented. Every size has three significant
digits, none of them a preferred number, and none is a sieve size or
designation in any unit. The screens are 103, 127, 163, 197 and 241 m. No
limit resembles a real specification.

## A series is not a single value

The percentage passing each screen is everything not retained on that screen
or on a coarser one:

```cpp
inline constexpr auto passing = formula::yields<Passing>(
    formula::constant<unit::Percent>(100)
    - formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<Retained, 5>) / var<TotalMass>);
```

`yields<Passing>` names the quantity the formula computes, once, where it is
written. Everything that evaluates or traces the formula then takes it as it
is, with no result to repeat, and a formula built on it reuses it through
`.expression`.

A series is its own family of expressions. It is deliberately **not** a
`Node`, the library's name for an expression that yields one value
([Expressions and evaluation](expressions.md)). Every `Node` promises one
value, and `checked_evaluate`, `variant<Tag>` and the scalar operators are all
built on that promise. A series derived from `Node` could stand silently where
a number stands. Kept apart, a series in a single value's place does not
compile. Handed to `checked_evaluate`, `evaluate` or `variant<Tag>`, it is
refused in the library's words:

```
static assertion failed: formula: this expression is a series, not a single value; evaluate it with checked_evaluate_series, or reduce it to one value first (sum, interpolate_at)
```

Anywhere else, the compiler reports only that no function or operator
matches. That covers the operand of `snapped`, `rounded`, `pow` or
`documented`, the point `interpolate_at` reads at, and one side of a
constraint's comparison. The other way round, a single value handed to `sum`,
`cumulative` or `rounded_elementwise` is refused in the library's words.

**The index marker says which is which in the formula itself.** A series
variable is written with `(i)`, and a single value is not:

```text
100 % - cumulative(m_r(i), from last) / m_t
sum(m_r(i))
interpolate(curve(domain(103, 127, 163, 197, 241 m), 100 % - cumulative(m_r(i), from last) / m_t), at 173 m)
```

`m_r(i)` is a series and `m_t` is one value. Only the variable carries the
marker: an operation over a series takes its meaning from its operands, and a
**reduction** is one value again, so it is unmarked. There are two
reductions:
- `sum` adds the elements up;
- `interpolate_at` reads a curve at a point.

A reduction is a `Node`. That means it can be a method's variant, be changed
by a jurisdiction's overlay, and be rendered in a jurisdiction's own
vocabulary of symbols ([Methods and overlays](methods-and-overlays.md)).

In LaTeX the marker is `{m_r}_{i}`, and in Markdown `` `m_r(i)` ``. The symbol
table says it again, and adds how many values there are:

```text
  m_r: series, 5 value(s)
  m_t: single value, 1 value(s)
```

Read at 173 m, 5/17 of the way from 163 to 197 m, the curve gives
27708/425 %, about 65.2 %. The derivation shows every step, and the last one
names the two screens the answer lay between:

```text
1. 103 m; 127 m; 163 m; 197 m; 241 m
2. 100 %
3. m_r = 130 g; 210 g; 95 g; 340 g; 28 g
4. cumulative(#3, from last) = 803 g; 673 g; 463 g; 368 g; 28 g
5. m_t = 1250 g
6. #4 / #5 = 803/1250; 673/1250; 463/1250; 184/625; 14/625
7. #2 - #6 = 447/1250; 577/1250; 787/1250; 441/625; 611/625
8. curve(#1, #7) = 103 m: 447/1250; 127 m: 577/1250; 163 m: 787/1250; 197 m: 441/625; 241 m: 611/625
9. 173 m
10. interpolate(#8, at #9) = 6927/10625 [between 163 and 197 m]
```

A computed step has no declared unit, as a computed single value has none, so
it reads in the **coherent unit**: the SI unit of its dimension, with no prefix
([Expressions and evaluation](expressions.md)), times one of each
[named base dimension](dimensions.md#base-dimensions-the-si-does-not-have) it
carries -- the euro, for an amount in euros. For a percentage that is a plain
fraction, so 447/1250 is 35.76 % and 6927/10625 is 27708/425 %. For a mass it
is the kilogram.

A few computed steps are still in their series' unit, and say so: a running
total, a `sum` and a range read in the unit of the series they add up, and a
series multiplied or divided by a pure number reads in that series' unit, as
does a curve over it and a value read off that curve. Grams times 3/2 are
grams:

```text
1. m_r = 137 g; 213 g; 293 g
2. 3/2
3. #1 * #2 = 411/2 g; 639/2 g; 879/2 g
```

Only a unit that can show the result is used. A sum of Celsius readings is no
reading, nor is their range, a running total or a multiple, and in degrees
Celsius each would be off by the offset, so each reads in the coherent unit,
kelvin, as a difference of two readings does. A unit without a symbol is not
used either, since the value could not say what scale it is on. A mean is a
reading, so it stays in degrees Celsius. Over 23.7, 41.3 and 37.9 °C, the sum
is 922.35 K, the range 17.6 K and the mean 34.3 °C:

```text
the readings: 1. T_r = 237/10 °C; 413/10 °C; 379/10 °C
their sum: 2. sum(#1) = 18447/20 K
their range: 2. sample_range(#1) = 88/5 K
their mean: 2. sample_mean(#1) = 343/10 °C
```

## "Map" is elementwise arithmetic

There is no general `map(series, expression)`. What the library offers is
arithmetic lifted element by element: `+ - * /` and negation, with a single
value **broadcast** to every element, and per-element rounding. Each operation
records **one** trace step for the whole series, and a broadcast value is read
once:

```text
1. 100 %
2. m_r = 130 g; 210 g; 95 g; 340 g; 28 g
3. cumulative(#2, from last) = 803 g; 673 g; 463 g; 368 g; 28 g
4. m_t = 1250 g
5. #3 / #4 = 803/1250; 673/1250; 463/1250; 184/625; 14/625
6. #1 - #5 = 447/1250; 577/1250; 787/1250; 441/625; 611/625
```

That is the cost, and the reason for it. A general map would have to trace
each element's own sub-derivation, and a five-node expression over twenty
screens would then add a hundred steps. The alternative is to trace only the
vectors in and out, and then the total mass it read would appear nowhere, so
nobody could reproduce the number. A per-element lookup or conditional is
therefore not available. It is a recorded follow-up, not a gap to work around.

`render_trace` takes one budget, `maxSteps`, which bounds what a derivation
prints ([Tracing and audit trails](tracing.md)). Each element shown spends one
unit of it, as each line does. A series cut short says exactly how many
elements were left out:

```text
the same, within a budget of 8:
1. 100 %
2. m_r = 130 g; 210 g; 95 g; 340 g; 28 g
3. cumulative(#2, from last) = ... 5 more
... 3 further steps not shown
```

## Absence is decided at the size of what is produced

A screen whose mass was not recorded makes absent exactly what depends on it,
and nothing more. Each row of the table below was run. The label before each
line names the operation and its input. The first four and the sixth use the
analysis with its third screen unrecorded. The splice uses a fine analysis with
its second value unrecorded. The last uses the analysis with its total
unrecorded:

```text
m_r / m_t, third screen unrecorded: 3. #1 / #2 = 13/125; 21/125; (not measured); 34/125; 14/625
round to whole grams, third screen unrecorded: 2. round(#1, to 0/0/0/0/0 dp of g) = 130 g; 210 g; (not measured); 340 g; 28 g [nearest, ties away from zero]
running total from the last, third screen unrecorded: 2. cumulative(#1, from last) = (not measured); (not measured); (not measured); 368 g; 28 g
sum, third screen unrecorded: 2. sum(#1) = (not measured)
the curve read at 173 m, third screen unrecorded: 10. interpolate(#8, at #9) = (not measured)
splice, the fine analysis's second value unrecorded: 7. splice(#3, #6, non-decreasing) = (not measured): (not measured); (not measured): (not measured); (not measured): (not measured); (not measured): (not measured); (not measured): (not measured); (not measured): (not measured)
conformity of the passing, third screen unrecorded: not checked; not checked; not checked; satisfied; satisfied;
m_r / m_t, the total unrecorded: 3. #1 / #2 = (not measured); (not measured); (not measured); (not measured); (not measured)
```

The first row is each mass's share of the total, a plain fraction: 13/125 is
130 g of 1250 g. The rounding row reads in grams, the unit it rounded in. The
conformity row judges the percentage passing, and the running total inside it
leaves the passing at the three finest screens unknown, so all three go
unchecked.

| Operation | An absent element makes... |
|---|---|
| elementwise arithmetic, per-element rounding | that element absent, and no other |
| a running total | that total, and every later one in the running direction, absent |
| `sum`, `interpolate_at`, `splice` | the whole result absent |
| conformity | that element not checked; the others are judged |
| an absent single value broadcast to a series | every element absent |

A `sum` that skipped the absent screen would silently redefine the total as
"the total of what someone happened to enter". `interpolate_at` is strict too:
one absent screen, even one far from the point read, leaves the curve without
an answer. The remedy is to declare the curve without that screen. An absent
answer states no segment and no range, since nothing was located.

## A failed element fails the series, and names itself

An arithmetic error in any element fails the whole series. There is no partial
series, because a series with one wrong element is not a series of right ones.
The failure names its element, counted from one in the trace:

```text
1. m_t = 1250 g
2. m_r = 130 g; 210 g; 95 g; 340 g; 0 g
3. #1 / #2 = division by zero at element 5
```

In the API the position is zero-based, and `SeriesFailure::site` says it
counts an element of the result:

```text
SeriesFailure: division by zero, element 4, counted from zero, a result element: yes
```

There is deliberately no throwing twin of `checked_evaluate_series`: an
exception would have to drop the position.

## Conformity against a limit envelope

`conformity<U>(subject, envelope, verdict)` judges each element against its own
row of an `Envelope<N>`. The verdict is a `Verdict`, the words a failed check
reports ([Constraints and verdicts](constraints.md)). A conformity check is not
a `Node`: it produces verdicts, not a quantity.

```cpp
inline constexpr formula::Envelope<5> gradingEnvelope {
    formula::LimitRow { formula::limit(31), formula::limit(43) },
    formula::LimitRow { formula::limit(47), formula::limit(59) },
    formula::LimitRow { formula::limit(62.96_r), formula::unbounded },
    formula::LimitRow { formula::limit(61), formula::limit(79) },
    formula::LimitRow { formula::limit(83), formula::limit(99) }
};
```

```text
  screen 1: satisfied
  screen 2: violated
  screen 3: satisfied
  screen 4: satisfied
  screen 5: satisfied
```

**A limit envelope is master data.** Its numbers are the product
specification, registered per customer and free to change, not part of any
formula. The library supplies the mechanism: `envelope_from` builds an
envelope at run time, and the trace records each element's value, in the
check's unit, and the row it was judged against:

```text
7. conform(#6) [1 satisfied, 894/25 % (from 31 to 43 %); 2 violated, 1154/25 % (from 47 to 59 %): outside the grading envelope; 3 satisfied, 1574/25 % (at least 1574/25 %); 4 satisfied, 1764/25 % (from 61 to 79 %); 5 satisfied, 2444/25 % (from 83 to 99 %)]
```

**A limit is closed at both ends, where a band is half-open.** A limit states
the least and the greatest *permitted* value, so a value on it is permitted. A
band, the interval a lookup table's row covers
([Lookup tables](lookup-tables.md)), partitions a line, so a boundary value
must belong to exactly one of two neighbouring bands. The third screen's
62.96 % sits exactly on its lower limit, and is satisfied. That is also why a
conformity check is evaluated with exact `Rational` arithmetic only: in
`double`, a value exactly on a limit could come out a few units in the last
place below it.

## Snapping to a permitted value

`snapped<KeyUnit, Permitted, Tie>(value)` is a single-value operation. It
takes any `Node` and replaces its value with the nearest permitted one. A
series is refused, because it has no one value to snap. Snapping is often
applied to an interpolation's result. Here the curve is turned round and read
at 50 % passing, and the opening that gives is snapped to the nearest declared
screen:

```text
snap(interpolate(curve(100 % - cumulative(m_r(i), from last) / m_t, domain(103, 127, 163, 197, 241 m)), at 50 %), to 103, 127, 163, 197, 241 m)
```

```text
10. interpolate(#8, at #9) = 4733/35 m [between 577/1250 and 787/1250]
11. snap(#10) = 127 m [127 m to 163 m; nearer 127 m]
```

The turned-round curve's domain is the computed percentages, so its segment
reads in the coherent unit: 577/1250 and 787/1250 are 46.16 % and 62.96 %.

**The tie rule is required**, with no default. A value exactly midway between
two permitted values is ordinary data, so the author states which way it
goes. 145 m is midway between 127 and 163 m:

```text
2. snap(#1) = 127 m [127 m to 163 m; tie, toward lower]
2. snap(#1) = 163 m [127 m to 163 m; tie, toward higher]
2. snap(#1) = argument outside the domain of the operation [outside the permitted set, 103 m to 241 m]
```

**A value outside the permitted set is a miss**, never the nearest end. The
last line shows 251 m: the method never defined an opening beyond 241 m, and
quietly answering 241 m would be a confident wrong answer.

## Splicing two curves

`splice<Monotone>(a, b)` joins two curves into one: the sorted union of their
points. The direction the values must run in is required. **Argument order
does not matter**:

```text
splice(curve(domain(103, 127, 163 m), values(894/25 %, 1154/25 %, 1574/25 %)), curve(domain(103/10, 137/10, 163/10 m), values(31/10 %, 42/5 %, 71/5 %)), non-decreasing)
7. splice(#3, #6, non-decreasing) = 103/10 m: 31/10 %; 137/10 m: 42/5 %; 163/10 m: 71/5 %; 103 m: 894/25 %; 127 m: 1154/25 %; 163 m: 1574/25 %
splice(curve(domain(103/10, 137/10, 163/10 m), values(31/10 %, 42/5 %, 71/5 %)), curve(domain(103, 127, 163 m), values(894/25 %, 1154/25 %, 1574/25 %)), non-decreasing)
7. splice(#3, #6, non-decreasing) = 103/10 m: 31/10 %; 137/10 m: 42/5 %; 163/10 m: 71/5 %; 103 m: 894/25 %; 127 m: 1154/25 %; 163 m: 1574/25 %
```

The direction is judged on the union, not on each curve. Raise the fine
analysis's last value to 40 %: each curve still rises, but the union falls
where they join, and the failure names the point and the rule:

```text
7. splice(#3, #6, non-decreasing) = argument outside the domain of the operation at element 4 [breaks non-decreasing at 103 m]
```

Two points at one opening are a miss at the second of them,
`[duplicate domain point ...]`, whichever curve came first. **Nothing is
rescaled at the join.** Putting one basis onto another is the method's own
algebra, written with elementwise arithmetic before the splice, where the
trace shows it.

## Binning raw observations

Particles measured one by one are not a series: how many there are is data.
`MeasuredObservations<Q, Capacity>` holds as many as were made, up to a stated
capacity. More are refused, at compile time for a list written out and by
`from` at run time, which returns an `ObservationsOverCapacity` holding both
counts. Nothing is ever truncated.
`binned<KeyUnit, Classes>(observations<Q, Capacity>)` counts them into the
half-open classes of a `BandTable`, the table of bands a banded lookup uses
([Lookup tables](lookup-tables.md)). The counts are a series:

```text
bin(s(i), 0 to under 127 m, 127 to under 197 m, 197 to under 331 m) / sum(bin(s(i), 0 to under 127 m, 127 to under 197 m, 197 to under 331 m))
```

```text
1. s = 103 m; 127 m; 163 m; 277 m; 113 m; 197 m; 241 m
2. bin(#1) = 2; 2; 3
```

127 and 197 m sit exactly on boundaries and are counted in the upper class.
**An observation in no class is a miss, never dropped.** 331 m is the last
class's high bound, so it falls in none:

```text
1. s = 103 m; 127 m; 163 m; 331 m; 113 m; 197 m; 241 m
2. bin(#1) = argument outside the domain of the operation at observation 4 [331 m in no class; the classes cover 0 to under 331 m]
```

The position is the **observation's**, counted from one. In the API it is
zero-based, and `SeriesFailure::site` says `FailureSite::InputObservation`, so
a consumer cannot take it for the fourth count. An operation over the counts,
such as the shares above, relays the failure without it, because it would name
a count that is not at fault. Such a step names the operand it evaluated, and
says what stands in the other's place: in the gallery's `#2 / (not
evaluated)`, the divisor was never reached. Binning a series instead of
observations is refused:

```
static assertion failed: formula: binned counts raw observations into classes, and this is not a set of observations; the operand appears in this diagnostic as the template argument of RequireBinnedOfObservations -- read them with observations<Q, Capacity> and supply them with MeasuredObservations; a series already holds one value at each point of a domain, and a single value is one observation, not a set of them
```

## Limits, stated plainly

- **The limit envelope is master data**, as above. No example or test here
  carries a limit that resembles a real specification.
- **The marker is appended, not parsed.** A symbol that already ends in `)`,
  such as `w(t)`, reads `w(t)(i)`. A LaTeX symbol that is not a balanced TeX
  group breaks the braces of `{...}_{i}`.
- **A one-element series reads like a single value in the trace.** Its step
  lists one value, as a single value's does. The formula's marker and the
  symbol table still tell the two apart.
- **Nothing catches a split across translation units in your program.** Two
  translation units that build a series expression differently, or define a
  quantity differently (its symbol or its unit), link without complaint: that
  is an ODR violation, which no linker is required to diagnose. This
  repository's own cross-unit test (`test/series_cross_tu_b.cpp`) catches a
  split in node structure only, because its declarations name the formula's
  types. A split in values, or in a quantity's definition, is caught by
  nothing.
- Everything that compares -- snapping, conformity, splicing,
  `interpolate_at`, binning -- is evaluated with `Rep = Rational` only, and
  refused otherwise, because a comparison a few units in the last place off
  picks the wrong answer silently.
