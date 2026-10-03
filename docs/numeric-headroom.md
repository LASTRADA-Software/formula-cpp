# Numeric headroom

`formula-cpp` computes exactly: every value is a `Rational`, a fraction whose
numerator and denominator are signed 128-bit integers (`formula::Int128`). A
result that does not fit is never wrapped or rounded away; it is refused as
`ArithmeticError::Overflow`. This page answers one question with
measurements: **how much of those 128 bits do real formulas use, and is that
enough?**

**Every figure in the tables below is generated.** Each table is what the
census programs print, rebuilt by `cmake/CheckCensusPage.cmake`, and the
test `docs.numeric-headroom` fails when this page differs from a fresh run.
Regenerate it by building the target `formula-cpp-census-page`; never edit
a table by hand.

## The answer

**Yes, for every realistic case measured.** The four kinds of realistic
formula that come closest all answer, at every input measured, with far more
than 8 bits to spare; the tables below give their figures:

- the **sample variance** of six masses near 40 g read to 6 decimal places of
  a gram: none of 1000 samples overflows, and the least headroom any leaves is
  62 bits;
- **rejecting outliers by 7/4 standard deviations** from such a sample, which
  forms limit² × s² on top: none of 1000 overflows, with at least 58 bits left;
- a **cylinder's compressive strength**, 4F / (π d²), at 89.3 kN: no diameter
  from 101 to 163 mm overflows, 139 mm among them, with at least 63 bits left;
- a **least-squares line** through readings at 3 decimal places: no size from
  2 to 128 points overflows, with at least 54 bits left.

What still overflows is a stress control, built to do so: a different
denominator on every point outgrows a line's exact sums from 28 points, and
the wider integers of its rounded route from 58.

The project's decision rule stays: **any realistic case under 8 bits of
headroom recommends wider arithmetic.** No realistic formula measured here is
under it; the one program the examples table shows under it, `opaque_and_retry`,
fits a stress control there on purpose (below). The rule is what a change that
quietly spends headroom is judged by.

**Headroom** here is `127` minus the bits used by the largest integer an
evaluation formed -- numerators, denominators *and* the intermediates between
them: a sign bit aside, a 128-bit signed integer holds 127 bits, and a cross
term that only just fits is as close to overflowing as a numerator that only
just fits. So it can read lower than a count of the result's numerator and
denominator alone. 0 bits of headroom means the evaluation came within a
factor of 2 of overflowing.

## What was chosen, and why

At 64 bits, the variance and the rejection at fine resolution, the cylinder's
strength and the least-squares line were all under the 8-bit line, and many
of their evaluations overflowed. Three findings decided the remedy:

- **128-bit intermediates alone could not have been enough.** `checked_mul`
  reduces across its operands before it multiplies, so its product is already
  in lowest terms: a product that overflows is a result that overflows. Only a
  sum can overflow before it reduces.
- **The values are stored in SI.** Every node's `Evaluated<Rational>`, and
  every trace step's value, is in the coherent unit, and there the widest
  exact variance in kg² needs 65 bits and the widest strength in Pa 64, though
  in the declared g² and MPa they need at most 45 and 44 (the exact sizes
  below). Storing them in the declared unit would change the evaluator's rule
  that every leaf is converted to SI, and a variance node has no declared unit
  to work in.
- **So the stored integer was widened.** `Rational` stores its numerator and
  denominator in `formula::Int128`, 128 bits: the compiler's own 128-bit
  integer computes where it has one (GCC, Clang), and portable `constexpr`
  code everywhere else (cl, clang-cl). A computation that answered at 64 bits
  gives the same answer; some that were refused with `Overflow` now answer.

## Why a fraction's integers grow

Adding fractions puts them over a common denominator. A mass of 40.053270 g
is 4005327/100000 g, in kilograms 4005327/100000000. Squaring a deviation
squares the denominator; summing six squared deviations whose denominators
differ multiplies in each new factor. A variance at microgram resolution
needs denominators near 10^18 before anything is divided, and 10^18 takes
60 bits: nearly all of a 64-bit integer, and under half of a 128-bit one.
The value is small; the integers that hold it exactly are not. `double`
does not have this problem because it gives up exactness instead, which is
exactly what this library exists not to do.

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
  128-bit integers and so has 128 bits, not 127.

The hooks sit in `detail/checked_int.hpp`'s checked primitives, in
`Rational::make`, in `rounding.hpp`'s decimal-exponent comparison and in
`rounded_root.hpp`'s unsigned arithmetic. Leaf unit conversion, statistics,
the rejection loop and `rounded_sqrt` compute in `Rational` directly rather
than through a representation's `RepTraits`, so a wrapping representation
would have missed them. Powers of ten up to 10^38 are formed unhooked, and
counted only when they reach a product or a fraction. Without the macro
every hook expands to nothing, its arguments unevaluated: a release object
built with it off disassembles identically to one built before the hooks
existed, and gains one linker directive, the mismatch check (cl 19.51,
`/O2`).

The logarithm and exponential kernel (`detail/transcendental.hpp`) carries
no hooks: it computes in the wide words of `detail/wide_int.hpp`, outside the
census, and only the rounded decimal it answers is counted, made by
`Rational::from_decimal` as `rounded_sqrt`'s and `rounded_output`'s are.

The census does not see evaluations that happen at compile time
(`constexpr`): a constant evaluation cannot report to a tally. They fit --
an overflow there is still a refused result -- but their headroom is not
measured. Nine examples evaluate some of their formulas that way:
`constraints`, `dimensions_and_units`, `expressions`, `lookup_tables`,
`quantities`, `records`, `rounding_and_conditionals`, `series` and `statistics`.
A row that reads 0 | 0 | 0 and the full 127 bits means the program counted no
integer at run time. For `quantities` that is because it evaluates its
formulas at compile time; the one thing it does at run time, combining an
absent input, computes no integer, so there is nothing for the census to tally. `expressions`
evaluates one formula at run time, and that evaluation returns a value a
person entered without computing it, so its row reports no integer either.

The figures are deterministic: the census program prints the same on cl
19.51 and gcc 13.3, and the clang and gcc presets hold it to the same pins.
The examples table below is cl's. clang and gcc evaluate a `const` local's
constant initialiser at compile time, where cl runs it, so under them a
program can report fewer integers and leave more headroom. The test holds
every compiler to at least this table's headroom.

## The census

**Realistic** cases are formulas as a method would state them, with inputs at
a resolution a laboratory reads. **Stress controls** are built to probe the
instrument or the limits, not to resemble a method.

### Every example and the gallery (realistic)

Each program's largest integers over everything it evaluates at run time.

<!-- census:examples -->

| program | numerator bits | denominator bits | intermediate bits | headroom |
|---|---|---|---|---|
| example `simple` | 4 | 10 | 6 | 117 |
| example `exact_numbers` | 9 | 10 | 9 | 117 |
| example `dimensions_and_units` | 22 | 10 | 22 | 105 |
| example `quantities` | 0 | 0 | 0 | 127 |
| example `expressions` | 0 | 0 | 0 | 127 |
| example `citations` | 4 | 10 | 6 | 117 |
| example `composition` | 10 | 10 | 9 | 117 |
| example `electricity_bill` | 31 | 26 | 31 | 96 |
| example `tracing` | 10 | 10 | 9 | 117 |
| example `rounding_and_conditionals` | 27 | 20 | 27 | 100 |
| example `constraints` | 26 | 20 | 26 | 101 |
| example `lookup_tables` | 26 | 20 | 26 | 101 |
| example `methods_and_overlays` | 58 | 39 | 58 | 69 |
| example `statistics` | 20 | 35 | 35 | 92 |
| example `series` | 15 | 15 | 15 | 112 |
| example `records` | 25 | 25 | 25 | 102 |
| example `opaque_and_retry` | 93 | 96 | 121 | 6 |
| example `display` | 20 | 21 | 21 | 106 |
| the gallery generator | 29 | 27 | 29 | 98 |

<!-- /census:examples -->

The lowest is `opaque_and_retry`, at 6 bits, on purpose: it fits twenty-seven points on
distinct denominators to show a least-squares fit refusing with `Overflow`, and the census
counts the integers the fit formed before it was refused (see [Least squares](#least-squares-realistic-and-one-stress-control)). Of the examples that
compute only results, the lowest is `methods_and_overlays`, at 69 bits: its cylinder
variant divides a force of 89.3 kN by the library's rational π,
245850922/78256779, times a squared diameter of 135 mm, and a jurisdiction's
replacement of that variant divides it by 1127/1000 times the squared
diameter. It is the cylinder strength the tables below take apart.

### Statistics, rejection and grading curves (realistic)

The fixtures are the shared fixtures of the statistics tests: masses of
about 40 g read to 0.1 g. The spread is `rounded_sqrt` of the variance; its
unsigned bits are out of 128. The 64-point curve reads invented screen
openings from 101 to 461 mm. The last two rows are the least-squares fit
([Opaque operations and bounded retry](opaque-and-retry.md)) on its own test
fixtures; the fit over every size is below.

<!-- census:statistics -->

| formula | numerator bits | denominator bits | intermediate bits | unsigned bits | headroom |
|---|---|---|---|---|---|
| fixture A: mean, variance, range | 20 | 27 | 27 | 0 | 100 |
| fixture B: mean, variance, range | 20 | 32 | 32 | 0 | 95 |
| fixture C: mean, variance, range | 20 | 20 | 17 | 0 | 107 |
| fixture D: mean, variance, range | 20 | 22 | 22 | 0 | 105 |
| fixture E: mean, variance, range | 20 | 20 | 6 | 0 | 107 |
| fixture F: mean, variance, range | 20 | 22 | 22 | 0 | 105 |
| fixture A: rejection, 6 % of the mean | 12 | 21 | 21 | 0 | 106 |
| fixture B: rejection, 7/4 standard deviations | 18 | 35 | 35 | 0 | 92 |
| fixture B: rejection, gap to range 9/20 | 14 | 16 | 16 | 0 | 111 |
| fixture A: spread at 2 dp | 20 | 27 | 27 | 19 | 100 |
| fixture A: spread at 3 dp | 20 | 27 | 27 | 26 | 100 |
| fixture A: spread at 4 dp | 20 | 27 | 27 | 33 | 100 |
| fixture A: spread at 6 dp | 21 | 29 | 29 | 46 | 98 |
| fixture F: exact root at 0 dp | 20 | 22 | 22 | 0 | 105 |
| passing from the cumulative retained, 5 screens | 17 | 10 | 17 | 0 | 110 |
| interpolation along a 5-point grading curve | 13 | 12 | 13 | 0 | 114 |
| 20 masses at 3 dp: mean, variance, range | 25 | 45 | 45 | 0 | 82 |
| 64-point grading curve: cumulative percentages, one reading | 26 | 24 | 26 | 0 | 101 |
| 20 masses at 3 dp: spread at 3 dp | 25 | 45 | 48 | 40 | 79 |
| least squares, the 4-point fixture: slope and intercept | 11 | 17 | 17 | 0 | 110 |
| least squares, 5 points on distinct denominators (stress control) | 21 | 20 | 21 | 0 | 106 |

<!-- /census:statistics -->

### Six masses near 40 g, by resolution (realistic)

1000 samples of six masses each, drawn from a fixed seed between 39 and
41 g at the resolution shown: 4 decimal places of a gram is 0.1 mg, 5 is
0.01 mg, 6 is 1 µg. "Least headroom" is the smallest any sample that did not
overflow left.

<!-- census:resolution -->

| formula | resolution | overflowed | least headroom |
|---|---|---|---|
| variance | 4 dp | 0 of 1000 | 75 |
| variance | 5 dp | 0 of 1000 | 68 |
| variance | 6 dp | 0 of 1000 | 62 |
| rejection by 7/4 standard deviations | 4 dp | 0 of 1000 | 71 |
| rejection by 7/4 standard deviations | 5 dp | 0 of 1000 | 64 |
| rejection by 7/4 standard deviations | 6 dp | 0 of 1000 | 58 |
| rejection by 6 % of the mean | 4 dp | 0 of 1000 | 97 |
| rejection by 6 % of the mean | 5 dp | 0 of 1000 | 93 |
| rejection by 6 % of the mean | 6 dp | 0 of 1000 | 90 |

<!-- /census:resolution -->

The named sample 40.053270, 39.475922, 39.025798, 40.615904, 39.418416 and
40.131659 g has a variance of exactly 2026588050217/6000000000000 g², and a
rejection by 7/4 standard deviations; a census test holds both.

A criterion relative to the mean compares a deviation with a limit and
squares nothing, so it keeps a wide margin at any resolution. Criteria in
standard deviations square twice, and use the most bits: at 6 decimal places
they leave 58, where a criterion relative to the mean leaves 90.

### Exact sizes (realistic)

`tools/census/exact_sizes.py` draws the same samples with Python's exact
`fractions` and sizes the exact, fully reduced results twice: in the
coherent unit the evaluator works in (kg², Pa), and in the result's
declared unit (g², MPa), the unit `checked_evaluate` returns. The census test
and CTest's `census.exact-sizes-self-check` hold both generators to the same
literals, and `census.exact-sizes` holds these figures. Every value fits 128
bits in either unit; in SI the widest variance needs 65 bits and the widest
strength 64:

<!-- census:exact -->

```text
six masses near 40 g at 4 dp: the exact variance does not fit 128 bits in 0 of 1000 in kg2 (SI; widest 52 bits), in 0 of 1000 in g2 (declared; widest 32 bits)
six masses near 40 g at 5 dp: the exact variance does not fit 128 bits in 0 of 1000 in kg2 (SI; widest 59 bits), in 0 of 1000 in g2 (declared; widest 39 bits)
six masses near 40 g at 6 dp: the exact variance does not fit 128 bits in 0 of 1000 in kg2 (SI; widest 65 bits), in 0 of 1000 in g2 (declared; widest 45 bits)
4F / (pi * d^2), F = 89.3 kN, d = 101 to 163 mm: in Pa (SI) the exact strength does not fit 128 bits at 0 of 63 (widest 64 bits)
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
| area, pi * d^2 / 4 (the expressions example) | none | -- | 79 |
| strength, 4F / (pi * d^2), F = 89.3 kN, in MPa (the methods example's cylinder) | none | -- | 63 |

<!-- /census:cylinder -->

**Dividing by the area is what costs bits.** The product π d² itself stays
small. The strength divides by it, which puts π's 27-bit denominator into the
numerator, next to the force (4 × 89,300 N, 19 bits) and the 10^6 of mm² to
m² (20 bits): 4F × 78256779 × 10^6 needs 64.6 bits, more than a 64-bit
integer holds unless d² cancels some of it. In pascals, the exact strength in
SI that the evaluator holds before its last conversion, the widest needs 64
bits; in megapascals, the declared unit, at most 44; the 10^6 is the whole
difference. 128 bits hold every one, at 139 mm as at 135 mm. "Refused at"
would name the step the arithmetic refused, re-done by hand in the
evaluator's order; no step is refused.

### Stress controls

These are asserted by the census program's own tests.

| case | result |
|---|---|
| (2^126 − 1) + 2^126 = 2^127 − 1, from operands of 126 and 127 bits | the addition's own intermediate uses 127 bits: headroom 0 |
| 2^63 × 2^62 = 2^125, from operands of 64 and 63 bits | the product uses 126 bits: headroom 1 |
| (2^127 − 1) + 1 | `Overflow`, no figure |
| 2^64 × 2^63 | `Overflow`; the count holds nothing past the operands' 65 bits |
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
is checked against it at 27 and 28 points on a different denominator for
every point, and at 128 on the readings at 3 decimal places. The last two
rows fit the same shapes the way `rounded_output` does: the slope reported to
4 decimal places of N/s, computed by `LinearLeastSquares::compute_exact` in
256-bit integers and rounded exactly; the node is checked against that at 57,
58 and 128 points. For those two rows the last column counts `Rational`'s
128-bit integers only,
the rounded result and its conversion among them, and not the fit's 256-bit
intermediates, which the census does not see: they reach 68 bits on the
readings at 3 decimal places, and up to 249 of the 256 on a different
denominator for every point, at the sizes that still answer. So a large
figure there says nothing of how close the fit came to its 256 bits.

<!-- census:least-squares -->

| data (invented) | sizes that overflow | first to overflow | least headroom otherwise |
|---|---|---|---|
| readings at 1 dp (realistic) | 0 of 127 | none | 93 |
| readings at 3 dp near 2410 N, a load cell's (realistic) | 0 of 127 | none | 54 |
| a different denominator on every point (stress control) | 101 of 127 | 28 points | 6 |
| the slope rounded to 4 dp by rounded_output: readings at 3 dp near 2410 N (realistic) | 0 of 127 | none | 105 |
| the slope rounded to 4 dp by rounded_output: a different denominator on every point (stress control) | 71 of 127 | 58 points | 112 |

<!-- /census:least-squares -->

**Overflow depends on the data far more than on the number of points.**
Readings at 1 and at 3 decimal places fit at every size up to 128 points,
with at least 54 bits to spare; a different denominator on every point
overflows from 28 points. So no number of points is safe to state for every
kind of data; an overflowing fit is `Overflow`, never a line. Where it
overflows, a method that states the precision it reports the slope at gets
that instead, from `rounded_output`:
exact, traced and documented, at every size here for readings at 3 decimal
places, and `Overflow` from 58 points on a different denominator for every
point, where even 256 bits are outgrown. There is no traced fallback in
`double`: a curve evaluates only in `Rational`, so
`checked_evaluate_si<double>` over a fit is refused.
`LinearLeastSquares::compute<double>` can be called directly, on numbers
already in coherent units, but nothing it returns is checked, traced,
rendered or documented.

### Regression over observations (realistic, and one stress control)

`linear_least_squares` over raw observations, up to 128 of them, is fitted on
the census's own shapes, read as observations: readings at 3 decimal places
near 2410 N, readings at 4 decimal places near 2410 mm, and a different
denominator on every point. Each size from 2 to 128 points is fitted twice:
through `opaque_output`, exactly, in the wide integers of the regression
kernel, and through `rounded_output`, the slope to 4 decimal places and R²
floored at 6. The last row regresses the load on the time and a temperature at
1 decimal place in degrees Celsius (`multiple_least_squares`, coefficient 1
to 4 decimal places of N/s, R² floored at 6), from 3 to 128 rows, so 126
sizes; every size answers or is `Overflow`. The columns count sizes, not bits:
the kernel's wide integers do not report to the census, so there is no
headroom figure to print, and none is implied.

<!-- census:regression -->

| data (invented) | exact route: sizes that overflow | first | rounded route: sizes that overflow | first |
|---|---|---|---|---|
| a line through readings at 3 dp near 2410 N (realistic) | 0 of 127 | none | 0 of 127 | none |
| a line through readings at 4 dp near 2410 mm (realistic) | 0 of 127 | none | 0 of 127 | none |
| a line through a different denominator on every point (stress control) | 107 of 127 | 22 points | 66 of 127 | 62 points |
| two regressors: readings at 3 dp and a temperature at 1 dp in degrees Celsius (realistic) | 0 of 126 | none | 0 of 126 | none |

<!-- /census:regression -->

**Neither route stops on realistic data.** A call's outputs answer or fail
together, and R²'s exact fraction is about twice as wide as the slope's, so
the exact route is the first to stop: on a different denominator on every
point it stops at 22 points. Reported at declared decimals, the same fit
answers at every size below 62 points, the first at which it outgrows even
the wide kernel, and is `Overflow`.

## Which cases decide

No realistic formula measured is under 8 bits. The ones that come closest
square values read at fine resolution, and they are the ones a future change
would push under the line first: **a least-squares line through 3-decimal
readings** (54 bits left), **rejection by standard deviations at 6 decimal
places** (58), the **sample variance at 6 decimal places** of a gram (62) and
**a cylinder's strength** (63). A balance reading to 1 µg is ordinary
laboratory equipment, and so is a 139 mm cylinder, so these are not
contrived. The cases with a wide margin are the ones that add or scale values
at a resolution of 0.1 g or coarser, or that do not square. The one program
under the line, `opaque_and_retry`, is there on purpose: its fit on a
different denominator for every point is built to overflow.

## What this does not decide

Only the integer `Rational` stores changed. These stay as they were:

- **Rounding's decimal places**, `from_decimal`'s exponents and the `_r`
  literal's 18 places and 64-bit mantissa ([Numbers](numbers.md#limits)).
  Widening them is a separate decision.
- **The logarithm and exponential kernel** (`detail/transcendental.hpp`)
  takes an argument whose numerator and denominator each fit 64 bits, the
  range it was built for; a wider argument, which a `Rational` can now hold,
  is `Overflow`, and so is the exponential of more than 44.
- **The 64-bit fields** of `Unit`, `Band` and `Breakpoint`: `band` and
  `breakpoint` refuse a `Rational` bound or key that does not fit them.

Beside the exact arithmetic, a formula can declare the precision a value is
reported at, and `rounded_output` computes that decimal in wider integers
([Displaying numbers](display.md#values-the-exact-layer-cannot-hold)); that
answers for the one output reported, not for `Rational` itself. An
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
