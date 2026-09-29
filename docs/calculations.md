# Calculations and worksheets

A formula built from other formulas is one larger expression tree
([Composing a formula from other formulas](expressions.md#composing-a-formula-from-other-formulas)):
a named sub-formula is written into every formula that uses it. That is the
right model for one published equation, and the wrong one for a bill, a
report or a mix design of many named values, each built on the ones before
it. Evaluated as a tree, a sub-result two formulas share is evaluated again
for each of them, and nothing remembers it: change one input and everything
is evaluated again, and nothing can say which values the change reached.

formula-cpp has a **calculation** for this. Each named value is defined once,
by the expression that calculates it, and read by name wherever another
definition needs it. What each definition reads is known while the program
compiles, so the calculation's dependency graph is known -- and checked -- at
compile time: a definition that reads itself, two that read each other, or a
quantity defined twice does not compile. A **worksheet** holds a
calculation's inputs. It calculates each named value once, and after a change
recalculates only what the change reaches. It answers what-if questions on a
copy, takes a value a person typed in place of a calculated one, and explains
any result as one derivation per named value, which always describes the
current inputs.

The worked example is `examples/electricity_bill.cpp`: a household's monthly
electricity bill, from what three appliances draw, what the solar panels
yield, the grid price, the feed-in tariff and a base fee, to the total with
tax. The page holds three kinds of quoted block. **Code** is copied from the
example's source, and `docs.calculations-snippets` fails unless each code
block appears there as a run of consecutive lines, compared without their
indentation (`cmake/CheckGuideSnippets.cmake`). A code block that is
deliberately *not* from the example -- each misuse under
[What is refused](#what-is-refused) -- carries a
`<!-- snippet: not from the example -->` comment directly above it, which
that check skips. **Compiler diagnostics** -- the blocks opening
`static assertion failed` -- are the library's refusals of those misuses as
g++ 14.2 printed them; cl 19.51 prints the same message, and each misuse
draws exactly one.

## Money of its own

A bill is in money, and the library ships no currency. Each currency is a
base dimension of its own
([Base dimensions the SI does not have](dimensions.md#base-dimensions-the-si-does-not-have)),
so a price cannot be added to a bare number, and euros are never converted
into yen:

```cpp
inline constexpr formula::Unit Euro { .dimension = formula::base_dimension("EUR"),
                                      .symbolText = formula::symbol("EUR"),
                                      .decimals = 2 };
inline constexpr formula::Unit EuroCent { .dimension = Euro.dimension,
                                          .magnitudeNumerator = 1,
                                          .magnitudeDenominator = 100,
                                          .symbolText = formula::symbol("ct"),
                                          .decimals = 0 };
// One euro per kilowatt-hour is one euro per 3600000 joules.
inline constexpr formula::Unit EuroPerKilowattHour { .dimension = Euro.dimension / formula::dim::Energy,
                                                     .magnitudeNumerator = 1,
                                                     .magnitudeDenominator = 3'600'000,
                                                     .symbolText = formula::symbol("EUR/kWh"),
                                                     .decimals = 4 };
inline constexpr formula::Unit Yen { .dimension = formula::base_dimension("JPY"),
                                     .symbolText = formula::symbol("JPY"),
                                     .decimals = 0 };
```

An exchange rate is data, not a constant: a quantity in yen per euro, read like
any other input, and never a factor the dimension system applies on its own.
The two currencies measure different things, and the program says so where it
is compiled:

```cpp
static_assert(!formula::SameDimension<Euro.dimension, Yen.dimension>);
```

The power and energy units -- watts, kilowatts, hours and kilowatt-hours --
are the library's own.

## Defining named values

`define<Q>(expression)` says that the quantity `Q` is calculated by
`expression`, and `calculation(...)` puts the definitions together. The bill's
two plain numbers -- the share of the solar yield the household uses itself,
and the rate of the tax -- are constants:

```cpp
inline constexpr Rational selfUseShare { 4, 5 };
inline constexpr Rational vatRate { 19, 100 };
```

and every other value is defined once, by what it is calculated from:

```cpp
inline constexpr auto bill = formula::calculation(
    formula::define<FridgeKw>(var<FridgeW>),
    formula::define<FridgeKwh>(var<FridgeKw> * var<FridgeH>),
    formula::define<OvenKwh>(var<OvenKw> * var<OvenH>),
    formula::define<HeaterKwh>(var<HeaterKw> * var<HeaterH>),
    formula::define<DailyLoad>(var<FridgeKwh> + var<OvenKwh> + var<HeaterKwh>),
    formula::define<MonthlyLoad>(var<DailyLoad> * Rational { 30 }),
    formula::define<SelfUsed>(var<Solar> * selfUseShare),
    formula::define<Exported>(var<Solar> - var<SelfUsed>),
    formula::define<NetDraw>(var<MonthlyLoad> - var<SelfUsed>),
    formula::define<GridCost>(var<NetDraw> * var<Price>),
    formula::define<FeedInCredit>(var<Exported> * var<FeedIn>),
    formula::define<EnergyCost>(var<GridCost> - var<FeedInCredit>),
    formula::define<Subtotal>(var<EnergyCost> + var<BaseFee>),
    formula::define<Vat>(var<Subtotal> * vatRate),
    formula::define<Total>(
        formula::rounded<EuroCent, formula::DecimalPlaces { 0 }, formula::RoundingMode::HalfAwayFromZero>(
            var<Subtotal> + var<Vat>)),
    formula::define<PricePaid>(var<EnergyCost> / var<NetDraw>),
    formula::define<PricePaidGross>(var<PricePaid> + var<PricePaid> * vatRate));
```

A definition is an ordinary formula of single values, and may hold what such
a formula may hold: here, a rounding to whole cents
([Rounding and conditionals](rounding-and-conditionals.md)). `var<NetDraw>` in
the grid cost's definition reads the value the net draw's definition
calculates. It is not the net draw's expression written out again.

**The order the definitions are given in does not matter.** The calculation
works out what each one reads, and calculates every value after the values it
reads. A quantity that is read and never defined is an **input**: the bill
has ten, from the fridge's power to the base fee, in the order the
definitions first read them.

**Each definition is checked where it is written.** Its expression must
measure what its quantity measures: the net draw is an energy, and a
definition that multiplied an energy by a price would be a cost, and does not
compile. The calculation as a whole must be one whose values can be
calculated in some order. The misuses and their messages are under
[What is refused](#what-is-refused).

## The graph, known while the program compiles

Because what each definition reads is part of its type, so is the graph: which
value reads which, directly and through any chain. A program can state a fact
about its own calculation and have it checked while it compiles:

```cpp
static_assert(formula::depends_on<Total, Price>(bill));
static_assert(!formula::depends_on<Exported, Price>(bill));
```

The queries name the values themselves, each as its symbol, in the order the
calculation calculates them:

```cpp
std::printf("affected by price    : %s\n", listed(formula::affected_by<Price>(bill)).c_str());
std::printf("upstream of net_draw : %s\n", listed(formula::upstream_of<NetDraw>(bill)).c_str());
std::printf("read by self_used    : %s\n", listed(formula::dependents_of<SelfUsed>(bill)).c_str());
```

`affected_by<Q>` is every value a change of `Q` reaches, and `upstream_of<Q>`
every value `Q` is reached from; `dependents_of<Q>` and `dependencies_of<Q>`
are the direct readers and reads only, `inputs_of` the inputs, and
`calculation_order` the calculated values. Each returns a `std::array` of
`std::string_view` whose size is known at compile time, so a query is a
constant expression as well. `listed` is the example's own, joining the names
with commas. Each query takes a vocabulary as a second argument, and then
names each value as that vocabulary writes it
([Methods and overlays](methods-and-overlays.md)).

The graph can also be read whole. `describe_graph` lists the inputs, and then
every calculated value with what it reads, the arrows aligned; `to_dot` writes
it in the DOT language of Graphviz, for `dot -Tsvg` to draw, the inputs as
boxes and the calculated values as ellipses:

```cpp
std::printf("its graph:\n%s\n", formula::describe_graph(bill).c_str());
```

```cpp
std::string const drawn = formula::to_dot(bill);
```

`render(bill)` writes the calculation itself, one `symbol = expression` line
per definition in the order it calculates them, in any of the three dialects,
and `document(bill)` makes its documentation page, whose symbol table gives
each calculated value its definition
([Citations and rendering](citations.md)).

## A worksheet

A worksheet is made from a calculation and an environment of its inputs,
exactly the environment a formula is evaluated in
([The environment](expressions.md#the-environment)):

```cpp
auto sheet = formula::worksheet(bill,
                                formula::environment(formula::Measured<FridgeW> { Rational { 200 } },
                                                     formula::Measured<FridgeH> { Rational { 24 } },
                                                     formula::Measured<OvenKw> { Rational { 5, 2 } },
                                                     formula::Measured<OvenH> { Rational { 1 } },
                                                     formula::Measured<HeaterKw> { Rational { 3, 2 } },
                                                     formula::Measured<HeaterH> { Rational { 4 } },
                                                     formula::Measured<Solar> { Rational { 150 } },
                                                     formula::Measured<Price> { Rational { 8, 25 } },
                                                     formula::Measured<FeedIn> { Rational { 2, 25 } },
                                                     formula::Measured<BaseFee> { Rational { 25, 2 } }));
```

Every input must be given. One nobody measured is given as
`Measured<Q>::absent()`, which says so, where an input simply left out would
let a missing reading pass for a mistake nobody noticed. An entry the
calculation neither reads nor defines is refused too: it would do nothing,
and most likely names the wrong quantity. Nothing is calculated when the
worksheet is made.

## Asking for values

`calculate<Q>()` answers with an `Outcome<Q>`, as evaluating a formula does
([The outcome](expressions.md#the-outcome)): a value, an absence, or a value
typed in by hand. Asked for several values at once, it answers with a
`std::tuple` of them, and a structured binding takes them apart:

```cpp
auto const [total, netDraw] = sheet.calculate<Total, NetDraw>();
```

The same question can name the variables instead of their types:

```cpp
auto const [sunnierTotal, sunnierDraw] = sunnier.calculate(var<Total>, var<NetDraw>);
```

**`checked_calculate` returns what `calculate` throws.** A calculation can
fail -- a division by zero, an overflow -- and `checked_calculate` answers
with a `std::expected<Outcome<Q>, ArithmeticError>` for each value asked,
where `calculate` throws `ArithmeticException` for the first failure.
[Failure and absence](#failure-and-absence) shows both.

Asking is not `const`. It brings every value the answer is reached through up
to date, calculating each at most once, and counts: `recomputed()` is how
many values the worksheet has calculated since it was made, and `reused()`
how many it found still up to date after a change and did not calculate
again. The first time the bill is asked for its total and net draw, it
calculates the fifteen values those are reached through, each once --
118.26 EUR and 279 kWh -- and reuses none. The price paid per kilowatt-hour
and its gross figure are not among them, and are not calculated until
something asks for them.

## A change, and what it reaches

`set(...)` gives an input a new value:

```cpp
sheet.set(formula::Measured<Price> { Rational { 1, 4 } });
```

Nothing is calculated when an input is set. The worksheet marks every value
the change can reach as out of date, and the next question calculates those
of them it needs. After a new price, the grid cost and what is built on it --
the energy cost, the subtotal, the tax and the total -- are calculated again:
five values, and the total is 95.02 EUR.

**The same value again is no change.** Setting the price to 0.25 EUR/kWh a
second time marks nothing, and the next question calculates nothing:

```cpp
sheet.set(formula::Measured<Price> { Rational { 1, 4 } });
```

A new base fee reaches three values, the subtotal, the tax and the total:

```cpp
sheet.set(formula::Measured<BaseFee> { Rational { 15 } });
```

**A value that comes out the same stops the change there.** Several inputs
can be set at once; here the fridge draws twice the power for half the time:

```cpp
sheet.set(formula::Measured<FridgeW> { Rational { 400 } }, formula::Measured<FridgeH> { Rational { 12 } });
```

The fridge's power in kilowatts is calculated again, and so is its energy a
day -- which comes out at 4.8 kWh, what it was. The eight values built on it,
from the daily load to the total, are then up to date as they are: none of
what they read has changed. The worksheet reuses them rather than calculating
them again, so the change costs two calculations, and `reused()` counts
eight. A value calculated again that comes out equal to its old value, failure
and all, leaves what reads it alone.

## What if?

`with(...)` answers a what-if question on a copy, and leaves the worksheet it
was asked of as it was:

```cpp
auto sunnier = sheet.with(formula::Measured<Solar> { Rational { 200 } });
```

The copy holds everything the worksheet had calculated, so a question to it
calculates only what the change reaches: nine values, for 85.14 EUR and
239 kWh drawn. Its counters start where the worksheet's stood. Asked again,
the worksheet itself still answers 98.00 EUR and calculates nothing.

## How a value was reached

`explain_worksheet<Q>(sheet)` asks for `Q` and records how the answer was
reached, as one **block** per named value; `render_derivation` writes them
out for a person:

```cpp
auto const fridgeEnergy = formula::explain_worksheet<FridgeKwh>(sheet);
std::string const fridgeText = formula::render_derivation(fridgeEnergy, { .maxSteps = 12 });
```

`Q`'s block comes first: its definition, evaluated step by step as a trace
states a formula ([Reading a derivation](tracing.md#reading-a-derivation)).
In it, a calculated value it reads is one step, marked `calculated`, and that
value's own block, further on, says how it was reached. The inputs read come
last, one line each. A value the evaluation never reached -- read only in a
`when()` branch not taken, or to the right of an operand that failed -- gets
no block. One step limit bounds every line, as `render_trace`'s does, and a
last line says how many were left out.

**A derivation is never stale.** It is recorded afresh on every call, from the
values the worksheet holds, never kept from when a value was calculated. After
the fridge's change, the eight values built on its energy a day were reused
rather than calculated again -- yet every derivation reads the fridge at
400 W for 12 h, because that is what the worksheet holds. The rule that lets
a value be reused is what makes this sound: a value is reused only when
nothing it reads has changed since it was calculated, so evaluating its
definition again against what it reads now gives the value it holds.
Recording a derivation calculates nothing again.

A computed step states its value in the coherent unit of its dimension, as
every trace does: the fridge's 4.8 kWh reads `17280000` there, in joules,
under a header in kilowatt-hours.

## A value typed in by hand

A person may know a calculated value better than the calculation: a meter
reading, say, in place of the net draw worked out from the appliances. Set it
with `entered(...)`, as a value typed in:

```cpp
sheet.set(formula::entered(formula::Measured<NetDraw> { Rational { 250 } }));
```

The value typed in stands in place of the calculated one: what is built on it
is calculated again from it -- five values, for 89.37 EUR -- its source reads
`ManuallyEntered`, and what the net draw was calculated from is no longer
read. `is_overridden<NetDraw>()` says whether it stands. In a derivation it
is one line, `net_draw = 250 kWh, entered by hand in place of monthly_load -
self_used`. `clear_override` goes back to calculating it:

```cpp
sheet.clear_override<NetDraw>();
```

The net draw is calculated again, 279 kWh, and so are the five values built on
it: six calculations. A calculated value given as a plain measurement,
`set(Measured<NetDraw> { ... })`, is refused: a measurement of a value the
calculation calculates is only ever a person's value in its place, and says
so by being `entered`.

## Failure and absence

A value whose calculation fails is kept like any other answer, and a
definition that reads it fails where it reads it, with the same error, as an
operand's failure fails a formula. With nothing drawn from the grid, the price
a kilowatt-hour drawn cost divides by zero, and the price with tax, which
reads it, fails with it. The total reads neither, and has its value,
14.99 EUR:

```cpp
auto nothingDrawn = sheet.with(formula::entered(formula::Measured<NetDraw> { Rational { 0 } }));
auto const [pricePaid, pricePaidGross, drawnTotal] =
    nothingDrawn.checked_calculate<PricePaid, PricePaidGross, Total>();
```

`calculate` throws the same failure, as an `ArithmeticException` whose
`code()` is the `ArithmeticError`:

```cpp
try
{
    static_cast<void>(nothingDrawn.calculate<PricePaidGross>());
}
catch (formula::ArithmeticException const& failure)
{
    std::printf("calculate<PricePaidGross>() threw: %s\n", failure.what());
    thrown = failure.code() == formula::ArithmeticError::DivisionByZero;
}
```

A failure is not calculated again until something it read changes, and a
value calculated again that fails with the same error counts as unchanged. Its
derivation is a block like any other, the failure in its header and in the
step that read it.

**Absence is not failure.** An input given as `Measured<Q>::absent()` makes
what reads it absent, as it makes a formula's result absent
([Absence propagates](expressions.md#absence-propagates)), never zero. And a
`when()` branch not taken reads nothing: a failed value read only there does
not fail the value whose definition holds it.

## What is refused

Each misuse below is refused where it is written, with one message. Each
snippet uses the example's quantities; `ApplianceKwh`, `DryerKw` and
`LastMonth` are quantities and a role the example does not declare.

**A definition whose expression measures something else.** A kilowatt-hour
times a price is money, not energy:

<!-- snippet: not from the example -->
```cpp
formula::define<NetDraw>(var<MonthlyLoad> * var<Price>)
```

```
static assertion failed: formula: this definition's expression measures a different dimension from the quantity it defines; every read of the quantity would be a value it does not measure -- the quantity and the expression appear in this diagnostic as the template arguments Q and Expr of RequireDefinitionMeasuresQuantity
```

**A definition that reads what it defines**, which could never be calculated:

<!-- snippet: not from the example -->
```cpp
formula::calculation(formula::define<MonthlyLoad>(var<MonthlyLoad> + var<DailyLoad>))
```

```
static assertion failed: formula: this definition reads the quantity it defines, so it can never be calculated -- the quantity appears in this diagnostic as the template argument Q of RequireDefinitionNotSelfReferential
```

**A quantity defined twice.** Taking the first or the last definition would be
a guess either way:

<!-- snippet: not from the example -->
```cpp
formula::calculation(formula::define<Vat>(var<Subtotal> * vatRate),
                     formula::define<Vat>(var<Subtotal> / Rational { 5 }))
```

```
static assertion failed: formula: this calculation defines the same quantity more than once; first-wins and last-wins are equally arbitrary, so neither is guessed -- the quantities defined appear in this diagnostic as the template arguments of RequireDistinctDefinitions
```

**Definitions that read one another in a cycle**, none of which could be
calculated first. The message names every quantity on the cycle:

<!-- snippet: not from the example -->
```cpp
formula::calculation(formula::define<Subtotal>(var<Total> - var<Vat>),
                     formula::define<Vat>(var<Subtotal> * vatRate),
                     formula::define<Total>(var<Subtotal> + var<Vat>))
```

```
static assertion failed: formula: these definitions read one another in a cycle, so none of them can be calculated first -- the quantities on the cycle appear in this diagnostic as the template arguments of RequireAcyclicDefinitions
```

cl 19.51 says the same, at the check's own line:

```
include\formula-cpp/calculation.hpp(550): error C2338: static assertion failed: 'formula: these definitions read one another in a cycle, so none of them can be calculated first -- the quantities on the cycle appear in this diagnostic as the template arguments of RequireAcyclicDefinitions'
```

**A worksheet missing an input.** Here the environment has no price:

<!-- snippet: not from the example -->
```cpp
formula::worksheet(bill,
                   formula::environment(formula::Measured<FridgeW> { Rational { 200 } },
                                        formula::Measured<FridgeH> { Rational { 24 } },
                                        formula::Measured<OvenKw> { Rational { 5, 2 } },
                                        formula::Measured<OvenH> { Rational { 1 } },
                                        formula::Measured<HeaterKw> { Rational { 3, 2 } },
                                        formula::Measured<HeaterH> { Rational { 4 } },
                                        formula::Measured<Solar> { Rational { 150 } },
                                        formula::Measured<FeedIn> { Rational { 2, 25 } },
                                        formula::Measured<BaseFee> { Rational { 25, 2 } }))
```

```
static assertion failed: formula: this worksheet's environment provides no value for an input of its calculation; supply one, as Measured<Q>::absent() if it was not measured -- the input and the environment appear in this diagnostic as the template arguments of RequireCalculationInput
```

**An entry nobody reads.** The ten inputs, and a dryer's power no definition
reads:

<!-- snippet: not from the example -->
```cpp
formula::worksheet(bill, bill_inputs(formula::Measured<DryerKw> { Rational { 2 } }))
```

```
static assertion failed: formula: this worksheet's environment supplies a quantity its calculation neither reads nor defines; an entry nobody reads would silently do nothing, most likely because it names the wrong quantity -- the entry appears in this diagnostic as the template argument of RequireEntryReadByCalculation
```

(`bill_inputs` stands for the example's ten measurements, followed by what it
is given.)

**A calculated value given as a measurement**, rather than typed in by hand:

<!-- snippet: not from the example -->
```cpp
sheet.set(formula::Measured<NetDraw> { Rational { 250 } });
```

```
static assertion failed: formula: this quantity is calculated by the worksheet's calculation, and was given as a measurement; override a calculated value by hand with entered(Measured<Q> { ... }) -- the quantity appears in this diagnostic as the template argument Q of RequireCalculatedOverriddenByEntry
```

**A question about a quantity the calculation does not hold:**

<!-- snippet: not from the example -->
```cpp
sheet.checked_calculate<DryerKw>()
```

```
static assertion failed: formula: this worksheet's calculation neither defines nor reads the quantity asked for; the quantity and the calculation appear in this diagnostic as the template arguments of RequireWorksheetResult
```

**Clearing an override of an input.** An input is never overridden; it is
simply set again:

<!-- snippet: not from the example -->
```cpp
sheet.clear_override<Price>();
```

```
static assertion failed: formula: clear_override or is_overridden names an input of the calculation; only a calculated quantity is overridden by hand, and an input is simply set again -- the quantity appears in this diagnostic as the template argument Q of RequireCalculatedQuantity
```

A refused calculation says nothing more: a query over it, a worksheet of it, a
question to that worksheet, a derivation or a documentation page of it adds no
second message to the one that refused it.

## Limits

**Single values only.** A calculation holds one value per quantity. A
definition that reads a series or raw observations, even to reduce them, is
refused -- read their reduction as an input, calculated elsewhere:

<!-- snippet: not from the example -->
```cpp
formula::define<DailyLoad>(formula::sum(formula::series<ApplianceKwh, 3>))
```

```
static assertion failed: formula: a calculation holds single values, and this definition reads a quantity as a series or as raw observations; read it with var<Q> -- the quantity appears in this diagnostic as the template argument Q of RequireSingleValueReadsInCalculation
```

**No reads from another record.** A definition reads the worksheet's own
values; a formula that reads another sample or another test
([Other samples and other tests](records.md)) is evaluated against a
`record_context` instead:

<!-- snippet: not from the example -->
```cpp
formula::define<Exported>(var<Solar> - formula::from_record<LastMonth>(var<SelfUsed>))
```

```
static assertion failed: formula: a calculation's definitions read the worksheet's own values, and this one reads from another record with from_record; evaluate that formula against a record_context instead -- the role appears in this diagnostic as the template argument of RequireNoRecordReadInCalculation
```

**At most 64 quantities**, inputs and calculated values together: the graph
is kept as one 64-bit word per quantity. `test/negative/calculation_over_capacity.cpp`
builds a calculation of 65, and is refused with:

```
static assertion failed: formula: a calculation holds at most 64 quantities, inputs and definitions together, and this one holds more; the count appears in this diagnostic as the template argument of RequireCalculationWithinCapacity -- split it into two, the second reading the first's results as inputs
```

**The graph is what a definition may read.** A `when()` counts both its
branches as read, since which one is taken is known only when it is
evaluated. So a value read only on the branch not taken is still brought up
to date when the value holding the `when()` is asked for, and a change to it
still marks that value out of date -- though it is not read, its failure does
not reach, and it has no block in a derivation.

**A calculated value is kept in its declared unit.** It is stored in the unit
its quantity was declared in, and converted back to the coherent unit where a
definition reads it. Both conversions are exact, but either can overflow
where the formula written out as one tree, which makes neither, would not
([Numeric headroom](numeric-headroom.md)).
