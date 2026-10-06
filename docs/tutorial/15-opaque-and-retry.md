# 15. Opaque operations and bounded retry

Two things a strength test method states are not a formula over its inputs.
It may name a computation without spelling it out, such as the stiffness of
the loading as the slope of a straight line fitted by least squares through
the load and displacement readings. And it may repeat a step until it is
accepted, a bounded number of times, such as a retest repeated until two
results agree. This chapter fits that line and runs that retest, and shows
what the trace says about each.

The program is self-contained: it declares its own quantities and does not
build on the previous chapters' programs.

## A named operation whose inside is not traced

The readings are a displacement in mm and a load in kN. The fit's outputs
are quantities of their own: the stiffness, the slope of the line, in kN/mm;
the load at zero displacement, its intercept, in kN; and the coefficient of
determination, R², a pure number. The library has no kN/mm, so the program
declares it, as one kN/mm is 1000000 N/m:

```cpp
--8<-- "examples/tutorial/15_opaque_and_retry.cpp:quantities"
```

`formula::linear_least_squares` is an **opaque operation**: the call names it,
a citation and its inputs, and the trace shows its inputs and outputs but not
the sums inside. Its inputs here are `formula::observations<Q, Capacity>`, as
many readings as were made, up to the capacity, so how many points there are
is data. `formula::opaque_output<"name">` chooses one output, `intercept`,
`slope`, `r squared` or `points`, and a formula uses it like any other value:

```cpp
--8<-- "examples/tutorial/15_opaque_and_retry.cpp:fit"
```

The citation has no default: the reference that defines the operation is
what a reader has instead of its inside.

Four invented readings lie exactly on the line load = 100 kN/mm ×
displacement + 5 kN: (0.1 mm, 15 kN), (0.2 mm, 25 kN), (0.3 mm, 35 kN) and
(0.4 mm, 45 kN). The program renders the stiffness, explains it, and prints
it:

```cpp
--8<-- "examples/tutorial/15_opaque_and_retry.cpp:evaluate-fit"
```

Lines 1 and 2 of the trace are the readings, exact, so 0.1 mm is 1/10 mm.
Line 3 is the operation: every output it produced, then `[inside not shown]`,
then its citation. The library writes `[inside not shown]` for every opaque
call, rather than leave a reader to wonder whether a step is missing. Line 4
chooses the slope. The fit is exact: the means are 0.25 mm and 30 kN, the
sum of the squared displacements about their mean is 0.05 mm², and the sum of
the products about the means 5 kN·mm, so the slope is 5/0.05 = 100 kN/mm and
the intercept 30 kN less 100 kN/mm × 0.25 mm, 5 kN. Every point lies on the
line, so R² is 1.

The other two outputs are evaluated the same way:

```cpp
--8<-- "examples/tutorial/15_opaque_and_retry.cpp:other-outputs"
```

Each output is its own value, so each evaluation runs the fit again; the
operation is pure, so every run gives the same line.

## A step repeated a bounded number of times

An invented rule retests a specimen at most 3 times, until two consecutive
results differ by at most 0.5 MPa; if none do, the method's verdict is "test
further specimens". `formula::retry<R, Max, FirstJudged>(attempt, acceptance,
verdict, citation)` states it:

```cpp
--8<-- "examples/tutorial/15_opaque_and_retry.cpp:retry"
```

- `formula::attempt_input<Retest>` is the attempt: at attempt `k`, the `k`th
  element of a series of retest results, one per attempt allowed.
- `formula::this_attempt<AgreedStrength>` is the value just produced, and
  `formula::previous_attempt<AgreedStrength>` the one before; the acceptance
  compares the two with `formula::abs`.
- `3` is the most attempts: a template argument, so no run makes more.
- `formula::FirstJudged::AtSecondAttempt` judges from the second attempt,
  since at the first there is only one result to compare.

The results are 30.0, 31.2 and 31.0 MPa. `formula::explain_retry` runs the
retry and records how; its `outcome` is a `std::expected`, the outcome or the
arithmetic failure that stopped it, and is checked before it is read:

```cpp
--8<-- "examples/tutorial/15_opaque_and_retry.cpp:evaluate-retry"
```

The render writes 0.5 MPa as the exact 1/2 MPa. Attempt 1 is 30 MPa and is
not judged. Attempt 2, 31.2 MPa, differs from it by 1.2 MPa, more than
0.5 MPa, and is rejected. Attempt 3, 31.0 MPa, differs from 31.2 MPa by
0.2 MPa, and is accepted: the retry's value is 31 MPa.

## Ending in exactly one of a fixed set of ways

A retry ends in exactly one of the six ways `formula::RetryEnd` names.
`outcome->end()` reports five of them; a failed retry has no outcome, only
the failure:

- **accepted:** the acceptance held; the value is that attempt's, as here.
- **exhausted:** the acceptance never held in the attempts allowed; the
  outcome is the verdict, "test further specimens", not the last result.
- **not judgeable:** an attempt's value, or its judgement, was absent.
- **not recorded:** a retest result an attempt needed was not recorded.
- **failed:** an attempt failed arithmetically; there is no outcome, only
  the failure, naming the attempt.
- **manually entered:** a person typed in the result; no attempt runs.

Had the third result been 30.4 MPa, 0.8 MPa from the second, the retry would
have ended exhausted, with the verdict as its outcome.

## Output

```text
--8<-- "examples/tutorial/15_opaque_and_retry.expected.txt"
```

## Summary

- `formula::linear_least_squares(x, y, citation)` -- a straight line fitted
  by least squares, an opaque operation with the outputs `intercept`,
  `slope`, `r squared` and `points`.
- `formula::opaque_output<"name">(call)` -- one output of an opaque
  operation, used like any other value.
- `formula::observations<Q, Capacity>` -- as many readings of `Q` as were
  made, up to `Capacity`.
- `formula::retry<R, Max, FirstJudged>(attempt, acceptance, verdict, citation)`
  -- a step repeated at most `Max` times until `acceptance` holds.
- `formula::explain_retry(retry, environment)` -- runs a retry and records
  how; `outcome->end()` says how it ended.

## Further reading

- [An opaque operation](../opaque-and-retry.md#an-opaque-operation)
- [A line through observations](../opaque-and-retry.md#a-line-through-observations)
- [A retry](../opaque-and-retry.md#a-retry)
- [Six ways to end](../opaque-and-retry.md#six-ways-to-end)
- [Opaque operations and bounded retry](../opaque-and-retry.md), which also covers operations of your own, rounded outputs and several regressors
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
