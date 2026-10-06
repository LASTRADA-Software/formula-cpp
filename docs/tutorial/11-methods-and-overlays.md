# 11. Methods and overlays

The strength of a cube and the strength of a cylinder are calculated by two
different formulas, and which one applies depends on the specimen, not on a
number. This chapter declares one method with a variant for each shape, and
then changes how the method rounds for one jurisdiction with an overlay.
Each trace says which variant ran and whose rounding rule applied.

The program is self-contained: it declares its own quantities and does not
build on the previous chapters' programs.

## One method, several variants

A variant applies to a kind of specimen, and the kind is named by a tag: an
empty struct, never instantiated:

```cpp
--8<-- "examples/tutorial/11_methods_and_overlays.cpp:tags"
```

The cube is measured by the two sides of its loaded face, the cylinder by its
diameter:

```cpp
--8<-- "examples/tutorial/11_methods_and_overlays.cpp:quantities"
```

`formula::method(variants, rounding rule, constraints)` builds a method from
its three parts, in that order:

```cpp
--8<-- "examples/tutorial/11_methods_and_overlays.cpp:method"
```

- `formula::variants(...)` lists the variants, each written
  `formula::variant<Tag>(expression)`. The cube's strength is the load over
  `a * b`, the cylinder's the load over `pi * d^2 / 4`. `formula::pi` is the
  library's exact fraction for pi, 245850922/78256779. Every variant must
  measure the same dimension, here a stress.
- `formula::rounding_rule<tenthMpa>()` rounds whichever variant ran, to one
  decimal place of a megapascal, ties away from zero.
- `formula::constraints()` is the list of checks a result must pass; this
  method has none.

`formula::explain_method<Tag>(method, environment)` runs the variant `Tag`
names, rounds the result by the method's rule, and records each step. Its
`outcome` is a `std::expected` that holds an error or a `std::optional`,
empty when a measurement the variant reads is missing; a value is in the
coherent SI unit, here pascals. Its `trace` is the trace:

```cpp
--8<-- "examples/tutorial/11_methods_and_overlays.cpp:show"
```

The tag is always stated by the caller, who knows what the specimen is. A tag
the method has no variant for does not compile.

## The specimens

A cube of 150 mm by 150 mm carried 675 kN, and a cylinder of 150 mm diameter
carried 530 kN:

```cpp
--8<-- "examples/tutorial/11_methods_and_overlays.cpp:specimens"
```

Each environment holds only what its variant reads.

```cpp
--8<-- "examples/tutorial/11_methods_and_overlays.cpp:base"
```

The cube's strength is 675 kN over 22500 mm2, exactly 30 MPa. The cylinder's
loaded area is pi times 22500 mm2 over 4, about 17671.5 mm2, and its strength
530 kN over that area, about 29.99 MPa, which the method's rule rounds to
30.0 MPa, written 30 MPa. The last two steps of each trace say whose rule
rounded the value, `(method default)`, and which variant ran, `variant Cube
(1st of 2), selected by tag`.

## An overlay changes a method for a jurisdiction

A jurisdiction's annex may change a method: fix a constant, replace a
variant's formula, drop a variant, or round differently. An overlay lists
those changes, each with the citation of the clause that makes it, and
`formula::apply(overlay, method)` returns a new method with the changes made.
The base method stays as it is.

The invented "Example jurisdiction" rounds the strength to whole
megapascals. A rounding rule keeps a number of decimal places of a unit, so
a coarser step such as 0.5 MPa cannot be stated as one; whole megapascals is
the nearest coarser rule.

```cpp
--8<-- "examples/tutorial/11_methods_and_overlays.cpp:overlay"
```

`formula::with_rounding<rounding>(citation)` replaces the method's rounding
rule. Every overlay operation needs a citation; an operation without one does
not compile. The overlaid method is run exactly like the base method:

```cpp
--8<-- "examples/tutorial/11_methods_and_overlays.cpp:jurisdiction"
```

Both strengths round to 30 MPa under the jurisdiction's rule too. What
differs is the rounding step: it reads `rounded to 0 dp`, and names the
jurisdiction's overlay and its citation in place of `(method default)`. A
reader of the trace can see which rule applied and where it comes from.

## Output

```text
--8<-- "examples/tutorial/11_methods_and_overlays.expected.txt"
```

## Summary

- `formula::method(variants, rounding_rule, constraints)` -- one method, its
  variants, the rule that rounds whichever ran, and its checks.
- `formula::variant<Tag>(expression)` -- the formula for the specimens `Tag`
  names.
- `formula::explain_method<Tag>(method, environment)` -- runs the variant for
  `Tag`, rounds it, and records the steps; the value is in the coherent SI
  unit.
- `formula::overlay(...)` -- a jurisdiction's changes to a method, each with
  its citation.
- `formula::with_rounding<rounding>(citation)` -- replaces the method's
  rounding rule.
- `formula::apply(overlay, method)` -- the method with the overlay's changes
  made; the base method is unchanged.

## Further reading

- [A method: variants, tags, and one rounding rule](../methods-and-overlays.md#a-method-variants-tags-and-one-rounding-rule)
- [An overlay yields a method](../methods-and-overlays.md#an-overlay-yields-a-method)
- [Methods and jurisdiction overlays](../methods-and-overlays.md), which also covers constants, replaced and pruned variants, and constraints
- [API reference](https://lastrada-software.github.io/formula-cpp/api/)
