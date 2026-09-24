# Quantities and measurements

`formula-cpp` provides a compile-time quantity type, `formula::Quantity`, that
carries a variable's own documentation as part of its type; a single
metadata-reading access point, `formula::Describe<T>`, that reaches both our
own types and ones we do not own; and a runtime value that may honestly be
unmeasured, `formula::Measured<Q>`. This page explains why a variable's
identity is a type, how to declare one, how `Describe` works for foreign
types, what it means for a measurement to be absent, how bounds, precision
and conversion behave once absence is possible, and where the limits are. The
worked example below is `examples/quantities.cpp`; every block on this page
that is formatted as program output is copied verbatim from that program's
actual output, not worked out by hand.

## Why a variable is a type

Phase 3 already made a *dimension* a compile-time thing:
`formula::RequireSameDimension<Left, Right>` lets code refuse, at compile
time, to combine two values whose dimensions differ -- adding a volume to a
mass fails to compile, with both exponent vectors spelled out in the
diagnostic (see [`docs/dimensions.md`](dimensions.md)). Quantities take the
same idea one step further and make a *variable* -- not just its dimension,
but its symbol, its description and its unit -- a compile-time thing too.
Declaring `WaterVolume` and `CementVolume` as two distinct types, each
carrying its own symbol/description/unit through a `formula::Quantity` base,
means they are two different, unrelated C++ types even when every one of
their parameters but the tag is identical, and neither is usable where the
other is expected.

`test/negative/quantity_wrong_type.cpp` is exactly this case, kept in the
suite as a negative-compile test:

```cpp
struct WaterVolume: formula::Quantity<WaterVolume, "V", "a volume", formula::unit::Litre>
{
};

struct CementVolume: formula::Quantity<CementVolume, "V", "a volume", formula::unit::Litre>
{
};

void takes_water(WaterVolume);

int main()
{
    takes_water(CementVolume {}); // does not compile
    return 0;
}
```

Attempting this gives, verbatim, on cl.exe:

```
error C2664: 'void takes_water(WaterVolume)': cannot convert argument 1 from 'CementVolume' to 'WaterVolume'
note: No user-defined-conversion operator available that can perform this conversion, or the operator cannot be called
```

The mistake is caught exactly where the wrong call was written, not
discovered later by a runtime check, or -- because the two types agree on
symbol, description and unit -- not discovered at all. That is the payoff of
making a variable's identity a type rather than a runtime tag: a tag has to
be compared at run time to catch the same mistake, and only for the inputs
that happen to be exercised.

## Declaring a quantity

A quantity is declared by deriving from `formula::Quantity`, which takes
exactly four template parameters:

```cpp
struct WaterVolume:
    formula::Quantity<WaterVolume,                    // the type's own name -- the tag
                       "V_w",                          // symbol
                       "volume of water added",        // description
                       formula::unit::Litre>            // unit
{
};
```

**The tag is first, and it earns its place — but not for the reason it is
tempting to give.** It is the type's own name, given back to itself.

The tempting claim is that without the tag, two quantities whose symbol,
description and unit coincide would be the *same* C++ type. For the spelling
above that is false, and this page said it until a reviewer checked. C++ types
are nominal: two separately declared `struct`s are distinct however identical
their base. Measured, with the tag removed from a stand-in template:

```
struct WaterVolume: NoTag<"V", "a volume", unit::Litre> {};
struct CementVolume: NoTag<"V", "a volume", unit::Litre> {};

derived structs are the same type   : 0
aliases are the same type           : 1
their bases are the same type       : 1
a base-taking function accepts both : 2
```

What the tag actually buys is two narrower things, and both are real. It makes
the *alias* spelling impossible to get wrong — with a tag you cannot name two
distinct quantities without distinguishing them, and without one
`using A = Quantity<...>; using B = Quantity<...>;` silently gives one type.
And it keeps the **bases** distinct, so a function taking the base cannot
accept two different quantities, as the last line above shows it otherwise
would.

`examples/quantities.cpp` declares `WaterVolume` and `CementVolume`, alike in
every parameter but the tag:

```
WaterVolume and CementVolume share symbol, description and unit: yes
...but the tag keeps them different types: yes
```

**There is no fifth parameter for the dimension.** A `Unit` already carries
its dimension (`unit.dimension`), so a separate dimension parameter would
state it a second time and let the two disagree. That is not a hypothetical
risk: a spike compiled the five-parameter spelling with `dim::Mass` paired
against `unit::Litre`, and all three compilers accepted the contradiction in
silence. `Quantity::dimension` is derived from the unit instead, so there is
no second place for it to disagree with, and no spelling that lets a caller
write the contradiction at all.

## `Describe<T>`, and foreign types

Nothing above the metadata layer reads a `Quantity` base directly. Everything
-- our own types and types we do not own alike -- goes through one template,
`formula::Describe<T>`. A type derived from `formula::Quantity` gets its
`Describe<T>` for free, by base-class detection. A type nobody owns -- a
`double`, something from a vendor SDK, a struct we cannot add a base class to
-- gets one by explicit specialisation:

```cpp
struct ForeignTemperature
{
    double celsius {};
};

template <>
struct formula::Describe<ForeignTemperature>
{
    static constexpr std::string_view symbol = "theta";
    static constexpr std::string_view description = "a temperature from somebody else's library";
    static constexpr formula::Unit unit = formula::unit::Celsius;
    static constexpr formula::Dimension dimension = formula::unit::Celsius.dimension;
};
```

```
ForeignTemperature: symbol=theta dimension is temperature: yes
```

Nothing downstream knows, or needs to know, which of the two ways a type
joined. Both are read the same way, which is what lets a foreign type take
part in a formula without owning its source.

`Describe`'s primary template is deliberately empty rather than a
`static_assert` with a helpful message: a hard-error `static_assert` is not
in the immediate context, so it would make `formula::Described<T>` -- whose
entire job is to answer "is this type described?" -- fail to *compile* for
every undescribed type, instead of answering `false`. The helpful diagnostic
lives separately, in `formula::RequireDescribed<T>`, and it only fires once
the type is *completed* -- a bare alias to `RequireDescribed<T>` instantiates
nothing and checks nothing. Write `RequireDescribed<T>::value` to actually
force the check.

## Measurements that may be absent

`formula::Measured<Q>` holds a value of quantity `Q`, in `Q`'s declared unit
-- or nothing at all. A default-constructed `Measured` is **absent, not
zero**: zero is a measurement, and starting an unmeasured quantity at zero
would produce a confident wrong answer, which is exactly the failure this
type exists to prevent.

`value()` throws (`formula::ArithmeticException` carrying
`formula::ArithmeticError::DomainError`) when the measurement is absent,
because there is no number to return and returning zero would be a lie.
`value_or(fallback)` is the sanctioned way to get a number out of an absent
measurement, and it is deliberately explicit: the caller states what an
absent reading counts as, because there is no default answer the library
could supply that would be right for every caller.

Absence propagates rather than producing a wrong number. `formula::transform`
applies a function to a present value and leaves an absent one absent;
`formula::combine<Result>` takes two measurements and is **absent if
*either* input is absent, not only if both are** -- a formula with one
missing input has no answer, and computing one from just the inputs that
happen to be present is the wrong number this layer exists to prevent.

**`Result` is named by the caller and is not deduced from either operand.**
Combining two quantities generally produces a third -- a mass and a volume
combine into a density, not into either operand's own quantity -- and there
is no honest default `combine` could deduce instead. An earlier signature
deduced the result as the right-hand operand's quantity, so
`combine(mass, volume, divide)` was statically a measurement of *volume*,
reporting a volume's symbol and unit for a value that was actually a
density: a wrong label on a right number, worse than a wrong number because
it looks authoritative. Write `formula::combine<Density>(mass, volume,
[](Rational m, Rational v) { return m / v; })` instead. From the worked
example, a present volume combined with an absent mass, into a `Density`
that shares neither operand's tag, symbol or unit:

```
a present volume combined with an absent mass: absent
```

`formula::checked_convert_to<R>` converts a `Measured<Q>` into a
`Measured<R>` and keeps this rule too -- an absent input converts to an
absent output, and the dimension check runs regardless, so a conversion
nobody could perform is refused even when there was no value to get wrong:

```
an absent measurement, converted: still absent
```

## Bounds, precision and conversion

`formula::checked_within_bounds` and `formula::checked_round_to_declared`
are overloaded for `Measured<Q>` alongside phase 3's `Rational`-and-`Unit`
forms, and both keep the same absence rule: rounding an absent measurement
leaves it absent,

```
an absent measurement, rounded: still absent
```

and checking an absent measurement against its unit's declared bounds
answers `NotMeasured`, never a verdict:

```
an absent measurement, bounds-checked: no value was measured
```

**`NotMeasured` and `NotChecked` answer two different questions, and neither
substitutes for the other.** `NotChecked` means the unit declares no bounds
at all -- there is a value, but nothing to check it against. `NotMeasured`
means there is no value in the first place, regardless of whether the unit
declares bounds. A reading nobody took and a range nobody declared are
different facts. `test/measured_tests.cpp:198-227` pins all five
`BoundsCheck` outcomes side by side -- `WithinBounds`, `BelowMinimum` and
`AboveMaximum` for present values against a bounded unit, `NotMeasured` for
an absent value regardless of whether its unit declares bounds, and
`NotChecked` for a present value in a unit (such as `unit::Litre`) that
declares no bounds at all.

A present measurement still converts exactly, carrying its quantity's own
unit rather than needing one passed alongside it. From the worked example,
450 l converted to m³:

```
450 l converted to m3 = 9/20
```

## Limits

`formula::detail::FixedString`, which gives a quantity's symbol and
description their types, counts **bytes, not characters**: a multi-byte
UTF-8 symbol reports its encoded length, not its glyph count.

`formula::Measured<Q>` is deliberately **not** a structural type and cannot
be used as a non-type template parameter -- `std::optional`, which it holds,
is not structural in any of the standard libraries this project supports.
Confirmed on cl.exe: naming `Measured<WaterVolume>` as a non-type template
parameter fails with

```
error C2993: 'formula::Measured<WaterVolume>': is not a valid type for non-type template parameter 'V'
note: '_value' is not a public, non-mutable, non-static data member
```

The quantity type itself is different: an empty struct deriving publicly
from `formula::Quantity` (no non-static data members of its own, a
structural base) *is* structural, and the same compiler accepts it cleanly
as a non-type template parameter. Nothing in this library uses that, but the
type is capable of it, where `Measured` never can be -- the compile-time
identity and the runtime value are deliberately different kinds of thing.

For `Dimension`'s and `Unit`'s own limits -- `Exponent`'s integer width,
`SymbolCapacity`, the fields' bit widths -- see
[`docs/dimensions.md`](dimensions.md#limits) rather than a restatement here.
For `Rational`'s and the rounding layer's limits, see
[`docs/numbers.md`](numbers.md#limits); `Measured` is built directly on
`Rational` and the `checked_` arithmetic functions, and inherits their
overflow behaviour and rounding limits exactly.
