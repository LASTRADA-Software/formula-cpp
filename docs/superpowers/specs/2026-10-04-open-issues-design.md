# Closing the open issues — design

**Status:** draft, ready for review · **Date:** 2026-10-04 · **Owner:** Christian Parpart

Resolves issues #11, #13, #14, #15, #16, #17, #18, #19, #20 and #21: every issue open on 2026-10-04. They land
together, on the branch `feature/open-issues` from `master` at `1ed39ed`, in one pull request whose body closes all
ten.

## 1. Delivery

The ten issues fall into three groups that touch different code:

| Group | Issues | Main files |
|---|---|---|
| Lane A: trace text | #14, #15, #17, #16, #18 | `trace_render.hpp`, `trace.hpp`, `precision.hpp`, the unit checks, `test/trace_shown_unit_tests.cpp`, pinned trace texts in tests and guides |
| Lane B: numerics | #20, then #19 | `detail/transcendental.hpp`, `rounded_transcendental.hpp`, `detail/least_squares_kernel.hpp`, the census, `docs/numeric-headroom.md`, `docs/opaque-and-retry.md` |
| Final tasks | #11, #21, then #13 | `trace.hpp`, `README.md`, one design document, then comments in about 20 headers and most test files |

- Lane A and Lane B run in parallel. Lane A runs in the session's worktree. Lane B runs in agent-owned worktrees,
  each fast-forwarding from the previous Lane B head. Lane B merges into the branch before the final tasks start.
- #20 precedes #19 inside Lane B: both edit `docs/numeric-headroom.md` and the kernels' comments.
- #13 runs last. It rewrites comments in files both lanes change, so running it in parallel would conflict with
  both. Running it last also catches any label a lane adds.

## 2. Lane A: trace text

### 2.1 #14: a scaled dimensionless unit must have a symbol

**Problem.** A dimensionless quantity in a unit with a scale and no symbol (hundredths, magnitude 1/100, no symbol)
traces as the number in that scale with nothing after it: one half prints as `50`. `detail::spells_coherent_unit`
(`trace_render.hpp:662`) leaves every dimensionless value in its declared unit, which is right only at scale 1.

**Decision.** Refuse such a unit where it is declared. A scaled dimensionless unit without a symbol cannot be
written truthfully anywhere, so it is a declaration error, not a rendering problem.

**Design.**

- A new check, `detail::RequireNamedScaledScalar<Unit U>`, beside the other unit checks. It fails when all three
  hold:
  - `U.dimension == dim::Scalar`;
  - `view(U.symbolText)` is empty;
  - the magnitude is not 1 or the offset is not 0, compared as reduced fractions.
- Its message: `formula: a dimensionless unit with a scale must have a symbol (for example "%"), or the quantity
  must be declared in scale 1`.
- It is asserted at every entry point that accepts a `Unit`:
  - a quantity's description, beside `DescribesConsistentDimension` (`quantity.hpp:244`);
  - `constant<U>`;
  - every node or factory with a unit parameter: lookup, snap and band keys, `rounded<…>` (including
    `DecimalRounding` and `SignificantRounding`), `rounded_output`, conformity, escape, and any other.
  The plan lists every site, found with `git grep` for `Unit ` template parameters.
- Where an entry point already gates its checks so that one mistake gives one message, the new check is gated the
  same way.
- `unit::One` and every shipped unit pass. The scaled ones (`Percent`, `PerMille`, `PartsPerMillion`,
  `MilligramPerKilogram`) have symbols.
- `spells_coherent_unit` keeps its rule. Its comment states that a dimensionless unit with no symbol now always
  has scale 1, so its bare number is the value.

**Tests.** Negative tests (`test/negative/`, `formula_add_negative_test`) on MSVC and clang-cl, one per kind of
entry point: a quantity, a constant, and a keyed node. Each expected text includes `must have a symbol`.

**Compatibility.** This is a breaking change: a program that declares such a unit stops compiling. The CHANGELOG
entry says so and names the fix: give the unit a symbol, or declare the quantity in scale 1.

### 2.2 #15: the rounding clause names an unnamed unit by its size

**Problem.** `round(#1, to 2 dp)` writes no unit clause when the unit has no symbol, while the value after it reads
in the coherent unit. "2 dp" then reads as places of a kilogram, which is not what was computed.

**Decision.** Name the unnamed unit by its size in the coherent unit, in the existing `to N dp of <unit>` shape.

**Design.**

- A new `detail::rounding_unit_text(Unit)` gives the text after `of`:
  - a unit with a symbol gives its escaped symbol, as now;
  - a dimensionless unit at scale 1 with no symbol gives nothing, as now. After §2.1 this is the only symbol-less
    dimensionless unit that can reach a rounding;
  - a dimensioned unit with no symbol and no offset gives its magnitude, spelled exactly in fraction style, then
    the coherent unit's spelling (`coherent_unit_text`): `1/1000 kg`. A magnitude of 1 is still written: `1 kg`;
  - a dimensioned unit with no symbol and an offset gives its size and its zero, both in the coherent unit:
    `1 K from 27315/100 K`. The places then count steps of the size from that zero, which is how the rounding
    computes them.
- It replaces `unit_symbol_text(...)` at every `rounding_call_text` call site (`trace_render.hpp:1163`, `:1275`,
  `:1341`, `:3007`), and at `render()`'s `detail::rounding_call` call sites (`render.hpp`), so the formula text
  and its trace use the same words.
- Rounding to significant digits that writes a unit clause gets the same text.
- The rounded transcendental keeps writing no unit clause: it rounds a pure number at scale 1.

**Example.** `2. round(#1, to 2 dp of 1/1000 kg) = 3/1000 kg`.

**Tests.**
- In `test/trace_shown_unit_tests.cpp`: a rounding to 2 places of a quantity in `UnnamedGram`. Its line contains
  `to 2 dp of 1/1000 kg`.
- A `render()` test for the same formula.
- A test for the offset form, if any rounding form accepts a unit with an offset. If none does, the plan records
  that, and the offset branch is unreachable and stays untested.

### 2.3 #17: an inverse-only coherent unit uses negative exponents

**Problem.** `coherent_unit_text` spells a dimension with no positive exponent as `1/X`, so after a fraction a
line reads `20000/413 1/kg`.

**Decision.** When nothing would stand above the slash, write each factor with its exponent negated, in the caret
style the spelling already uses. Units with a numerator keep the slash.

**Design.**

- In `coherent_unit_text` (`trace_render.hpp:610`), when `above` is empty, `below` is built with negated
  exponents and no slash.
- Factors keep their existing order and are separated by a space: named bases first, then the SI bases.
- Exponent spelling, for a factor of exponent -n/d:
  - `^-n` when d is 1, including n = 1: `kg^-1`;
  - `^(-n/d)` otherwise: `kg^(-1/2)`.
- Examples: `kg^-1`, `s^-1`, `m^-3`, `m^-1 s^-1`, `JPY^-1`, `kg^(-1/2)`. Unchanged: `m/s`, `EUR/JPY`,
  `EUR s^2/(m^2 kg)`.
- The function's comment loses `1/JPY` and gains `JPY^-1`.

**Tests.**
- The `coherent_unit_text` cases in `test/opaque_tests.cpp` change to the new spelling, plus a case for each
  example above.
- Every pinned trace text containing a `1/<unit>` coherent spelling, in tests and guides, is updated.
- The whole-trace walker in `test/trace_shown_unit_tests.cpp` accepts the `^-n` and `^(-n/d)` forms.
- A new walker case: a value with an inverse coherent unit, in the fraction style. Its line contains
  `20000/413 kg^-1`, or the equivalent for the case's data.

### 2.4 #16: a precision limit's first pass borrows its unit

**Problem.** Pass 1 of a precision limit restates the level's value, but its unit is chosen statically
(`precision.hpp:1015`): the limit's first placeholder's quantity, else the level expression's quantity, else the
coherent unit. A `ConstantNode` names no quantity, so a constant level declared in grams reads `40 g`, then
`1/25 kg` on the pass-1 line.

**Design.** Where the trace records the pass-1 step, its unit becomes
`detail::restated_unit_or(steps, operands, dimension, value, levelUnit)`, as pass 2's already is
(`trace.hpp:3441`). The static `levelUnit` stays the fallback. The safety rule is the one every restating step
uses: `restated_unit_or` borrows only a unit with a symbol, of the same dimension, for exactly the same value, and
passing `borrowable_for_a_point`.

**Tests.** In `test/trace_shown_unit_tests.cpp`, a precision limit whose level is `constant<unit::Gram>(…)`. Its
pass-1 line reads in grams, and the case is added to the whole-trace walker.

### 2.5 #18: the snap's on-a-value test, and an unused bound

**Problem.** A snap decides "on a permitted value" by comparing raw numerator/denominator pairs
(`trace_render.hpp:2164`), while table rows compare bounds by value (`same_declared_bound`).

**Finding.** The raw compare is exact, so no change of behaviour is needed:
- An exact hit records one row twice: `Segment { Permitted[i], Permitted[i] }` from a single index
  (`snap.hpp:134`).
- The permitted set passes `RequireValidBreakpointTable`, which makes it strictly ascending by value, so two
  different rows never hold the same value.

**Design.**
- A comment at the comparison states the two facts above, so the difference from table rows reads as intended.
- `lookup_miss_text` spells its low bound only on the path that writes it.

**Tests.** None new: the behaviour does not change. Existing snap and lookup-miss tests must stay green.

## 3. Lane B: numerics

### 3.1 #20: the logarithm and exponential kernel takes 128-bit arguments

**Problem.** `Rational` holds 128-bit integers, but the kernel behind `rounded_ln`, `rounded_log10` and
`rounded_exp` (`detail/transcendental.hpp`) narrows its argument's numerator and denominator to 64 bits
(`narrow_to_int64`, `:220-225`, `:297-302`) and refuses wider ones with `Overflow`. `rounded_exp` refuses arguments
above 44, a bound derived when `Rational` was 64-bit. The kernel's fixed point is already 384 bits
(`WideUnsigned<12>`, 128 fraction bits). Only its inputs, and the 64-bit long division `scaled_quotient`, are
narrow.

**Design.**

- **Inputs.**
  - `natural_log_magnitude` and `exponential_enclosure` take the argument's magnitudes through `wide_magnitude`
    (`UInt128`), not `narrow_to_int64`.
  - A reduced `Rational`'s numerator and denominator have magnitudes below 2^127.
  - Every reduction step is restated for magnitudes below 2^127, where it was for magnitudes below 2^63.
- **`scaled_quotient`** becomes a bitwise long division of `UInt128` operands. It produces 128 quotient bits, and
  carries the bit the remainder's doubling shifts out. The general `divmod` stays out of the kernel, for the cost
  reason in the file comment.
- **ln(a/b)**, a > b:
  - B = b·2^k with B ≤ a < 2B, so k ≤ 126 and a + B < 2^128.
  - z = (a − B)/(a + B) < 1/3 as before, so the atanh series, `AtanhSlack` (64) and `AtanhTermLimit` (42) are
    unchanged.
  - The ends lie at most k + 128 ≤ 254 units of 2^-128 apart: under 2^-120, absolute.
- **log10:**
  - |ln(a/b)| < ln 2^127 < 89.
  - The ends lie under 254 · 0.44 + 89 + 2 < 203 units apart.
  - The widest product, upper_ln · (M + 1), is below 2^262.
- **exp(x):**
  - The rounded form refuses x > 887/10 with `Overflow`. 887/10 is below 128 ln 2 ≈ 88.72, which bounds the
    reduction's k by 127, and it replaces the literal 44 (`rounded_transcendental.hpp:79`).
  - Whether an answer below that cap fits is decided by the rounding's existing narrowing into `Int128`
    (`round_wide_ratio`), so every answer that fits at the requested places is given:
    - e^88 at 0 places fits;
    - e^88.5 does not fit at any allowed number of places and refuses with `Overflow`.
  - The −43 lower end stays: below it the value rounds to 0 at every allowed number of places.
  - **The exponential computes with 192 fraction bits**, not 128. Near 2^127, 128 fraction bits leave the
    enclosure up to 2^8 last units wide, so e^45 at 18 places and e^88 at 0 places could never be decided: every
    answer would be `Overflow`. With 192 bits the worst case is 2^-56 of a last kept unit, the margin the 64-bit
    kernel had.
      - ln 2 is carried to 192 bits.
      - `TaylorTermLimit` becomes 50, since the series ends by its 43rd term.
      - `ExponentialSlack` stays 512, since 360 is needed.
      - The logarithms stay at 128 fraction bits.
  - X = floor(|x| 2^192) comes from the widened `scaled_quotient`. Its error is 128 units, not 64, because k now
    reaches 127.
  - The widest numerator is below 2^321. `decide_rounding` scales it by up to 10^18, keeping it under 2^381. So
    `KernelLimbs` stays 12, and the comment on it states the new widths.
- **The file comment's derivation** is rewritten for these bounds. Every number in it is either derived there or
  pinned by a test.
- **Cost.**
  - The constant-evaluation steps are re-measured on cl, and the "What it costs" figures are replaced with the new
    measurements.
  - The existing compile-time check (`test/transcendental_tests.cpp:316-324`, `log10 2` to 3 places) must still
    compile on all four compilers under their default budgets.
  - If it does not, the kernel is made cheaper. Raising the budget is not an option: a consumer cannot be asked to
    do that.
- **Native and portable paths.** `UInt128` uses `__int128` on GCC and Clang and portable code on cl and clang-cl.
  The kernel's results are the same on both paths: the new tests run on all four compilers.

**Tests** (`test/rounded_transcendental_tests.cpp`, `test/transcendental_tests.cpp`):
- The pinned refusals become pinned answers: `ln 2^70`, `log10 2^70` and `exp 2^-64`.
- New points:
  - ln and log10 of 2^127 − 1, and of 1/(2^127 − 1);
  - ln of a ratio near 1 of two integers near 2^126;
  - exp 45 at 18 places;
  - exp 88 at 0 places;
  - exp 88.5 and exp 89, both refused with `Overflow`.
- The −43 behaviour stays pinned.
- Expected values are computed independently with Python's `decimal` module at 60 digits. A comment at the tests
  says so and gives the expression used.

**Docs.**
- `docs/expressions.md:510-519` loses the 64-bit statement and the 44 bound, and gains the 887/10 cap and its
  reason.
- `rounded_transcendental.hpp`'s comments (`:46`, `:50-51`, `:125-127`, `:136-138`, `:147-151`) are updated the
  same way.
- `docs/numeric-headroom.md` loses the kernel's entry under "What this does not decide" (`:424-431`).
- CHANGELOG: an entry for the widened arguments.

### 3.2 #19: the guides' bit widths are pinned by tests

**Problem.** Two guides quote bit widths that nothing measures:
- the least-squares fit's widest intermediates, `docs/numeric-headroom.md:336-338`;
- the opaque example's coefficient widths, `docs/opaque-and-retry.md:327-328`.

The headroom page's least-squares rows come from `LinearLeastSquares::compute_exact`, which computes in
`LinearLeastSquares::exact_limbs` = 8 limbs, 256 bits. The 12-limb kernel belongs to the fit over observations,
which those rows do not use.

**Design: the fit kernel's widest intermediate.**

- The census hook (`FORMULA_CENSUS_NOTE`, `detail/checked_int.hpp`) gains a width form,
  `census_record_width(CensusRole, std::size_t bits)`.
- `LinearLeastSquares::compute_exact` calls it with `bit_length()` of every wide sum, centred sum and solve product
  it forms.
- The hook keeps its existing guarantee: outside the census program it expands to nothing, and its arguments are
  never evaluated.
- The census program (`support/census_tally.cpp` and the generator of the headroom page's tables) records the
  widest value per fixture.
- The least-squares table gains a generated column, "widest fit intermediate (of N bits)", for the rows the exact
  kernel computes. N is generated from `LinearLeastSquares::exact_limbs · 32`, never written by hand.
- The hand-written "68 bits … up to 249 of the 256" sentence goes. The prose names the width through the generated
  table.
- `docs.numeric-headroom` already fails when the generated tables drift, so the column is pinned by that test.

**Design: the opaque example's coefficients.**

- A test fits the example's data with the library's exact kernel and pins the bit widths of the numerator and
  denominator of the slope, the intercept and R². The example's data is fifty readings at eight decimals, the
  data `docs/opaque-and-retry.md` describes. If an example program already holds the data, the test includes it
  from there. Otherwise the data lives in the test, and the guide points at the test.
- The guide quotes the test's figures and names the test.
- If the test's figures differ from the Python-computed figures now in the guide (65 and 73, 93 and 91, 130 and
  130), the test's figures are the ones published.
- The example's refusal with `Overflow` stays pinned as it is.

## 4. Final tasks

### 4.1 #11: `explain` and `checked_explain` refuse a series in the library's words

**Problem.** `explain<Q>(series, environment)` and `checked_explain<Q>(series, environment)` fail with "no
matching function". `trace.hpp` has only a `Node` overload and a `Yields` overload of each, so overload resolution
fails before any library check runs.

**Design.**

- A new check beside `RequireSingleValueExpression` (`evaluate.hpp:191`): `detail::RequireSingleValueTraced
  <Expression>`.
  - It has the same `refused_already` exemption, so a series refused where it was written is not refused twice.
  - Its message: `formula: this expression is a series, not a single value; explain it with explain_series, or
    reduce it to one value first (sum, interpolate_at)`.
- A `SeriesNode Expression` overload of `explain` and of `checked_explain` in `trace.hpp`. Each asserts the check,
  then returns an empty value of the verb's declared return type, so that only the one message appears. This is the
  shape of `evaluate`'s `SeriesNode` overloads (`evaluate.hpp:514`, `:532`).
- `trace_of`, and the `Yields` forms through `detail::SingleValueBoundCheck`, keep refusing a series exactly as
  they do now.

**Tests.**
- Two negative tests, `test/negative/explain_series_refused.cpp` and `checked_explain_series_refused.cpp`,
  registered with `formula_add_negative_test`. Their expected text includes `explain_series`, and they pass on
  MSVC and clang-cl.
- Before they are committed, removing the new overload is checked to bring back the generic "no matching function"
  diagnostic.

**Docs.** One sentence each in `docs/tracing.md` and `docs/series.md`, where `explain` and `explain_series` are
described.

### 4.2 #21: two documentation corrections

- `README.md:247-252` links the tracing guide twice in one paragraph. The link in the middle of the paragraph goes,
  and the closing "See [the tracing guide](docs/tracing.md)." stays.
- `docs/superpowers/specs/2026-10-03-int128-rational-design.md:87` says the checked forms use the compiler's
  overflow builtins. The sentence is corrected to describe the code:
  - For `Int128`, add and subtract compute on the two words' bit patterns with the portable routines on every
    compiler, and detect overflow from the signs.
  - Only multiply uses the native checked builtin, through `u128_mul_checked`.
  - The reason: calling `Int128`'s `+` or `-` first would break their precondition that the exact result fits.

### 4.3 #13: no internal development labels

**Problem.** More than 80 comments, test names, one guide sentence and one CMake comment refer to the project's
development history ("phase 12", "phase 15's spike, step 9", "spec phase 8"). A reader cannot look these up.

**Design.** Every hit of the issue's search outside `docs/superpowers/` is rewritten:

- A comment that quotes a measurement keeps the measurement and says where it holds, by compiler and version, or
  points to the test that pins it. For example: "measured on cl 19.51 and g++ 14: the five-parameter spelling
  compiles".
- A reference that adds nothing is removed.
- Section banners name the feature, not the work that added it.
- The two census test cases are renamed after what they cover: `census: phase 13's fixtures` is named after the
  features those fixtures exercise, and `census: phase 12's cumulative sums and interpolation` becomes `census: cumulative sums and interpolation`.
  No build file refers to their names.
- `docs/quantities.md:186` and `cmake/CheckInstalledHeaders.cmake:7` are rewritten the same way.

**Acceptance.** This search returns nothing:

```text
git grep -nE '\b[Pp]hase [0-9]+|\bspike\b|\bstep [0-9]+\)' -- include test examples tools docs/*.md README.md cmake CMakeLists.txt
```

## 5. Verification

- **Per task:**
  - `cl-debug`, plus the negative tests on `clangcl-debug`.
  - The #20 task also builds and tests `gcc-release` and `clang-debug` under WSL, since `UInt128` takes its native
    path only there.
  - Each task is reviewed, and fixed until no finding is open.
- **After the lanes merge:** the census page is regenerated once, and `docs.numeric-headroom` passes.
- **Finish:**
  - a whole-branch review, its fixes, and a re-review;
  - all eight presets green locally;
  - Doxygen, and `mkdocs build --strict`;
  - CI green on the pushed head.
  - Then the pull request, whose body closes the ten issues, is marked ready for review.
- **Docs and CHANGELOG:**
  - Each task updates the guides whose text it changes: the trace samples for §2.2-§2.4, the expressions guide
    and the headroom page for §3.1, the opaque guide for §3.2, the tracing and series guides for §4.1.
  - Each task adds its CHANGELOG entry under the unreleased section. §2.1's entry is marked as a breaking change.

## 6. Out of scope

- New features.
- Any change to how `trace_of` or the `Yields` forms refuse a series.
- `docs/superpowers/`, except the one sentence §4.2 corrects. Those documents record how earlier work was planned.
