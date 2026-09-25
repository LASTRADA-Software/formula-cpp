# Expressions and evaluation

`formula-cpp` provides a formula written with ordinary operators
(`formula::var`, `+`, `-`, `*`, `/`, `formula::pow`, `formula::sqrt`,
`formula::cbrt`, `formula::root`, `formula::pi`), a set of inputs keyed by
quantity (`formula::Environment`), and two ways to turn a formula and an
environment into a number (`formula::checked_evaluate_si`,
`formula::checked_evaluate`). The result is never a bare number: it is
`formula::Outcome<Q>`, a value, an absence, or -- once a later phase adds
constraints -- a verdict or an invalidation, each carrying where it came from.
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
`test/negative/expression_add_dimension_mismatch.cpp` pins this:

```cpp
struct Volume: formula::Quantity<Volume, "V", "a volume", formula::unit::Litre>
{
};
struct Length: formula::Quantity<Length, "L", "a length", formula::unit::Metre>
{
};

// A volume plus a length has no meaning, and must not compile.
inline constexpr auto broken = formula::var<Volume> + formula::var<Length>;
```

Attempting this gives, verbatim, on MSVC's `cl.exe` (19.51, from Visual
Studio's `cl-debug` preset):

```
D:\formula-cpp\include\formula-cpp/expression.hpp(135): error C2338: static assertion failed: 'formula: the two sides of this addition or subtraction measure different dimensions; the offending operands appear in this diagnostic as the template arguments of RequireAddendsAgree'
D:\formula-cpp\include\formula-cpp/expression.hpp(135): note: the template instantiation context (the oldest one first) is
D:\formula-cpp\test\negative\expression_add_dimension_mismatch.cpp(13): note: see reference to function template instantiation 'auto formula::operator +<formula::VarNode<Volume>,formula::VarNode<Length>>(Left,Right) noexcept' being compiled
        with
        [
            Left=formula::VarNode<Volume>,
            Right=formula::VarNode<Length>
        ]
D:\formula-cpp\include\formula-cpp/expression.hpp(216): note: see reference to class template instantiation 'formula::BinaryNode<formula::BinaryOperator::Add,formula::VarNode<Volume>,formula::VarNode<Length>>' being compiled
D:\formula-cpp\include\formula-cpp/expression.hpp(191): note: see reference to class template instantiation 'formula::detail::AdditiveDimensionsAgree<formula::BinaryOperator::Add,Left,Right>' being compiled
        with
        [
            Left=formula::VarNode<Volume>,
            Right=formula::VarNode<Length>
        ]
D:\formula-cpp\include\formula-cpp/expression.hpp(152): note: see reference to class template instantiation 'formula::detail::RequireAddendsAgree<Left,Right>' being compiled
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
operand types instead. `examples/expressions.cpp` shows the same mistake as a
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

## The environment

`formula::Environment` is a set of inputs keyed by quantity **type**, not by
name. It is built with `formula::environment(...)`, one entry per quantity,
each either a plain `formula::Measured<Q>` (an observation) or
`formula::entered(Measured<Q>{...})` (a value a person typed in). Asking for a
quantity the environment does not hold is a compile error naming that
quantity, never a runtime lookup failure and never a zero.
`test/negative/environment_missing_quantity.cpp` pins this:

```cpp
struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct Ratio: formula::Quantity<Ratio, "w/c", "water/cement ratio", formula::unit::One>
{
};

// An environment that does not hold Ratio; asking for it is a compile error.
inline constexpr auto env = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } });

int main()
{
    return env.get<Ratio>().has_value() ? 0 : 1;
}
```

which gives, verbatim, on MSVC's `cl.exe` (19.51, `cl-debug` preset):

```
D:\formula-cpp\include\formula-cpp/environment.hpp(126): error C2338: static assertion failed: 'formula: this environment provides no value for this quantity; the quantity and the environment appear in this diagnostic as the template arguments of RequireProvided'
D:\formula-cpp\include\formula-cpp/environment.hpp(126): note: the template instantiation context (the oldest one first) is
D:\formula-cpp\test\negative\environment_missing_quantity.cpp(17): note: see reference to function template instantiation 'formula::Measured<Ratio> formula::Environment<formula::Measured<WaterVolume>>::get<Ratio>(void) noexcept const' being compiled
D:\formula-cpp\test\negative\environment_missing_quantity.cpp(17): note: see the first reference to 'formula::Environment<formula::Measured<WaterVolume>>::get' in 'main'
D:\formula-cpp\include\formula-cpp/environment.hpp(171): note: see reference to class template instantiation 'formula::detail::RequireProvided<Ratio,formula::Environment<formula::Measured<WaterVolume>>>' being compiled
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
  Nothing in the expression layer produces either yet: they exist so that a
  later phase's constraints have somewhere to put their answer, without a
  fifth alternative meaning "everything above, but different".

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
  representation-agnostic core. Every leaf is converted to the coherent SI
  unit of its dimension on the way in, the tree is evaluated there, and the
  answer comes back as `Rep` -- exact `formula::Rational` by default, or
  `double` when a formula needs values an exact rational cannot hold.
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
is converted to the coherent SI unit in exact `Rational` before it is handed
to `RepTraits<double>`, and that conversion can overflow -- a quantity whose
declared unit puts it near the edge of the representable range reports
`Overflow` from `checked_evaluate_si<double>` exactly as it would from the
exact representation, before the `double` arithmetic ever runs. The primary
template is undefined, so a representation nobody has taught the library
fails at the point of use, naming itself.

Both entry points also convert every leaf as a **point** on its unit's scale,
never as a difference -- `checked_convert`'s own documentation says so, and
the evaluator does not qualify it further. An affine unit, of which this
library ships one, degrees Celsius, therefore behaves as an absolute
temperature inside a formula, not as a delta: 20 °C minus 15 °C is exactly 5 K
once both leaves have been converted to the coherent SI unit (kelvin) and
subtracted there, but reading that same computed 5 K back through a result
quantity declared in degrees Celsius gives −268,15, because the conversion
adds the offset the point 5 K sits at, not the offset the interval spans. A
quantity that represents a *difference* -- a temperature swing, not a
temperature -- must declare a non-offset unit such as kelvin; declaring it in
an affine unit asks the library a different question than the one intended.
`test/evaluate_tests.cpp`'s `"evaluate: an offset unit converts a point, not a
difference"` pins today's behaviour, so a later phase changes it deliberately
rather than by accident.

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
circular area of a 100 mm diameter = 0.007854 m2 (computed)
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

One asymmetry is worth knowing before you rely on it. Using the same
sub-formula **twice** in one tree reaches its citation twice, and the citation
list reports it twice — while the symbol table still reports each symbol once.
If you are building a reference list from `.citations`, collapse duplicates
yourself.

`examples/composition.cpp` is this, complete and runnable; its output is
where the blocks above come from.
