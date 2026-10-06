# 12. Series

A strength test is not always one specimen crushed once. One mix is often
tested at several ages, 7, 14 and 28 days, and the method then states the
same quantity at each age. This chapter declares the strengths at the three
ages as one series, calculates each as a share of the 28-day strength
element by element, and shows what happens when one age was not recorded.

The program is self-contained: it declares its own quantities and does not
build on the previous chapters' programs.

## One quantity at several points

The strength at each age is one quantity, the compressive strength, with a
value at each of three points. The shares are taken of the 28-day strength,
the series' third element, but a formula cannot read one element of a
series. So the 28-day strength is also entered on its own, as `f_c28`, and
must be the same value as the series' third element. Nothing checks the two
against each other, so keep them in step. The share is a pure number:

```cpp
--8<-- "examples/tutorial/12_series.cpp:quantities"
```

`formula::series<Q, N>` is the quantity `Q` at `N` points. The length is part
of the type, because the ages are part of the method, not data: an age the
laboratory did not record is an absent element, never a shorter series.
A series is not a single value, and a series in a single value's place does
not compile.

The share of each age's strength in the 28-day strength divides the series
by the single value:

```cpp
--8<-- "examples/tutorial/12_series.cpp:share"
```

Rendered, the series variable is marked with `(i)`, and the single value is
not:

```cpp
--8<-- "examples/tutorial/12_series.cpp:render"
```

## Arithmetic element by element

Arithmetic on a series is applied to each element: here each of the three
strengths is divided by the 28-day strength, which is read once and used for
every element. `formula::explain_series` evaluates a series and records how,
as `formula::checked_explain` does for a single value. Its `outcome` is a
`std::expected`: the series, or a `formula::SeriesFailure` that names the
element that failed when the failure belongs to one.
`outcome->element(at)` is one element, at a zero-based position:

```cpp
--8<-- "examples/tutorial/12_series.cpp:report"
```

The strengths 21, 26 and 30 MPa are measured at the three ages, and the
28-day strength is 30 MPa:

```cpp
--8<-- "examples/tutorial/12_series.cpp:evaluate"
```

The trace records one step for the whole division, with every element's
result: 21/30 is 7/10, 26/30 is 13/15, and 30/30 is 1. The shares are pure
numbers, so the trace shows them without a unit. Rounded for reading, 7/10
is the exact decimal 0.7 and 1 is 1; 13/15 has no exact decimal, so it is
rounded to three places and marked: ≈0.867.

## An absent element stays absent

`formula::not_measured` in place of a value marks one element absent: here
the 14-day strength was not recorded:

```cpp
--8<-- "examples/tutorial/12_series.cpp:absent"
```

The share at 14 days is absent too, and printed `(not measured)`. The shares
at 7 and 28 days are still calculated, and are the same as before. An absent
element makes absent exactly the elements that depend on it, and the
division of one element depends on no other. A step that combines the
elements into one value, such as their sum, is absent as a whole when any
element is: a total of only the elements someone entered is not the total
the method states.

## Output

```text
--8<-- "examples/tutorial/12_series.expected.txt"
```

## Summary

- `formula::series<Q, N>` -- the quantity `Q` at `N` points, written `(i)`
  when rendered.
- `formula::measured_series<Q>(values...)` -- the measured values of a
  series, one per point; `formula::not_measured` marks one absent.
- `formula::explain_series(formula, environment)` -- evaluates a series and
  records how; `outcome->element(at)` is one element.

## Further reading

- ["Map" is elementwise arithmetic](../series.md#map-is-elementwise-arithmetic)
- [Absence is decided at the size of what is produced](../series.md#absence-is-decided-at-the-size-of-what-is-produced)
- [Series and grading curves](../series.md), which also covers sums, curves and conformity
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
