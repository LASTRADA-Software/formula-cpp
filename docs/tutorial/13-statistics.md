# 13. Statistics

A strength test usually crushes several specimens of one mix and reports one
value for them: their mean, often with their spread, and only after a
specimen far from the others has been rejected. This chapter reduces the
strengths of three specimens to a mean and a range, then rejects an outlier
from five specimens and takes the mean of those kept. The trace says which
specimen was rejected, in which pass, and why.

The program is self-contained: it declares its own quantities and does not
build on the previous chapters' programs.

## Summary statistics over a sample

The strengths of the specimens are one quantity at several points, a series
([chapter 12](12-series.md)). Their range is a quantity of its own, in the
same unit:

```cpp
--8<-- "examples/tutorial/13_statistics.cpp:quantities"
```

A statistic reduces a series to a single value, so it is an ordinary formula:
`formula::sample_mean` is the mean of the elements, and `formula::sample_range`
the highest less the lowest:

```cpp
--8<-- "examples/tutorial/13_statistics.cpp:statistics"
```

The library also has `formula::sample_count` and `formula::sample_variance`.
A mean is taken only when every element is present: one specimen not
measured, and the mean is absent, never the mean of the rest.

The three specimens have strengths of 30.2, 29.8 and 31.0 MPa:

```cpp
--8<-- "examples/tutorial/13_statistics.cpp:evaluate"
```

The three add up to 91 MPa, so the mean is 91/3 MPa exactly. It has no exact
decimal, so it prints as a fraction, and `{:~.2HalfEven}` rounds it to two
places for reading: ≈30.33 MPa. The range is 31.0 less 29.8, 1.2 MPa.

## Outliers rejected pass by pass

Five specimens have strengths of 30.2, 29.8, 31.0, 30.4 and 36.0 MPa. An
invented rule rejects a specimen more than 3 MPa from the mean, one specimen
per pass. Some methods measure from the mean of the others; this one
measures from the mean of each pass, the strength itself included. With
these five strengths, both rules reject 36.0 MPa and nothing else.

`formula::deviation_from_mean(limit)` is that criterion, the limit here a
constant 3 MPa. `formula::without_outliers` applies it, pass by pass, and is
itself a sample that a statistic takes as it takes a series:

```cpp
--8<-- "examples/tutorial/13_statistics.cpp:rejection"
```

Every template argument shapes the result, so none has a default:

- `formula::PerPass::MostExtreme` rejects only the specimen furthest from the
  mean in each pass, and `formula::PerPass::EveryExceeding` every one past
  the limit;
- `formula::OnLimit::Keep` keeps a specimen exactly on the limit, and
  `formula::OnLimit::Reject` rejects it;
- `formula::AtMost<2>` rejects at most two specimens in all;
- `formula::KeepAtLeast<3>` keeps at least three.

When a rejection would break `AtMost` or `KeepAtLeast`, nothing more is
rejected and the result is the verdict, here "test further specimens", not
a mean of whatever was left.

The program prints the rejection, then the trace of the mean of the
specimens kept:

```cpp
--8<-- "examples/tutorial/13_statistics.cpp:reject"
```

The limit is evaluated again in every pass, so the trace states it at the
start of each: lines 2 and 5, 3 MPa both times, since it is a constant.
In pass 1, the mean of the five is 787/25 MPa, 31.48 MPa. 36.0 MPa is
113/25 MPa, 4.52 MPa, from it, more than 3 MPa, and the furthest of the
five: the trace says it rejected element 5 of 5 in pass 1, and why. In
pass 2, the mean of the four left is 607/20 MPa, 30.35 MPa, and none of them
is more than 3 MPa from it, so the rejection settles with one rejected and
four kept. The mean of the specimens kept is 30.35 MPa.

## Output

```text
--8<-- "examples/tutorial/13_statistics.expected.txt"
```

## Summary

- `formula::sample_mean(sample)` -- the mean of the sample's values.
- `formula::sample_range(sample)` -- the highest value less the lowest.
- `formula::deviation_from_mean(limit)` -- rejects a value further than
  `limit` from the mean of its pass.
- `formula::without_outliers<PerPass, OnLimit, AtMost, KeepAtLeast>(sample, criterion, verdict)`
  -- the sample with its outliers rejected, pass by pass, until a pass
  rejects nothing, or until a rejection would break `AtMost` or
  `KeepAtLeast` and the verdict is given.

## Further reading

- [A sample](../statistics.md#a-sample)
- [Rejecting outliers](../statistics.md#rejecting-outliers)
- [Statistics, outliers and precision](../statistics.md), which also covers the spread, other criteria and precision checks
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
