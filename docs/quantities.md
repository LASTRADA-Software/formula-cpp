# Quantities and measurements

`formula-cpp` provides a compile-time quantity type, `formula::Quantity`, that
carries a variable's own documentation as part of its type; a single
metadata-reading access point, `formula::Describe<T>`, that reaches both our
own types and ones we do not own; and a runtime value that may honestly be
unmeasured, `formula::Measured<Q>`. This page explains why a variable's
identity is a type, how to declare one -- by alias or by struct -- how
`Describe` works for foreign
types, what it means for a measurement to be absent, how bounds, precision
and conversion behave once absence is possible, and where the limits are. The
worked example below is `examples/quantities.cpp`; every block on this page
that is formatted as program output is copied verbatim from that program's
actual output, not worked out by hand.

## Why a variable is a type

A *dimension* is already a compile-time thing:
`formula::RequireSameDimension<Left, Right>` lets code refuse, at compile
time, to combine two values whose dimensions differ -- adding a volume to a
mass fails to compile, with both exponent vectors spelled out in the
diagnostic (see [`docs/dimensions.md`](dimensions.md)). Quantities take the
same idea one step further and make a *variable* -- not just its dimension,
but its symbol, its description and its unit -- a compile-time thing too.
Declaring `Rise` and `Run` as two quantities, each carrying
its own symbol/description/unit through `formula::Quantity`, makes them two
different, unrelated C++ types even when every one of their parameters but
the tag is identical, and neither is usable where the other is expected.

`test/negative/quantity_wrong_type.cpp` is exactly this case, kept in the
suite as a negative-compile test, with the two quantities declared by struct:

```cpp
struct Rise: formula::Quantity<Rise, "x", "a distance", formula::unit::Millimetre>
{
};

struct Run: formula::Quantity<Run, "x", "a distance", formula::unit::Millimetre>
{
};

void takes_rise(Rise);

int main()
{
    takes_rise(Run {});
    return 0;
}
```

Attempting this gives, verbatim, on cl.exe:

```
error C2664: 'void takes_rise(Rise)': cannot convert argument 1 from 'Run' to 'Rise'
note: No user-defined-conversion operator available that can perform this conversion, or the operator cannot be called
```

Declared by alias instead -- `using Rise =
formula::Quantity<struct RiseTag, "x", "a distance", formula::unit::Millimetre>;`,
and `Run` likewise with `RunTag` -- the call is refused the same way. cl's
words are the ones above; g++ 14.2 and clang-cl 22.1 name the `Quantity`
specialisation each alias stands for, by its tag:

```
error: could not convert ‘Run()’ from ‘Quantity<RunTag,[...],[...],[...]>’ to ‘Quantity<RiseTag,[...],[...],[...]>’
note: candidate function not viable: no known conversion from 'Quantity<struct RunTag, [3 * ...]>' to 'Quantity<struct RiseTag, [3 * ...]>' for 1st argument
```

The mistake is caught exactly where the wrong call was written, not
discovered later by a runtime check, or -- because the two types agree on
symbol, description and unit -- not discovered at all. That is the payoff of
making a variable's identity a type rather than a runtime tag: a tag has to
be compared at run time to catch the same mistake, and only for the inputs
that happen to be exercised.

## Declaring a quantity

`formula::Quantity` takes exactly four template parameters, and a quantity is
declared with it in one of two spellings. The alias is the shorter, and the
one these guides and the examples use:

```cpp
using Rise = formula::Quantity<struct RiseTag,        // the tag
                               "h",                   // symbol
                               "height gained",       // description
                               formula::unit::Metre>; // unit
```

The struct derives a type of its own, and gives that type's own name back to
it as the tag:

```cpp
struct Rise:
    formula::Quantity<Rise,                 // the type's own name -- the tag
                      "h",                  // symbol
                      "height gained",      // description
                      formula::unit::Metre> // unit
{
};
```

Both are supported everywhere a quantity is named -- `var<Q>`,
`Measured<Q>`, an environment, a vocabulary, an overlay, a series, a record,
a retry -- and the two can be used together in one formula.
`test/quantity_alias_tests.cpp` runs every one of those surfaces with alias
quantities; most other tests declare theirs by struct, so both spellings
stay covered.

**The tag is what makes a quantity distinct.** Two quantities whose symbol,
description and unit coincide are two types as long as their tags differ. In
the alias form, `struct RiseTag` in the argument list declares the tag:
an incomplete class, never defined and never needing to be, in the nearest
enclosing namespace or block. Inside a class that is the namespace around the
class, not the class -- two classes that each declare `struct QTag` this way
name one tag, which is harmless, as the next section says. An alias cannot name itself, so
`using Rise = formula::Quantity<Rise, ...>;` does not compile:
an alias needs a second name for its tag. In the struct form the type is its
own tag, which also keeps two quantities' *bases* distinct, so a function
taking one quantity's base cannot accept another's.

`examples/quantities.cpp` declares `Rise` and `ReturnRise` by alias, alike in
every parameter but the tag, and one quantity, `TubeMass`, by struct beside
them:

```
Rise and ReturnRise share symbol, description and unit: yes
...but the tag keeps them different types: yes
```

### What each spelling costs

| | alias | struct |
|---|---|---|
| forward declaration | not possible | `struct Rise;` |
| two declarations with all four arguments equal | one type, under two names | two types |
| one tag, another argument different | two types | -- (a struct is its own tag) |
| how g++ and clang name it in a diagnostic | `Quantity<RiseTag, ...>` | `Rise` |
| how cl names it in a diagnostic | usually `Rise`, not always | `Rise` |

**An alias cannot be forward-declared.** A header that only names a quantity
-- a function declaration taking `Measured<Rise>` -- can say
`struct Rise;` for a struct quantity, and must include an alias's
declaration.

**Two aliases with all four arguments equal are one type.** Repeating a
declaration -- `using A = formula::Quantity<ATag, "V", "a volume", unit::Litre>;`
and a `using B` with the same four arguments -- declares one quantity under two
names, and nothing can object: there is only one type, and naming it twice is
not an error anywhere in C++. Give every alias a tag of its own. Two structs
never collapse this way, whatever their bases.

**A tag shared by two quantities is harmless while any other argument
differs.** The two are still two distinct types, and nothing in the library
reads the tag on its own. That is what happens in an alias template that
declares its tag inside itself: every instantiation names the same tag, and
each is a quantity of its own as long as the arguments differ -- here by unit:

```cpp
template <formula::Unit U>
using LengthIn = formula::Quantity<struct LengthInTag, "L", "a length", U>;
```

`LengthIn<formula::unit::Metre>` and `LengthIn<formula::unit::Millimetre>` are
two quantities, and add up in one formula. To give each instantiation a tag of
its own, make the tag depend on what the other arguments depend on:

```cpp
template <formula::Unit U>
struct LengthInTag;
template <formula::Unit U>
using LengthIn = formula::Quantity<LengthInTag<U>, "L", "a length", U>;
```

`test/quantity_alias_tests.cpp` runs both, and two aliases that share a tag
and differ only in symbol.

**A diagnostic names the tag.** g++ and clang print the `Quantity`
specialisation an alias stands for, tag first. cl usually keeps the alias's
name where the alias was written, in the library's own messages among them,
but not always -- see
[where a dimensional error appears](expressions.md#where-a-dimensional-error-appears).
Naming a tag after its quantity, `RiseTag`, is what keeps such a
diagnostic readable.

**There is no fifth parameter for the dimension.** A `Unit` already carries
its dimension (`unit.dimension`), so a separate dimension parameter would
state it a second time and let the two disagree. That is not a hypothetical
risk: the five-parameter spelling, with `dim::Mass` paired against
`unit::Litre`, compiled without a diagnostic on every compiler it was tried
on. `Quantity::dimension` is derived from the unit instead, so there is
no second place for it to disagree with, and no spelling that lets a caller
write the contradiction at all.

## `Describe<T>`, and foreign types

Nothing above the metadata layer reads a `Quantity` base directly. Everything
-- our own types and types we do not own alike -- goes through one template,
`formula::Describe<T>`. A type declared through `formula::Quantity` -- an
alias of it, or a struct derived from it -- gets its `Describe<T>` for free. A type nobody owns -- a
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
that shares neither operand's tag, symbol or unit, printed as `std::format`
writes an absent `Measured`:

```
a present volume combined with an absent mass: (not measured)
```

`formula::checked_transform` and `formula::checked_combine<Result>` are the
same two functions for a callback that can fail: the callback returns
`std::expected<Rational, ArithmeticError>` -- `formula::checked_mul` and its
siblings do -- and the result is a
`std::expected<Measured<...>, ArithmeticError>` that carries the callback's
error unchanged. Absence propagates exactly as above, and an absent input
never reaches the callback. Both are `noexcept` when the callback is, so
arithmetic on a measurement can be written under a no-throw rule. A
tenfold dilution of a reading:

```cpp
auto const diluted = formula::checked_transform(
    reading, [](Rational litres) noexcept { return formula::checked_mul(litres, Rational { 10 }); });
if (!diluted)
    return std::unexpected { diluted.error() }; // Overflow, say
```

A callback that returns a bare `Rational` does not compile with either: it
cannot fail, so it belongs to `transform` or `combine`.

`formula::checked_convert_to<R>` converts a `Measured<Q>` into a
`Measured<R>` and keeps this rule too -- an absent input converts to an
absent output. The two dimensions are checked where the call is written,
with or without a value: converting a volume into a mass, or euros into yen,
does not compile, and it draws one message, so a conversion nobody could
perform cannot look like it succeeded merely because there was no value to
get wrong. (Before this check moved to compile time, such a call compiled
and returned `ArithmeticError::DomainError`.) With no value present the
result is absent. The worked example converts a constant, so it checks the
`std::expected` with a `static_assert`, and an error would stop the build:

```cpp
constexpr auto convertedAbsent = formula::checked_convert_to<RiseInKilometres>(absentRise);
constexpr auto roundedAbsent = formula::checked_round_to_declared(absentRise, RoundingMode::HalfAwayFromZero);
constexpr auto boundsOfAbsent = formula::checked_within_bounds(absentRise);
static_assert(convertedAbsent.has_value() && roundedAbsent.has_value() && boundsOfAbsent.has_value());
```

```
an absent measurement, converted: (not measured)
```

## Supplying values

A measurement is written with the number it holds, not with `Rational`
spelled out around it. An integer is a value as it stands, and `_r`
(`using namespace formula::literals;`) is an exact decimal:

```cpp
using namespace formula::literals;

formula::Measured<Rise> const whole { 139 };
formula::Measured<Rise> const fractional { 10.3_r };
```

`10.3_r` is exactly 103/10. A plain `10.3` is refused with a message that
says why -- it is the double nearest 10.3, not 10.3. Every built-in integer
of up to 64 bits, `std::uint64_t` among them, converts exactly; a wider one
is refused.

`formula::measured_series<Q>` takes the same spellings, mixed freely, and
`formula::not_measured` for a point that was not measured. It is the same
as `Measured<Q>::absent()`, and either may stand in one series:

```cpp
constexpr auto rises = formula::measured_series<Rise>(127, 10.3_r, formula::not_measured, 139);
```

The series has four elements and the third is absent, not zero. Each element
may still be a `Measured<Rise>`; a `Measured` of another quantity is
refused, and only one message says so.

A band's bounds and a breakpoint's key are numbers in the same way:
`formula::band(83.7_r, 97.3_r)` and `formula::breakpoint(12.7_r)` are the
bands and breakpoints that `band(837, 10, 973, 10)` and `breakpoint(127, 10)`
spell as numerator over denominator, and those spellings stay.

## Bounds, precision and conversion

`formula::checked_within_bounds` and `formula::checked_round_to_declared`
are overloaded for `Measured<Q>` alongside the `Rational`-and-`Unit` forms of
[Dimensions and units](dimensions.md), and both keep the same absence rule:
rounding an absent measurement leaves it absent,

```
an absent measurement, rounded: (not measured)
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
different facts. `test/measured_tests.cpp:206-268` pins all five
`BoundsCheck` outcomes side by side -- `WithinBounds`, `BelowMinimum` and
`AboveMaximum` for present values against a bounded unit, `NotMeasured` for
an absent value regardless of whether its unit declares bounds, and
`NotChecked` for a present value in a unit (such as `unit::Litre`) that
declares no bounds at all.

A present measurement still converts exactly, carrying its quantity's own
unit rather than needing one passed alongside it. From the worked example,
a rise of 450 m converted to kilometres:

```
450 m converted to km = 0.45 km
```

The conversion, the rounding and the bounds check each have a throwing twin,
for callers who would only rethrow the error: `formula::convert_to<R>`,
`formula::round_to_declared` and `formula::within_bounds`, which take the
same arguments and return the value itself, and throw `ArithmeticException`
where the `checked_` form returns an error. Absence behaves as above -- an
absent measurement converts and rounds to an absent one and is `NotMeasured`
for its bounds -- and a conversion across dimensions does not compile in
either spelling. The worked example keeps the `checked_` forms, and checks
each result before it reads it, as shown [above](#measurements-that-may-be-absent)
for the conversion.

## Limits

`formula::detail::FixedString`, which gives a quantity's symbol and
description their types, counts **bytes, not characters**: a multi-byte
UTF-8 symbol reports its encoded length, not its glyph count.

`formula::Measured<Q>` is deliberately **not** a structural type and cannot
be used as a non-type template parameter -- `std::optional`, which it holds,
is not structural in any of the standard libraries this project supports.
Confirmed on cl.exe: naming `Measured<Rise>` as a non-type template
parameter fails with

```
error C2993: 'formula::Measured<Rise>': is not a valid type for non-type template parameter 'V'
note: '_value' is not a public, non-mutable, non-static data member
```

The quantity type itself is different: `formula::Quantity` has no
non-static data members, so an alias of it, and an empty struct deriving
publicly from it (a structural base), *are* structural, and the same compiler accepts both cleanly
as non-type template parameters. Nothing in this library uses that, but the
type is capable of it, where `Measured` never can be -- the compile-time
identity and the runtime value are deliberately different kinds of thing.

For `Dimension`'s and `Unit`'s own limits -- `Exponent`'s integer width,
`SymbolCapacity`, the fields' bit widths -- see
[`docs/dimensions.md`](dimensions.md#limits) rather than a restatement here.
For `Rational`'s and the rounding layer's limits, see
[`docs/numbers.md`](numbers.md#limits); `Measured` is built directly on
`Rational` and the `checked_` arithmetic functions, and inherits their
overflow behaviour and rounding limits exactly.
