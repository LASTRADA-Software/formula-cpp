# Statistics, outliers and precision

A test method rarely reports one determination. It takes several, reduces
them to one value -- a mean, a spread -- and often decides first which of them
may enter: an outlier is rejected and the mean taken again. Two
determinations are then checked against each other, against a limit that may
depend on the very level they measure. formula-cpp models each step as part
of the formula, exactly, so that the trace can say what was rejected, in
which pass, and why.

The worked example is `examples/statistics.cpp`. **Program output** on this
page -- the blocks of numbered steps and results -- is copied verbatim from
that program's output, and `docs.statistics-output` fails unless each block
is a run of consecutive lines the program prints, exactly as quoted
(`cmake/CheckGuideOutput.cmake`). **Code** is copied from the example's
source, and `docs.statistics-snippets` fails unless each code block appears
there as a run of consecutive lines, compared without their indentation
(`cmake/CheckGuideSnippets.cmake`). A code block deliberately not from the
example would carry a `<!-- snippet: not from the example -->` comment
directly above it; none on this page does.

Every number, table and citation here is invented. **No critical value on
this page, or anywhere in this library, comes from a published table**: the
library holds none, and reads the author's.

## A sample

A sample is repeated determinations of one quantity. When the method fixes
how many -- "six determinations" -- the sample is a **series**,
`series<Q, N>`, and a series expression is a sample too:
`sample_mean(series<A, 3> / series<B, 3>)` is the mean of the per-element
ratios.

```cpp
inline constexpr auto determinations = formula::series<Mass, 6>;
inline constexpr auto mean = formula::sample_mean(determinations);
inline constexpr auto count = formula::sample_count(determinations);
inline constexpr auto variance = formula::sample_variance(determinations);
inline constexpr auto range = formula::sample_range(determinations);
```

Each statistic is one value, and so an ordinary formula: it can be divided,
rounded, compared and checked like any other. The variance divides by n - 1,
in two passes -- the mean first, then the squared deviations from it.

```text
sample_mean(m(i)) = 413/10 g
sample_count(m(i)) = 6
sample_variance(m(i)) = 427/125 g2
sample_range(m(i)) = 21/5 g
```

When the method does not fix the count -- as many particles as were
measured -- the sample is **raw observations**, `observations<Q, Capacity>`.
`Capacity` is a **bound, not a count**: evaluation allocates nothing, so the
most there can be is part of the type, and how many there are is data.
Every statistic reads the determinations actually made:

```text
observations of 8 at most, 6 made: mean 413/10 g, count 6
```

More than `Capacity` is refused, never truncated to fit: a list written out
fails to compile, naming both counts, and `MeasuredObservations<Q,
Capacity>::from`, for a set known only at run time, returns an
`ObservationsOverCapacity` holding both:

```text
9 observations for 8 places: refused
```

None made is an empty sample: its count is 0, and it has no mean. A trace
counts their positions as observations, where a series' are elements: a mean
that overflows `at observation 3`, a rejection's `rejected observation 4 of 6`.

**Absence is strict.** One determination not made, and every statistic of the
sample is absent -- the count included, which is then neither N nor N - 1 but
unknown. A mean of the determinations that happen to be present is exactly
the wrong number.

```text
1. m = 201/5 g; 199/5 g; (not measured); 44 g; 40 g; 433/10 g
2. sample_mean(#1) = (not measured)
```

## The spread, reported exactly

There is no `sample_stddev`. A standard deviation is a square root, which is
rarely rational, and a method reports it rounded -- so formula-cpp rounds the
square root itself, exactly: `rounded_sqrt` finds the correctly rounded
decimal without ever forming an inexact root.

```cpp
inline constexpr auto spread =
    formula::rounded_sqrt<unit::Gram, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(variance);
```

```text
round(sqrt(sample_variance(m(i))), to 2 dp of g) = 37/20 g
LaTeX: \operatorname{round}_{2\,\mathrm{g}}(\sqrt{s^{2}({m}_{i})})
```

The trace shows the variance as the evaluator holds it, in the coherent unit
(kg², so 427/125 g² is 427/125000000), and the root rounded in the unit the
formula declares:

```text
1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g
2. sample_variance(#1) = 427/125000000
3. round(sqrt(#2), to 2 dp of g) = 37/20 g [nearest, ties away from zero]
```

A method whose own rounding rule differs from `rounded_sqrt`'s granularity
rounds twice: the rule re-rounds an already rounded value, which at the same
granularity changes nothing and at a coarser one can.

## Rejecting outliers

`without_outliers` rejects outliers from a sample and **is a sample** itself:
every statistic takes it as it takes a series. It rejects, then takes the
mean again, and repeats until a pass rejects nothing -- a value inside the
limit at first can be outside it once another has gone.

```cpp
inline constexpr auto sixPercent = formula::deviation_from_mean(rat(6, 100) * formula::pass_mean<Mass>);

inline constexpr auto withoutOutliers =
    formula::without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<2>, formula::KeepAtLeast<4>>(
        determinations, sixPercent, repeatTest, rejectionRule);
```

Every parameter that shapes the result is required, none defaulted:

- `PerPass`: `MostExtreme` rejects the most extreme candidate each pass,
  `EveryExceeding` every one past the limit;
- `OnLimit`: whether a determination exactly on the limit is kept or
  rejected;
- `AtMost<k>`: the most determinations rejected in total;
- `KeepAtLeast<m>`: the fewest that may remain.

**A rejection's sample is a series or observations, never another
rejection.** The outer one would count and place its determinations among
the inner one's survivors, and its trace could not say which it rejected or
what it decided, so `without_outliers` over `without_outliers` is refused
where it is written. State both criteria as one rejection, or evaluate the
first and enter its survivors as the sample.

The limit is an expression, evaluated again in every pass: `pass_mean<Mass>`
is that pass's mean, and `pass_count` its size. Termination is guaranteed by
the type: every pass but the last removes at least one, and at most k are
removed, so at most k + 1 passes run.

```text
without outliers(m(i); abs(x - pass mean) > 3/50 * pass mean; most extreme per pass; keep on limit; at most 2; keep at least 4)
```

Six determinations, 44.0 g rejected in pass 1, 43.3 g -- inside the limit
at first -- in pass 2, and pass 3 settles:

```text
1. m = 201/5 g; 199/5 g; 81/2 g; 44 g; 40 g; 433/10 g
2. 3/50
3. pass mean = 413/10 g
4. #2 * #3 = 1239/500000
5. pass 1: 6 values, mean 413/10 g
6. rejected element 4 of 6 (44 g) in pass 1: abs(x - mean) = 27/10 g > 1239/500 g (deviation from mean)
7. 3/50
8. pass mean = 1019/25 g
9. #7 * #8 = 3057/1250000
10. pass 2: 5 values, mean 1019/25 g
11. rejected element 6 of 6 (433/10 g) in pass 2: abs(x - mean) = 127/50 g > 3057/1250 g (deviation from mean)
12. 3/50
13. pass mean = 321/8 g
14. #12 * #13 = 963/400000
15. pass 3: 4 values, mean 321/8 g
16. settled: 2 rejected, 4 remain
17. sample_mean(#16) = 321/8 g
```

**An abort is the author's verdict.** When the next rejection would pass
`AtMost` or `KeepAtLeast`, nothing more is rejected: the trace records the
author's `Verdict` and its citation, and anything reduced from the rejection
fails -- a mean of whatever happened to survive is not what the method
reports. Allowed one rejection, the same determinations abort in pass 2:

```text
11. element 6 of 6 would be rejection 2 of at most 1: discard the determinations and repeat the test [Outliers, Example Standard 5:2022, 7.4]
12. sample_mean(#11) = argument outside the domain of the operation
```

`checked_evaluate_rejection` returns the whole result instead: the mean of
the survivors or the verdict, the rejected positions and the passes.

**A tie rejects both.** Two candidates equally far from the mean are both
rejected in the same pass, so the result never depends on the order the
determinations were entered in:

```text
a tie: elements 3 and 5 rejected together in pass 1, result 40 g
```

**`KeepAtLeast<m>` is also the method's precondition.** A sample that starts
with fewer than m determinations -- raw observations, whose count is data --
gives the verdict before pass 1, whether or not it holds an outlier. A
series shorter than m, or observations with room for fewer, is refused where
it is written.

```text
1. m = 40 g; 40 g; 41 g
2. 3 values, fewer than the at least 4 to keep: discard the determinations and repeat the test [Outliers, Example Standard 5:2022, 7.4]
```

## Three criteria, and the author's table

A criterion compares each determination's statistic with the pass's limit:

- `deviation_from_mean(limit)`: abs(x - mean) against a limit in the
  sample's own unit -- a constant, or a share of `pass_mean`, as above;
- `deviation_in_stddevs(limit)`: abs(x - mean) / s against a bare number,
  s the pass's sample standard deviation. It needs three determinations in
  every pass -- two always lie one standard deviation either side of their
  mean -- so a rejection by it must declare `KeepAtLeast<3>` or more, and
  fewer is refused where it is written;
- `gap_to_range(limit)`: for the lowest and the highest determination only,
  the gap to its neighbour over the range, against a bare number.

**A deviation in standard deviations is decided exactly, by squares.** s is
a square root, rarely rational; the library compares (x - mean)² with
limit² × s² instead, which is the same decision for a limit of zero or more,
and the trace shows the squares it compared:

```cpp
inline constexpr auto sevenQuarters = formula::deviation_in_stddevs(formula::number(rat(7, 4)));
```

```text
4. rejected element 4 of 6 (226/5 g) in pass 1: (x - mean)^2 = 80089/3600 g2 > limit^2 * s^2 = 198793/9600 g2 (deviation in standard deviations)
```

A negative limit is no rule under any criterion, and is refused in the pass
that evaluated it.

**A critical value by sample size is a limit read from a table.** Methods
that give their critical values in a table by n need no criterion of their
own: the limit expression reads the author's table with `critical_value`, at
`pass_count`, so each pass reads the row for its own size. A size the table
does not declare is a miss, never a neighbouring row. The table below is
invented, and plainly so -- a gap ratio never exceeds 1, and its first two
limits could not be exceeded by any sample:

```cpp
inline constexpr formula::SampleSizeTable<5> declaredSizes { 3, 4, 5, 6, 8 };
inline constexpr auto gapLimit = formula::gap_to_range(
    formula::critical_value<declaredSizes, unit::One>(formula::pass_count,
                                                      { rat(900), rat(700), rat(30), rat(45), rat(5) })
    * rat(1, 100));
```

```text
2. pass n = 6
3. critical(#2) = 45 [critical value at n = 6]
4. 1/100
5. #3 * #4 = 9/20
6. pass 1: 6 values, mean 2429/60 g
7. rejected element 4 of 6 (226/5 g) in pass 1: gap / range = 47/80 > 9/20 (gap to range)
8. pass n = 5
9. critical(#8) = 30 [critical value at n = 5]
```

## Precision

A precision check compares two determinations, or two laboratories' results,
with a limit. It takes one of two forms:

- a limit that is a constant, or looked up by a category the method declares
  -- an ordinary formula, checked in one pass;
- a limit that is a **function of the level of the results it checks**, such
  as r = 0.1 g + level / 50. The level is the results' own value, so the check
  is evaluated in two declared passes: the level first, then the limit at it.

`precision_level<Q>` is a **placeholder** for that level, meaningful only
inside a limit expression; `precision_limit<K>(level, limit)` binds it to the
level expression. The kind -- `Repeatability`, r, or `Reproducibility`, R --
changes the symbol and the trace's words, never the arithmetic.

```cpp
inline constexpr auto limitAtLevel =
    formula::constant<unit::Gram>(rat(1, 10)) + rat(1, 50) * formula::precision_level<FirstResult>;

inline constexpr auto agreement = formula::constraint(
    formula::abs(var<FirstResult> - var<SecondResult>)
        <= formula::precision_limit<formula::PrecisionKind::Repeatability>(pairMean, limitAtLevel),
    formula::Verdict { "repeat the determinations" });
```

```text
require abs(x_A - x_B) <= r(1/10 g + 1/50 * level; level = (x_A + x_B) / 2)
LaTeX: \text{require } \left\lvert x_A - x_B\right\rvert \leq r\left(1/10\,\mathrm{g} + 1/50 \cdot \text{level}\right)\Big\vert_{\text{level} = \frac{x_A + x_B}{2}}
```

40.0 and 40.905 g are 0.905 g apart. At their mean, 40.4525 g, the limit is
0.90905 g, and they agree. The trace names each pass:

```text
10. level (pass 1 of 2) = #9 = 16181/400 g
11. 1/10 g
12. 1/50
13. level = 16181/400 g [bound by #16]
14. #12 * #13 = 16181/20000000
15. #11 + #14 = 18181/20000000
16. r at level #10 (pass 2 of 2) = #15 = 18181/20000000
17. require #4 <= #16 [satisfied]
```

**Which level is part of the method.** Rounded to 1 g first, the level is
40 g, the limit 0.9 g, and the same two determinations no longer agree:

```text
level = the mean: satisfied
level = the mean rounded to 1 g: violated
```

A precision check is a constraint, and joins a method's constraints like any
other; `check_method` checks it:

```text
the method's acceptance check: satisfied
```

In plain text and Markdown, an absolute value is spelt `abs(...)` and a
precision limit `r(...; level = ...)`, because a bare `|` is a Markdown
table's cell delimiter: measured with `mkdocs build`, it ends the cell
mid-formula, and escaped as `\|` it survives but MathJax reads it as a
double bar. LaTeX spells the same bars without one -- `\left\lvert x_A -
x_B\right\rvert`, and `\Big\vert_{\text{level} = ...}` for the level -- so
no rendering in any dialect holds a `|`.

## How much room exact arithmetic has

Statistics of determinations read at fine resolution are where a 64-bit
exact fraction runs out first: a variance of masses read to 1 µg overflows on
about four samples in ten, and a rejection in standard deviations sooner.
The result is then `Overflow`, never a wrong number. See
[Numeric headroom](numeric-headroom.md) for the measurements.

## What is not modelled

- **Another laboratory's record.** Reproducibility compares results from
  other laboratories; here such a result is an ordinary input the author
  supplies. Reading another test's record is a different feature: see
  [Other samples and other tests](records.md).
- **Retrying with a further determination.** "If the two results differ by
  more than r, make a third determination" is a retry, which
  [Opaque operations and bounded retry](opaque-and-retry.md) covers. It
  differs from rejection in kind: rejection changes *which data* enter an
  aggregate; a retry makes *another determination* and computes an
  expression again.
- **Partitioning and best-of-two strategies**, and rolling windows, are
  downstream of this library.
