# Numeric headroom

`formula-cpp` computes exactly: every value is a `Rational`, a fraction whose
numerator and denominator are 64-bit signed integers. A result that does not
fit is never wrapped or rounded away; it is refused as
`ArithmeticError::Overflow`. This page answers one question with
measurements: **how much of those 64 bits do real formulas use, and is that
enough?**

**Every figure in the tables below is generated.** Each table is what the
census programs print, rebuilt by `cmake/CheckCensusPage.cmake`, and the
test `docs.numeric-headroom` fails when this page differs from a fresh run.
Regenerate it by building the target `formula-cpp-census-page`; never edit
a table by hand.

## The answer

**Not for every realistic case.** Most formulas leave a wide margin, 30 bits
or more. Four kinds of realistic formula do not, and the tables below give
their figures:

- the **sample variance** of masses near 40 g read to 5 or 6 decimal places
  of a gram, which at 6 places overflows on a large share of samples;
- **rejecting outliers by standard deviations** from such a sample, which
  forms limit² × s² on top and runs out sooner;
- a **cylinder's compressive strength**, 4F / (π d²), which overflows at many
  ordinary diameters -- 139 mm among them, while 135 mm fits, with only 5
  bits to spare in the methods example;
- a **least-squares line** through readings at 3 decimal places, which
  overflows from 34 points, though not at every size above.

The project's decision rule is: **any realistic case under 8 bits of headroom
recommends wider intermediates**: 128-bit intermediate arithmetic, computing
each product and sum in 128 bits before reducing. These cases are under it,
so the census recommends 128-bit intermediate arithmetic.

**Whether 128-bit intermediate arithmetic is enough depends on where the last
conversion happens.** Wider intermediates help only when every value that is
stored fits 64 bits. The evaluator works in the coherent unit and converts to the
result's declared unit last. For most of the variances that overflow at 6
decimal places, and for every cylinder strength that overflows, the exact
value *in SI* (kg², Pa) needs 64 bits or more; in the declared unit (g², MPa)
every one fits, in at most 45 bits (the exact sizes below). So 128-bit
intermediate arithmetic is enough only if the SI value is never stored:
every node's `Evaluated<Rational>`, and every trace step's
value, is the SI number, so the variance node's own result would have to be
computed and recorded in the declared unit or a scaled one -- a change to the
unit a node computes and records in, not only to the last conversion.
Otherwise these cases need a wider stored representation: a fixed-width
wide-integer `Rational` offered as a `Rep`. Which of these to build is a
design decision, tracked in
[issue #1](https://github.com/LASTRADA-Software/formula-cpp/issues/1); this
page does not make it.

**Headroom** here is `63` minus the bits used by the largest integer an
evaluation formed -- numerators, denominators *and* the intermediates between
them: a sign bit aside, a 64-bit signed integer holds 63 bits, and a cross
term that only just fits is as close to overflowing as a numerator that only
just fits. So it can read lower than a count of the result's numerator and
denominator alone. 0 bits of headroom means the evaluation came within a
factor of 2 of overflowing.

## Why a fraction's integers grow

Adding fractions puts them over a common denominator. A mass of 40.053270 g
is 4005327/100000 g, in kilograms 4005327/100000000. Squaring a deviation
squares the denominator; summing six squared deviations whose denominators
differ multiplies in each new factor. A variance at microgram resolution
needs denominators near 10^18 before anything is divided, and 10^18 is
already 60 of the 63 bits. The value is small; the integers that hold it
exactly are not. `double` does not have this problem because it gives up
exactness instead, which is exactly what this library exists not to do.

## How it was measured

The census is this repository's own instrument, not part of the library's
contract: its programs are built with the internal macro
`FORMULA_OVERFLOW_CENSUS`, which no consumer's build defines. It must be
defined in every translation unit of a program or in none -- one of each
gives the arithmetic two definitions, which a linker would silently merge.
cl and clang-cl refuse such a program at link time (`#pragma
detect_mismatch`); an ELF linker has no such check.

Every integer the exact arithmetic forms at run time is reported to a
tally, which keeps the largest seen in four roles:

- **numerator** and **denominator**: every fraction handed to
  `Rational::make`, before it is reduced;
- **intermediate**: every other integer a sum, difference or product forms
  on the way, such as the cross terms of a sum, and the two scaled
  magnitudes a decimal-exponent comparison forms (`at_least_pow10`);
- **unsigned**: `rounded_sqrt`'s integer square root, which works in unsigned
  64-bit integers and so has 64 bits, not 63.

The hooks sit in `detail/checked_int.hpp`'s checked primitives, in
`Rational::make`, in `rounding.hpp`'s decimal-exponent comparison and in
`rounded_root.hpp`'s unsigned arithmetic. Leaf unit conversion, statistics,
the rejection loop and `rounded_sqrt` compute in `Rational` directly rather
than through a representation's `RepTraits`, so a wrapping representation
would have missed them. Powers of ten up to 10^18 are formed unhooked, and
counted only when they reach a product or a fraction. Without the macro
every hook expands to nothing, its arguments unevaluated: a release object
built with it off disassembles identically to one built before the hooks
existed, and gains one linker directive, the mismatch check (cl 19.51,
`/O2`).

The census does not see evaluations that happen at compile time
(`constexpr`): a constant evaluation cannot report to a tally. They fit --
an overflow there is still a refused result -- but their headroom is not
measured. Three examples evaluate some of their formulas that way:
`expressions` three, `rounding_and_conditionals` six and `constraints` five.

The figures are deterministic: the census program prints the same on cl
19.51 and gcc 13.3, and the clang and gcc presets hold it to the same pins.
The examples table below is cl's. clang and gcc evaluate a `const` local's
constant initialiser at compile time, where cl runs it, so under them a
program can report fewer integers -- today `expressions` leaves one bit more
headroom. The test holds every compiler to at least this table's headroom.

## The census

**Realistic** cases are formulas as a method would state them, with inputs at
a resolution a laboratory reads. **Stress controls** are built to probe the
instrument or the limits, not to resemble a method.

### Every example and the gallery (realistic)

Each program's largest integers over everything it evaluates at run time.

<!-- census:examples -->

| program | numerator bits | denominator bits | intermediate bits | headroom |
|---|---|---|---|---|
| example `simple` | 4 | 10 | 6 | 53 |
| example `exact_numbers` | 9 | 10 | 9 | 53 |
| example `dimensions_and_units` | 22 | 10 | 22 | 41 |
| example `quantities` | 4 | 10 | 9 | 53 |
| example `expressions` | 3 | 2 | 0 | 60 |
| example `citations` | 4 | 10 | 6 | 53 |
| example `composition` | 10 | 10 | 9 | 53 |
| example `electricity_bill` | 31 | 26 | 31 | 32 |
| example `tracing` | 10 | 10 | 9 | 53 |
| example `rounding_and_conditionals` | 27 | 20 | 27 | 36 |
| example `constraints` | 26 | 20 | 26 | 37 |
| example `lookup_tables` | 26 | 20 | 26 | 37 |
| example `methods_and_overlays` | 58 | 39 | 58 | 5 |
| example `statistics` | 20 | 35 | 35 | 28 |
| example `series` | 15 | 15 | 15 | 48 |
| example `records` | 25 | 25 | 25 | 38 |
| example `opaque_and_retry` | 48 | 60 | 60 | 3 |
| example `display` | 15 | 17 | 17 | 46 |
| the gallery generator | 29 | 27 | 29 | 34 |

<!-- /census:examples -->

The lowest is `opaque_and_retry`, at 3 bits, on purpose: it fits fifteen points on
distinct denominators to show a least-squares fit refusing with `Overflow`, and the census
counts the integers the fit formed before it was refused (see [Least squares](#least-squares-realistic-and-one-stress-control)). Of the examples that
compute only results, the lowest is `methods_and_overlays`, under the 8-bit line: its cylinder
variant divides a force of 89.3 kN by the library's rational π,
245850922/78256779, times a squared diameter of 135 mm, and a jurisdiction's
replacement of that variant divides it by 1127/1000 times the squared
diameter. It is the cylinder strength the tables below take apart, at a
diameter they list as fitting, and it shows how little such a division
leaves.

### Statistics, rejection and grading curves (realistic)

The fixtures are the shared fixtures of the statistics tests: masses of
about 40 g read to 0.1 g. The spread is `rounded_sqrt` of the variance; its
unsigned bits are out of 64. The 64-point curve reads invented screen
openings from 101 to 461 mm. The last two rows are the least-squares fit
([Opaque operations and bounded retry](opaque-and-retry.md)) on its own test
fixtures; the fit over every size is below.

<!-- census:statistics -->

| formula | numerator bits | denominator bits | intermediate bits | unsigned bits | headroom |
|---|---|---|---|---|---|
| fixture A: mean, variance, range | 20 | 27 | 27 | 0 | 36 |
| fixture B: mean, variance, range | 20 | 32 | 32 | 0 | 31 |
| fixture C: mean, variance, range | 20 | 20 | 17 | 0 | 43 |
| fixture D: mean, variance, range | 20 | 22 | 22 | 0 | 41 |
| fixture E: mean, variance, range | 20 | 20 | 6 | 0 | 43 |
| fixture F: mean, variance, range | 20 | 22 | 22 | 0 | 41 |
| fixture A: rejection, 6 % of the mean | 12 | 21 | 21 | 0 | 42 |
| fixture B: rejection, 7/4 standard deviations | 18 | 35 | 35 | 0 | 28 |
| fixture B: rejection, gap to range 9/20 | 14 | 16 | 16 | 0 | 47 |
| fixture A: spread at 2 dp | 20 | 27 | 27 | 19 | 36 |
| fixture A: spread at 3 dp | 20 | 27 | 27 | 26 | 36 |
| fixture A: spread at 4 dp | 20 | 27 | 27 | 33 | 36 |
| fixture A: spread at 6 dp | 21 | 29 | 29 | 46 | 34 |
| fixture F: exact root at 0 dp | 20 | 22 | 22 | 0 | 41 |
| passing from the cumulative retained, 5 screens | 17 | 10 | 17 | 0 | 46 |
| interpolation along a 5-point grading curve | 13 | 12 | 13 | 0 | 50 |
| 20 masses at 3 dp: mean, variance, range | 25 | 45 | 45 | 0 | 18 |
| 64-point grading curve: cumulative percentages, one reading | 26 | 24 | 26 | 0 | 37 |
| 20 masses at 3 dp: spread at 3 dp | 25 | 45 | 48 | 40 | 15 |
| least squares, the 4-point fixture: slope and intercept | 11 | 17 | 17 | 0 | 46 |
| least squares, 5 points on distinct denominators (stress control) | 21 | 20 | 21 | 0 | 42 |

<!-- /census:statistics -->

### Six masses near 40 g, by resolution (realistic)

1000 samples of six masses each, drawn from a fixed seed between 39 and
41 g at the resolution shown: 4 decimal places of a gram is 0.1 mg, 5 is
0.01 mg, 6 is 1 µg. "Least headroom" is the smallest any sample that did not
overflow left.

<!-- census:resolution -->

| formula | resolution | overflowed | least headroom |
|---|---|---|---|
| variance | 4 dp | 0 of 1000 | 11 |
| variance | 5 dp | 0 of 1000 | 4 |
| variance | 6 dp | 423 of 1000 | 0 |
| rejection by 7/4 standard deviations | 4 dp | 0 of 1000 | 7 |
| rejection by 7/4 standard deviations | 5 dp | 0 of 1000 | 0 |
| rejection by 7/4 standard deviations | 6 dp | 897 of 1000 | 0 |
| rejection by 6 % of the mean | 4 dp | 0 of 1000 | 33 |
| rejection by 6 % of the mean | 5 dp | 0 of 1000 | 29 |
| rejection by 6 % of the mean | 6 dp | 0 of 1000 | 26 |

<!-- /census:resolution -->

The named sample 40.053270, 39.475922, 39.025798, 40.615904, 39.418416 and
40.131659 g overflows in its variance, and so does its rejection by 7/4
standard deviations; a census test holds both.

A criterion relative to the mean compares a deviation with a limit and
squares nothing, so it keeps a wide margin at any resolution. Criteria in
standard deviations square twice, and are the first to run out.

### Exact sizes (realistic)

`tools/census/exact_sizes.py` draws the same samples with Python's exact
`fractions` and sizes the exact, fully reduced results twice: in the
coherent unit the evaluator works in (kg², Pa), and in the result's
declared unit (g², MPa), the unit `checked_evaluate` returns. The census test
and CTest's `census.exact-sizes-self-check` hold both generators to the same
literals, and `census.exact-sizes` holds these figures:

<!-- census:exact -->

```text
six masses near 40 g at 4 dp: the exact variance does not fit 64 bits in 0 of 1000 in kg2 (SI), in 0 of 1000 in g2 (declared; widest 32 bits)
six masses near 40 g at 5 dp: the exact variance does not fit 64 bits in 0 of 1000 in kg2 (SI), in 0 of 1000 in g2 (declared; widest 39 bits)
six masses near 40 g at 6 dp: the exact variance does not fit 64 bits in 374 of 1000 in kg2 (SI), in 0 of 1000 in g2 (declared; widest 45 bits)
4F / (pi * d^2), F = 89.3 kN, d = 101 to 163 mm: in Pa (SI) the exact strength needs 64 bits, more than a signed 64-bit integer's 63, at d = 101 (64 bits), 103 (64 bits), 107 (64 bits), 109 (64 bits), 113 (64 bits), 119 (64 bits), 121 (64 bits), 127 (64 bits), 131 (64 bits), 137 (64 bits), 139 (64 bits), 143 (64 bits), 149 (64 bits), 151 (64 bits), 157 (64 bits), 161 (64 bits), 163 (64 bits)
4F / (pi * d^2), F = 89.3 kN, d = 101 to 163 mm: in MPa (declared) it does not fit at 0 of 63 (widest 44 bits)
```

<!-- /census:exact -->

### A cylinder's cross-section and strength (realistic)

The library's π is 245850922/78256779: a 28-bit numerator over a 27-bit
denominator. Every diameter from 101 to 163 mm, in 1 mm steps, at a failure
load of 89.3 kN:

<!-- census:cylinder -->

| formula | overflows at d = | refused at | least headroom otherwise |
|---|---|---|---|
| area, pi * d^2 / 4 (the expressions example) | none | -- | 15 |
| strength, 4F / (pi * d^2), F = 89.3 kN, in MPa (the methods example's cylinder) | 101, 103, 107, 109, 113, 119, 121, 127, 131, 137, 139, 143, 149, 151, 157, 161, 163 mm | 4F / (pi * d^2) | 0 |

<!-- /census:cylinder -->

**The area fits; dividing by it does not.** The product π d² itself never
overflows. The strength divides by it, which puts π's 27-bit denominator into
the numerator, next to the force (4 × 89,300 N, 19 bits) and the 10^6 of
mm² to m² (20 bits): 4F × 78256779 × 10^6 needs 64.6 bits. It fits only
when d² cancels enough of it -- a diameter with a factor of 2, 3 or 5, as
135 = 3³ × 5 has, or of 19, which divides this force (133 = 7 × 19). Every
other diameter in the range leaves a 64-bit numerator in pascals: the exact
strength in SI, a value the evaluator holds before its last conversion. In
megapascals, the declared unit, it needs at most 44 bits; the 10^6 is the
whole difference. "Refused at" is the step the arithmetic refused, re-done
by hand in the evaluator's order.

### Stress controls

These are asserted by the census program's own tests.

| case | result |
|---|---|
| (2^62 − 1) + 2^62 = 2^63 − 1, from operands of 62 and 63 bits | the addition's own intermediate uses 63 bits: headroom 0 |
| 2^31 × 2^30 = 2^61, from operands of 32 and 31 bits | the product uses 62 bits: headroom 1 |
| (2^63 − 1) + 1 | `Overflow`, no figure |
| 2^32 × 2^31 | `Overflow`; the count holds nothing past the operands' 33 bits |
| (2^40 / 3) × (3 / 2^20) = 2^20 | intermediates within 21 bits, because a product is cross-reduced before it is formed |
| a sum evaluated at compile time | nothing reported |

### Least squares (realistic, and one stress control)

`linear_least_squares` sums, over the points, squares and products of each
point's coordinates, centred on their means. The data come in three
shapes, in seconds and newtons so that the fit sees them
unconverted: readings at 1 decimal place; readings at 3 decimal places of a
few thousand newtons, a load cell's; and a different denominator on every
point, the stress control. Every size from 2 to 128 points is fitted through
`LinearLeastSquares::compute`, the fit the node calls, and the node itself
is checked against it at 33 and 34 points. The last two rows fit the same
shapes the way `rounded_output` does: the slope reported to 4 decimal places
of N/s, computed by `LinearLeastSquares::compute_exact` in 256-bit integers
and rounded exactly; the node is checked against that at 57, 58 and 128
points. For those two rows the last column counts the 64-bit integers only,
the rounded result and its conversion among them, and not the fit's 256-bit
intermediates, which the census does not see: they reach 68 bits on the
readings at 3 decimal places, and up to 249 of the 256 on a different
denominator for every point, at the sizes that still answer. So a large
figure there says nothing of how close the fit came to its 256 bits.

<!-- census:least-squares -->

| data (invented) | sizes that overflow | first to overflow | least headroom otherwise |
|---|---|---|---|
| readings at 1 dp (realistic) | 0 of 127 | none | 29 |
| readings at 3 dp near 2410 N, a load cell's (realistic) | 57 of 127 | 34 points | 0 |
| a different denominator on every point (stress control) | 114 of 127 | 15 points | 2 |
| the slope rounded to 4 dp by rounded_output: readings at 3 dp near 2410 N (realistic) | 0 of 127 | none | 41 |
| the slope rounded to 4 dp by rounded_output: a different denominator on every point (stress control) | 71 of 127 | 58 points | 48 |

<!-- /census:least-squares -->

**Overflow depends on the data far more than on the number of points.** At
3 decimal places the first size to overflow is 34 points, but not every
larger size does. So no number of points is safe to state; an overflowing
fit is `Overflow`, never a line. Where it overflows, a method that states the
precision it reports the slope at gets that instead, from `rounded_output`:
exact, traced and documented, at every size here for readings at 3 decimal
places, and `Overflow` from 58 points on a different denominator for every
point, where even 256 bits are outgrown. There is no traced fallback in
`double`: a curve evaluates only in `Rational`, so
`checked_evaluate_si<double>` over a fit is refused.
`LinearLeastSquares::compute<double>` can be called directly, on numbers
already in coherent units, but nothing it returns is checked, traced,
rendered or documented.

## Which cases decide

Under 8 bits, and realistic: the **sample variance at 5 and 6 decimal
places** of a gram, **rejection by standard deviations at 4, 5 and 6
decimal places**, and **a cylinder's strength** at the diameters the table
names, and in the methods example even at 135 mm, where it fits; and
**a least-squares line through 3-decimal readings** from 34 points. A balance
reading to 0.01 mg or 1 µg is ordinary laboratory equipment, and so is a
139 mm cylinder, so these are not contrived. The
cases with a wide margin are the ones that add or scale values at a
resolution of 0.1 g or coarser, or that do not square.

## What this does not decide

The census builds neither remedy. 128-bit intermediate arithmetic would
compute each product and sum in 128 bits before reducing; a wider stored
representation would offer a fixed-width wide-integer `Rational` as a `Rep`.
[Issue #1](https://github.com/LASTRADA-Software/formula-cpp/issues/1) tracks
the choice between them. Beside them, a formula can declare the precision a
value is reported at, and `rounded_output` computes that decimal in wider
integers ([Displaying numbers](display.md#values-the-exact-layer-cannot-hold));
that answers for the one output reported, not for `Rational` itself. An
arbitrary-precision integer is out of scope: it allocates, which in
`noexcept` code turns running out of memory into `std::terminate`, and it
cannot run at compile time.

## Regression pins

The census program holds the twenty masses', the 64-point curve's and
fixture A's 3 dp spread's headroom to what they were measured with, less 4
bits each, and the spread's numerator and unsigned bits to at most 4 more. A
change that quietly spends more headroom fails there. Measured: without
`checked_mul`'s cross-reduction, the curve pin and the spread's numerator
pin fail, and so does the cross-reduction control; with a sum scaled by the
product of the denominators instead of their least common multiple, the
twenty masses' variance no longer evaluates.

To run the census yourself, build and run `formula-cpp-census-tests`; the
census builds of the examples and the gallery print their line when they
exit.
