# Opaque operations and bounded retry

Most of what a method states is a formula over its inputs, and this library
keeps every such formula as an expression tree: rendered, documented and
traced node by node. Two things a method states are not like that.

- **A named operation whose inside the method does not spell out.** "Fit a
  straight line by least squares" names a computation, and a method relies on
  its result, but the method does not write out the sums. Spelling them out as
  an expression tree would put on the page, and in every trace, a derivation
  the method never states.
- **A step repeated until it is accepted.** "Repeat the determination until
  two successive results agree, at most four times" is a loop with a bound and
  a verdict when the bound is reached. No formula over the inputs expresses it.

formula-cpp has an **opaque operation** for the first and a **retry** for the
second. Neither is a way round the expression tree: an opaque operation's
inputs and outputs are nodes, and a retry's attempt and acceptance are
formulas. What each adds is a boundary the trace states honestly.

The worked example is `examples/opaque_and_retry.cpp`. The page holds two kinds
of quoted block. **Program output** is copied verbatim from that program's
actual output, and `docs.opaque-and-retry-output` fails unless each of these
blocks is a run of consecutive lines the program prints, exactly as quoted
(`cmake/CheckGuideOutput.cmake`). **Code** is copied from the example's source,
and `docs.opaque-and-retry-snippets` fails unless each code block appears
there as a run of consecutive lines, compared without their indentation
(`cmake/CheckGuideSnippets.cmake`).

## An opaque operation

An operation is a type the author declares: a name, the shape of each input, a
name for each output, what each output measures given what the inputs measure,
and the computation itself.

```cpp
struct SeriesSpan
{
    static constexpr std::string_view name = "series span";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 3> outputs { "lowest", "highest", "span" };

    static consteval std::optional<std::array<formula::Dimension, 3>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0], declared[0], declared[0] };
    }
```

**The computation receives values and returns values.** `compute` is handed
each input already evaluated, in the coherent unit of its dimension, as a
`std::span` over the representation -- `Rational` for an exact evaluation,
`double` for a floating one. It is never handed the environment. An operation
that could read the environment could read an input the formula does not
name, and the page, which lists what a formula reads, would be wrong about it.
An operation sees exactly the inputs its call names, and nothing else:

```cpp
    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 3>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        Rep least = readings[0];
        Rep most = readings[0];
        for (Rep const& each: readings)
        {
            if (each < least)
                least = each;
            if (most < each)
                most = each;
        }
        std::expected<Rep, formula::ArithmeticError> const difference = formula::RepTraits<Rep>::subtract(most, least);
        if (!difference.has_value())
            return std::unexpected { difference.error() };
        return std::array { least, most, *difference };
    }
```

**`compute` does its arithmetic through `RepTraits<Rep>`, and never
throws.** `RepTraits<Rep>::add`, `subtract`, `multiply` and `divide` return
their result or an `ArithmeticError`, and `compute` returns that error as its
own, as the subtraction above does: an overflow is then `Overflow` on the
trace. `Rational`'s own `+`, `-`, `*` and `/` throw `ArithmeticException` on
overflow instead, and a throw out of `compute`, which is `noexcept`, calls
`std::terminate`: the program ends, in a debug build with an abort dialog.
Comparing and copying values never throw. The library checks that `compute`
is `noexcept`; it cannot check what the body calls, so this rule is the
operation author's part of the contract.

**`OpaqueOperation` is the extension contract.** A consumer adds a
computation to this library only by declaring an operation: the members
above, and the rule on arithmetic. It is one of the library's customisation
points, with `TagName`, `EnumeratorName`, `Describe`, `RepTraits` and the
vocabulary, and nothing else is one. Specialising a `detail::` template
instead -- `ConstantRewrite`, `StepKindOf` or any other -- is outside the
contract, and can make a trace say anything.

A call names the operation, a citation, and the inputs. Each output is a node,
chosen by name, and a formula uses it like any other:

```cpp
constexpr auto spanCall = formula::opaque<SeriesSpan>(
    { .title = "Spread of readings", .reference = "Example Standard 12", .section = "4.2" }, formula::series<Reading, 4>);
constexpr auto span = formula::opaque_output<"span">(spanCall);
```

On the page it reads as a call, and in LaTeX its name is set as text:

```text
series span(r(i)).span
\text{series span}({r}_{i})_{\text{span}}
```

**The trace says the inside is not shown.** The operation's step lists every
output it produced and its citation. Its inputs are traced as every input is.
What happened inside is not traced, and the line says so rather than leaving a
reader to wonder whether a step is missing:

```text
1. r = 127 g; 103 g; 191 g; 139 g
2. series span(#1) = lowest = 103 g; highest = 191 g; span = 88 g [inside not shown] [Spread of readings, Example Standard 12, 4.2]
3. span of #2 = 88 g
```

`[inside not shown]` is written for every opaque call, whatever it holds, and
nothing an operation declares can switch it off. **An operation's name is a
label, not an identity**: the trace names an operation as its type names
itself, and two operations may declare one name, so a consumer's operation
named "linear least squares" reads as the fit this library ships. The
citation says which computation the method means. `document()` lists the
operation separately from the symbols (`Documentation::opaqueOperations`),
with its outputs and what each measures:

```text
operation: series span, outputs: lowest highest span
```

### Two outputs run the operation twice

Each output is its own node, and a node evaluates everything beneath it. So a
formula that uses two outputs of one call runs the operation twice, and the
trace shows both runs:

```cpp
constexpr auto highestLessLowest = formula::opaque_output<"highest">(spanCall) - formula::opaque_output<"lowest">(spanCall);
```

```text
1. r = 127 g; 103 g; 191 g; 139 g
2. series span(#1) = lowest = 103 g; highest = 191 g; span = 88 g [inside not shown] [Spread of readings, Example Standard 12, 4.2]
3. highest of #2 = 191 g
4. r = 127 g; 103 g; 191 g; 139 g
5. series span(#4) = lowest = 103 g; highest = 191 g; span = 88 g [inside not shown] [Spread of readings, Example Standard 12, 4.2]
6. lowest of #5 = 103 g
7. #3 - #6 = 11/125
```

```text
operation runs: 2
```

That is the price of every node being a value: nothing in an expression tree
is shared between two branches. An operation is pure -- the same inputs give
the same outputs -- so the two runs agree, and the trace shows the work that
was done rather than pretending it was done once. Where one output alone is
enough, as `span` is here, use one.

## A least-squares line

`linear_least_squares` is an opaque operation this library ships. It fits a
straight line to a curve -- a domain series and a value series of one length
-- and has two outputs, `intercept` and `slope`:

```cpp
constexpr auto fit =
    formula::linear_least_squares(formula::curve(formula::series<Elapsed, 4>, formula::series<Length, 4>),
                                  { .title = "Rate of change", .reference = "Example Standard 12", .section = "5.1" });
constexpr auto slope = formula::opaque_output<"slope">(fit);
```

```text
linear least squares(t(i), L(i)).slope
1. t = 1 s; 2 s; 4 s; 7 s
2. L = 51/5 mm; 109/10 mm; 121/10 mm; 143/10 mm
3. curve(#1, #2) = 1 s: 51/5 mm; 2 s: 109/10 mm; 4 s: 121/10 mm; 7 s: 143/10 mm
4. linear least squares(#3) = intercept = 19/2 mm; slope = 19/28 mm/s [inside not shown] [Rate of change, Example Standard 12, 5.1]
5. slope of #4 = 19/28 mm/s
```

- **Exact in `Rational`.** The slope is 19/28 mm/s exactly -- 285/7 mm/min in
  the declared unit of the result -- and not a rounded neighbour of it. The
  fit uses sums centred on the means.
- **A degenerate set of points is the fit's own error.** One point, or points
  whose domain values are all equal, has no line through it. The fit returns
  `DomainError` and never a slope of 0:

```text
one point: argument outside the domain of the operation
```

- **Overflow, never a wrong line.** The fit sums, over the points, squares
  and products of each point's coordinates about their means, and `Rational`
  keeps each numerator and denominator in 64 bits.
  When an exact sum does not fit, the result is `Overflow`:

```text
fifteen distinct denominators: overflow in exact arithmetic
```

**When a fit overflows depends on the data far more than on the number of
points.** Measured on cl 19.51, clang-cl and clang++ 22.1.3, g++ 13.3 and
g++ 14.2, with identical results on all five: integers, readings at one
decimal place, and thirds mixed with sevenths never overflow for 2 to 128
points; readings at three decimal places of a few thousand first overflow at
34 points, and not at every larger size; a different denominator on every
point overflows from 15. So there is no safe number of points to state. The
[numeric headroom](numeric-headroom.md) page carries the fit's census over
every size, regenerated with every build. **A fit that overflows has a
traced answer only at a declared precision** (`rounded_output`, below), **and
none in `double`.** A curve evaluates only in `Rational`, so
`checked_evaluate_si<double>` over a curve fit is refused where it is
written. `LinearLeastSquares::compute<double>` can be called directly, on
numbers the caller has put in coherent units, but it returns bare numbers:
nothing checks their dimensions, and nothing reaches the trace or the page.
A fit over raw observations, [below](#a-line-through-observations), is
evaluated in `double` too, untraced, and at a declared precision it answers
where its exact route overflows, within the kernel's width and beyond it
`Overflow`.

### Rounded where it is used

A method that reports the slope at a stated precision -- "to 0.0001 mm/s" --
does not need the exact fraction: it needs the decimal that fraction rounds
to. `rounded_output` states that precision, as `rounded<>` does. For
`linear_least_squares`, which computes in wider integers, the library
computes that decimal exactly, even where the exact fit leaves `Rational`'s
range:

```cpp
constexpr formula::Unit millimetrePerSecond { .dimension = formula::dim::Velocity,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 1000,
                                              .symbolText = formula::symbol("mm/s"),
                                              .decimals = 4 };
constexpr auto roundedSlope = formula::rounded_output<"slope",
                                                      millimetrePerSecond,
                                                      formula::DecimalPlaces { 4 },
                                                      formula::RoundingMode::HalfEven>(fit);
```

```text
round(linear least squares(t(i), L(i)).slope, to 4 dp of mm/s)
1. t = 1 s; 2 s; 4 s; 7 s
2. L = 51/5 mm; 109/10 mm; 121/10 mm; 143/10 mm
3. curve(#1, #2) = 1 s: 51/5 mm; 2 s: 109/10 mm; 4 s: 121/10 mm; 7 s: 143/10 mm
4. linear least squares(#3) = intercept, slope: rounded where used [inside not shown] [Rate of change, Example Standard 12, 5.1]
5. round(slope of #4, to 4 dp of mm/s) = 3393/5000 mm/s [nearest, ties to even]
```

- **The true slope is written nowhere.** The call's line names its outputs
  without values -- none exists until one is rounded -- and the output's own
  line states its rounding. 3393/5000 mm/s is 0.6786 mm/s exactly, the
  correct rounding of 19/28 mm/s, and the step's value; no number style marks
  it approximate.
- **It answers where the exact fit overflows.** `linear_least_squares`
  computes the fit for it in 256-bit integers, and the fifteen distinct
  denominators that overflow above give a slope:

```text
fifteen distinct denominators, rounded where used: 116.232 mm/min
```

- **It still refuses rather than guess.** A different denominator on every
  point outgrows 256 bits from 58 points, and the answer is `Overflow`
  ([numeric headroom](numeric-headroom.md#least-squares-realistic-and-one-stress-control)).
  A unit that does not measure the output, or one with an offset, is refused
  at compile time, and so is `Rep = double`.
- **Only an operation that computes in wider integers answers further.** Any
  other operation's rounded output is computed in `Rational`, rounded
  exactly, and fails with `Overflow` where `opaque_output` would.
- `rounded<...>(opaque_output<"slope">(fit))` keeps its meaning: the exact
  slope, rounded afterwards, which overflows where the exact slope does. Each
  output used runs the whole call, rounded or not.

## A line through observations

A method often fits every determination that meets a condition, so how many
points there are is data, not part of the formula. Read them as raw
observations, `observations<Q, Capacity>` ([statistics](statistics.md)
introduces them), and the fit takes as many as were made:

```cpp
constexpr auto observedFit = formula::linear_least_squares(
    formula::observations<Elapsed, 64>,
    formula::observations<Length, 64>,
    { .title = "Rate of change", .reference = "Example Standard 12", .section = "5.1" });
```

**Pairing is by row.** Observation i of each input belongs to row i, so build
every column from the same rows. Inputs whose counts differ make the fit fail
with its own `DomainError`; so do fewer than two observations, points that
are all equal, and values that are all equal. Flat values are refused
because R² would be 0/0, so a flat response never passes an R² acceptance:

```text
flat lengths: argument outside the domain of the operation
```

**Four outputs.** `intercept` and `slope` as before, `r squared` -- the
coefficient of determination, S_xy² / (S_xx S_yy), a bare number -- and
`points`, the number of observations fitted, exact in every representation.
The degrees of freedom are `points` minus two.

```text
1. t = 1 s; 2 s; 4 s; 7 s
2. L = 51/5 mm; 109/10 mm; 121/10 mm; 143/10 mm
3. linear least squares(#1, #2) = intercept = 19/2 mm; slope = 19/28 mm/s; r squared = 1083/1085; points = 4 [inside not shown] [Rate of change, Example Standard 12, 5.1]
4. slope of #3 = 19/28 mm/s
```

The four observations were made in room for 64, and the call's line shows
the four.

### When the exact fractions do not fit

An exact fit through fifty readings at four decimals does not fit
`Rational`. Computed with Python's fractions, the slope is a fraction of 46
and 54 bits, which fits, but the intercept's numerator needs 64 bits and R²
92 bits over 92. `opaque_output` then answers `Overflow` -- for every
output of the call, since its outputs answer or fail together. A formula
that declares the precision it reports a coefficient at -- a unit, decimal
places and a rounding mode, as `rounded<>` does -- gets the correctly
rounded decimal instead, as long as the fit stays within the wide integers
the kernel computes in (`detail/least_squares_kernel.hpp`; beyond them the
answer is `Overflow` again, and the
[numeric headroom](numeric-headroom.md#regression-over-observations-realistic-and-one-stress-control)
page measures where). [Displaying numbers](display.md#values-the-exact-layer-cannot-hold)
explains values the exact layer cannot hold. The example declares the slope
this way:

```cpp
constexpr auto observedSlope = formula::rounded_output<"slope", millimetrePerSecond, formula::DecimalPlaces { 4 },
                                                       formula::RoundingMode::HalfEven>(observedFit);
```

```text
round(linear least squares(t(i), L(i)).slope, to 4 dp of mm/s)
1. t = 1 s; 2 s; 4 s; 7 s
2. L = 51/5 mm; 109/10 mm; 121/10 mm; 143/10 mm
3. linear least squares(#1, #2) = intercept, slope, r squared, points: rounded where used [inside not shown] [Rate of change, Example Standard 12, 5.1]
4. round(slope of #3, to 4 dp of mm/s) = 3393/5000 mm/s [nearest, ties to even]
```

```text
fifty readings at 4 decimals, exact: overflow in exact arithmetic
fifty readings at 4 decimals, rounded: slope 3.1707 mm/s, intercept 2406.6455 mm, r squared 0.999996
```

The slope is 3.1707 mm/s at four decimals (a floor would give 3.1706), the
intercept 2406.6455 mm, and R² 0.999996 floored at six decimals (to nearest
it would be 0.999997).

### R² as an acceptance

A constraint can accept a fit by its R². `RoundingMode::Floor` makes the
rounding never lift a fit over the line:

```cpp
constexpr auto closeEnough = formula::constraint(
    formula::rounded_output<"r squared", unit::One, formula::DecimalPlaces { 4 }, formula::RoundingMode::Floor>(observedFit)
        >= formula::constant<unit::One>(formula::Rational { 998, 1000 }),
    formula::Verdict { "repeat the readings" });
```

```text
r squared at 4 dp, floored, at least 0.998: satisfied
```

### In `double`, and against a temperature in degrees Celsius

`checked_evaluate_si<double>` fits observations approximately, in coherent
units, with nothing traced: it is the exploratory route, and a design the
exact route answers may be refused there (the section on several regressors
states the tolerance).

The fit sees coherent units, so a regressor in degrees Celsius is fitted in
kelvin: the slope per kelvin is the slope per degree Celsius, but the
intercept is the value at 0 K. The value at 0 °C is the intercept plus the
slope times 273.15 K, written as a formula over the two outputs.

## Several regressors

`multiple_least_squares` fits a constant and one coefficient per regressor.
The regressors are held by `regressors(...)`, because a parameter pack
cannot stand before the values and the citation; they come first and the
values last, as a curve has points then values:

```cpp
constexpr auto byTemperatureAndContent = formula::multiple_least_squares(
    formula::regressors(formula::observations<Temperature, 64>, formula::observations<Content, 64>),
    formula::observations<Length, 64>,
    { .title = "Length by temperature and content", .reference = "Example Standard 12", .section = "5.3" });
```

Its outputs are `constant`, `coefficient 1` to `coefficient K` (one-based,
K from 1 to 8), `r squared` and `points`. Each coefficient is in the
values' dimension over its regressor's. Six rows -- a temperature in degrees
Celsius, a content in percent, and a length -- give:

```text
1. T = 113/10 °C; 137/10 °C; 179/10 °C; 191/10 °C; 233/10 °C; 297/10 °C
2. w_c = 23/10 %; 31/10 %; 29/10 %; 41/10 %; 37/10 %; 43/10 %
3. L = 2588/25 mm; 10413/100 mm; 10433/100 mm; 1051/10 mm; 10521/100 mm; 106 mm
4. multiple least squares(#1, #2, #3) = constant = 22365154943/276592800 mm; coefficient 1 = 346407/4609880000 m/K; coefficient 2 = 19214255/345741 mm; r squared = 27398849648/27403085919; points = 6 [inside not shown] [Length by temperature and content, Example Standard 12, 5.3]
5. coefficient 1 of #4 = 346407/4609880000 m/K
```

Coefficient 1 is shown in the coherent `m/K`, because degrees Celsius have an
offset and are never borrowed as a unit. The fit sees kelvin, so the
constant is the length at 0 K and 0 % content: the length at 0 °C is a
formula over two outputs. Coefficient 2 is per unit of content, a fraction, so
a method that reports it per percent declares a unit of mm per %:

```cpp
constexpr auto lengthAtZeroCelsius = formula::rounded<unit::Millimetre, formula::DecimalPlaces { 2 },
                                                      formula::RoundingMode::HalfEven>(
    formula::opaque_output<"constant">(byTemperatureAndContent)
    + formula::opaque_output<"coefficient 1">(byTemperatureAndContent)
          * formula::constant<unit::Kelvin>(formula::Rational { 27315, 100 }));
```

```text
coefficient 1: 0.0751 mm/K, coefficient 2: 0.5557 mm/%, length at 0 degrees Celsius: 101.39 mm
```

### A singular design is an error, not a number

Two regressors that measure the same thing, twice over, have no unique fit
whatever was observed. The fit refuses them:

```text
a delay twice the elapsed time on every row: argument outside the domain of the operation
```

| design | exact (`opaque_output`, `rounded_output`) | `double` (`checked_evaluate_si<double>`) |
|---|---|---|
| one regressor a multiple of another (`x2 = 2 x1`) | `DomainError` | `DomainError` |
| one regressor offset from another (`x2 = x1 + 273.15`, the same temperature in kelvin) | `DomainError` | `DomainError` |
| an affine combination (`x2 = 3 x1 - 7`) | `DomainError` | `DomainError` |
| nearly collinear: 1 - R² of x2 on x1 about 2 * 10⁻⁹ | answered, exactly | answered |
| nearly collinear: about 5 * 10⁻¹⁰ | answered, exactly | `DomainError`, by the tolerance |
| the same quantity read twice | refused where it is written | refused where it is written |

The first three rows and the two nearly collinear ones are pinned by
`test/least_squares_kernel_tests.cpp` and
`test/multiple_least_squares_tests.cpp`; the last row by the refusals in
`test/negative/`.

**The tolerance, stated.** In `double` a design of several regressors is taken
for singular when a
pivot of the centred normal equations is at or below 10⁻⁹ of its diagonal --
when 1 - R² of a regressor on the ones before it is at or below 10⁻⁹. Rounded
data cannot decide exact singularity, and this route promises no digits.
The exact routes decide it exactly. Fewer than
K + 1 rows, a flat regressor and flat values are the fit's own `DomainError`;
no regressor, more than eight, anything but raw observations, and a call
without a citation are refused where they are written.

## A citation is required

Every call names a citation. An operation's inside is exactly what the page
cannot show, so the reference that defines it is what a reader has instead.
The citation argument has no default, so a call without one does not compile;
`linear_least_squares` refuses it in the library's own words. `{}` is allowed, deliberately, for an operation a
method uses without citing. The page then says so, where a citation would
stand:

```text
2. series span(#1) = lowest = 103 g; highest = 191 g; span = 88 g [inside not shown] (no citation given)
```

## A retry

A retry repeats one attempt expression, at most a fixed number of times, until
an acceptance holds. The fixture is an invented iterated estimate:
`w(k) = 6.08 g + w(k-1) / 2`, starting from 0 g, accepted once it rose by at
most 0.76 g. The sequence rises, so the acceptance is written
`w(k-1) - w(k) >= -0.76 g`:

```cpp
constexpr auto halving = formula::constant<unit::Gram>(formula::Rational { 152, 25 })
                         + formula::previous_attempt<Estimate> / formula::Rational { 2 };
constexpr auto settled = formula::previous_attempt<Estimate> - formula::this_attempt<Estimate>
                         >= formula::constant<unit::Gram>(formula::Rational { -19, 25 });
constexpr auto fromZero = formula::starting_from(formula::constant<unit::Gram>(formula::Rational { 0 }));
constexpr formula::Verdict repeatDetermination { "repeat the determination" };
constexpr formula::Citation settledCitation { .title = "Settled estimate",
                                              .reference = "Example Standard 12",
                                              .section = "6" };

constexpr auto fourAttempts = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(
    fromZero, halving, settled, repeatDetermination, settledCitation);
```

- **`previous_attempt<R>`** is the attempt before, or the starting value at
  the first attempt; **`this_attempt<R>`** is the value just produced, and is
  read only in the acceptance; **`attempt_number`** is the method's own `k`,
  from 1. Each is refused where it is evaluated outside a retry. A retry that
  names another quantity than its own result is refused where it is written,
  so a page never labels the retry's value as another quantity's, and so is one
  whose starting value reads any of the three: it runs before any attempt.
- **The number of attempts is a template argument**, at most 64. The loop is a
  `for` over it: no path runs more attempts, and none depends on the data to
  stop.
- **The verdict must say something.** A retry built in a constant expression
  with a blank verdict does not compile; one built at run time is refused when
  it is evaluated, before any attempt.

It renders as the method states it:

```text
up to 4 attempts: w(k) = 152/25 g + w(k-1) / 2, starting from w(0) = 0 g; accept when w(k-1) - w(k) >= -19/25 g; otherwise: repeat the determination
```

A retry is not a node, and is evaluated at the top by
`checked_evaluate_retry`, or traced by `explain_retry`. Every attempt's
derivation is traced, then the attempt with its judgement, then how the retry
ended:

```text
31. attempt 3: w(k) = #26 = 266/25 g; judged #29 >= #30: rejected
32. 152/25 g
33. w(k-1) = 266/25 g
34. 2
35. #33 / #34 = 133/25000
36. #32 + #35 = 57/5000
37. w(k-1) = 266/25 g
38. w(k) = 57/5 g
39. #37 - #38 = -19/25000
40. -19/25 g
41. attempt 4: w(k) = #36 = 57/5 g; judged #39 >= #40: accepted
42. w = retry: accepted at attempt 4 of 4 = 57/5 g [Settled estimate, Example Standard 12, 6]
```

### Six ways to end

A retry ends in exactly one of six ways (`RetryEnd`), and the example runs
each. Each line below says how it ended, and the line its trace ends with:

```text
allowed four: Accepted after 4 attempt(s)
  42. w = retry: accepted at attempt 4 of 4 = 57/5 g [Settled estimate, Example Standard 12, 6]
allowed three: Exhausted after 3 attempt(s)
  32. w = retry: exhausted after 3 of 3: repeat the determination [Settled estimate, Example Standard 12, 6]
tolerance not measured: NotJudgeable after 1 attempt(s)
  14. w = retry: not judgeable at attempt 1 [Settled estimate, Example Standard 12, 6]
third determination missing: NotRecorded after 3 attempt(s)
  12. d_a = retry: attempt 3 not recorded [Agreed determination, Example Standard 12, 7]
divides by k - 1: Failed, division by zero at attempt 1
  12. w = retry: failed at attempt 1: division by zero [Settled estimate, Example Standard 12, 6]
typed in by a person: ManuallyEntered after 0 attempt(s); nothing traced
```

- **Accepted:** the acceptance held; the outcome is that attempt's value.
- **Exhausted:** the acceptance never held in the attempts allowed. The
  outcome is **the method's verdict, not a missing value and not the last
  attempt's value.** A method that says "repeat the determination" when a step
  never settles has decided something, and a result that read as "not
  measured", or as the last value computed, would hide the decision.
- **NotJudgeable:** an attempt's value, or its judgement, was absent -- here
  the tolerance the acceptance compares with was not measured. An absent
  comparison is "cannot tell", never "try again", so the retry stops.
- **NotRecorded:** an attempt needed a recorded determination that nobody
  recorded (see below). Nothing was compared.
- **Failed:** an attempt or its judgement failed arithmetically. There is no
  outcome, only the failure, naming the attempt. A starting value that fails
  is at `RetryFailure::atStartingValue`, a position no attempt has, so it is
  never read as the first attempt's failure.
- **ManuallyEntered:** the environment holds a value a person typed in for the
  result. It is returned, and no attempt runs: a person's entry is never
  replaced by a computation.

### Judged from which attempt

`FirstJudged` is required, with no default. "Until the result reaches a
threshold" can be judged at the first attempt; "until two successive results
agree" cannot, since at the first attempt there is only one result.
`FirstJudged::AtSecondAttempt` does not judge the first attempt, which is not
the same as rejecting it.

A retry with no starting value has no value before its first attempt, so
`previous_attempt` read there is the author's mistake. It is not taken for
zero and not for "not measured": the attempt fails, and its trace says why:

```text
2. w(k-1) = previous attempt: none before attempt 1
```

### Two successive results agree

A retry can read a new recorded determination at each attempt:
`attempt_input<Q>` reads element `k - 1` of a series of `Q` holding one
determination per attempt allowed. "Accepted when two successive results agree
within 1.27 g" compares the absolute difference, written with `abs`. A
`when` choosing the larger minus the smaller says the same, at more length:

```cpp
constexpr auto agree = formula::abs(formula::this_attempt<Agreed> - formula::previous_attempt<Agreed>)
                       <= formula::constant<unit::Gram>(formula::Rational { 127, 100 });

constexpr auto successive = formula::retry<Agreed, 4, formula::FirstJudged::AtSecondAttempt>(
    formula::attempt_input<Determination>,
    agree,
    formula::Verdict { "repeat the test" },
    { .title = "Agreed determination", .reference = "Example Standard 12", .section = "7" });
```

```text
up to 4 attempts: d_a(k) = d(k); accept from attempt 2 when abs(d_a(k) - d_a(k-1)) <= 127/100 g; otherwise: repeat the test
41.3, 43.9, 42.7, 45.7 g: Accepted after 3 attempt(s)
  17. d_a = retry: accepted at attempt 3 of 4 = 427/10 g [Agreed determination, Example Standard 12, 7]
```

43.9 and 41.3 g differ by 2.6 g; 42.7 and 43.9 g by 1.2 g, so the third
attempt is accepted with 42.7 g. The fourth determination is never read. A
determination the method needed but nobody recorded ends the retry
`NotRecorded` at that attempt, as the six ways above show; one after the
attempt that ends the retry is never read, recorded or not. A series a person
typed in says so on every determination read from it, as any typed-in input
does.

## A retry is not an outlier rejection

[Statistics, outliers and precision](statistics.md) rejects outliers from a
sample pass by pass, and that loop also ends in the author's verdict when it
rejects too much. The two differ in what repeats. A rejection re-reads one
sample that was measured once, removing values from it; every pass sees less
of the same data. A retry evaluates its attempt again, and each attempt makes
a new value -- the next step of an iteration, or a new determination read at
that attempt. A rejection answers "which of these values belong", a retry
"when has the step settled".

## What this is not

- **Not a solver.** A retry runs one attempt expression the method states, a
  bounded number of times. It finds no root, minimises nothing, and never
  chooses its own next step; the method's attempt expression does.
- **Not a place for other constructions.** Hysteresis state machines, set
  partitioning and graphical constructions remain out of scope. An opaque
  operation can compute a value any of them would, from values; the library
  does not model them.

## Every citation here is invented

Every citation on this page, in `examples/opaque_and_retry.cpp` and in the
gallery names a fictional `Example Standard`, for the reason
[Citations and rendering](citations.md) gives. Every number is invented.
