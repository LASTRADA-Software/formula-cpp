# SPDX-License-Identifier: Apache-2.0
include(PedanticCompiler)

# Every example is also a ctest case, so every example must fail the way a test
# fails: a message on stderr and a non-zero exit code, never a modal dialog.
# `support/fail_without_dialogs.cpp` is what arranges that, and the reason it
# exists is written at the top of that file.
#
# It goes through this function rather than being spelled out per target so that
# a new example cannot be added without it. That is not hypothetical tidiness: a
# failing example put a "Debug Error!" window on a maintainer's screen during
# this branch's own development, and on CI the same window is an hour of silence
# followed by a timeout.
function(formula_add_example name source expectedOutput)
    set(target "formula-cpp-example-${name}")
    add_executable(${target} "${source}" "${PROJECT_SOURCE_DIR}/support/fail_without_dialogs.cpp")
    target_link_libraries(${target} PRIVATE formula-cpp::formula-cpp)
    formula_apply_warnings(${target})

    add_test(NAME "example.${name}" COMMAND ${target})
    set_tests_properties("example.${name}" PROPERTIES PASS_REGULAR_EXPRESSION "${expectedOutput}")
endfunction()

formula_add_example(simple simple.cpp "w/c = 0.600000 \\(computed\\)")
formula_add_example(exact_numbers exact_numbers.cpp "ten tenths == one: yes")
formula_add_example(dimensions_and_units dimensions_and_units.cpp "all checks passed: yes")
formula_add_example(quantities quantities.cpp "all checks passed: yes")
formula_add_example(expressions expressions.cpp "all checks passed: yes")
formula_add_example(citations citations.cpp "all checks passed: yes")
formula_add_example(composition composition.cpp "all checks passed: yes")
formula_add_example(tracing tracing.cpp "all checks passed: yes")
formula_add_example(rounding_and_conditionals rounding_and_conditionals.cpp "all checks passed: yes")

# Every other example pins only its final summary line -- "all checks passed:
# yes" says the program's own booleans agreed with themselves, but says
# nothing about what those booleans actually checked. That is not enough
# here: docs/constraints.md quotes this program's output verbatim, including
# the very outcome words and trace shape this phase exists to get right, and
# nothing but an implementer's own diligence stood between a withdrawn
# spelling and a stale guide during phase 8. So this regex additionally pins,
# in the order the program prints them: all four ConstraintOutcomeKind words
# (satisfied, violated, not checked, invalid), the verdict label surviving
# into output (`reject the specimen`), the arithmetic-error text surviving
# into output (`division by zero`), and two literal `require ... [...]` trace
# lines -- the satisfied case and, especially, the not-checked case that is
# this whole phase's central argument -- plus the degenerate one-operand
# shape a left-side arithmetic error produces. A spelling change to any of
# these now fails the build instead of silently leaving the guide wrong.
#
# A `[==[ ... ]==]` bracket argument, not an ordinary quoted string: the
# regex needs literal `[` and `]` around each outcome word (`\[satisfied\]`),
# and a bracket argument passes backslash-escapes through to the regex engine
# unchanged, whereas an ordinary quoted string needs the backslash doubled by
# CMake's own parsing *and* doubled again by set_tests_properties storing it
# -- confirmed empirically, and confusing enough that getting it wrong here
# silently drops the escape rather than failing to configure.
formula_add_example(constraints constraints.cpp [==[satisfied.*violated.*reject the specimen.*not checked.*invalid.*division by zero.*require #1 >= #2 \[satisfied\].*require #1 >= #2 \[not checked\].*require #3 \[division by zero\].*all checks passed: yes]==])

# Same reasoning as the constraints example just above, applied to the
# spellings docs/lookup-tables.md quotes verbatim. Pinned here, in the order
# the program prints them:
#
#  - `0 to under 127 mm gives 913/10 %` -- the ONE spelling of a half-open
#    interval in this library. It exists because `[0, 127)` is Markdown link
#    syntax, which silently dropped an operand from a published page in phase
#    8; a guard test already forbids `](` and a bare `[` in any Markdown
#    rendering, and this regex additionally pins that the replacement wording
#    itself does not drift.
#  - `at 127 mm gives 913/10 %` -- the interpolating table's point spelling, which
#    must NOT read as an interval: a breakpoint is a row, not a boundary.
#  - `key Cylinder gives 863/10 %` -- an exact key renders as its enumerator's
#    name, recovered at compile time, not as a number of any kind.
#  - `lookup(key 13)` -- the trace of a key that names no row, which falls
#    back to the underlying value. The example numbers its enumerators 3, 7,
#    11 and 13 precisely so a value and an index cannot be confused, which is
#    why this pins 13 and not 3.
#  - `key sealed in foil gives 937/10 %` -- a key its author spelled through
#    `EnumeratorName`, rendered that way.
#  - `[between 127 and 173 mm]`, `[837/10 to under 973/10 %]` and
#    `[on the row at 211 mm]` -- the trace clauses that say where an answer
#    came from, in the order the program prints them. Three rather than two:
#    an interpolating lookup has a clause for the two rows it drew on AND a
#    different one for a value sitting exactly on a row, and those say
#    genuinely different things, so pinning only the first would leave the
#    row-hit spelling free to drift. (Order is load bearing here -- `.*`
#    between the clauses means this asserts a sequence, and the first draft of
#    this regex listed them in the wrong order and failed, which is how the
#    sequencing was confirmed to be doing work rather than decorating.)
#  - `[in no band ... the bands cover 0 to under 211 mm]` -- the clause that
#    keeps a miss from being an opaque DomainError. Written as two halves
#    joined by `.*` rather than quoting the literal `;` between them:
#    PASS_REGULAR_EXPRESSION is a LIST property, so a `;` would split this
#    into two alternative regexes, either of which passing would satisfy the
#    test -- silently weakening it to a fraction of what it says.
#
# A `[==[ ... ]==]` bracket argument for the reason the constraints entry
# gives: the regex needs literal backslash escapes around `[` and `]`.
formula_add_example(lookup_tables lookup_tables.cpp [==[0 to under 127 mm gives 913/10 %.*at 127 mm gives 913/10 %.*key Cylinder gives 863/10 %.*lookup\(key 13\).*key sealed in foil gives 937/10 %.*\[between 127 and 173 mm\].*\[837/10 to under 973/10 %\].*\[on the row at 211 mm\].*\[in no band.*the bands cover 0 to under 211 mm\].*all checks passed: yes]==])

# docs/methods-and-overlays.md quotes this program's output verbatim. Pinned
# here, in the order the program prints them: the selection step naming a tag
# through its TagName spelling at its published position, a method's own
# rounding rule, a jurisdiction's rounding rule with its citation, the fixed
# and derived provenance clauses, the replacement marker, and the southern
# vocabulary's crossed-over edge in a trace line.
formula_add_example(methods_and_overlays methods_and_overlays.cpp [==[\[variant Cube \(2nd of 3\), selected by tag\].*\(method default\).*\[variant cylinder 135 x 271 mm \(3rd of 3\), selected by tag\].*fixed by jurisdiction overlay: Shape factor.*jurisdiction overlay: Example Standard 12:2021 NA, NA\.4.*derived by jurisdiction overlay.*replaced by jurisdiction overlay: Example Standard 7:2019 A, A\.5.*1 of 3 pruned by jurisdiction overlay: Example Standard 7:2019 A, A\.1.*pinned by jurisdiction overlay: Example Standard 3:2023 E, E\.1.*4\. b = 163 mm.*all checks passed: yes]==])

# Every line of docs/methods-and-overlays.md's output blocks must be a line the
# example really prints -- see the script for why the regex above cannot be
# that check.
add_test(NAME docs.methods-and-overlays-output
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_EXE=$<TARGET_FILE:formula-cpp-example-methods_and_overlays>"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/methods-and-overlays.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideOutput.cmake")

# Every ```cpp block of docs/methods-and-overlays.md must appear in the
# example's source as consecutive lines, indentation aside -- see the script.
add_test(NAME docs.methods-and-overlays-snippets
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/methods_and_overlays.cpp"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/methods-and-overlays.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideSnippets.cmake")

# docs/series.md quotes this program's output verbatim. Pinned here, in the
# order the program prints them: the index marker on the series and not on the
# total, the segment an interpolation lay between, the symbol table's shape,
# a list cut short with its count, every row of the absence table (rounding in
# grams, a running total made absent from the unrecorded screen on, an absent
# curve read with no clause, three screens not checked), a failed element
# named from one and zero, a closed lower limit
# satisfied, the snap's nearer and tie clauses and its miss, the splice's
# failure naming the point and the rule, the binning's classes, and its miss
# naming the observation. No literal `;`: PASS_REGULAR_EXPRESSION is a list
# property, and a `;` would split the regex (see the lookup_tables entry).
formula_add_example(series series.cpp [==[100 % - cumulative\(m_r\(i\), from last\) / m_t.*\[between 163 and 197 m\].*m_r: series, 5 value\(s\).*m_t: single value.*cumulative\(#2, from last\) = \.\.\. 5 more.*round\(#1, to 0/0/0/0/0 dp of g\) = 130 g. 210 g. \(not measured\).*\(not measured\). 368 g. 28 g.*interpolate\(#8, at #9\) = \(not measured\).*not checked. not checked. not checked. satisfied.*division by zero at element 5.*element 4, counted from zero, a result element: yes.*3 satisfied, 1574/25 % \(at least 1574/25 %\).*\[127 m to 163 m. nearer 127 m\].*tie, toward lower\].*tie, toward higher\].*\[outside the permitted set, 103 m to 241 m\].*at element 4 \[breaks non-decreasing at 103 m\].*bin\(s\(i\), 0 to under 127 m, 127 to under 197 m, 197 to under 331 m\).*at observation 4 \[331 m in no class. the classes cover 0 to under 331 m\].*all checks passed: yes]==])

# Every ```text block of docs/series.md must be lines the example really
# prints, and every ```cpp block lines of its source.
add_test(NAME docs.series-output
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_EXE=$<TARGET_FILE:formula-cpp-example-series>"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/series.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideOutput.cmake")
add_test(NAME docs.series-snippets
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/series.cpp"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/series.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideSnippets.cmake")
