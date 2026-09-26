# Cross-Record Context and Lineage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a formula read values from other samples and other tests, not only from the specimen in front of it. Every such value must appear in the trace with where it came from: which record, which sample and test, and whether it was measured or typed in. A cross-record read can also be *gated* on lineage, meaning it runs only if the two records share declared attributes such as a material batch or a test method.

**Architecture:** A **record** is a role (a plain class tag, closed in the type) bound at run time to a record key, a set of lineage keys and an ordinary `Environment`. A `RecordContext` *is* its own record's environment, because it inherits from it, and it also holds the other records by role. So every existing evaluator accepts a context unchanged. A cross-record read is a scope node, `from_record<Role>(expression)`: the evaluator evaluates the operand against that role's environment, checks the declared lineage first, and tells the sink the record's origin through two optional hooks. `RecordingSink` stamps that origin on every step recorded inside the scope. The origin is built only by the library, from the same record object the values were read from.

**Tech Stack:** C++23, header-only. Catch2 via CPM. `STATIC_REQUIRE` for compile-time behaviour, plus the `test/negative/` must-not-compile harness, whose asserted message strings are tested API.

**Spec:** `docs/superpowers/specs/2026-09-23-formula-cpp-design.md`:
- §9.1, fourth pattern ("an input comes from another test on another specimen"), and "a **sample/test context** able to reference other samples and tests";
- §16.1 ("the trace must say which": measured, derived or manually entered);
- §16.6, last bullet ("the evaluation context must be able to reference other samples and other tests");
- §16.8 ("lineage predicates", and the boundary paragraph after the list);
- §17, row 14.

**Read before any task:** `.superpowers/sdd/2026-09-25-methods-and-overlays/defect-classes.md`. Every task below names the classes its steps are built to prevent. Every implementer's report says, for each of the eight classes, what they checked and how.

---

## Dependencies: can this start now, in parallel with phase 12?

**Verdict: yes. Tasks 1–9 depend on nothing in phase 12 and can start now, from `48d03ac` in a separate worktree. Only task 10 touches series, and it must run on whichever of the two branches lands on `master` second.**

### Why nothing before task 10 needs series

- **Nothing reads a series.** A record's environment is an ordinary `Environment`. `from_record` wraps a `Node`, and phase 12's S3 keeps a series disjoint from `Node`. Tasks 1–9 never construct a series, a curve or an observation set.
- **The context forwards phase 12's future accessors without being edited.** `RecordContext` *inherits* its own record's `Environment` (X2), so phase 12's `get_series` and `get_observations` members become members of the context as soon as both branches are on `master`. Task 1 measures this inheritance against today's accessors. Task 10 re-measures it against phase 12's.
- **Phase 12 needs nothing from here.** Its plan never mentions records, contexts or lineage.

### Why task 10 is mandatory, and must not be skipped when the second branch lands

Once both branches are on `master`, `from_record<Reference>(sum(series<Retained, 5>))` compiles without task 10:
- `sum` is a `Node`;
- the context inherits `get_series`;
- the series variable is read from the reference record.

But its step is recorded by phase 12's `series_produced` hook, which task 5 of this plan never stamps with a record origin. **The trace would then show another sample's series as this sample's, silently.** That breaks this phase's hard constraint, and it is exactly the join that phase 11's task 9 existed to catch. So:

- **If phase 12 lands first (the lead expects this):** this branch rebases onto `master` and runs task 10 before it merges. The plan is written for this case.
- **If phase 14 lands first:** phase 12 runs task 10 (as written here) after its own rebase, before it merges. The lead adds the task to phase 12's list when phase 14 merges.

Task 10 depends on these phase 12 tasks: **2** (series input and evaluation, `EnteredSeries`), **3** (the series hooks and `Step::elements`), and **5** (`sum`, the first series-holding `Node` a scope can wrap). In practice that means "phase 12 merged".

### Overlap with the files phase 12 touches

Phase 12's File Structure lists these shared files: `environment.hpp`, `evaluate.hpp`, `method.hpp`, `sink.hpp`, `trace.hpp`, `trace_render.hpp`, `render.hpp`, `document.hpp`, `overlay.hpp`, `formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt`, the per-surface test files `trace_tests.cpp`, `trace_render_tests.cpp`, `render_tests.cpp`, `document_tests.cpp`, `vocabulary_tests.cpp` and `overlay_tests.cpp`, and the guide wiring (`examples/CMakeLists.txt`, `tools/gallery/main.cpp`, `docs/gallery.md`, `mkdocs.yml`, `README.md`, `docs/index.md`).

This plan is designed to stay out of as many of them as it can:

| Shared file | Phase 14 touches it? | How the overlap is kept small |
|---|---|---|
| `environment.hpp` | **No** | The context inherits `Environment` and lives in `record.hpp` (X2) |
| `method.hpp` | **No** | A context *is* an environment, so `evaluate_method` and `check_method` take one unchanged (task 2 proves it) |
| `sink.hpp` | **No** | `RecordOrigin` and `LineageCheck` live in `record.hpp` and `lineage.hpp`, not beside `VariantSelection` |
| `overlay.hpp` | **No** | `detail::ConstantRewrite` is *specialized* from `record.hpp`, which includes `overlay.hpp` (X10; task 1 measures it) |
| `*_tests.cpp` per surface | **No** | Every phase-14 test goes in new files: `record_*_tests.cpp`, `lineage_tests.cpp` |
| `evaluate.hpp` | Yes, **one hunk** in the `VarNode` overload (task 4) | Phase 12 edits `checked_evaluate` for its refusals, which is a different function |
| `trace.hpp` | Yes | Appends 2 `StepKind`s and 4 `Step` fields, adds one `StepKindOf` specialization and 2 `RecordingSink` hooks, and adds stamping inside `produced` and a `recordStack` in `Trace` |
| `trace_render.hpp` | Yes | Appends 2 `case`s to the kind switch, plus a suffix for origin and source |
| `render.hpp` | Yes | Appends 1 `render_node` overload and 1 `PrecedenceOf` specialization |
| `document.hpp` | Yes | Adds 1 `SymbolEntry` field, a role on `Walk`, and 1 `collect` block |
| `formula.hpp`, `CMakeLists.txt` (`FILE_SET`), `test/CMakeLists.txt`, `test/consumer_globals_tests.cpp` | Yes | Adds one line per new header or test; conflicts are trivial list merges |
| guide wiring | Yes, task 9 only | See below |

**Expected textual conflicts on the second rebase:**
- the tail of the `StepKind` enum;
- the tail of `Step`;
- the `trace_render.hpp` switch;
- the list of `StepKindOf` specializations;
- the list of `SymbolEntry` fields.

Both phases append in the same places. Every one of these is a two-sided append, and resolving it means keeping both. Task 1 step 7 measures the real conflict set with `git merge-tree` against the series branch's HEAD at that time, so the lead knows it in advance and does not discover it on the day.

**Also outside phase 12:**
- Phase 11's task 11 (its guide) is still in flight on `phase-11-methods-and-overlays` and touches the same guide wiring. So **task 9 of this plan runs only after this branch has rebased onto `master` once phase 11 has merged.** Tasks 1–8 do not touch the guide wiring.
- Phase 13's plan was not available to me. If it adds `StepKind`s or `Step` fields (statistics steps probably will), it joins the append conflicts listed above.

### Recommended order

1. Tasks 1–8 in worktree `D:/formula-cpp-context`, branched from `48d03ac`.
2. Rebase onto `master` after phase 11 merges.
3. Task 9, the guide.
4. Task 10 on whichever branch lands second.

If phase 12 has already merged by step 2, run task 10 before task 9, so that the guide can show a reference sample's grading curve.

---

## Design decisions, and the lead's rulings (2026-09-26)

Each decision gives a recommendation, the reason for it, and what is still unmeasured. Decisions marked **SPIKE** depend on a measurement that task 1 makes. The lead ruled on X4, X6, X7, X10, X12 and X13 and confirmed the names on 2026-09-26; X1, X3, X8 and X9 needed no separate ruling. Task 2 does not start until task 1's findings have confirmed or amended the SPIKE items (X2, X5, X10's mechanism, X11).

### X1. A record is a role type, bound at run time to a key, lineage keys and an environment

**Recommendation:**
- A **role** is a plain class tag the author declares (`struct Reference {};`, `struct PriorTest {};`). The formula names the role. Which actual record plays it is decided at run time.
- `formula::ThisRecord` is the library's role for the specimen being evaluated.
- A record is built with `record<Role>(record_key(sample_id { 17 }, test_id { 3 }), environment(...), lineage<MaterialBatch>(4411), ...)`.

**Why:** this is phase 11's V3 shape carried over. The set of possibilities is code, and which one applies is data. It is also §16.1's line: the method is code, and which reference sample a lab paired with this one is master data.
- With the role in the type, a formula that reads from a role the context does not bind is a compile error naming the role, never a run-time miss.
- Role names reuse `tag_name` / `TagName` (`tag.hpp`). An author who has customized a variant tag has already learned how to customize a role.

**`RecordKey` is two strong types, not an aggregate.** Consider `RecordKey { .sample = 17 }`. A designated-initializer aggregate silently value-initializes the missing `test` to 0, and a test key of 0 is a real key. That is a silent default in the one field the trace exists to show. So the key is `record_key(SampleId, TestId)`. Both types have `explicit` constructors, and a swapped pair does not compile.

### X2. The context *is* its own record's environment. **SPIKE**

**Recommendation:** `RecordContext<ThisRec, Others...>` publicly inherits `ThisRec`'s `Environment` type and adds `record<Role>()` and `this_record()`. The context is built with `record_context(record<ThisRecord>(...), record<Reference>(...), ...)`.

**Why:**
- Every evaluator in the library takes `Env const&` and uses only `get<Q>()`, `provides<Q>`, `is_entered<Q>` and `source_of<Q>()`. I checked this by grepping `environment\.` and `Env::` across `include/formula-cpp/`: only `evaluate.hpp:214,346,348` use them.
- With inheritance, `checked_evaluate`, `evaluate_method`, `check_method`, `check_all`, the predicates, the lookups and `explain` all take a context **with no edit**. `method.hpp` and `environment.hpp` stay untouched, which is most of what keeps this phase out of phase 12's files.
- Phase 12's future `get_series` / `get_observations` members arrive for free (see Dependencies).

**Rejected alternative: forwarding members.** Each accessor would be written again, and phase 12's accessors would then be missing from the context until someone remembered to add them. That is a silent gap of exactly the kind this project keeps finding.

**The spike measures, on cl, clang-cl, clang++ and g++:**
- that all of the entry points above compile and give identical results with a context;
- that `RequireProvided`'s message still fires **once** when a context lacks a quantity (defect class 2), and that it names something a reader can act on through the base-class name;
- that a context is usable in a constant expression, for `STATIC_REQUIRE`.

### X3. A cross-record read is a scope, `from_record<Role>(expression)`

**Recommendation:**
- `RecordScopeNode<Role, Requirement, Operand>` is a `Node`, with `dimension = Operand::dimension`.
- Its operand is evaluated against **that role's environment**, the record's plain `Environment` and not a context.
- `from_record<Reference>(var<Strength>)` reads one value.
- `from_record<Reference>(var<Force> / (var<EdgeX> * var<EdgeY>))` computes over the reference specimen's measurements. That is §9.1's "a reference value measured on separate material".

**Refused, each with one message in this library's words:**
- **Evaluating a scope against a plain `Environment`.** There are no other records to read from.
- **A role the context does not bind.**
- **`from_record<ThisRecord>`.** Reading this record "from another record" is a mislabel waiting to appear in a trace.
- **A scope inside a scope.** This is refused structurally, because the operand is evaluated against a plain environment, so the first refusal fires. It needs no second guard and draws no second message (defect class 2). A record has no records of its own.

**Rejected: `var<Q, Role>`.** It covers only a single value, while §9.1 asks for a computation over separate material. It also puts the role on every leaf, where a scope states it once and the trace can show one origin for a whole sub-derivation.

**Not in this phase: a method evaluated over another record.** A method is not a `Node` (phase 11), so a scope cannot hold one. A prior test's *reported result* is a stored value, so `from_record<PriorTest>(var<PriorResult>)` covers the common case. Recomputing a whole method over another record is follow-up **F1**.

### X4. Record and lineage keys are integers. **Ruling: accepted**

**Recommendation:**
- `SampleId`, `TestId` and a lineage key are all `std::uint64_t` underneath.
- The trace stores the integers and `render_trace` prints them.

**Why:**
- `trace.hpp` holds no `<string>`, and says so in its file comment.
- A `Step` is copied into a `std::vector`, and every text field it has today is a `std::string_view` into static storage. That guarantee cannot be given for a run-time sample name.
- Integers are `noexcept` to copy.
- The consumer the spec names (LASTRADA) keys its records by database id. Turning an id into a display name is a report's job, done outside the trace.

**The alternative:** a bounded text key, for example `detail::FixedText<32>`, refused at run time when too long. It is honest, it costs 32+ bytes per stamped step, and it needs a truncation rule.

**Ruling: accepted, `std::uint64_t` keys.** A LIMS maps its record keys to integers. Display labels are follow-up **F5**.

### X5. The origin is stamped on every step inside a scope, and shown where a reader needs it. **SPIKE (for `sizeof(Step)`)**

**Recommendation:**
- `Step::record` is a `std::optional<RecordOrigin>`: the role's name, the record key, and whether the role was bound. `RecordingSink` sets it on **every** step recorded while a scope is open, using a `recordStack` in `Trace`. That stack is bookkeeping, as `branchStack` is.
- `render_trace` shows the origin on:
  - the scope's own step: `5. #4 from record Reference (sample 17, test 3) = …`;
  - every step that names a quantity: `Variable`, `OverriddenConstant`, `DerivedQuantity`, and, after task 10, phase 12's series variable.
  
  Computed steps inside the scope carry the field and do not print it, because their operands already say where the numbers came from.
- Two new `StepKind`s: `RecordScope` and `LineageChecked`. Both are appended.

**Why every step carries it:** the hard constraint says "*anything* evaluated from another sample or test must be visible in the trace with its origin". A programmatic consumer reading one step alone must not have to walk up the operand graph to learn whose number it is. Printing the origin on every computed line would turn a ten-node reference computation into ten repeated suffixes, which is noise. Printing it on the leaves and on the scope keeps every rendered number attributable.

**Why new kinds, not a flag on `Variable`:** a consumer that switches on `StepKind` and ignores a new field would render a foreign value as a local one. A new enumerator makes `-Wswitch` flag every such switch in this library, and in a consumer that builds with it. Existing tests prove that `trace_render.hpp`'s switch is exhaustive today.

**`RecordOrigin` is built only by the library** (X-forgery below). **The spike measures** `sizeof(Step<Rational>)` before and after, on cl and g++. Phase 12 measured 776 → 896 bytes for its own fields, so the two phases compound.

### X6. Measured or typed in: the trace says which, for every input. **Ruling: accepted**

**Recommendation:** add `Step::inputSource` (`std::optional<ValueSource>`) and set it on every `Variable` step, local or cross-record. The `VarNode` evaluator tells the sink through one optional hook, `sink.input_source(node, ValueSource)`, called before `produced`. `render_trace` prints `, entered by hand` for `ManuallyEntered` and nothing for `Measured`.

**Why, and why here:**
- §16.6: "every result carries its origin — measured, derived, or manually entered — and **the trace says which**".
- Today the trace does not say it for inputs. `Environment::source_of` carries a comment saying it exists for "a future tracing layer" that never arrived (`environment.hpp:191-201`). `RecordingSink` has no path to it.
- A cross-record value is where this matters most. A reference strength copied by hand from a certificate is a typed-in number in another sample's record, and the trace must say both things.
- The spec uses the word *origin* for exactly measured, derived and entered. So the hard constraint "visible in the trace with its origin" reads naturally as covering this too.

**Cost:**
- One hunk in `evaluate.hpp`'s `VarNode` overload, the only phase-12-shared file this plan edits outside the trace and render layer.
- An existing rendered trace whose input was `entered(...)` gains the suffix. Task 4 greps the tests for such traces first and reports how many change.

**Ruling: accepted.** Task 4 is in this phase.

### X7. Lineage is a gate on the scope, not a verdict beside the value. **Ruling: accepted**

**Recommendation:** `from_record<Reference>(expression, same_lineage<MaterialBatch, TestMethod>())` checks each named attribute **before** it evaluates the operand, and records one `LineageChecked` step per attribute. The attribute steps are recorded in the order the requirement names them, and each reuses `Step::outcome` (`ConstraintOutcome`).

| Lineage keys | Attribute step | Scope result |
|---|---|---|
| Both known and equal | `Satisfied` | the operand's value |
| Both known and different | `Violated` | `ArithmeticError::DomainError`, operand **not evaluated** |
| Either one unknown | `NotChecked` | **absent**, operand not evaluated |

**Why a gate:** §16.8 says "a computation **permitted only if** its two inputs came from the same method and the same material batch".
- A verdict reported *beside* a computed value, in the way `check_method` reports constraints, would still produce the number. The caller could then report it by forgetting to look at the verdict.
- A gate produces no number, and the trace says exactly which attribute refused, with both keys.

**Why `DomainError` for a violation:** the scalar channel is `Evaluated<Rep>`, with no verdict alternative, and phase 10 refused to widen it for every node kind (`lookup.hpp:80-100`). The lookups have the same problem, and their precedent is `DomainError` on the value plus a trace field that says *why* (`LookupFailure::Missed`). Here, the `Violated` attribute step is that field.

**Why absent for an unknown key:** this is the library-wide rule: "a formula with one missing input has no answer" (`measured.hpp`, `combine`). An unknown batch is a missing input. It is not evidence that two batches differ. `NotChecked` is phase 9's word for a predicate with an absent side.

**The known cost, which the guide must state:** `explain()` goes through the throwing `evaluate()`. So a lineage violation *throws* from `explain` and yields no trace at all. This is the open STATUS follow-up "the trace layer handles failure badly". Until that is fixed, a refused read is traced only through `checked_evaluate` plus a hand-built `RecordingSink`, and the tests do it that way. **Ruling (X7b): accepted, fold it in.** A non-throwing `checked_explain` is added in task 6 (step 5), and the guide shows a refused read traced through it.

**Rejected alternative:** lineage as a `Constraint` in the method's `ConstraintSet`. `Predicate` is `IsPredicateNode` only, so that would widen `predicate.hpp`, `constraint.hpp` and phase 11's `with_constraints`, and it would still compute the number. It is recorded as follow-up **F2**, for a method that wants a lineage *verdict* in its acceptance logic as well.

### X8. Lineage attributes are the author's, not the library's

**Recommendation:**
- An attribute is a plain class tag (`struct MaterialBatch {};`), named through `TagName`.
- The library knows no attribute: it compares keys and nothing else.
- `same_lineage<Attrs...>()` compares against `ThisRecord` by default. `same_lineage<Attrs...>(against<OtherRole>)` compares against another bound record, for §16.8's "its **two** inputs", where neither input need be this specimen.

**Refused:**
- An empty attribute list. A requirement that names nothing checks nothing, and would read in the source as a check.
- An attribute named twice in one requirement.
- An attribute that either record type does not declare, named by attribute and role.
- A record declaring one attribute twice.
- `against<Role>` where Role is the scope's own role, because that compares a record with itself and is always satisfied.

**Why:** this is §16.8's boundary. The library "does not model plant lifecycles". Which batch a specimen belongs to is LASTRADA's data. The library's job is the declared comparison, and its trace.

### X9. A role bound to no record at run time

**Recommendation:** `Record<Role, Env, Lineage...>::unbound()` keeps the record's type but has no key, no lineage and no values. The reference test "has not been done yet" is ordinary lab data, and a type-level absence would make one formula need two context types.
- A scope over an unbound record evaluates to **absent**, never zero.
- It checks no lineage and records no attribute steps.
- Its trace step reads `from record Reference (no record bound)`.
- `RecordOrigin` carries `bound = false`, and has no key to print.

### X10. An overlay's constant rewrites *through* a scope. **Ruling: accepted; SPIKE for the mechanism**

**Recommendation:** specialize `detail::ConstantRewrite<Sub, RecordScopeNode<…>>` in `record.hpp`, so that `with_constant<Q>` and `add_derived<Q>` rewrite the operand exactly as they rewrite any other child.

**Why:**
- A jurisdiction's constant is the *method's* constant. The scope evaluates the method's algebra over another record's measurements, and a shape factor in that algebra is the same shape factor.
- Without a specialization, the primary template refuses **every** overlay over a method holding a scope (`RequireOverlaySeesNode`, `overlay.hpp:1011`). A cross-record method could then never be overlaid at all.
- An `OverriddenConstant` step inside the scope is stamped with the scope's origin (X5), so the trace says the overlay fixed a value inside the computation over record Reference.

**The alternative:** refuse `with_constant<Q>` when Q is mentioned inside a scope. That is safer if a jurisdiction's intent for another record's measurements is ever in doubt, but it forbids the ordinary case.

**Ruling: accepted, rewrite through.** Test both orders (defect class 6), in task 8 step 4.

**Measured by the spike:** that a partial specialization declared in `record.hpp`, after `overlay.hpp` is included, is found by `apply()` on all four compilers.

### X11. How a scope reads on the page. **Decided by the spike; Ruling: option (b)**

**Recommendation:**
- Plain: `(F / A) of Reference`.
- Markdown: `` (`F` / `A`) of Reference ``.
- LaTeX: `\left(\frac{F}{A}\right)\ \text{of Reference}`.

A single-symbol operand is unbracketed: `f_c of Reference`. The scope takes the lowest precedence (`Precedence::Conditional`), so as an operand it is always bracketed: `f_c / (f_c of Reference)`. **This is unambiguous only because of that bracket.** Without it, `a / f_c of Reference` could be read as `(a / f_c) of Reference`.

**Why words:** this is the library's own convention (`to under`, `rounded to 1 dp of mm`). It passes the Markdown guard, with no `[`, and it combines with phase 12's series marker without collision: `m_r(i) of Reference`.

**The spike must typeset** the candidates under MathJax 3.2.2 (the site's pinned version) and under tectonic with `\usepackage[OT1]{fontenc}`, including a role whose `TagName` spelling holds a LaTeX special character. STATUS records that the site's MathJax does not load `textmacros`, so an escaped name inside `\text{}` may show literal macros. That is the likeliest failure, and the spike may choose `\mathrm{}` or another form instead.

**Spike result (task 1, `task-1-spike.md` step 5), and the lead's ruling (b):**
- **Plain:** `f_c of Reference`, `(F / A) of Reference`, and as an operand `f_c / (f_c of Reference)`.
- **Markdown:** `` `f_c` of Reference `` and `` (`F` / `A`) of Reference ``. The role name goes through the existing author-words escaping (`render.hpp`, the one used for lookup keys). Unescaped, a `TagName` spelling holding `*1*` came out as emphasis.
- **LaTeX:** `f_c\ \text{of }\mathrm{Reference}`, `\left(\frac{F}{A}\right)\ \text{of }\mathrm{Reference}`, and `\frac{f_c}{f_c\ \text{of }\mathrm{Reference}}`. **The role name is in math mode, inside `\mathrm{}`.** It is not inside `\text{}`: the site's MathJax does not load `textmacros`, so a text-mode escape such as `\_` or `\%` inside `\text{}` is shown **literally** there (measured: `of Reference\_B`). In `\mathrm{}` the same escapes set correctly under MathJax 3.2.2 and under tectonic OT1 (measured: `Reference_B`, and `reference 1:2 %`). **Ruling: the LaTeX this library writes must come out right in any renderer**, not only where textmacros happens to be loaded.
- **The math-mode escaper** is a new `detail::latex_math_words(std::string_view) -> std::string` in `include/formula-cpp/detail/latex_math.hpp`:
  - a space becomes `\ `, because `\mathrm` drops spaces (measured: `\mathrm{reference specimen}` gave `referencespecimen`);
  - `#`, `$`, `%`, `&`, `_`, `{` and `}` become their backslash forms;
  - `\` becomes `\backslash{}`, `^` becomes `\hat{}`, and `~` becomes `\sim{}`;
  - a colon becomes `{:}`, so that it is not spaced as a relation.

  **It lives in `detail/` so that the existing lookup-key follow-up in STATUS (an escaped key name inside `\text{}` shows literal macros on the site) can reuse it**, instead of growing a second one. Task 7 builds it, and tests every character on the list under both engines with task 1's harness (`spike/x11/`). A controlled failing case (`\frac{1}{`) must come back as an error in both.
- **Combined with phase 12's series marker:** `{m_r}_{i}\ \text{of }\mathrm{Reference}` is clean in both engines.

**The symbol table:** a quantity read inside a scope gets its **own row**, keyed by (role, quantity), with `SymbolEntry::record` set to the role's name. `f_c` read here and `f_c` read from Reference are two inputs, and a merged row would tell a reader to supply one value where the formula reads two.

### X12. Forgery: what the library prevents, and what it documents

Every field the trace reads for an origin comes from one of these:

- **Role name:** from the type, via `tag_name<Role>()`.
- **Record key and lineage keys:** from the `Record`. That class has private members set only by `record<Role>(...)` and `unbound()`. The keys are the caller's statement about its own data, and the library records them as stated.
- **`RecordOrigin`:** from the private constructor behind `detail::RecordOriginAccess`, called only by the scope's evaluator, **from the same `Record` object whose environment the operand is evaluated against**. A value and its origin cannot come from two different records. Task 5 proves this with a mutation.
- **`LineageCheck`:** from a private constructor behind `detail::LineageCheckAccess`, built from the two records' keys in the same evaluator call.
- **Value source:** from the environment entry's type, `Measured<Q>` or `Entered<Q>`.

**What cannot be prevented, and is documented plainly** (defect class 3):
- `Trace::steps` is a public arena any code can append to or edit, and has been since phase 7.
- A valid `RecordOrigin` can be copied and handed to a sink hook by hand.
- A caller can wrap a typed-in value as `Measured<Q>`.
- A role whose `TagName` spells "this record" still renders as `from record this record (sample …)`. The framing and the key show it is foreign, but the name misleads.

The guarantee is about the **recording path**: the evaluator never attributes a value to a record it did not read it from.

**Ruling: accepted.** Document the boundary in plain words, as phase 11 did for `RoundingRule` (task 5 step 10, task 9 item 4).

### X14. Names. **Ruling: confirmed**

`record_context`, `from_record`, `same_lineage`, `against` and `ThisRecord` are confirmed. Each is checked against the consumer-globals rule: no namespace-scope name that is a common local name. The factories `record`, `lineage`, `sample_id`, `test_id`, `record_key` and `unknown_lineage` get the same check in task 1 step 6. If `record` or `lineage` fails it, the spike proposes a replacement.

Phase 15 adds `StepKind`s `RetryAttempt`, `RetryConcluded`, `OpaqueOperation` and `OpaqueOutput`, and the names `AttemptNumber`, `PreviousAttempt`, `ThisAttempt` and `AttemptInput`. None collides with this phase's names (`RecordScope`, `LineageChecked`, `RecordOrigin`, `LineageCheck`, `ThisRecord`).

### X15. What a wrapping environment must forward (coordination with phase 15)

Phase 15's retry wraps the environment in `detail::AttemptEnvironment` and forwards every `Environment` member that nodes call. **This phase adds no `Environment` member that an ordinary node calls:**
- `get`, `provides`, `is_entered` and `source_of` are inherited unchanged (X2);
- `input_source` (task 4) is a *sink* hook, and it reads only `Env::template is_entered<Q>`, which already exists.

**The context's own members** are `this_record()`, `record<Role>()` and the static `binds<Role>`. **Only the scope's evaluator calls them.** It does not find them by duck typing: it recognises a context through `detail::RecordContextOf<Env>`, a detail trait that yields the underlying `RecordContext`. A user type that merely has a `record<Role>()` member must not be able to supply a forged origin (X12).

So **a `from_record` inside a retry works only if `detail::AttemptEnvironment` specializes `detail::RecordContextOf`** to reach the context it wraps. Otherwise it is refused with task 3's "evaluated against an environment that holds only one record" message. That refusal is safe, but wrong for that case. Whichever of phase 14 and phase 15 lands second adds the specialization and a test that a scope inside a retry reads the right record.

### X13. The spec disagrees with itself about lineage predicates. **Ruling: accepted**

§17 row 14 puts "lineage predicates" in this phase. §16.8 lists lineage predicates among the things that "belong to the downstream norm libraries and to LASTRADA, **not** to formula-cpp".

**Recommendation:** read §16.8's boundary as applying to what it names at the end: "does not model plant lifecycles, solve assignment problems, or read graphs". Deciding *which* batch a specimen belongs to is downstream. Declaring that a computation requires two records to agree on an attribute, and tracing that check, is expressing a method constraint, which §16.8 itself says this library does.

X7 and X8 are written to that reading. **Ruling: accepted.** The lead amends §16.8 by one sentence on the phase 14 branch; no task here waits on it.

### Follow-ups (not scheduled)

- **F1.** A method evaluated over another record (X3).
- **F2.** A lineage verdict inside a method's acceptance logic (X7).
- **F3.** A series-valued scope, `from_record<Role>(series<…>)` itself rather than a reduction of it. Task 10 refuses it, in phase 12's S13 spirit.
- **F4.** Rolling windows, hysteresis and plant lifecycle state (§16.8). These are explicitly not formula-cpp.
- **F5.** Display labels for record keys: a spelling the consumer supplies at render time (X4's ruling).

---

## Global Constraints

- **C++23**, header-only, and no dependency beyond the standard library in shipped headers (§3).
- Must compile on **MSVC cl, clang-cl, clang++ and GCC**. Verify every task on all eight presets: `cl-debug`, `cl-release`, `clangcl-debug`, `clangcl-release`, `clang-debug`, `clang-release`, `gcc-release`, `clang-ubsan`. Iterate on `cl-debug` + `gcc-release`, and hand in after the controller's `verify_all.ps1 -Root <tree>`.
- **Doxygen 1.9.8 (in WSL), zero warnings, and `mkdocs build --strict` (Windows Python) on every task**, not only before the merge.
- **No norm content** (§3): no identifiers, clause numbers, equations, thresholds or constants from any real standard. Cite invented `Example Standard` references only. Fixture keys are the deliberately arbitrary sample ids 17, 23 and 41, test ids 3 and 5, and lineage keys 4411, 4412, 12 and 13. Do not tidy them.
- **No abort, terminate or assert pop-ups.** Refusals are compile-time `static_assert`s in this library's words, or run-time results. Nothing new may `throw` from a `noexcept` path. Examples link `support/fail_without_dialogs.cpp` (already done by `formula_add_example`).
- **Never hardcode `/std:c++23`.** Never redirect a build to `/dev/null`. `FORMULA_WERROR=ON` everywhere.
- Every negative test asserts **both** that the build fails **and** that the output holds this library's own `static_assert` text. Every negative test gets a **deletion check**: delete the guard, confirm the case compiles, and restore it with a plain write or `touch`.
- **No `{}` default member initializer on any member that holds an expression or a node** (defect class 4). That covers `RecordScopeNode::operand`, and a record's environment member, since an environment holds `Measured`s and a context holds records.
- **Names.**
  - The namespace-scope names this phase adds are `record`, `record_key`, `sample_id`, `test_id`, `record_context`, `from_record`, `lineage`, `unknown_lineage`, `same_lineage`, `against` and `ThisRecord`.
  - Under GCC `-Wshadow`, no parameter or local in namespace `formula` code may share one of them.
  - **`context` is deliberately not a library name.** `render.hpp:638` already has a parameter `Precedence context`.
  - Under cl C4459, add `record`, `sample`, `batch`, `role` and `lineage` to the globals in `test/consumer_globals_tests.cpp`, and keep the build clean.
  - `trace.hpp:1499` already has a parameter named `origin`, so do not add a namespace-scope `origin`.
  - Check both new `StepKind` enumerators on GCC under `-Wshadow`, as `PiConstant` had to be.
- Every new header goes into the install `FILE_SET` (`hygiene.installed-headers`), into `formula.hpp`, and into `test/consumer_globals_tests.cpp`'s include list (`hygiene.consumer-globals`). `trace.hpp`, `render.hpp` and `document.hpp` stay out of the umbrella.
- **Coherent SI and a distinguishing fixture decide every assertion** (defect class 5).
  - This specimen and the reference specimen hold **different** values for the same quantity in every cross-record test, so that a read from the wrong record gives a different number.
  - Every assertion names the wrong implementation its value tells apart.
- A quoted compiler diagnostic in `docs/` must come from a real compile (`hygiene.documented-diagnostics`). Every ```` ```text ```` block in the guide must be whole consecutive lines of the example's real output.
- **Catch2 splits test filters on commas.** Prove a filter's selection count on the unmutated build before trusting a mutation run.
- **Baseline:** record the test count at `48d03ac` in task 1's findings. A task reporting fewer tests has lost tests.

## Review Focus

These are five input classes the spec implies and no feature test naturally exercises, most likely first. Each has its pinning test added to the task that owns the code.

1. **The reference record was never supplied.** The role is bound to `unbound()`. Expected: the scope is absent, never zero; no lineage steps; the trace reads `no record bound`; and a lineage requirement on that scope is neither `Violated` nor `Satisfied`. *Pinned in task 3 (value) and task 5 (trace).*
2. **One quantity read both here and from another record in one formula**, for example `var<Strength> / from_record<Reference>(var<Strength>)`. Expected: two different values, two trace steps with different origins, and two symbol-table rows. *Pinned in tasks 3, 5 and 7.*
3. **A reference value typed in by hand**, as `entered(...)` in the *reference* record's environment. Expected: the trace says both `from record Reference` and `entered by hand` on that one step. A `Measured` input in the same trace says neither of the two. *Pinned in task 5.*
4. **A lineage key unknown on one side, and a mismatch in the middle attribute of three.** Expected: unknown gives `NotChecked` and an absent result. A mismatch in attribute 2 of 3 fails the scope with `DomainError`, and its `Violated` step names that attribute and both keys. This kills a first-only and a last-only comparison, as phase 11's middle-placement did. *Pinned in task 6.*
5. **A cross-record formula evaluated where it cannot be**: against a plain `Environment`, or against a context that lacks the role. Expected: exactly one library message each, with no cascade (defect class 2). *Pinned in task 3.*

---

## Interfaces that already exist: read, not guessed

| Fact | Where |
|---|---|
| `NodeBase`, `concept Node`, and child members held with no `{}` | `expression.hpp:35-40`, `:173-189` |
| `Environment`: `provides`, `is_entered`, `get`, `source_of`, `RequireProvided`, and the gated `if constexpr (provides<Q>)` that keeps one message from becoming eight | `environment.hpp:137-225` |
| `Entered<Q>`, `EntryTraits<Entry>::source` | `environment.hpp:28-80` |
| `ValueSource { Measured, Derived, ManuallyEntered }` | `outcome.hpp:32` |
| The `VarNode` evaluator (the only reader of `get<Q>()` outside `checked_evaluate`) | `evaluate.hpp:207-222` |
| `detail::dispatch` and its two-parameter fallback | `sink.hpp:140-149` |
| Optional sink hooks detected by one `requires` naming both members (`variant_entered`/`variant_produced`) | `method.hpp`, `evaluate_method` |
| `detail::isPlainClassTag`, `tag_name<Tag>()`, `TagName` | `method.hpp:122`, `tag.hpp` |
| Library-only provenance: private constructor + `detail::…Access` + a refusing public constructor | `method.hpp:768-907` (`RoundingRule`) |
| `StepKind`, `Step`, `Trace` (`marks`, `unclaimed`, `branchStack`), `detail::StepKindOf`, and `RecordingSink::produced` | `trace.hpp:40-161`, `:296-697`, `:697-750`, `:754-880`, `:1183-1330` |
| `ConstraintOutcome` (`satisfied`, `violated`, `not_checked`, `invalid`) | `constraint.hpp:67-162` |
| `render_trace`, `step_line`, the exhaustive kind switch | `trace_render.hpp:483-560` |
| `detail::PrecedenceOf`, `render_node(…, V const&)`, `detail::render_operand` | `render.hpp:87-130`, `:638-1200` |
| `SymbolEntry`, `detail::Walk`, `quantityIdentity<Q>` | `document.hpp:39-173` |
| `detail::ConstantRewrite<Sub, N>` primary template (refuses an unknown node) | `overlay.hpp:1164-1184` |
| `hygiene.vocabulary-reach`, which scans `render.hpp`, `document.hpp`, `trace.hpp` and `trace_render.hpp` | `cmake/CheckVocabularyReach.cmake` |

## File Structure

| File | Responsibility |
|---|---|
| `include/formula-cpp/record.hpp` *(new)* | `ThisRecord`, `SampleId`/`TestId`/`RecordKey`, `sample_id`/`test_id`/`record_key`, `Record<Role, Env, Lineage...>`, `record<Role>()`, `unbound()`, `RecordContext`, `record_context()`, `RecordOrigin` + `detail::RecordOriginAccess`, `RecordScopeNode`, `from_record<Role>()`, its evaluator and refusals, and `detail::ConstantRewrite`'s specialization |
| `include/formula-cpp/lineage.hpp` *(new)* | `LineageEntry<Attr>`, `lineage<Attr>()`, `unknown_lineage<Attr>()`, `against<Role>`, `LineageRequirement<Against, Attrs...>`, `same_lineage<…>()`, `LineageCheck` + `detail::LineageCheckAccess`, and the requirement's refusals. Included by `record.hpp` |
| `include/formula-cpp/evaluate.hpp` | one hunk: the `VarNode` overload calls the optional `input_source` hook (task 4) |
| `include/formula-cpp/trace.hpp` | `StepKind::RecordScope`, `StepKind::LineageChecked`; `Step::record`, `Step::inputSource`, `Step::lineage`; `Trace::recordStack`; the `StepKindOf` specialization; `RecordingSink::input_source`, `record_entered`, `lineage_checked`; stamping in `produced` |
| `include/formula-cpp/trace_render.hpp` | two `case`s; the origin and source suffixes |
| `include/formula-cpp/render.hpp` | `PrecedenceOf<RecordScopeNode>`, `render_node(RecordScopeNode, V)` |
| `include/formula-cpp/document.hpp` | `SymbolEntry::record`; per-(role, quantity) rows; a `collect` for the scope |
| `include/formula-cpp/formula.hpp`, `CMakeLists.txt`, `test/CMakeLists.txt`, `test/consumer_globals_tests.cpp` | umbrella, `FILE_SET`, registration, the C4459 guard |
| `test/record_tests.cpp`, `record_scope_tests.cpp`, `record_trace_tests.cpp`, `record_render_tests.cpp`, `lineage_tests.cpp`, `record_join_tests.cpp`, `record_cross_tu.hpp`, `record_cross_tu_b.cpp` *(new)* | per-surface tests, deliberately **not** in the shared `trace_tests.cpp` / `render_tests.cpp` / `document_tests.cpp` |
| `test/negative/record_*.cpp`, `lineage_*.cpp` *(new)* | one per author error |
| `examples/records.cpp`, `docs/records.md` *(new)* | the example, and the "Other samples and other tests" guide |

`record.hpp` and `lineage.hpp` are split because a reviewer could reject one and approve the other: a cross-record read is useful without any lineage gate, and §9.1's reference-value pattern has none.

## The shared fixture

Invented, generic physics, reused by every task:

| | This record (`ThisRecord`) | Reference record (`Reference`) |
|---|---|---|
| key | sample 17, test 5 | sample 23, test 3 |
| `Force` `F` (N) | 85 902 | 57 268 |
| `EdgeX` `x_m`, `EdgeY` `y_m` (mm) | 139, 103 | 139, 103 |
| `F / (x_m · y_m)` (Pa, coherent SI) | 6 000 000 | 4 000 000 |
| `Strength` `f_c` (MPa, stored) | 6 | 4 |
| `MaterialBatch` | 4411 | 4411 (the violation fixture uses 4412) |
| `TestMethod` | 12 | 12 (the violation fixture uses 13) |
| `CuringRegime` | 7 | 7 |

The ratio `f_c / (f_c of Reference)` is **3/2**:
- reading both from this record gives 1;
- reading both from the reference gives 1;
- swapping the two gives 2/3.

All three wrong answers differ from 3/2, and from each other.

The cross-test fixture uses a second record that shares the same sample, `PriorTest`: sample 17, test 3. Its environment `prior` holds one quantity, `Ratio` (`unit::One`), measured as 1/4. The trace must then print the test key as well as the sample key. A renderer that printed only the sample would show "sample 17" for both records.

---

## Task 1: Spike — measure what X2, X5, X10 and X11 depend on, and rehearse the merge

**Files:**
- Create (scratch, **not** committed, **not** in the tracked tree): `.superpowers/sdd/phase-14-prep/spike/` and `.superpowers/sdd/phase-14-prep/task-1-spike.md`

**Interfaces:**
- Produces: `task-1-spike.md`, with one section per probe below. Each section gives the exact command, the compiler and version, the observed output and a verdict. A final section lists "what I did not establish". The lead amends X2, X5, X10 and X11 from it before task 2 starts.

- [ ] **Step 1: Record the baseline.** Run the full suite on all eight presets at `48d03ac`, and record the test count.
- [ ] **Step 2: Probe X2, the context as an environment.** Write a stand-in `template <typename Env, typename... Rs> struct StandInContext: Env { std::tuple<Rs...> others; };` in a scratch TU that includes `formula.hpp` and `trace.hpp`. Pass it, unchanged, to each of these:
  - `checked_evaluate<Q>`, including a result the context holds as `entered(...)`;
  - `evaluate_method<Tag>`;
  - `check_method`;
  - `check_all`;
  - `checked_evaluate_predicate`;
  - a banded and an interpolating lookup;
  - `apply(overlay, m)` followed by `evaluate_method`;
  - `explain<Q>`.

  Compare every result with the same call on the plain environment. Then ask a context for a quantity it lacks. **Count the messages by hand** on cl, clang-cl, clang++ and g++, and quote the first one: it must be `RequireProvided`'s, once. Finally, `static_assert` one result through the context.
- [ ] **Step 3: Probe X5, `sizeof(Step<Rational>)`.** Measure before and after adding:
  - `std::optional<RecordOrigin> record`, where the stand-in `RecordOrigin` is a `std::string_view`, two `std::uint64_t` and a `bool`;
  - `std::optional<ValueSource> inputSource`;
  - `std::optional<LineageCheck> lineage`, where the stand-in is two `std::string_view` and two `std::optional<std::uint64_t>`.

  Measure on cl and g++, and report the numbers whatever they say. Add phase 12's measured +120 bytes to them to give the combined figure.
- [ ] **Step 4: Probe X10, the out-of-header specialization.** In a stand-in `record.hpp` that includes `overlay.hpp`, declare `template <typename Sub, typename Role, Node Operand> struct detail::ConstantRewrite<Sub, StandInScope<Role, Operand>>`. Build `apply(overlay(with_constant<Q>(…)), method(variants(variant<Cube>(from_record-standin(var<Q> * var<R>))), …))` and evaluate it, on all four compilers. Also confirm that removing the specialization gives `RequireOverlaySeesNode`'s message, once.
- [ ] **Step 5: Probe X11, the page spelling (the spike decides it).**
  - Typeset `f_c of Reference`, `(F / A) of Reference`, `f_c / (f_c of Reference)`, and the same with a role whose `TagName` spelling is `reference 1:2 %`, under MathJax 3.2.2 with the site configuration and under tectonic with `\usepackage[OT1]{fontenc}`. Try `\text{}`, `\mathrm{}` and `\operatorname{}` for the role.
  - Check the Plain and Markdown candidates against the Markdown guard (`render_tests.cpp`, the `](` and bare `[` test).
  - Recommend **one** spelling per dialect, with the evidence.
- [ ] **Step 6: Probe the names under the two warnings that bite.**
  - Compile `render.hpp` with a namespace-scope `formula::record_context` function and a `formula::from_record` template added. Run g++ with `-Wshadow` and cl at `/W4 /WX`.
  - Compile `consumer_globals_tests.cpp` with globals `record`, `sample`, `batch`, `role` and `lineage` added, on cl.
  - Report every warning.
- [ ] **Step 7: Rehearse the merge.** In a scratch clone, stub the planned hunks of `trace.hpp`, `trace_render.hpp`, `render.hpp`, `document.hpp`, `evaluate.hpp`, `formula.hpp`, `CMakeLists.txt` and `test/CMakeLists.txt`, as this plan's File Structure places them. Commit the stubs on `48d03ac`. Run `git merge-tree --write-tree <stub> phase-12-series`, and list every conflicting hunk. **Record the series branch's HEAD commit you measured against**, because it moves daily.
- [ ] **Step 8: Write "what I did not establish".** Say plainly, for example, "not measured on g++-14 or Apple clang (CI only)", and "the merge rehearsal is against phase 12 at `<sha>`, not its final state".
- [ ] **Step 9: Hand the findings to the lead.** Do not commit. The lead confirms or amends X2, X5, X10 and X11 before dispatching task 2.

---

## Task 2: Records, record keys and the context

**Defect classes this task is built against:** 3 (the key and the role are library-set), 4 (no `{}` on the environment member), and 2 (one message per context mistake).

**Files:**
- Create: `include/formula-cpp/record.hpp`, `test/record_tests.cpp`
- Create (negatives): `test/negative/record_context_duplicate_role.cpp`, `record_context_first_not_this_record.cpp`, `record_role_not_plain.cpp`, `record_key_swapped.cpp`
- Modify: `include/formula-cpp/formula.hpp`, `CMakeLists.txt` (`FILE_SET`), `test/CMakeLists.txt`, `test/consumer_globals_tests.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct ThisRecord;                               // declared, never defined: a tag
  class SampleId { public: constexpr explicit SampleId(std::uint64_t) noexcept; constexpr std::uint64_t value() const noexcept; };
  class TestId   { /* same shape */ };
  class RecordKey { public: constexpr RecordKey(SampleId, TestId) noexcept;
                    constexpr SampleId sample() const noexcept; constexpr TestId test() const noexcept;
                    constexpr bool operator==(RecordKey const&) const noexcept = default; };
  constexpr SampleId sample_id(std::uint64_t) noexcept;
  constexpr TestId   test_id(std::uint64_t) noexcept;
  constexpr RecordKey record_key(SampleId, TestId) noexcept;

  template <typename Role, typename Env, typename... Lineage> class Record;   // private members
  template <typename Role, typename Env, typename... Lineage>
  constexpr Record<Role, Env, Lineage...> record(RecordKey, Env, Lineage...) noexcept;
  //   Record::unbound(), Record::is_bound(), Record::key() (pre: bound), Record::environment()

  template <typename ThisRec, typename... Others> class RecordContext;         // : public ThisRec's Env
  template <typename ThisRec, typename... Others>
  constexpr RecordContext<ThisRec, Others...> record_context(ThisRec, Others...) noexcept;
  //   RecordContext::this_record(), RecordContext::template record<Role>()
  ```
  Task 6 fills in `Lineage...`. Until then, a record declares none.

- [ ] **Step 1: Write the failing test.** In `test/record_tests.cpp`:

```cpp
#include <formula-cpp/formula.hpp>
#include <catch2/catch_test_macros.hpp>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Cube { };
struct Reference { };

struct Force: formula::Quantity<Force, "F", "applied force", unit::Newton> { };
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", unit::Millimetre> { };
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", unit::Millimetre> { };
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", unit::Megapascal> { };

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 85'902 } },
                                           formula::Measured<EdgeX> { formula::Rational { 139 } },
                                           formula::Measured<EdgeY> { formula::Rational { 103 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 57'268 } },
                                            formula::Measured<EdgeX> { formula::Rational { 139 } },
                                            formula::Measured<EdgeY> { formula::Rational { 103 } });

constexpr auto ctx = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
} // namespace

TEST_CASE("a context evaluates exactly as its own record's environment", "[record]")
{
    constexpr auto strength = var<Force> / (var<EdgeX> * var<EdgeY>);
    constexpr auto viaContext = formula::checked_evaluate<Strength>(strength, ctx);
    constexpr auto viaEnvironment = formula::checked_evaluate<Strength>(strength, here);

    // 6 MPa from this record. The reference's 4 MPa would mean the context
    // read the wrong record; equality with `here` alone could not tell a
    // context that forwards from one that ignores its records entirely,
    // so the reference is deliberately different.
    STATIC_REQUIRE(viaContext->measurement() == formula::Measured<Strength> { formula::Rational { 6 } });
    STATIC_REQUIRE(viaContext == viaEnvironment);
}

TEST_CASE("a context binds each role to the record it was given", "[record]")
{
    STATIC_REQUIRE(ctx.template record<Reference>().key() == formula::record_key(formula::sample_id(23), formula::test_id(3)));
    STATIC_REQUIRE(ctx.this_record().key().test().value() == 5);
}
```

- [ ] **Step 2: Run it and confirm it fails to build.**

Run: `cmake --build --preset cl-debug`. Never redirect to `/dev/null`.
Expected: FAIL, because `record_context` and `record` are not declared.

- [ ] **Step 3: Write `record.hpp`'s keys and `Record`.** Follow `RoundingRule`'s pattern for the members: private, set by the factory only. Hold the environment as `Env _environment;` with **no** `{}` (defect class 4). The role is refused unless `detail::isPlainClassTag<Role>`, with a message of its own:

```cpp
template <typename Role>
struct RequirePlainRole
{
    static_assert(detail::isPlainClassTag<Role>,
                  "formula: this record role is not a plain class type; a role names which record a formula "
                  "reads from, so it must be a class type without const or volatile -- the role appears in "
                  "this diagnostic as the template argument of RequirePlainRole");
    static constexpr bool value = true;
};
```

`Record::key()` on an unbound record is a precondition violation. So it returns the key only when bound. There is no key to return otherwise, and **no zero key** is ever invented for one: `key()` is `constexpr` and `noexcept` and documented `@pre is_bound()`. Every library caller branches on `is_bound()` first. Task 3 pins the unbound path.

- [ ] **Step 4: Write `RecordContext` (X2).**

```cpp
/// The records a formula may read from: its own, which it *is*, and the
/// others, by role. Inheriting the own record's environment is the design
/// (X2): every evaluator takes an environment, and a context passes for one
/// without any of them changing.
template <typename ThisRec, typename... Others>
class RecordContext: public RecordEnvironmentOf<ThisRec>
{
    static_assert(detail::RequireThisRecordFirst<ThisRec>::value);
    static_assert(detail::RequireDistinctRoles<ThisRec, Others...>::value);
  public:
    constexpr RecordContext(ThisRec own, Others... others) noexcept;
    [[nodiscard]] constexpr ThisRec const& this_record() const noexcept;
    template <typename Role>
    [[nodiscard]] constexpr auto const& record() const noexcept;   // RequireBoundRole<Role, RecordContext>
  private:
    ThisRec _own;
    std::tuple<Others...> _others;
};
```

`RequireDistinctRoles` is written in `RequireDistinctQuantities`' style, naming the roles. `ThisRecord` bound a second time is simply a duplicate role, and draws that one message.

- [ ] **Step 5: Run the tests and confirm they pass.** First confirm that the filter `[record]` selects exactly **2** tests.
- [ ] **Step 6: Prove every existing entry point takes a context unchanged.** Add one test per entry point probed in task 1 step 2, each comparing the result against the plain environment's. For `evaluate_method`, use a method with **two** variants whose results differ, so that a context selecting the wrong variant fails. Add `checked_evaluate` of a result that this record holds as `entered(...)`: it must return `ManuallyEntered`, because inheritance also carries `is_entered`.
- [ ] **Step 7: Write the four negative tests, then the deletion checks.**
  - `record_context_duplicate_role.cpp`: `Reference` is bound twice, in positions 2 and 4 of 4. The duplicate is non-adjacent and not in the first pair, following phase 11's task 3 placement.
  - `record_context_first_not_this_record.cpp`: `record_context(record<Reference>(…), record<ThisRecord>(…))`.
  - `record_role_not_plain.cpp`: `record<Reference const>(…)`.
  - `record_key_swapped.cpp`: `record_key(test_id(3), sample_id(23))`. Its EXPECT is the compiler's own "no matching function" text, because the strong types are what refuse it. Say so in the case's comment, and **REJECT** any library `static_assert` text, since none should fire.

  For each of the first three, delete the guard and confirm the case **compiles**. Restore it with a plain write.
- [ ] **Step 8: Wire in the header.** Add it to `formula.hpp`, the `FILE_SET`, and `consumer_globals_tests.cpp`'s include list, and add the five new globals there (Global Constraints). Run `hygiene.installed-headers` and `hygiene.consumer-globals`.
- [ ] **Step 9: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 10: Commit.**

```bash
git add include/formula-cpp/record.hpp include/formula-cpp/formula.hpp CMakeLists.txt test/CMakeLists.txt test/record_tests.cpp test/consumer_globals_tests.cpp test/negative/record_*.cpp
git commit -m "feat(record): add records, record keys and a context that is its own record's environment"
```

---

## Task 3: `from_record<Role>(expression)`: reading from another record

**Defect classes:** 2 (one message per misuse, and no cascade), 5 (the fixture tells every wrong record apart), and 7 (every refusal is deletion-checked).

**Files:**
- Modify: `include/formula-cpp/record.hpp`
- Create: `test/record_scope_tests.cpp`
- Create (negatives): `record_scope_plain_environment.cpp`, `record_scope_role_not_bound.cpp`, `record_scope_this_record.cpp`, `record_scope_nested.cpp`
- Modify: `test/CMakeLists.txt`

**Interfaces:**
- Consumes: `RecordContext::record<Role>()`, `Record::is_bound()`, `Record::environment()`, `detail::dispatch`.
- Produces:
  ```cpp
  template <typename Role, typename Requirement, Node Operand>
  struct RecordScopeNode: NodeBase
  {
      using role = Role;
      using requirement = Requirement;                 // detail::NoLineageRequirement until task 6
      static constexpr Dimension dimension = Operand::dimension;
      Operand operand;                                 // no {} -- defect class 4
  };
  template <typename Role, Node Operand>
  constexpr RecordScopeNode<Role, detail::NoLineageRequirement, Operand> from_record(Operand) noexcept;

  template <typename Env> struct detail::RecordContextOf;   // ::type for a RecordContext (and, later, a wrapper that specializes it -- X15)
  template <typename Rep = Rational, typename Role, typename Req, Node Operand, typename Env, typename Sink = NullSink>
  constexpr Evaluated<Rep> checked_evaluate_si(RecordScopeNode<Role, Req, Operand> const&, Env const&, Sink = {}) noexcept;
  //   reaches the context through detail::RecordContextOf<Env>; refuses (RequireRecordContext) when it has none
  ```
  The sink hooks come in task 5. This task calls only `entered` and `produced`.

- [ ] **Step 1: Write the failing test: Review Focus 2, one quantity read from two records.**

```cpp
TEST_CASE("one quantity read here and from the reference gives two values", "[record-scope]")
{
    // 6 MPa here over 4 MPa there is 3/2. Reading both from one record gives 1;
    // swapping them gives 2/3. Each wrong implementation has its own number.
    constexpr auto ratio = var<Force> / formula::from_record<Reference>(var<Force>);
    constexpr auto evaluated = formula::checked_evaluate_si<formula::Rational>(ratio, ctx);
    STATIC_REQUIRE(evaluated.has_value());
    STATIC_REQUIRE(**evaluated == formula::Rational { 3, 2 });
}

TEST_CASE("a scope computes over the other record's measurements", "[record-scope]")
{
    constexpr auto referenceStrength = formula::from_record<Reference>(var<Force> / (var<EdgeX> * var<EdgeY>));
    constexpr auto evaluated = formula::checked_evaluate_si<formula::Rational>(referenceStrength, ctx);
    STATIC_REQUIRE(**evaluated == formula::Rational { 4'000'000 });   // Pa: coherent SI, the reference's
}
```

- [ ] **Step 2: Run it and confirm it fails to build.**
- [ ] **Step 3: Implement `RecordScopeNode`, `from_record` and the evaluator.**
  - The evaluator calls `sink.entered(node)`.
  - It then takes `auto const& other = context.template record<Role>();`. **Rename that parameter** so it does not shadow anything (Global Constraints), for example `recordContext`.
  - If `!other.is_bound()`, it produces and returns `detail::nothing<Rep>()`.
  - Otherwise it evaluates `detail::dispatch<Rep>(node.operand, other.environment(), sink)`, produces, and returns.

  **The value and whatever the trace will later say about its origin must both come from the one reference `other`.** Task 5 depends on that, and mutates it.
- [ ] **Step 4: Implement the four refusals, each gated so that it is the only message.**
  - **A scope against something that is not a `RecordContext`:** one overload, branching on `detail::RecordContextOf<Env>` (X15). The branch with no context is a `static_assert` of `detail::RequireRecordContext<Env>`, followed by `return detail::nothing<Rep>();` **without** dispatching the operand. Otherwise the operand's own `RequireProvided` would fire too.

    ```cpp
    static_assert(alwaysFalse<Env>,
                  "formula: this formula reads from another record, but was evaluated against an environment "
                  "that holds only one; evaluate it against a record_context(...) that binds the role -- the "
                  "environment appears in this diagnostic as the template argument of RequireRecordContext");
    ```
  - **A role the context does not bind:** `RequireBoundRole<Role, Context>`, raised from `record<Role>()`. The evaluator gates its dispatch on `if constexpr (Context::template binds<Role>)`, so the operand is not instantiated against a record that does not exist.
  - **`from_record<ThisRecord>`:** refused in `RecordScopeNode`'s class body, so that an aggregate spelling is refused too.
  - **A nested scope:** no new guard. The inner scope meets a plain `Environment`, and the first refusal fires. Pin it with `record_scope_nested.cpp`. Its EXPECT is that first message, and a REJECT of `RequireBoundRole` and of `RequireProvided`. **Count the messages by hand** on cl and g++, because REJECT cannot refuse a second copy of the EXPECTed text (defect class 2).
- [ ] **Step 5: Pin Review Focus 1 (value half).** Bind `Reference` to `Record<Reference, decltype(there)>::unbound()`. The scope must be **absent**: `evaluated->has_value() == false`. The ratio must be absent too. Mutation: return `present(0)` for an unbound record. This test must fail, and nothing else must.
- [ ] **Step 6: Pin Review Focus 5.** The first two negatives above, each with a hand count of messages on cl and g++, recorded in the report.
- [ ] **Step 7: Deletion-check every negative.** Delete each guard, and confirm the case compiles or, for the nested case, reaches the next line. Restore with a plain write.
- [ ] **Step 8: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 9: Commit.**

```bash
git commit -m "feat(record): read from another record through from_record<Role>(expression)"
```

---

## Task 4: The trace says whether an input was measured or typed in (X6)

**Defect classes:** 1 (the count of changed existing traces is measured, not guessed), 5 (both sources in one test), and 6 (it is judged on the recorded step, not on the rendered line alone).

**If the lead defers X6, skip this task.** Task 5 then drops its `entered by hand` assertions, and Review Focus 3 is recorded as unmet.

**Files:**
- Modify: `include/formula-cpp/evaluate.hpp` (the `VarNode` overload only, `:207-222`)
- Modify: `include/formula-cpp/trace.hpp` (`Step::inputSource`, `RecordingSink::input_source`)
- Modify: `include/formula-cpp/trace_render.hpp` (the `Variable` line's suffix)
- Create: `test/record_trace_tests.cpp` (its first two cases)

**Interfaces:**
- Produces: the optional sink hook `void input_source(VarNode<Q> const&, ValueSource)`, called just before `produced` and only when the sink defines it and the environment answers `Env::template is_entered<Q>`. Also `Step<Rep>::inputSource`, a `std::optional<ValueSource>`, set only for `StepKind::Variable`.

- [ ] **Step 1: Count the existing traces that will change.** Grep `test/`, `examples/`, `docs/` and `tools/gallery/` for traces rendered over an `entered(...)` input. Record the count and the files in the report **before** changing anything.
- [ ] **Step 2: Write the failing test: both sources in one trace.**

```cpp
TEST_CASE("the trace says which input was typed in and which was measured", "[record-trace]")
{
    constexpr auto environment = formula::environment(formula::Measured<Force> { formula::Rational { 85'902 } },
                                                      formula::entered(formula::Measured<EdgeX> { formula::Rational { 139 } }),
                                                      formula::Measured<EdgeY> { formula::Rational { 103 } });
    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(var<Force> / (var<EdgeX> * var<EdgeY>), environment, sink);

    // Programmatic first: the step, not the line, is the record.
    CHECK(trace.steps[0].inputSource == formula::ValueSource::Measured);
    CHECK(trace.steps[1].inputSource == formula::ValueSource::ManuallyEntered);

    std::string const text = formula::render_trace(trace, { .maxSteps = 10 });
    CHECK(text.find("x_m = 139 mm, entered by hand\n") != std::string::npos);
    CHECK(text.find("F = 85902 N\n") != std::string::npos);   // measured: no suffix
}
```

- [ ] **Step 3: Run it and confirm it fails.**
- [ ] **Step 4: Implement the hook in `evaluate.hpp`.** Detect it with `if constexpr (requires { sink.input_source(node, ValueSource::Measured); } && requires { Env::template is_entered<Q>; })`. That keeps a consumer's own environment type, one without `is_entered`, compiling. Measure that claim with a stand-in environment in the test, and do not assert it by inspection.
- [ ] **Step 5: Implement the field, the recording and the rendered suffix.** The hook stores a pending source in a one-slot `Trace` member that `produced` consumes for the `Variable` step. `VarNode` has no operands, so no stack is needed. **Say so in the comment, and assert in `produced` that the slot is empty for every other kind.**
- [ ] **Step 6: Mutations.**
  - Record `Measured` always: this test must fail, and nothing else must.
  - Drop the suffix in `trace_render.hpp`: the rendered half must fail, and the programmatic half must pass.
  - Report both.
- [ ] **Step 7: Update every existing expectation found in step 1,** and report the before and after count.
- [ ] **Step 8: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 9: Commit.**

```bash
git commit -m "feat(trace): say whether each input was measured or typed in"
```

---

## Task 5: The record origin in the trace (X5, X12)

**Defect classes:** 3 (`RecordOrigin` is library-built only), 6 (origin and value come from one record, which is mutated to prove it), 7, and 1 (every claim in the forgery comment is compiled).

**Files:**
- Modify: `include/formula-cpp/record.hpp` (`RecordOrigin`, `detail::RecordOriginAccess`, and the hooks' calls in the evaluator)
- Modify: `include/formula-cpp/trace.hpp` (`StepKind::RecordScope`, `Step::record`, `Trace::recordStack`, `StepKindOf<RecordScopeNode>`, `RecordingSink::record_entered`, stamping)
- Modify: `include/formula-cpp/trace_render.hpp`
- Modify: `test/record_trace_tests.cpp`
- Create (negative): `record_origin_by_hand.cpp`

**Interfaces:**
- Produces:
  ```cpp
  class RecordOrigin
  {
    public:
      RecordOrigin(std::string_view, RecordKey, bool);  // refused: RequireLibraryStatesOrigin -- see RoundingRule
      [[nodiscard]] constexpr std::string_view role() const noexcept;       // tag_name<Role>(), static storage
      [[nodiscard]] constexpr bool is_bound() const noexcept;
      [[nodiscard]] constexpr std::optional<RecordKey> key() const noexcept; // empty when unbound
    private:
      friend struct detail::RecordOriginAccess;
      // ...
  };
  ```
  Also the optional hook `sink.record_entered(RecordOrigin const&)`, called right after `entered(scope)` and before anything inside the scope. In task 6 it is paired with `lineage_checked` in one `requires`: a sink defines both or neither. Also `Step<Rep>::record`, a `std::optional<RecordOrigin>`, and `StepKind::RecordScope`.

- [ ] **Step 1: Write the failing test: Review Focus 2 and 3, and the cross-test fixture.**

```cpp
TEST_CASE("every value read from another record says which record", "[record-trace]")
{
    // The reference's force was copied in by hand from its certificate.
    constexpr auto thereEntered = formula::environment(formula::entered(formula::Measured<Force> { formula::Rational { 57'268 } }));
    auto const context = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), thereEntered),
        formula::record<PriorTest>(formula::record_key(formula::sample_id(17), formula::test_id(3)), prior));

    formula::Trace<> trace {};
    formula::RecordingSink<> sink { trace };
    (void) formula::checked_evaluate_si<formula::Rational>(
        var<Force> / formula::from_record<Reference>(var<Force>) + formula::from_record<PriorTest>(var<Ratio>),
        context, sink);

    std::string const text = formula::render_trace(trace, { .maxSteps = 20 });
    CHECK(text.find("1. F = 85902 N\n") != std::string::npos);   // this record: no origin at all
    CHECK(text.find("F = 57268 N, from record Reference (sample 23, test 3), entered by hand\n") != std::string::npos);
    // Same sample, another test: the test key is what tells the two apart.
    CHECK(text.find("from record PriorTest (sample 17, test 3)") != std::string::npos);
    CHECK(text.find("from record Reference (sample 23, test 3) = ") != std::string::npos);  // the scope's own line
}
```

The exact line format (`#n from record …`) is pinned against the real output of the first green build. It must keep the words `from record <role> (sample <n>, test <n>)`, and it must contain no bare `[` (the gallery puts trace output in Markdown).

- [ ] **Step 2: Run it and confirm it fails.**
- [ ] **Step 3: Implement `RecordOrigin`.** Follow `RoundingRule`'s pattern exactly (`method.hpp:768-907`): a refusing public constructor, so that a hand-built origin is refused in this library's words, and a private one reached only through `detail::RecordOriginAccess::of(record)`. The access takes **the `Record` itself**, not its parts, so that its only input is the object the values come from.
- [ ] **Step 4: Call the hook from the scope's evaluator,** from the same `other` reference that task 3's step 3 reads the environment from. Then implement the recording:
  - `record_entered` pushes the origin onto `trace.recordStack`;
  - `produced` stamps `record = recordStack.back()` on every step while the stack is non-empty;
  - `produced` for a `RecordScope` step stamps it too, and then pops.

  Register `StepKindOf<RecordScopeNode<…>>` in `trace.hpp`, which includes `record.hpp`.
- [ ] **Step 5: Render it.** Add a `case StepKind::RecordScope:`. Put the origin suffix on `Variable`, `OverriddenConstant` and `DerivedQuantity` lines when `record` is set. An unbound record reads `from record Reference (no record bound)`.
- [ ] **Step 6: Pin Review Focus 1 (trace half).** With an unbound `Reference`, the scope's line must say `no record bound`, the scope must have no operands, and no line may say `sample`.
- [ ] **Step 7: Pin "stamped on every step" programmatically.** For `from_record<Reference>(var<Force> / (var<EdgeX> * var<EdgeY>))`, assert that **every** step from the first `Variable` to the scope has `record->role() == "Reference"`, and that steps outside the scope have no `record`.
- [ ] **Step 8: The mutations that make X12 true rather than a wish.**
  - (a) Build the origin from `context.this_record()` instead of `other`. Step 1's test must fail.
  - (b) Stamp only `Variable` steps. Step 7's test must fail, and nothing else must.
  - (c) Omit the test key from the rendered suffix. The `PriorTest` assertion must fail.
  - Report which test each mutation killed.
- [ ] **Step 9: Write `record_origin_by_hand.cpp`,** which constructs `RecordOrigin { "Reference", record_key(…), true }`. Delete the guard, confirm that the case compiles, and restore it.
- [ ] **Step 10: Write the forgery paragraph in `record.hpp`'s file comment** from X12, and **compile every "cannot" claim in it**. Each one names the negative case that proves it, or is reworded as "is not prevented" (defect classes 1 and 3).
- [ ] **Step 11: A consumer's own sink,** defining only `entered` and `produced`, compiles and evaluates a scope. Measure it in the test.
- [ ] **Step 12: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 13: Commit.**

```bash
git commit -m "feat(trace): stamp every step read from another record with that record's origin"
```

---

## Task 6: Lineage requirements: a gate on the scope (X7, X8)

**Defect classes:** 5 and 6 (the mismatch is in the middle attribute, and the requirement is tested in both orders), 2 (an attribute undeclared on both records draws one message), 3, and 7.

**Files:**
- Create: `include/formula-cpp/lineage.hpp`, `test/lineage_tests.cpp`
- Modify: `include/formula-cpp/record.hpp` (includes `lineage.hpp`; `Record` gains `Lineage...` and `lineage_of<Attr>()`; `from_record` gains the requirement overload; the evaluator checks before dispatching)
- Modify: `include/formula-cpp/trace.hpp` (`StepKind::LineageChecked`, `Step::lineage`, `RecordingSink::lineage_checked`)
- Modify: `include/formula-cpp/trace_render.hpp`, `CMakeLists.txt` (`FILE_SET`), `formula.hpp`, `test/consumer_globals_tests.cpp`
- Create (negatives): `lineage_requirement_empty.cpp`, `lineage_attribute_twice.cpp`, `lineage_attribute_undeclared.cpp`, `lineage_record_declares_twice.cpp`, `lineage_against_own_role.cpp`

**Interfaces:**
- Produces:
  ```cpp
  template <typename Attr> struct LineageEntry;          // private optional<uint64_t>
  template <typename Attr> constexpr LineageEntry<Attr> lineage(std::uint64_t) noexcept;
  template <typename Attr> constexpr LineageEntry<Attr> unknown_lineage() noexcept;
  template <typename Role> struct Against { };
  template <typename Role> inline constexpr Against<Role> against {};
  template <typename Comparand, typename... Attrs> struct LineageRequirement { };
  template <typename... Attrs> constexpr LineageRequirement<ThisRecord, Attrs...> same_lineage() noexcept;
  template <typename... Attrs, typename Role> constexpr LineageRequirement<Role, Attrs...> same_lineage(Against<Role>) noexcept;
  template <typename Role, Node Operand, typename C, typename... Attrs>
  constexpr RecordScopeNode<Role, LineageRequirement<C, Attrs...>, Operand> from_record(Operand, LineageRequirement<C, Attrs...>) noexcept;
  class LineageCheck;   // attribute(), comparand(), subjectKey(), comparandKey(); built only by detail::LineageCheckAccess
  ```
  Also the hook `sink.lineage_checked(LineageCheck const&, ConstraintOutcome)`, one call per attribute and in declared order, detected **together with** `record_entered` in one `requires`. Also `Step<Rep>::lineage`, a `std::optional<LineageCheck>`, whose verdict is in `Step::outcome`.

- [ ] **Step 1: Write the failing test: all three outcomes, with the mismatch in the middle.**

```cpp
TEST_CASE("a lineage requirement gates the read, attribute by attribute", "[lineage]")
{
    constexpr auto gated = formula::from_record<Reference>(
        var<Force>, formula::same_lineage<MaterialBatch, TestMethod, CuringRegime>());

    // Same everywhere: the reference's 60 000 N comes through.
    constexpr auto agreeing = formula::checked_evaluate_si<formula::Rational>(gated, contextWith(4411, 12, 7));
    STATIC_REQUIRE(**agreeing == formula::Rational { 60'000 });

    // The MIDDLE attribute differs (method 12 here, 13 there). A check of the
    // first attribute only, or of the last only, would let this through.
    constexpr auto refused = formula::checked_evaluate_si<formula::Rational>(gated, contextWith(4411, 13, 7));
    STATIC_REQUIRE(!refused.has_value());
    STATIC_REQUIRE(refused.error() == formula::ArithmeticError::DomainError);

    // Unknown batch there: no answer, not a refusal and not a pass.
    constexpr auto unknown = formula::checked_evaluate_si<formula::Rational>(gated, contextWithUnknownBatch());
    STATIC_REQUIRE(unknown.has_value());
    STATIC_REQUIRE(!unknown->has_value());
}
```

Here `contextWith(batch, method, regime)` builds the shared fixture's context, with the reference's lineage keys set to the three arguments and this record's set to 4411, 12 and 7.

- [ ] **Step 2: Run it and confirm it fails to build.**
- [ ] **Step 3: Implement `lineage.hpp`, and `Record`'s lineage.**
- [ ] **Step 4: Implement the gate in the scope's evaluator.** After `record_entered`, and before dispatching, compare each attribute of the subject record (the scope's role) against the comparand (`ThisRecord`, or `against<Role>`'s), in declared order.
  - Call `lineage_checked` for **every** attribute. Do not stop at the first mismatch: the trace should show every attribute that disagrees, and a stop-early loop hides the second one.
  - Then decide:
    - any `Violated` gives `DomainError`;
    - otherwise, any `NotChecked` gives absent;
    - otherwise, dispatch.
  - **Test the precedence** with one `Violated` and one `NotChecked` attribute in both orders: the result is `DomainError` either way (defect class 6).
  - An unbound record checks nothing (X9).
- [ ] **Step 5a: Record and render the attribute steps.** The line form is `N. same TestMethod as this record: 12 and 13, violated`. With `against<PriorTest>`, it is `… as PriorTest: …`. An unknown key reads `unknown`. The attribute steps are the scope step's first operands, through the existing `unclaimed` mechanism. Assert the operand indices programmatically.
- [ ] **Step 5b: Add `checked_explain` (X7b's ruling).** Signature: `template <Described Result, typename Rep = Rational, Node Expression, typename Env, Vocabulary V = DefaultVocabulary> [[nodiscard]] std::expected<Explained<Result, Rep>, CheckedExplainFailure<Rep>> checked_explain(...)`.
  - It goes through `checked_evaluate`, not the throwing `evaluate`.
  - On an arithmetic error it returns the error **together with the trace recorded up to it**. A refusal without its derivation is the defect this exists for.
  - Settle `CheckedExplainFailure`'s shape in review: `{ ArithmeticError error; Trace<Rep> trace; }` is the recommendation.
  - `explain` stays as it is.
  - Test: a lineage violation traced through `checked_explain` shows the `Violated` attribute step. Mutation: dropping the trace from the failure must fail that test.
  - Also modify: `include/formula-cpp/trace.hpp` (next to `explain`).
- [ ] **Step 6: Pin Review Focus 4 in the trace.** A mismatch in attribute 2 of 3 renders three attribute lines: satisfied, violated, satisfied. An unknown batch renders `unknown` and `not checked`.
- [ ] **Step 7: The five negatives, each followed by its deletion check.**
  - `lineage_attribute_undeclared.cpp` names an attribute that **neither** record declares. EXPECT one message. **Count them by hand**: it must not say the same thing once per record (defect class 2). Gate the second record's check on the first's with `std::conditional_t`.
- [ ] **Step 8: Mutations.**
  - Compare only the first attribute.
  - Compare only the last attribute.
  - Stop at the first mismatch.
  - Treat unknown as equal.
  - Treat unknown as violated.

  Each must fail a named test and nothing else. Report which.
- [ ] **Step 9: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 10: Commit.**

```bash
git commit -m "feat(lineage): gate a cross-record read on declared lineage, and trace every attribute checked"
```

---

## Task 7: The page: rendering a scope, and its own symbol-table rows (X11)

**Defect classes:** 5 (the same symbol read from two records), 1 (the typesetting claims come from task 1's measurements, named), and 3 (`SymbolEntry::record` is filled by the walk only).

**Files:**
- Modify: `include/formula-cpp/render.hpp` (`PrecedenceOf<RecordScopeNode>`, `render_node`)
- Create: `include/formula-cpp/detail/latex_math.hpp` (`detail::latex_math_words`, X11's ruling). Add it to the install `FILE_SET` if `hygiene.installed-headers` covers `detail/`.
- Modify: `include/formula-cpp/document.hpp` (`SymbolEntry::record`, the walk's current role, per-(role, quantity) identity, the scope's `collect`)
- Create: `test/record_render_tests.cpp`

**Interfaces:**
- Produces: `render<D>(scope, vocabulary)` in the spelling task 1 chose. `SymbolEntry::record` is a `std::string_view`: empty for this record, and `tag_name<Role>()` for a row read inside a scope.

- [ ] **Step 1: Write the failing test, in all three dialects.** Take `var<Strength> / from_record<Reference>(var<Strength>)`.
  - Plain `f_c / (f_c of Reference)`.
  - Markdown `` `f_c` / (`f_c` of Reference) ``.
  - LaTeX `\frac{f_c}{f_c\ \text{of }\mathrm{Reference}}`.
  - A role spelled through `TagName` as `reference 1:2 %_B`. LaTeX: `\mathrm{reference\ 1{:}2\ \%\_B}`. Markdown: escaped by the existing author-words escaping. Typeset under both engines, with task 1's harness.

  Add a compound operand, `from_record<Reference>(var<Force> / var<EdgeX>)`, whose brackets must appear.
- [ ] **Step 2: Write the failing symbol-table test: Review Focus 2.**

```cpp
TEST_CASE("a quantity read here and from the reference has two rows", "[record-render]")
{
    auto const page = formula::document<formula::Dialect::Plain>(var<Strength> / formula::from_record<Reference>(var<Strength>));
    REQUIRE(page.symbols.size() == 2);   // deduplicating by quantity alone would give 1
    CHECK(page.symbols[0].record.empty());
    CHECK(page.symbols[1].record == "Reference");
}
```

- [ ] **Step 3: Run both and confirm they fail.**
- [ ] **Step 4: Implement `render_node` and `PrecedenceOf`.** Follow the vocabulary threading of `render.hpp`'s other nodes. The Markdown guard test must still pass, and it must cover the new spelling. Add the scope to its inputs.
- [ ] **Step 5: Implement the walk.**
  - `Walk` gains a `std::string_view role` that is set while the scope's operand is walked.
  - Identity is `recordQuantityIdentity<Role, Q>`, keyed by role as well as quantity. It is writable, for the ICF reason given in `document.hpp:113-125`.
- [ ] **Step 6: Vocabulary.** Under a vocabulary that renames `Strength`, both the page and the trace (task 5's) must show the renamed symbol in both positions. Run `hygiene.vocabulary-reach`.
- [ ] **Step 7: Mutations.**
  - Deduplicate by quantity only: step 2's test must fail.
  - Drop the bracket around a scope that is an operand: the compound-operand assertion must fail.
- [ ] **Step 8: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 9: Commit.**

```bash
git commit -m "feat(render): render a read from another record, with its own rows in the symbol table"
```

---

## Task 8: The join: an overlaid method reading from another record, in a vocabulary, across translation units

**This task exists because two separately verified things do not verify their join.** Phase 11's task 9 found exactly that.

**Defect classes:** 6 (the overlay operations are tested in both orders around a scope), and 7.

**Files:**
- Modify: `include/formula-cpp/record.hpp` (`detail::ConstantRewrite<Sub, RecordScopeNode<…>>`, per X10's ruling)
- Create: `test/record_join_tests.cpp`, `test/record_cross_tu.hpp`, `test/record_cross_tu_b.cpp`
- Create (negative, **only if X10 is ruled "refuse"**): `overlay_constant_inside_record_scope.cpp`

- [ ] **Step 1: Write the failing test: everything at once.**
  - A method with two variants. One of them is `var<Strength> / from_record<Reference>(var<Force> / (var<EdgeX> * var<EdgeY>) * var<ShapeFactor>)`, with a lineage requirement.
  - An overlay applying `with_constant<ShapeFactor>(97/100)` and `with_rounding`.
  - Evaluate it with `evaluate_method<Cube>` against the shared context, through a `RecordingSink` in a renaming vocabulary.
  - Assert all of these:
    - the value, computed by hand in coherent SI after the overlay's rounding. Name the fixture's wrong answers: no overlay, overlay applied outside the scope only, and read from this record;
    - `VariantSelected` names `Cube`;
    - the `OverriddenConstant` step inside the scope carries `record->role() == "Reference"`;
    - the lineage steps are present;
    - the renamed symbol appears in the trace and on the page.
- [ ] **Step 2: Run it.** Without X10's specialization, it fails with `RequireOverlaySeesNode`. That is the measured reason X10 exists.
- [ ] **Step 3: Implement X10 as ruled.** Rewrite through: specialize `ConstantRewrite`, so that `mentions` and `apply` recurse into `operand` and rebuild the scope with the same role and requirement. Refuse: the specialization's `apply` refuses, in its own words, when `mentions` is true.
- [ ] **Step 4: Both orders.** Apply `with_constant` then `add_derived`, and the reverse, on a quantity used inside and outside the scope. Both must produce the same method, following phase 11's "judge against what is produced" (defect class 6).
- [ ] **Step 5: `check_method` through a context.** A constraint whose predicate reads `from_record<Reference>(…)` returns its verdict, and its `Constraint` step's operands carry the origin.
- [ ] **Step 6: Across translation units.** Declare an `inline constexpr` overlaid method and a context factory in `record_cross_tu.hpp`. Use them from `record_tests.cpp` and `record_cross_tu_b.cpp`, and **link**. Mangling defects appear only at link time, and phase 10 shipped one that no compile caught.
- [ ] **Step 7: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 8: Commit.**

```bash
git commit -m "test(record): join a cross-record method with an overlay and a vocabulary, and check across TUs"
```

---

## Task 9: Guide, example and gallery

**Runs only after this branch has rebased onto `master` once phase 11 has merged** (see Dependencies). Phase 11's task 11 touches the same wiring.

**Files:**
- Create: `examples/records.cpp`, `docs/records.md` ("Other samples and other tests")
- Modify: `examples/CMakeLists.txt`, `tools/gallery/main.cpp`, `docs/gallery.md` (regenerated, never hand-edited), `mkdocs.yml`, `README.md`, `docs/index.md`, and the docs-output guard

**The guide is this project's cheapest reachability probe.** Phase 9's most expensive defect was found by a guide author whose natural spelling did not compile. **If a spelling you reach for does not compile, that is a finding about the library.** Report it; do not work around it.

**What a reader is owed**, each decided by a ruling above:
1. A role is code and a record is data (X1). Show why a formula names a role and never a sample.
2. That a context *is* its own record's environment, so everything they already call takes one (X2).
3. `from_record` reads a value **or computes over another specimen** (X3). Also why it cannot hold a method yet (F1).
4. What the trace says, and what it guarantees: show a real rendered trace, with a typed-in reference value. State X12's boundary in plain words, including what is *not* prevented.
5. The lineage gate's three outcomes, shown by running it (X7). Also the `explain()` caveat, **unless** the lead folds `checked_explain` into this phase.
6. That lineage attributes are the author's, and that the library does not decide which batch a specimen belongs to (X8, §16.8).
7. That an unbound record gives no answer, and never zero (X9).

**Gallery rules:** use `write_worked_formula(out, node)` for every worked section, and never hand-type a rendering.

- [ ] **Step 1: Write `examples/records.cpp`,** ending with `all checks passed: yes`. Register it with `formula_add_example` and a regex pinning every spelling the guide quotes.
- [ ] **Step 2: Run it, and capture the real output.**
- [ ] **Step 3: Write `docs/records.md`,** quoting only captured output. Extend the `docs.<guide>-output` guard to it.
- [ ] **Step 4: Add a cross-record formula and a gated one to the gallery.** Regenerate, and confirm `gallery.is-current`.
- [ ] **Step 5: Wire the guide into `mkdocs.yml`, `README.md` and `docs/index.md`.**
- [ ] **Step 6: Verify on all eight presets, plus Doxygen 1.9.8 and `mkdocs build --strict`, then commit.** Message: `docs(record): add the other-samples-and-tests guide, example and gallery entries`.

---

## Task 10: The series join (runs on whichever branch lands second)

**Depends on phase 12's tasks 2, 3 and 5 being on `master`.** See Dependencies for why skipping it would make the trace lie silently.

**Files:**
- Modify: `include/formula-cpp/trace.hpp` (stamp `recordStack.back()` in `series_produced` as `produced` does)
- Modify: `include/formula-cpp/record.hpp` (refuse `from_record` over a `SeriesNode`: F3)
- Modify: `test/record_join_tests.cpp`, `test/record_render_tests.cpp`
- Create (negative): `record_scope_of_series.cpp`

- [ ] **Step 1: Re-run task 1 step 2's inheritance probe against phase 12's accessors.** Take a series formula evaluated against a context: its result must equal the plain environment's.
- [ ] **Step 2: Write the failing test.** `sum(series<Retained, 5>) / from_record<Reference>(sum(series<Retained, 5>))`, with different series in the two records. Choose them so that the ratio differs from 1 and from its inverse. Assert:
  - the value;
  - that the reference's series step carries `record->role() == "Reference"`;
  - that the rendered line says `from record Reference (sample 23, test 3)`;
  - that this record's series step carries no origin.
- [ ] **Step 3: Run it and confirm the origin assertion fails.** That failure is the silent defect this task exists for.
- [ ] **Step 4: Stamp series steps.** Extend task 5's stamping to `series_produced`, and to any other recording path phase 12 added. **Grep `trace.hpp` for every `steps.push_back`**, and list them in the report with how each is handled.
- [ ] **Step 5: An entered series in the reference record.** Take phase 12's `EnteredSeries`. The trace must say both the origin and that the series was entered by hand, if phase 12 records series source. If it does not, report that and do not invent it.
- [ ] **Step 6: The page.** `document()` gives a separate row for (Reference, `Retained`) that also carries phase 12's `shape` and `length`. The series marker combines with the scope spelling as task 1 predicted (`m_r(i) of Reference`), and that is confirmed under MathJax 3.2.2.
- [ ] **Step 7: Refuse `from_record<Role>(series<…>)` itself (F3),** with one message saying to wrap a reduction. Deletion-check it.
- [ ] **Step 8: Verify on all eight presets, plus Doxygen and mkdocs.**
- [ ] **Step 9: Commit.**

```bash
git commit -m "feat(record): stamp series steps read from another record, and refuse a series-valued scope"
```

---

## Self-Review

**1. Spec coverage.**
- §9.1, fourth pattern. "An input comes from another test on another specimen": tasks 3 and 5 (`PriorTest`, same sample and another test). "A reference value measured on separate material": task 3's computing scope. "A sample/test context able to reference other samples and tests": task 2.
- §16.1's "the trace must say which": task 4 for every input, and task 5 for cross-record inputs.
- §16.6's last bullet: tasks 2 and 3.
- §16.8's lineage predicates: task 6, with X13 recording the spec's tension for a ruling.
- §17 row 14: tasks 2–8.
- The hard constraint that an origin is visible and unforgeable: tasks 5 (stamping, `RecordOrigin`, mutations (a)–(c)), 6 (`LineageCheck`) and 10 (series). X12 draws the boundary of what cannot be prevented.

**Not covered, and correctly not:**
- A method over another record (F1).
- A lineage verdict in acceptance logic (F2).
- A series-valued scope (F3).
- Plant lifecycle state (F4, §16.8).

**2. Placeholder scan.** No "TBD" and no "similar to Task N".
- Tasks 2, 3, 4, 5, 6 and 7 give their failing tests in full.
- Task 8 and task 10 give their assertions as a list. Their fixtures depend on rulings (X10) and on phase 12's final API respectively. Writing them in full now would be guessing at names.
- Three exact spellings are pinned against the first green build rather than written here, because task 1 decides them: the rendered trace line, the LaTeX spelling, and the scope's value line. Each step says which words must survive.

**3. Type consistency.** These names mean one thing throughout:
- `RecordKey`, `SampleId`, `TestId`, `Record<Role, Env, Lineage...>` and `RecordContext<ThisRec, Others...>`;
- `RecordScopeNode<Role, Requirement, Operand>` with `::operand`;
- `RecordOrigin` with `role()`, `is_bound()`, `key()`;
- `LineageRequirement<Comparand, Attrs...>`, `LineageCheck`, `Step::record`, `Step::lineage`, `Step::inputSource` and `Trace::recordStack`;
- the hooks `input_source`, `record_entered` and `lineage_checked`.

The two new `StepKind`s are `RecordScope` and `LineageChecked`. They differ from the types `RecordScopeNode` and `LineageCheck` in spelling, and each is still checked on GCC `-Wshadow`.

**4. Review Focus.** There are five lines, and each has its test in the owning task: 3 and 5; 3, 5 and 7; 5; 6; 3.
