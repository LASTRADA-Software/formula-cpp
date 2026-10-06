# 8. Tracing

This chapter shows how a number was reached: the strength together with
every step that calculated it, each input in the unit it was entered in, and
the citation on the step it describes. A strength entered by hand has no
such steps, and the trace says so by being empty.

The program is chapter 7's without the rendering and the documentation, with
the strength explained rather than evaluated, for the specimen of chapter 3:
150.2 mm by 149.8 mm, carrying 675.4 kN.

## How a number was reached

`formula::checked_explain<Strength>(formula, environment)` evaluates the
formula as `formula::checked_evaluate` does, and records each step on the
way:

```cpp
--8<-- "examples/tutorial/08_tracing.cpp:explain"
```

It returns a `std::expected`. On success it holds a
`formula::Explained<Strength>`: its `outcome` is exactly what
`checked_evaluate` would have returned, and its `trace` is a `formula::Trace`
with one step per node of the formula, each naming the steps it consumed.
On an arithmetic error it holds the error and the trace recorded up to it,
the failing step included, so a failure can be shown together with the steps
that led to it.

`formula::render_trace(trace, options)` writes a trace as one numbered line
per step. `options.maxSteps` is the most lines it writes; it has no default,
and a longer trace ends with one line saying how many steps were left out.

## Reading a trace

The trace is the numbered lines of the [output](#output). Its steps are
numbered in the order they finished, so an operand always appears above the step that
reads it: the load, the two sides, their product, the quotient, the rounding,
and the citation.

Each input reads in its declared unit, as it was entered: `3377/5 kN` is
675.4 kN, and `751/5 mm` is 150.2 mm. A trace writes every number as an exact
fraction unless its options ask for another style.

A step that calculates, such as `#2 * #3`, has no declared unit of its own.
Here it reads in the coherent SI unit of its dimension, spelt from the base
units: the product is 0.02249996 m^2, that is 22499.96 mm2, and the quotient
`kg/(m s^2)` is the pascal, about 30.02 MPa. The rounding step names its
rule and its mode, and gives 30 MPa in the megapascal it rounds in.

The citation appears on its own step, step 7, `#6 = 30 MPa`, the one
`formula::documented` added, and not on the division it wraps.

## A value entered by hand has no derivation

A strength entered by hand is returned as it was entered, without evaluating
the formula, so nothing is recorded:

```cpp
--8<-- "examples/tutorial/08_tracing.cpp:entered"
```

`explained->trace` is empty, and `explained->outcome.is_overridden()` is
true: the number was not derived, so there is no derivation to show.

Tracing costs nothing when it is not asked for: `checked_evaluate` without a
sink builds no trace and allocates nothing for one.

## Output

```text
--8<-- "examples/tutorial/08_tracing.expected.txt"
```

## Summary

- `formula::checked_explain<Q>(formula, environment)` -- evaluates the
  formula and records each step; on an arithmetic error, the error and the
  steps up to it.
- `formula::Explained<Q>` -- what `checked_explain` returns on success: the
  `outcome` `checked_evaluate` would have returned, and its `trace`.
- `formula::Trace` -- the recorded steps, one per node, each naming the steps
  it consumed; empty for a value entered by hand.
- `formula::render_trace(trace, options)` -- the trace as one numbered line
  per step.
- `formula::TraceRenderOptions::maxSteps` -- the most steps `render_trace`
  writes; it has no default.

## Further reading

- [Two ways to evaluate, and when to reach for each](../tracing.md#two-ways-to-evaluate-and-when-to-reach-for-each)
- [Reading a derivation](../tracing.md#reading-a-derivation)
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
