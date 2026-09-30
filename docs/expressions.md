# Expressions and evaluation

`formula-cpp` provides a formula written with ordinary operators
(`formula::var`, `+`, `-`, `*`, `/`, `formula::pow`, `formula::sqrt`,
`formula::cbrt`, `formula::root`, `formula::pi`), a set of inputs keyed by
quantity (`formula::Environment`), and two ways to turn a formula and an
environment into a number (`formula::checked_evaluate_si`,
`formula::checked_evaluate`). The result is never a bare number: it is
`formula::Outcome<Q>`, a value, an absence, a verdict or an invalidation, each
carrying where it came from.
This page explains how to write a formula, where a dimensional mistake shows
up, how the environment supplies and withholds values, how absence and
provenance travel through evaluation, and how to choose between an exact and
an approximate answer. The worked example is `examples/expressions.cpp`;
every block on this page formatted as program output is copied verbatim from
that program's actual output, not worked out by hand.

## Writing a formula

A formula is a *type*, not a computation. `formula::var<Q>` is the spelling of
a variable, and the arithmetic operators build a tree out of it at compile
time:

```cpp
inline constexpr auto waterCementRatio = formula::var<WaterVolume> / formula::var<CementVolume>;
```

`waterCementRatio`'s type is
`formula::BinaryNode<formula::BinaryOperator::Divide, formula::VarNode<WaterVolume>, formula::VarNode<CementVolume>>`.
Nothing has been computed by this line -- `decltype(waterCementRatio)` is the
whole formula, and every node publishes
`static constexpr Dimension dimension`, computed at class scope from its
operands', so the dimension is known before any value is. A node declares no
members of its own; its whole
shape is in the type. It is not literally an empty class, though: a node
holds its children **by value**, and an empty child still occupies a byte, so
a tree costs roughly one byte per leaf and nothing per level -- measured, a
five-leaf tree was 5 bytes on cl and clang-cl and 9 on g++
(`test/expression_tests.cpp`,
`"expression: a deep tree carries no state beyond its constants"`). The exact
number is not portable and is not the point; the point is that traversing the
tree inlines away and there is no per-node state beyond the constants a
formula actually names. `formula::ConstantNode` is the one node with real
state, because a coefficient may arrive from a table at runtime rather than
being written into the formula.

## Where a dimensional error appears

Because the operator the user wrote is what instantiates the node, a
dimensional mistake is a compile error on the line the formula is written on,
not a runtime one discovered when the formula finally runs.
`test/negative/quantity_alias_add_dimension_mismatch.cpp` pins this, with
the two quantities declared by alias as [the quantities guide](quantities.md)
declares them (`test/negative/expression_add_dimension_mismatch.cpp` pins the
same refusal for quantities declared by struct):

```cpp
using Volume = formula::Quantity<struct VolumeTag, "V", "a volume", formula::unit::Litre>;
using Length = formula::Quantity<struct LengthTag, "L", "a length", formula::unit::Metre>;

// A volume plus a length has no meaning, and must not compile.
inline constexpr auto broken = formula::var<Volume> + formula::var<Length>;
```

Attempting this gives, verbatim but for the paths, which are shown relative to
the repository, on MSVC's `cl.exe` (19.51, from Visual Studio's `cl-debug`
preset):

```
include\formula-cpp/expression.hpp(186): error C2338: static assertion failed: 'formula: the two sides of this addition or subtraction measure different dimensions; the offending operands appear in this diagnostic as the template arguments of RequireAddendsAgree'
include\formula-cpp/expression.hpp(186): note: the template instantiation context (the oldest one first) is
test\negative\quantity_alias_add_dimension_mismatch.cpp(12): note: see reference to function template instantiation 'auto formula::operator +<formula::VarNode<Volume>,formula::VarNode<Length>>(Left,Right) noexcept' being compiled
        with
        [
            Left=formula::VarNode<Volume>,
            Right=formula::VarNode<Length>
        ]
include\formula-cpp/expression.hpp(271): note: see reference to class template instantiation 'formula::BinaryNode<formula::BinaryOperator::Add,formula::VarNode<Volume>,formula::VarNode<Length>>' being compiled
include\formula-cpp/expression.hpp(244): note: see reference to class template instantiation 'formula::detail::AdditiveDimensionsAgree<formula::BinaryOperator::Add,Left,Right>' being compiled
        with
        [
            Left=formula::VarNode<Volume>,
            Right=formula::VarNode<Length>
        ]
include\formula-cpp/expression.hpp(203): note: see reference to class template instantiation 'formula::detail::RequireAddendsAgree<Left,Right>' being compiled
        with
        [
            Left=formula::VarNode<Volume>,
            Right=formula::VarNode<Length>
        ]
```

The message names the two operand types (`VarNode<Volume>`, `VarNode<Length>`)
and the formula's own source line, not an opaque template instantiation
failure. The check is written as a named class template,
`formula::detail::RequireAddendsAgree<Left, Right>`, templated on the
*operands* rather than on the operator, entirely for this diagnostic's sake:
an assertion whose condition mentions the operator makes clang print
`(formula::BinaryOperator)0` in its "due to requirement" clause, because an
enumerator used as a value in a dependent expression is rendered as a cast.
Written this way, all three compilers (cl, clang-cl and g++) name the two
operand types instead. How they name them depends on how the quantities were
declared: here cl names each alias by the alias, as it usually does, while
clang and g++ print the `formula::Quantity` specialisation the alias stands
for, tag first --
`VarNode<formula::Quantity<VolumeTag, formula::detail::FixedString<2>{"V"}, ...>>`
-- which is why a tag is best named after its quantity. A quantity declared by
struct is named by the struct's own name on all three.
`examples/expressions.cpp` shows the same mistake as a
comment rather than as compiled code, since it must not fail the build:

```cpp
// Diameter measures length; Area measures length squared. Adding them has no
// meaning, and the expression layer refuses it at the formula's own source
// line, not at evaluation:
//
//     constexpr auto broken = formula::var<Diameter> + formula::var<Area>;
//
// Uncommenting the line above does not compile.
```

Multiplication and division impose nothing on the operands' dimensions -- a
volume divided by a mass is a perfectly good density -- so only `+` and `-`
carry this check; `*` and `/` combine the two dimensions instead of requiring
them to agree.

The check covers money too, once each currency is a base dimension of its own
([Base dimensions the SI does not have](dimensions.md#base-dimensions-the-si-does-not-have)).
A price in euros plus a pure number, and a price in euros plus a price in yen,
have the same SI exponents on both sides -- all zero -- so only the named base
dimensions tell the sides apart, and each addition fails to compile with
exactly one error, the message above: `test/negative/money_plus_number.cpp`
and `test/negative/money_plus_other_currency.cpp` pin both. Multiplying is
where money composes: a tariff in euros per kilowatt-hour times an energy is
euros.

## The environment

`formula::Environment` is a set of inputs keyed by quantity **type**, not by
name. It is built with `formula::environment(...)`, one entry per quantity,
each either a plain `formula::Measured<Q>` (an observation) or
`formula::entered(Measured<Q>{...})` (a value a person typed in). Asking for a
quantity the environment does not hold is a compile error naming that
quantity, never a runtime lookup failure and never a zero.
`test/negative/quantity_alias_environment_missing.cpp` pins this (and
`test/negative/environment_missing_quantity.cpp` for quantities declared by
struct):

```cpp
using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", formula::unit::Litre>;
using Ratio = formula::Quantity<struct RatioTag, "w/c", "water/cement ratio", formula::unit::One>;

// An environment that does not hold Ratio; asking for it is a compile error.
inline constexpr auto env = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 183 } });

int main()
{
    return env.get<Ratio>().has_value() ? 0 : 1;
}
```

which gives, verbatim but for the paths, shown relative to the repository,
on MSVC's `cl.exe` (19.51, `cl-debug` preset):

```
include\formula-cpp/environment.hpp(439): error C2338: static assertion failed: 'formula: this environment provides no value for this quantity; the quantity and the environment appear in this diagnostic as the template arguments of RequireProvided'
include\formula-cpp/environment.hpp(439): note: the template instantiation context (the oldest one first) is
test\negative\quantity_alias_environment_missing.cpp(15): note: see reference to function template instantiation 'formula::Measured<Ratio> formula::Environment<formula::Measured<WaterVolume>>::get<Ratio>(void) noexcept const' being compiled
test\negative\quantity_alias_environment_missing.cpp(15): note: see the first reference to 'formula::Environment<formula::Measured<WaterVolume>>::get' in 'main'
include\formula-cpp/environment.hpp(584): note: see reference to class template instantiation 'formula::detail::RequireProvided<Ratio,formula::Environment<formula::Measured<WaterVolume>>>' being compiled
```

The prototype this layer replaced answered a missing input with a runtime
`assert`, reachable only by actually running the failing path. Here, the
question "does this environment supply everything the formula needs" is
answered once, while the formula is being written, for every input the
formula names -- there is no code path left in which it is answered wrong.
Supplying the same quantity twice is refused the same way, by
`formula::detail::RequireDistinctQuantities`: first-wins and last-wins are
equally arbitrary, and the caller meant exactly one of them, so neither is
guessed.

## Absence propagates

`formula::Measured<Q>` may be absent -- **not zero**. Zero is a measurement;
an unmeasured quantity starting at zero would produce a confident wrong
answer, which is exactly the failure this layer exists to prevent. Evaluation
carries that rule all the way through a formula: if any input a formula
actually needs is absent, the whole result is absent, never a number computed
from whatever happened to be present.

```cpp
constexpr auto partial =
    formula::environment(formula::Measured<WaterVolume>::absent(), formula::Measured<CementVolume> { rat(300) });
constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, partial);
```

(`test/evaluate_tests.cpp`,
`"evaluate: an absent input makes the whole result empty, not zero"`.) The
same rule holds through a power:
`examples/expressions.cpp` measures no diameter and asks for the resulting
area:

```
area with no diameter measured: empty
```

Absence and an arithmetic error are different facts and are not conflated:
when one side of a binary operation is absent and the other side's own
arithmetic fails, the arithmetic error wins, because it is still an error
even though the other side happened to have nothing to say
(`test/evaluate_tests.cpp`,
`"evaluate: an arithmetic error on one side outranks absence on the other"`).
Absence is only ever reported once both sides have actually been evaluated.

## The outcome

`formula::checked_evaluate<Result>` returns
`std::expected<Outcome<Result>, ArithmeticError>`, and `formula::Outcome<Q>`
is a sum type with four alternatives (`formula::OutcomeKind`):

- **`Value`** -- a number, together with `formula::ValueSource`: `Derived`
  (computed by the library), `Measured` (an observation), or
  `ManuallyEntered` (typed in by a person, replacing what would have been
  computed).
- **`Empty`** -- no value, because an input the formula needs was never
  measured. This is `Outcome::value(...)` given an absent measurement, not a
  separate code path -- so a caller checking `is_value()` cannot receive an
  empty `Value` and call it a number.
- **`Verdict`** and **`Invalid`** -- a decision rather than a number
  (`"reject the specimen"`) or a reason a result was discarded entirely.
  Evaluating a formula produces neither. Outlier rejection
  ([Statistics, outliers and precision](statistics.md)) and bounded retry
  ([Opaque operations and bounded retry](opaque-and-retry.md)) report a
  `Verdict` through this type, so that a decision has somewhere to go without
  a fifth alternative meaning "everything above, but different".

`Outcome::is_overridden()` is true exactly when `is_value()` and the source
is `ManuallyEntered` -- there is deliberately no separate `Overridden`
alternative, so provenance has exactly one home and a kind can never
contradict a source. When the environment holds an `entered` value for the
quantity being evaluated, `checked_evaluate` returns that value with
`ValueSource::ManuallyEntered` **without evaluating the formula at all** --
proven in `test/evaluate_tests.cpp` by an override whose formula would
divide by zero: the override still wins, because the formula is never
reached. `examples/expressions.cpp` overrides a computed ratio and prints
both the value and the fact that it was entered:

```
w/c = 0.500000 (entered)
```

## Choosing a representation

Two entry points evaluate a formula, and they answer different questions:

- **`formula::checked_evaluate_si<Rep>(node, environment)`** is the
  representation-agnostic core. Every leaf is converted to the coherent unit
  of its dimension -- the SI unit, times one of each
  [named base dimension](dimensions.md#base-dimensions-the-si-does-not-have)
  it carries -- on the way in, the tree is evaluated there, and the answer
  comes back as `Rep` -- exact `formula::Rational` by default, or `double`
  when a formula needs values an exact rational cannot hold.
- **`formula::checked_evaluate<Result>(expression, environment)`** is the
  auditable entry point, and it is **always exact**: it evaluates in
  `Rational`, converts the answer once into `Result`'s own declared unit, and
  returns `Outcome<Result>`. A result that goes into an audit trail as binary
  floating point would have to explain itself every time it was read back,
  so this entry point does not offer the choice.

`formula::RepTraits<Rep>` teaches each representation its own arithmetic --
`Rational`'s through the `checked_` functions in `rational.hpp`, `double`'s
directly, with `Overflow` deliberately not reported by that arithmetic itself
(`inf` is what a `double` says, and the caller asked for `double`). That is
not the same as saying a `double` evaluation never sees `Overflow`: every leaf
is converted to the coherent unit in exact `Rational` before it is handed
to `RepTraits<double>`, and that conversion can overflow -- a quantity whose
declared unit puts it near the edge of the representable range reports
`Overflow` from `checked_evaluate_si<double>` exactly as it would from the
exact representation, before the `double` arithmetic ever runs. The primary
template is undefined, so a representation nobody has taught the library
fails at the point of use, naming itself.

Both entry points also convert every leaf as a **point** on its unit's scale,
never as a difference -- `checked_convert`'s own documentation says so, and
the evaluator does not qualify it further. The library ships two affine units,
degrees Celsius and degrees Fahrenheit, and each therefore behaves as an
absolute temperature inside a formula, not as a delta: 20 °C minus 15 °C is
exactly 5 K once both leaves have been converted to the coherent SI unit
(kelvin) and subtracted there, but reading that same computed 5 K back through
a result quantity declared in degrees Celsius gives −268.15, because the
conversion adds the offset the point 5 K sits at, not the offset the interval
spans. Degrees Fahrenheit converts a leaf as a point in the same way, with a
degree of 5/9 K: a leaf of 98.6 °F enters a formula as exactly 310.15 K, which
is 37 °C. A quantity that represents a *difference* -- a temperature swing, not
a temperature -- must declare a non-offset unit such as kelvin; declaring it in
an affine unit asks the library a different question than the one intended.
`test/evaluate_tests.cpp`'s `"evaluate: an offset unit converts a point, not a
difference"` pins today's behaviour, so a change to it is deliberate rather than
accidental.

## Powers, roots and pi

`formula::pow<Exponent>(operand)`, `formula::sqrt(operand)`,
`formula::cbrt(operand)`, `formula::root<Degree>(operand)` and `formula::pi`
are nodes, not free functions applied to an already-evaluated number, because
a power or root acts on the *dimension* as well as the value: the square of a
length is an area, the cube root of a volume is a length, and a root can
produce a fractional exponent -- exactly why `Dimension`'s exponents are
rational rather than integer (see [`docs/dimensions.md`](dimensions.md)).
`examples/expressions.cpp` computes a circular area from a constant (`pi`)
and a power (`d^2`), with the result declared in a different unit
(`SquareMetre`) from the input (`Millimetre`):

```cpp
constexpr auto circularArea = formula::pi * formula::pow<2>(var<Diameter>) / formula::Rational { 4 };
```

```
circular area of a 103 mm diameter = 0.008332 m2 (computed)
```

`formula::Pi` is a documented rational convergent -- `245850922/78256779`,
which differs from pi by less than 8e-17 -- and is deliberately **not** pi
itself: it is the one approximation the exact layer makes on purpose, written
once so every caller gets the same number and the trace states which number
it was.

`checked_exact_nth_root` answers only when the root **is** a rational number.
The root of 4 is 2 and the root of 9/4 is 3/2, but the root of 2 is
`ArithmeticError::Inexact`, not a nearby fraction:

```cpp
constexpr auto inputs = formula::environment(formula::Measured<Area> { rat(2) });
constexpr auto computed = formula::checked_evaluate<Edge>(formula::sqrt(var<Area>), inputs);

STATIC_REQUIRE_FALSE(computed.has_value());
STATIC_REQUIRE(computed.error() == formula::ArithmeticError::Inexact);
```

(`test/function_tests.cpp`,
`"function: an inexact root is refused by the exact representation"`.)
`Inexact` is not a limitation the library
apologises for: a layer whose whole promise is "never a wrong number" has no
business rounding an irrational root silently into a rational one. A formula
that genuinely needs an irrational root is evaluated in a representation that
has room for one -- `formula::checked_evaluate_si<double>` answers the same
formula, approximately, on purpose. As a bounded fact rather than program
output -- pinned in the companion test
`"function: the double representation answers where the exact one cannot"` --
`checked_evaluate_si<double>` of that same `sqrt(Area)` formula lands
strictly between 1.41421356 and 1.41421357.

Where the method states the precision the root is reported at, the exact
layer gives that instead: `rounded_sqrt<unit, places, mode>(x)` is the decimal
the true root rounds to, exact, and its trace line carries no `≈`. An output
of an opaque operation whose exact computation leaves `Rational`'s range has
the same form, `rounded_output`, where the operation computes in wider
integers, as `linear_least_squares` does
([Displaying numbers](display.md#values-the-exact-layer-cannot-hold)). The
`double` route stays what it is: approximate, untraced, and for exploring.

There is one further refusal in the same function, for a different reason.
`checked_exact_nth_root` rejects the most negative representable numerator
(`IntMin`) with `ArithmeticError::Overflow` rather than `Inexact`: `IntMin`'s
cube root exists and is exactly representable, but negating `IntMin` to reach
a positive intermediate is signed overflow, undefined behaviour, before the
root is ever taken. `Overflow` names what actually goes wrong; treating it as
`Inexact` would blame the wrong layer.

A root of degree zero names no operation at all and is refused at compile
time, the same way a dimensional mismatch is, by the library's own
`static_assert` message, not by a runtime check:

```cpp
constexpr auto broken = formula::root<0>(formula::var<Area>);
// static_assert message: "formula: a root of degree zero describes no
// operation" -- pinned verbatim in test/negative/function_zero_root_degree.cpp.
```

## Logarithms and exponentials

`formula::ln(operand)`, `formula::log10(operand)` and `formula::exp(operand)`
take the natural logarithm, the decimal logarithm and the exponential of a
formula. They are nodes, as a power and a root are: the page shows `ln(x)`, a
trace names the step, and an overlay's constant reaches inside. Unlike a power,
they do nothing to a dimension, because they accept none.

### The argument is a bare number

The logarithm of 2 m would be ln 2 + ln(m), a number that changes with the
unit the length is read in, so an argument that has a dimension does not
compile. g++ 14 reports:

```text
static assertion failed: formula: the argument of this logarithm or exponential is not dimensionless; ln, log10 and exp take a bare number, and of a quantity they would change with the unit it is read in -- divide it by a reference value of its own dimension, or read it with numeric_value_of; the argument appears in this diagnostic as the template argument of RequireDimensionlessArgument
```

(`test/negative/transcendental_ln_dimensioned.cpp` pins the message's opening
words, and `hygiene.documented-diagnostic-text` that the line above is still
a message the library states.)

There are two ways to a bare number. Divide by a reference value of the same
dimension, here a length over a reference length of 2 m:

```cpp
constexpr auto growth = formula::ln(var<Length> / formula::constant<formula::unit::Metre>(formula::Rational { 2 }));
```

Or, where a method states its formula over a bare number, read a quantity in a
named unit with `numeric_value_of`
([the traced escape hatch](rounding-and-conditionals.md#the-traced-escape-hatch-numeric_value_of)).
A percentage is dimensionless, and is read in the coherent unit, as every
value is:

```cpp
// 1000 % is the number 10, so its decimal logarithm is 1 -- not 3, which reading it in percent gives.
constexpr auto tenfold = formula::environment(formula::Measured<Share> { rat(1000) });
STATIC_REQUIRE(**formula::checked_evaluate_si<formula::Rational>(formula::log10(var<Share>), tenfold) == rat(1));
```

(`test/function_tests.cpp`,
`"function: a percentage is read in the coherent unit under a logarithm"`.)

### Where the value is exact

| Function | Exact at | Elsewhere, in `Rational` |
|---|---|---|
| `ln(x)` | x = 1: 0 | `Inexact` |
| `log10(x)` | x = 10^k, k from -18 to 18: k -- `1000` and `1/1000` alike | `Inexact` |
| `exp(x)` | x = 0: 1 | `Inexact`, however large |

`Inexact` is the exact layer refusing to approximate, as it refuses
`sqrt(2)`. `checked_evaluate_si<double>` answers with `std::log`,
`std::log10` and `std::exp`. As a bounded fact rather than program output --
pinned in
`"function: the double representation answers where the exact one refuses"`
-- it puts ln 2 strictly between 0.69314718 and 0.69314719.

### Declaring a precision

Where the method states the precision the value is reported at, the exact
layer gives that instead: `rounded_ln<places, mode>(x)`,
`rounded_log10<places, mode>(x)` and `rounded_exp<places, mode>(x)` are the
decimal the true value rounds to, computed with integer arithmetic
([values the exact layer cannot hold](display.md#values-the-exact-layer-cannot-hold)).
They are single nodes of their own, not opaque operations. They have no unit,
since the argument and the result are bare numbers:

```cpp
CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfAwayFromZero>(Rational { 2 }) == Rational { 6931, 10000 });
CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfTowardZero>(Rational { 2 }) == Rational { 6931, 10000 });
CHECK(lnAt<DecimalPlaces { 4 }, RoundingMode::HalfEven>(Rational { 2 }) == Rational { 6931, 10000 });
```

(`test/rounded_transcendental_tests.cpp`,
`"rounded_transcendental: ln 2 to 4 dp in every mode and of 1/2 with the directions paired the other way"`,
whose `lnAt` helper evaluates `rounded_ln<Places, Mode>` at the given ratio.)

The places are the method's own, and at most 18; the result must fit a
`Rational` there, which at 18 places means a magnitude below about 9.2, so
the `log10` of a count near 10^18 is reported at 17. Only ln 1, log10 10^k and
exp 0 can tie, and the mode breaks the tie as `rounded<>` does: `log10` of
10^15 at -1 places is 20, 10 or 20 under `HalfAwayFromZero`,
`HalfTowardZero` and `HalfEven`. A rounding the computation cannot decide --
a value within its width, under 2^-118 (relative, for `exp`), of a rounding
boundary -- is `Overflow`, never a guess. `rounded<...>(ln(x))` is not
`rounded_ln`: the plain logarithm fails before the rounding sees a value, as
`rounded<...>(sqrt(x))` does.

### What goes wrong

| Case | `ln`, `log10` | `exp` | `rounded_ln`, `rounded_log10`, `rounded_exp` |
|---|---|---|---|
| argument absent | absent | absent | absent |
| argument failed | its error | its error | its error |
| argument zero or below | `DomainError` | -- | `DomainError` (the logarithms) |
| not a point where the value is rational | `Inexact` | `Inexact` | the rounded decimal |
| result too large at the declared places, or places outside -18 to 18 | -- | -- | `Overflow` |
| rounding not decidable | -- | -- | `Overflow` |
| `checked_evaluate_si<double>` | `std::log`, `std::log10`; `DomainError` for zero, below and NaN | `std::exp`; too large is `+inf` | does not compile, as `rounded_sqrt` does not |

### How they read

| Node | Plain | Markdown | LaTeX |
|---|---|---|---|
| `ln(x)` | `ln(x)` | ``ln(`x`)`` | `\ln\left(x\right)` |
| `log10(x)` | `log10(x)` | ``log10(`x`)`` | `\log_{10}\left(x\right)` |
| `exp(x)` | `exp(x)` | ``exp(`x`)`` | `\exp\left(x\right)` |
| `rounded_ln<DecimalPlaces { 4 }, mode>(x)` | `round(ln(x), to 4 dp)` | ``round(ln(`x`), to 4 dp)`` | `\operatorname{round}_{4}(\ln\left(x\right))` |

The exponential is `\exp`, not `e^{x}`: a power renders its base as an atom,
so `pow<2>(exp(x))` would read `e^{x}^{2}`, which LaTeX refuses. The call is
an atom to what holds it, so `pow<2>(ln(x))` reads `ln(x)^2`. The mode is not
part of the formula's text, as for `rounded<>`; a trace's line carries it.

### A real power

`pow(base, exponent)` in a formula, with an exponent computed from data, is
not offered. A constant rational exponent is already `pow<P>(root<Q>(x))`,
dimension-checked and exact wherever the answer is rational. A data exponent
is `exp(y ln x)`, written `rounded_exp<...>(y * rounded_ln<...>(x))` where a
precision is to be declared, so that both roundings are visible. A dimensioned
base under a run-time exponent would have no compile-time dimension. A real
power, if one is added, would be named `power`, not `pow`.

## Composing a formula from other formulas

A formula is an ordinary value, so it stands wherever a variable or a
constant stands. Give one a name and it can be an operand of the next:

```cpp
constexpr auto waterCementRatio =
    formula::documented(var<WaterVolume> / var<CementVolume>,
                        { .title = "Water/cement ratio", .reference = "Example Standard 1:2020", ... });

// The second formula uses the first by name. Nothing about the first
// declaration anticipated being reused.
constexpr auto mixCost =
    formula::documented(var<UnitPrice> * waterCementRatio,
                        { .title = "Cost of a mix at a given water/cement ratio", ... });
```

There is no separate composition step and no wrapper type. The outer formula
is simply a larger expression tree, so the dimension check, evaluation,
rendering, tracing and `document()` all treat the reused sub-tree the way
they treat any other node.

**Provenance travels upward through the seam.** The outer formula was never
told about the inner one's citation, but `document()` walks the whole tree
and finds it:

```
citations on the outer formula: 2
  - Cost of a mix at a given water/cement ratio [Example Standard 9:2021]
  - Water/cement ratio [Example Standard 1:2020]
```

**The trace names the reused formula as its own step**, which is what an
auditor needs and what the rendering alone does not show — `render()` gives
`c_u * V_w / V_c`, flattened, because `*` and `/` share a precedence and no
parenthesis is needed to preserve the meaning:

```
1. c_u = 250 EUR
2. V_w = 180 l
3. V_c = 300 l
4. #2 / #3 = 3/5
5. #4 = 3/5 [Water/cement ratio, Example Standard 1:2020, 5.4.2, (3)]
6. #1 * #5 = 150
7. #6 = 150 [Cost of a mix at a given water/cement ratio, Example Standard 9:2021, 2.1]
```

Step 5 is the reused formula, carrying its own citation; step 6 consumes it.
`c_u` is a price in euros, a dimension of its own rather than a bare number
([Base dimensions the SI does not have](dimensions.md#base-dimensions-the-si-does-not-have)),
so the cost is in euros too; steps 6 and 7 show no unit only because a
computed step carries no unit symbol of its own
([Tracing](tracing.md#reading-a-derivation)).

One asymmetry is worth knowing before you rely on it. Using the same
sub-formula **twice** in one tree reaches its citation twice, and the citation
list reports it twice — while the symbol table still reports each symbol once.
If you are building a reference list from `.citations`, collapse duplicates
yourself.

`examples/composition.cpp` is this, complete and runnable; its output is
where the blocks above come from.

Composition writes a named formula into every formula that uses it, so a
sub-result two formulas share is evaluated once for each, and nothing
remembers it. When the named parts are values in their own right -- a bill or
a report of many values, each built on the ones before -- define each once
instead, and let a worksheet calculate each once and recalculate only what a
change reaches: [Calculations and worksheets](calculations.md).
