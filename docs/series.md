# Series and grading curves

A screen analysis does not measure one mass. It measures the mass retained on
each screen, and the percentage passing each screen is computed from all of
them. formula-cpp calls one quantity at each point of a method's domain a
**series**, `series<Q, N>`. The length `N` is part of the type, because a
method's domain is part of the method, not data: a screen the laboratory did
not use is an **absent element**, never a shorter series.

The worked example is `examples/series.cpp`. As on every guide page, **program
output** is copied from that program's actual output, and
`docs.series-output` fails unless each ```` ```text ```` block is a run of
consecutive lines the program prints (`cmake/CheckGuideOutput.cmake`).
**Code** is copied from the example's source, checked by `docs.series-snippets`
(`cmake/CheckGuideSnippets.cmake`). **Compiler diagnostics**, the blocks opening
`static assertion failed`, are the library's refusals as g++ 14 prints them,
captured from this repository's negative tests.

Every number on this page is invented. The screens are 11, 29, 41, 59 and 83 m,
which are primes and not a sieve size in any unit, and no limit resembles a
real specification.

## A series is not a single value

The percentage passing each screen is everything not retained on that screen
or on a coarser one:

```cpp
inline constexpr auto passing =
    formula::constant<unit::Percent>(rat(100))
    - formula::cumulative<formula::CumulativeDirection::FromLast>(formula::series<Retained, 5>) / var<TotalMass>;
```

A series is its own family of expressions. It is deliberately **not** a
`Node`. Every `Node` promises one value, and `checked_evaluate`, `variant<Tag>`
and the scalar operators are all built on that promise. A series derived from
`Node` could stand silently where a number stands. Kept apart, a series in a
single value's place is refused, in the library's words:

```
static assertion failed: formula: this expression is a series, not a single value; evaluate it with checked_evaluate_series, or reduce it to one value first (sum, interpolate_at)
```

**The index marker says which is which in the formula itself.** A series
variable is written with `(i)`, and a single value is not:

```text
100 % - cumulative(m_r(i), from last) / m_t
sum(m_r(i))
interpolate(curve(domain(11, 29, 41, 59, 83 m), 100 % - cumulative(m_r(i), from last) / m_t), at 47 m)
```

`m_r(i)` is a series and `m_t` is one value. Only the variable carries the
marker: an operation over a series takes its meaning from its operands, and a
**reduction** is one value again, so it is unmarked. There are three
reductions:
- `sum` adds the elements up;
- `interpolate_at` reads a curve at a point;
- `snapped`, applied to what either of those produced, replaces the value with
  the nearest permitted one (see below).

A reduction is a `Node`, so it can be a method's variant, overlaid and
rendered in a jurisdiction's vocabulary.

In LaTeX the marker is `{m_r}_{i}`, and in Markdown `` `m_r(i)` ``. The symbol
table says it again, and adds how many values there are:

```text
  m_r: series, 5 value(s)
  m_t: single value, 1 value(s)
```

Read at 47 m, a third of the way from 41 to 59 m, the curve gives 4912/75 %.
The derivation shows every step, and the last one names the two screens the
answer lay between:

```text
1. 11 m; 29 m; 41 m; 59 m; 83 m
2. 100 %
3. m_r = 130 g; 210 g; 95 g; 340 g; 28 g
4. cumulative(#3, from last) = 803 g; 673 g; 463 g; 368 g; 28 g
5. m_t = 1250 g
6. #4 / #5 = 803/1250; 673/1250; 463/1250; 184/625; 14/625
7. #2 - #6 = 447/1250; 577/1250; 787/1250; 441/625; 611/625
8. curve(#1, #7) = 11 m: 447/1250; 29 m: 577/1250; 41 m: 787/1250; 59 m: 441/625; 83 m: 611/625
9. 47 m
10. interpolate(#8, at #9) = 1228/1875 [between 41 and 59 m]
```

A computed step has no declared unit, as a computed single value has none, so
it reads in the coherent one: 447/1250 is 35.76 %, and 1228/1875 is 4912/75 %.

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
nobody could reproduce the number. A per-element lookup or conditional
is therefore not available. It is a recorded follow-up, not a gap to work
around.

Each element shown spends one unit of the trace's one budget, `maxSteps`, as
each line does. A series cut short says exactly how many elements were left
out:

```text
the same, within a budget of 8:
1. 100 %
2. m_r = 130 g; 210 g; 95 g; 340 g; 28 g
3. cumulative(#2, from last) = ... 5 more
... 3 further steps not shown
```

## Absence is decided at the size of what is produced

A screen whose mass was not recorded makes absent exactly what depends on it,
and nothing more. Run on the analysis with the third screen unrecorded, and
with the total unrecorded:

```text
elementwise: 3. #1 * #2 = 13/50; 21/50; (not measured); 17/25; 7/125
running total: 2. cumulative(#1, from last) = (not measured); (not measured); (not measured); 368 g; 28 g
sum: 2. sum(#1) = (not measured)
absent scalar: 3. #1 / #2 = (not measured); (not measured); (not measured); (not measured); (not measured)
conformity: satisfied; satisfied; not checked; satisfied; satisfied;
```

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
an answer. The remedy is to declare the curve without that screen.

## A failed element fails the series, and names itself

An arithmetic error in any element fails the whole series. There is no partial
series, because a series with one wrong element is not a series of right ones.
The failure names its element, counted from one in the trace:

```text
1. m_t = 1250 g
2. m_r = 130 g; 210 g; 95 g; 340 g; 0 g
3. #1 / #2 = division by zero at element 5
```

In the API, the position is zero-based: `SeriesFailure { DivisionByZero, 4 }`.
`checked_evaluate_series` has no throwing twin, because a thrown exception
could not carry the position.

## Conformity against a limit envelope

`conformity<U>(subject, envelope, verdict)` judges each element against its own
row of an `Envelope<N>`. It is not a `Node`: it produces verdicts, not a
quantity.

```cpp
inline constexpr formula::Envelope<5> gradingEnvelope {
    formula::LimitRow { formula::limit(rat(30)), formula::limit(rat(40)) },
    formula::LimitRow { formula::limit(rat(50)), formula::limit(rat(60)) },
    formula::LimitRow { formula::limit(rat(1574, 25)), formula::unbounded },
    formula::LimitRow { formula::limit(rat(65)), formula::limit(rat(75)) },
    formula::LimitRow { formula::limit(rat(90)), formula::limit(rat(100)) }
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
formula. The library supplies the mechanism, `envelope_from` builds one at
run time, and the trace records the rows each element was judged against:

```text
7. conform(#6) [1 satisfied (from 30 to 40 %); 2 violated (from 50 to 60 %): outside the grading envelope; 3 satisfied (at least 1574/25 %); 4 satisfied (from 65 to 75 %); 5 satisfied (from 90 to 100 %)]
```

**A limit is closed at both ends, where a band is half-open.** A limit states
the least and the greatest *permitted* value, so a value on it is permitted.
A band partitions a line into intervals, so a boundary value must belong to
exactly one of two neighbouring bands. The third screen's 62.96 % sits exactly
on its lower limit, and is satisfied.

## Snapping to a permitted value

`snapped<KeyUnit, Permitted, SnapTie>(value)` replaces a computed value with
the nearest permitted one. The opening at which half the sample passes, read
off the curve turned round and snapped to the nearest declared screen:

```text
snap(interpolate(curve(100 % - cumulative(m_r(i), from last) / m_t, domain(11, 29, 41, 59, 83 m)), at 50 %), to 11, 29, 41, 59, 83 m)
```

```text
10. interpolate(#8, at #9) = 1111/35 m [between 577/1250 and 787/1250]
11. snap(#10) = 29 m [29 m to 41 m; nearer 29 m]
```

**The tie rule is required**, with no default. A value exactly midway between
two permitted values is ordinary data, so the author states which way it
goes. 50 m is midway between 41 and 59 m:

```text
2. snap(#1) = 41 m [41 m to 59 m; tie, toward lower]
2. snap(#1) = 59 m [41 m to 59 m; tie, toward higher]
2. snap(#1) = argument outside the domain of the operation [outside the permitted set, 11 m to 83 m]
```

**A value outside the permitted set is a miss**, never the nearest end. The
last line shows 84 m: the method never defined an opening beyond 83 m, and
quietly answering 83 m would be a confident wrong answer.

## Splicing two curves

`splice<Monotone>(a, b)` joins two curves into one: the sorted union of their
points. The direction the values must run in is required. **Argument order
does not matter**:

```text
splice(curve(domain(11, 29, 41 m), values(894/25 %, 1154/25 %, 1574/25 %)), curve(domain(1/3, 5/3, 13/3 m), values(31/10 %, 42/5 %, 71/5 %)), non-decreasing)
7. splice(#3, #6, non-decreasing) = 1/3 m: 31/10 %; 5/3 m: 42/5 %; 13/3 m: 71/5 %; 11 m: 894/25 %; 29 m: 1154/25 %; 41 m: 1574/25 %
splice(curve(domain(1/3, 5/3, 13/3 m), values(31/10 %, 42/5 %, 71/5 %)), curve(domain(11, 29, 41 m), values(894/25 %, 1154/25 %, 1574/25 %)), non-decreasing)
7. splice(#3, #6, non-decreasing) = 1/3 m: 31/10 %; 5/3 m: 42/5 %; 13/3 m: 71/5 %; 11 m: 894/25 %; 29 m: 1154/25 %; 41 m: 1574/25 %
```

The direction is judged on the union, not on each curve. Raise the fine
analysis's last value to 40 %: each curve still rises, but the union falls
where they join, and the failure names the point and the rule:

```text
7. splice(#3, #6, non-decreasing) = argument outside the domain of the operation at element 4 [breaks non-decreasing at 11 m]
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
`from` at run time, and nothing is ever truncated.
`binned<KeyUnit, Classes>(observations<Q, Capacity>)` counts them into the
half-open classes of a `BandTable`, and the counts are a series:

```text
bin(s(i), 0 to under 11 m, 11 to under 29 m, 29 to under 83 m) / sum(bin(s(i), 0 to under 11 m, 11 to under 29 m, 29 to under 83 m))
```

```text
1. s = 4 m; 11 m; 17 m; 79 m; 10 m; 29 m; 47 m
2. bin(#1) = 2; 2; 3
```

11 and 29 m sit exactly on boundaries and are counted in the upper class.
**An observation in no class is a miss, never dropped.** 83 m is the last
class's high bound, so it falls in none:

```text
1. s = 4 m; 11 m; 17 m; 83 m; 10 m; 29 m; 47 m
2. bin(#1) = argument outside the domain of the operation at observation 4 [83 m in no class; the classes cover 0 to under 83 m]
```

The position is the **observation's**, counted from one. An operation over
the counts, such as the shares above, relays the failure without it, because
it would name a count that is not at fault. Binning a series instead of
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
- **A split in values cannot be caught across translation units.** Two
  translation units that build the same series expression type with different
  runtime values link without complaint. Only a split in node structure fails
  the link.
- Everything that compares -- snapping, conformity, splicing,
  `interpolate_at`, binning -- is evaluated with `Rep = Rational` only, and
  refused otherwise, because a comparison a few units in the last place off
  picks the wrong answer silently.
