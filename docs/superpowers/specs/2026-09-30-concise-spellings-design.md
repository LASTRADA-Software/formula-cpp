# Concise Spellings Design

The implementation plan is `docs/superpowers/plans/2026-09-30-concise-spellings.md`.


## Why

The 19 programs in `examples/` repeat a few shapes, and those shapes make the library feel verbose:

| Shape | ≈ sites in examples | Evidence |
|---|---|---|
| `formula::Rational { 273, 10 }`, `*Rational::make(..)`, `*Rational::from_decimal(..)` | ~260 | **38 files define their own `rat()`**. Header comments call a `rat()` that does not exist (`expression.hpp:87,94`, `rejection.hpp:12,149,223-238`, `lookup.hpp`, `precision.hpp:837`, `series.hpp:193`). |
| `Measured<Q> { Rational { n } }`; `measured_series<Q>(Measured<Q>{..}, …)` repeating `Q` for every element | ~115, plus 20 series | `series.cpp` layers `m<Q>()` over `rat()`. |
| `r.has_value() && r->is_value() && r->measurement().value() == X` | ~60 | 7 local helpers (`valueOf`, `exact` ×2, `exact_text`, `fraction_string`, …) |
| A hand-built trace, `Trace<> t{}; RecordingSink<> s{t}; (void) verb(…, s);`, then evaluating again for the value | 8 helpers, 8 inline blocks | `explain*` exists only for a Node, a series, a retry and a worksheet. |
| printf noise: `%.*s`, `static_cast<long long>`, `.to_double()`, `? "yes" : "no"` | ~250 | Only `display.cpp` and `electricity_bill.cpp` use the existing `std::format` support. |
| An enum turned into text by hand | 3 switches | No `describe()` for `ConstraintOutcomeKind`, `RetryEnd`, … |
| `<unit::X, DecimalPlaces{N}, RoundingMode::M>` restated at every use | ~25 | A standard states its rounding once. |
| The result quantity named again at every call | ~100 | Mostly inside per-file helper lambdas. |

## Decisions (owner, 2026-09-30)

- All four addition groups are in scope: literals and inputs; reading and printing; a trace from every verb; stating a rule once.
- **Bound formulas are in scope.** `yields<Q>(expr)` names the result once. The author still names it; nothing is deduced from the expression.
- **`std::print` / `std::println` replace printf and iostream** throughout the examples, tools, support code and docs, not only in the examples.
- `trace_of` gives the trace of an evaluation, success or failure, in one call; it returns no outcome, so a caller who needs one still reads it with `checked_evaluate` or `checked_explain`.

## Rules nothing here may bend

- Exactness: no implicit `double`, and a literal is exact or refused.
- Absent is not zero.
- The author names the result quantity.
- No unit is guessed for a bare number.
- No macros.
- Every fallible operation keeps its `checked_` form.
- One mistake produces one message, which begins `formula: ` or names a `formula_` guard.

## Considered and not done

| Idea | Why not |
|---|---|
| Deduce the result from the expression | The result is the author's choice (`evaluate.hpp:469-472`). `yields` keeps it the author's choice. |
| A defaulted quantity tag via `decltype([]{})` | CONTRIBUTING invariant 6: the type differs in each translation unit, which was verified to fail at link time. |
| A macro to declare quantities | The design requires traceability without macros (`docs/superpowers/specs/2026-09-23-formula-cpp-design.md:194-201`). |
| Implicit `double` → `Rational` | Inexact. `_r` gives the short spelling exactly. |
| Deduce `constant<unit::X>`'s unit, or a lookup's key unit, from the other operand | A standard states 47.3 kN against a quantity declared in N, and a table in mm may key a quantity declared in m. |
| A default `render_trace` limit or format rounding mode | Both are deliberate (`trace_render.hpp:15`, `format.hpp:460`). |
| `var<Q> = 180` binding | It would be an assignment operator on a const object that assigns nothing. `Measured<Q> { 180 }` already compiles. |
| `yields` over a curve | `checked_evaluate_curve` names two results (domain and values). Out of scope. |
| `DecimalRounding` for `rounded_ln` / `log10` / `exp` | These take no unit, and `<DecimalPlaces{4}, Mode>` is already short. |
| Migrating the ~2600 `rat()` uses in `test/` | Out of scope; tests adopt `_r` as they are touched. |
