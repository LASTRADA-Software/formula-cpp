# Other samples and other tests

Some results are not computed from one specimen alone. A strength is reported
as a ratio to a reference sample's; a reading is compared with a prior test of
the same sample; a correction is computed from measurements made on separate
material. The formula then reads from a record other than the one being
evaluated, and an audit trail that lists only numbers cannot say whose they
were. formula-cpp lets a formula read from another record, and its trace keeps,
for every step, which record the step was read from.

The worked example is `examples/records.cpp`. The page holds two kinds of
quoted block. **Program output** is copied verbatim from that program's actual
output, and `docs.records-output` fails unless each of these blocks is a run of
consecutive lines the program prints, exactly as quoted
(`cmake/CheckGuideOutput.cmake`). **Compiler diagnostics** -- the block opening
`static assertion failed` -- are the library's refusals as g++ 13.3 printed
them, captured from this repository's negative tests.

**Code** is copied from the example's source, and `docs.records-snippets`
fails unless each code block appears there as a run of consecutive lines,
compared without their indentation (`cmake/CheckGuideSnippets.cmake`). A code
block that is deliberately *not* from the example would carry a
`<!-- snippet: not from the example -->` comment directly above it, which the
check skips. No code block on this page carries one.

## A role is code, a record is data

A formula never names a sample. It names a **role** -- a type the author
declares, exactly like a method's variant tag -- and which sample and test
play that role is decided at run time:

```cpp
struct Reference
{
};
struct SecondReference
{
};
```

The quantities are declared once, as everywhere in this library, and each
record holds its own values of them. The reference specimen's strength was
typed in by a person, so its entry is wrapped in `entered(...)`; a value that
was measured is a plain `Measured<Q>`:

```cpp
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", unit::Megapascal>
{
};
struct Force: formula::Quantity<Force, "F", "load at failure", unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", unit::Millimetre>
{
};
```

```cpp
constexpr auto here = formula::environment(formula::Measured<Strength> { formula::Rational { 30 } },
                                           formula::Measured<Force> { formula::Rational { 579'630 } },
                                           formula::Measured<EdgeX> { formula::Rational { 139 } },
                                           formula::Measured<EdgeY> { formula::Rational { 139 } });
constexpr auto there = formula::environment(formula::entered(formula::Measured<Strength> { formula::Rational { 20 } }),
                                            formula::Measured<Force> { formula::Rational { 386'420 } },
                                            formula::Measured<EdgeX> { formula::Rational { 139 } },
                                            formula::Measured<EdgeY> { formula::Rational { 139 } });
```

The formula that divides this specimen's strength by the reference's reads
the reference through `from_record`:

```cpp
constexpr auto ratio = var<Strength> / formula::from_record<Reference>(var<Strength>);
```

The records are data. Each carries a key, its own environment, and the lineage
keys the laboratory states for it (see [Lineage is a gate](#lineage-is-a-gate)).
A **context** holds this record and the others by role:

```cpp
constexpr auto records = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                         formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12)),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there,
                               formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12)));
```

- **A key is a sample and a test,** two separate strong types, so a swapped
  pair does not compile and a missing one is not silently zero.
- **Keys are integers.** A sample key, a test key and a lineage key are each a
  `std::uint64_t` -- whatever the laboratory's database uses to identify the
  row. The trace prints the integers it was given. Turning a key into the
  label a person reads, such as a sample code, is the report's job, outside
  the trace.
- **`formula::ThisRecord`** is the library's role for the specimen being
  evaluated.
- **A formula that reads from a role the context does not bind** is a compile
  error naming the role, never a run-time miss: the set of roles is code, and
  which record plays each one is data.

**A context is this record's environment.** It inherits it, so everything that
takes an environment -- `checked_evaluate`, `evaluate_method`, `check_method`,
`explain`, the lookups -- takes a context, and reads this record's values from
it exactly as from the environment itself:

```text
f_c / (f_c of Reference) = 3/2
f_c through the context: 30000000 Pa
f_c through this record's environment: 30000000 Pa
```

Values are answered in coherent SI, as everywhere in this library: 30 MPa is
30000000 Pa.

## Reading a value, or computing over another specimen

`from_record<Role>(expression)` is a scope. Its operand is evaluated against
that role's record, so it can read one value, as `ratio` does, or compute over
the other specimen's own measurements -- here the reference's strength from
its load and edges:

```cpp
constexpr auto referenceStrength = formula::from_record<Reference>(var<Force> / (var<EdgeX> * var<EdgeY>));
```

On the page, a scope reads as its operand and the words *of Reference*. An
operand of more than one symbol is bracketed, so the role qualifies the whole
computation and not its last symbol:

```text
(F / (x_m * y_m)) of Reference = 20000000 Pa
\left(\frac{F}{x_m \cdot y_m}\right)\ \text{of }\mathrm{Reference}
```

A scope holds an expression, not a method: a method is not a node, so a
method evaluated over another record is not possible yet. A prior test's
*reported result* is a stored value, and `from_record<PriorTest>(var<Result>)`
reads it.

### What the trace says

**Every step inside a scope carries the record it was read from**, as a
structured field of the step (`Step::record`), with both keys -- two tests of
one sample share the sample key, so a sample alone could name either. The
rendered line names the record on each value read from it and on the scope's
own line. A step computed inside the scope carries the record too, but its
line names only its operands (`#3 * #4`), whose own lines say where their
values came from. The reference's strength was typed in by a person, and its
line says that too:

```text
1. f_c = 30 MPa
2. f_c = 20 MPa, from record Reference (sample 23, test 3), entered by hand
3. #2 from record Reference (sample 23, test 3) = 20 MPa
4. #1 / #3 = 3/2
```

The documentation `document(ratio)` returns keeps a row in its symbol table
for each record a quantity is read from: `f_c` read here and `f_c` read from
the reference are two inputs, and one merged row would tell a reader to supply
one value where the formula reads two. The example prints the two rows of
`page.symbols` itself -- the library has no renderer for a symbol table, which
is the page's job:

```text
  f_c: compressive strength, this record
  f_c: compressive strength, record Reference
```

A quantity an overlay fixes or derives is the jurisdiction's, read from no
record, and keeps one row wherever it is used. An overlay's constant reaches
inside a scope as it reaches everywhere else in the method: a jurisdiction's
shape factor is the same shape factor in the computation over the reference
specimen. When it replaces a value a person typed in, the line says
`replacing a value entered by hand`.

### What the trace guarantees, and what it does not

The guarantee is about the **recording path**: the evaluator never attributes a
value to a record it did not read it from. The origin a step carries is built
by the library, in the same call, from the same record whose environment the
operand is evaluated against; so is every lineage comparison. The role's name
comes from its type, and the keys from the record, which only
`formula::record` and `Record<...>::unbound()` build.

What no library can prevent, stated plainly:

- `Trace::steps` is a public arena. Any code can append a step or edit one.
- A valid origin the library built can be copied and handed to a sink by hand.
- An origin is trivially copyable, so `std::bit_cast` from a struct of the
  same layout builds one holding anything. No access check applies to
  `bit_cast`, and nothing in the language lets a class refuse it while staying
  trivially copyable.
- A caller can wrap a typed-in value as `Measured<Q>`; the keys and the source
  are the caller's statement about its own data, recorded as stated.
- Explicitly specialising a library template or member is outside the
  contract. The supported customisation points are `TagName`,
  `EnumeratorName`, `Describe`, `RepTraits` and the vocabulary; a
  specialisation of anything else can make a trace say anything.

The structured fields of each step -- its kind, its origin, its lineage
comparison -- are what is authoritative; the rendered line is their
description. A role displayed as `this record`, in any case, is refused, since
every value read from it would be traced as read from the record being
evaluated.

## Lineage is a gate

A read from another record is often permitted only if the two records came
from the same material batch and the same test method. A **lineage
requirement** on the scope says so:

```cpp
constexpr auto gated = formula::from_record<Reference>(var<Strength>, formula::same_lineage<MaterialBatch, TestMethod>());
```

The attributes are the author's: `MaterialBatch` and `TestMethod` are empty
structs the example declares. The library knows no attribute and decides
nothing about which batch a specimen belongs to -- that is the laboratory's
data. It compares the two keys each record states, before it evaluates the
operand, and records one line per attribute in the order the requirement names
them. The rule is:

- **any attribute that differs refuses the read,** whatever the others say;
- **otherwise, any attribute with an unknown key gives no answer;**
- **otherwise the value is read.**

**Every attribute agrees:** the value is read.

```text
1. same MaterialBatch as this record: 4411 and 4411, satisfied
2. same TestMethod as this record: 12 and 12, satisfied
3. f_c = 20 MPa, from record Reference (sample 23, test 3), entered by hand
4. #3 from record Reference (sample 23, test 3) = 20 MPa
```

**An attribute differs:** the read is refused, and the operand is never
evaluated. A refusal is an error, not a number, so a caller cannot report it by
forgetting to look at a verdict. It is reported as
`ArithmeticError::DomainError`, whose words are *argument outside the domain of
the operation* -- the same channel a lookup that finds no row uses -- and the
trace names the attribute that refused it, with both keys. `checked_explain`
traces it without throwing, and hands back the trace with the error:

```text
1. same MaterialBatch as this record: 4411 and 4411, satisfied
2. same TestMethod as this record: 12 and 13, violated
3. from record Reference (sample 23, test 3) = argument outside the domain of the operation
```

`explain`, which goes through the throwing `evaluate`, would throw here and
return no trace at all; use `checked_explain` wherever a read can be refused.

**A key is unknown, and nothing differs:** there is no answer. An unknown batch
is a missing input, not evidence that two batches differ, so the read is
absent, as any formula with a missing input is. The trace shows an absent value
as `(not measured)`, the library's one word for absent:

```text
1. same MaterialBatch as this record: 4411 and unknown, not checked
2. same TestMethod as this record: 12 and 12, satisfied
3. from record Reference (sample 23, test 3) = (not measured)
```

**A key is unknown, and another differs:** the read is refused. A known
disagreement is evidence, and an unknown key does not outweigh it:

```text
1. same MaterialBatch as this record: 4411 and unknown, not checked
2. same TestMethod as this record: 12 and 13, violated
3. from record Reference (sample 23, test 3) = argument outside the domain of the operation
```

`same_lineage<...>()` compares with this record. To compare two other records
-- neither of them this specimen -- name the other one:
`same_lineage<MaterialBatch>(formula::against<PriorTest>)`.

**What the gate is not.** It refuses a read; it does not produce a verdict. A
method whose acceptance logic wants to accept a result *and* flag a batch
mismatch cannot express that as a constraint yet. Nor does the library model
where a batch came from or how long it stays valid: plant lifecycles, rolling
windows and batch assignment stay with the laboratory's own data.

## A record not yet made

The reference test may not have been done yet. That is ordinary laboratory
data, so the record keeps its role, and has no key and no values. It names the
lineage attributes it would declare in its type -- `unbound()` takes nothing
to declare them with -- and every one of them is unknown:

```cpp
auto const notYetTested = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here,
                                         formula::lineage<MaterialBatch>(4411), formula::lineage<TestMethod>(12)),
    formula::Record<Reference, decltype(there), formula::LineageEntry<MaterialBatch>,
                    formula::LineageEntry<TestMethod>>::unbound());
```

`decltype(there)` names the environment's type as it is declared -- `const`,
for a `constexpr` variable -- and the record is the same type
`formula::record<Reference>(...)` builds from `there`, so one formula takes
one context type whether the reference has been tested or not.

A read from it gives no answer -- never zero:

```text
1. f_c = 30 MPa
2. from record Reference (no record bound) = (not measured)
3. #1 / #2 = (not measured)
```

and the gated read over it compares no lineage, since every key is unknown and
no record is there to read:

```text
1. from record Reference (no record bound) = (not measured)
```

An entry a person typed in but left empty is not a measurement nobody made. It
is spelt `entered(Measured<Q>::absent())`:

```cpp
auto const typedInEmpty = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)),
                               formula::environment(formula::entered(formula::Measured<Strength>::absent()))));
```

and it reads as what it is:

```text
1. f_c = 30 MPa
2. f_c = (entered by hand as empty), from record Reference (sample 23, test 3)
3. #2 from record Reference (sample 23, test 3) = (not measured)
4. #1 / #3 = (not measured)
```

## A role's name reads as a name

A role's name is written into formulas in every dialect and into every trace
line read from its record. It must start with an ASCII letter, and hold only
ASCII letters, digits, underscores and single spaces between words. Anything
else would read as something the method does not do -- `Reference-B` typesets
in LaTeX as a subtraction, a role that is a template specialisation,
`Batch<2>`, as two comparisons, and `f_c of 9` as arithmetic on a number -- so
it is refused, and the author is pointed at `TagName`:

```
static assertion failed: formula: this record role's displayed name is not identifier-like; a role's name is written into formulas and traces in every dialect, so it must start with an ASCII letter and hold only ASCII letters, digits, underscores and single spaces between words -- a leading digit or an operator character such as - < ' * would read as arithmetic, a control character breaks the page, and a non-ASCII character is dropped by some LaTeX fonts; the role appears in this diagnostic as the template argument of RequireIdentifierLikeRoleName -- specialise formula::TagName for it to spell its name so
```

`TagName` spells a role as it should read:

```cpp
template <>
struct formula::TagName<SecondReference>
{
    static constexpr std::string_view of() noexcept { return "second reference"; }
};
```

```text
f_c of second reference
f_c\ \text{of }\mathrm{second\ reference}
```

## Every number here is invented

The keys, batches, loads and strengths on this page are invented, as in every
other example in this repository.
