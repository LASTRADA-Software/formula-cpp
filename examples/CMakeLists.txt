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

    # Its overflow-census twin (docs/numeric-headroom.md): the same program,
    # built with FORMULA_OVERFLOW_CENSUS, which prints how many of the 63 bits
    # its run-time evaluations used when it exits. Its own output must not
    # change, so the twin is held to the same expected output as well.
    set(censusTarget "formula-cpp-census-example-${name}")
    add_executable(${censusTarget}
        "${source}"
        "${PROJECT_SOURCE_DIR}/support/fail_without_dialogs.cpp"
        "${PROJECT_SOURCE_DIR}/support/census_tally.cpp"
        "${PROJECT_SOURCE_DIR}/support/census_report.cpp")
    target_link_libraries(${censusTarget} PRIVATE formula-cpp::formula-cpp)
    target_compile_definitions(${censusTarget} PRIVATE FORMULA_OVERFLOW_CENSUS)
    target_include_directories(${censusTarget} PRIVATE "${PROJECT_SOURCE_DIR}/support")
    formula_apply_warnings(${censusTarget})
    add_test(NAME "census.example.${name}" COMMAND ${censusTarget})
    set_property(GLOBAL APPEND PROPERTY FORMULA_CENSUS_EXAMPLES "${name}")
    set_tests_properties("census.example.${name}" PROPERTIES
        PASS_REGULAR_EXPRESSION "${expectedOutput}.*overflow census: ")
endfunction()

formula_add_example(simple simple.cpp "w/c = 0.600000 \\(computed\\)")
formula_add_example(exact_numbers exact_numbers.cpp "ten tenths == one: yes")
formula_add_example(dimensions_and_units dimensions_and_units.cpp "all checks passed: yes")

# Every ```text block of docs/dimensions.md must be a run of lines the example
# prints, and every ```cpp block a run of lines of its source, unless it is
# marked as not from the example.
add_test(NAME docs.dimensions-output
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_EXE=$<TARGET_FILE:formula-cpp-example-dimensions_and_units>"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/dimensions.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideOutput.cmake")
add_test(NAME docs.dimensions-snippets
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/dimensions_and_units.cpp"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/dimensions.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideSnippets.cmake")
formula_add_example(quantities quantities.cpp "all checks passed: yes")
formula_add_example(expressions expressions.cpp "all checks passed: yes")
formula_add_example(citations citations.cpp "all checks passed: yes")
formula_add_example(composition composition.cpp "all checks passed: yes")

# docs/calculations.md quotes this program's output. Pinned here, in the order
# the program prints them: the four totals -- the first run's, after a new
# price, after a new base fee, and on the what-if copy -- and how many values
# each step of the worksheet recalculated and reused: the first run, a new
# price, the same price again, a new base fee, the fridge's early cutoff, the
# what-if copy and the worksheet left as it was. Then the derivation of the
# daily load, which the fridge's change reused, reading the fridge's current
# 400 W; the grid cost's derivation stating the value typed in by hand, cut
# short with its footer; and, in the example's second calculation, the division
# by zero reaching the value that reads it, the failure not calculated again,
# the same failure again counted as no change, and an input nobody counted.
#
# Twice, one line must follow another: the daily load's first line its title,
# and the footer the net draw's line. A bracket argument cannot spell a line
# break, so it is spelled once here -- with the CR that Windows' output has.
set(lineBreak "\r?\n")
string(CONCAT billOutput
    [==[first run: .*total 118\.26 EUR, net draw 279 kWh, recomputed 15, reused 0.*price 0\.25 EUR/kWh: .*total 95\.02 EUR, .*recomputed 5, reused 0.*the same price again: .*recomputed 0, reused 0.*base fee 15 EUR: .*total 98\.00 EUR, .*recomputed 3, reused 0.*fridge 400 W for 12 h: .*recomputed 2, reused 8.*how the daily load was reached:]==]
    "${lineBreak}"
    [==[daily_load = fridge_kwh \+ oven_kwh \+ heater_kwh = 13\.3 kWh.*fridge_w = 400 W.*with 200 kWh of sun: .*total 85\.14 EUR, net draw 239 kWh, recomputed 9.*the worksheet itself: .*recomputed 0, reused 0.*net_draw = 250 kWh, entered by hand in place of monthly_load - self_used]==]
    "${lineBreak}"
    [==[\.\.\. 1 further step not shown.*shared by nobody: share: division by zero, in cents: division by zero.*asked again: recomputed 0.*100\.00 EUR, still shared by nobody: recomputed 1, reused 1.*occupants not counted: share \(not measured\), in cents \(not measured\).*all checks passed: yes]==])
formula_add_example(electricity_bill electricity_bill.cpp "${billOutput}")

# Every ```text block of docs/calculations.md must be a run of lines the
# example prints, and every ```cpp block a run of lines of its source.
add_test(NAME docs.calculations-output
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_EXE=$<TARGET_FILE:formula-cpp-example-electricity_bill>"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/calculations.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideOutput.cmake")
add_test(NAME docs.calculations-snippets
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/electricity_bill.cpp"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/calculations.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideSnippets.cmake")

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

# docs/statistics.md quotes this program's output verbatim. Pinned here, in
# the order the program prints them: the variance over n - 1, the spread
# rounded exactly, the observations' count as made, a missing determination
# absent, a settled rejection, an abort as the author's verdict, a tie, a
# sample too small to begin with, the squared comparison, a critical value
# read at the pass's size, the second precision pass, and the rounded level's
# verdict.
formula_add_example(statistics statistics.cpp [==[sample_variance\(m\(i\)\) = 427/125 g2.*= 37/20 g.*observations of 8 at most, 6 made: mean 413/10 g, count 6.*9 observations for 8 places: refused.*sample_mean\(#1\) = \(not measured\).*settled: 2 rejected, 4 remain.*would be rejection 2 of at most 1: discard the determinations and repeat the test.*rejected together in pass 1.*3 values, fewer than the at least 4 to keep.*limit\^2 \* s\^2.*\[critical value at n = 6\].*r at level #10 \(pass 2 of 2\).*level = the mean rounded to 1 g: violated.*all checks passed: yes]==])

# Every ```text block of docs/statistics.md must be a run of lines the example
# prints, and every ```cpp block a run of lines of its source.
add_test(NAME docs.statistics-output
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_EXE=$<TARGET_FILE:formula-cpp-example-statistics>"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/statistics.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideOutput.cmake")
add_test(NAME docs.statistics-snippets
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/statistics.cpp"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/statistics.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideSnippets.cmake")

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

# docs/records.md quotes this program's output verbatim. Pinned here, in the
# order the program prints them: the compound read's bracketed spelling, a
# typed-in reference value with its record, a typed-in reference series with
# its record (the `;` between its elements matched as `.`, since a `;` would
# split the pattern into a list), the lineage gate's three outcomes, a record
# not yet made, an entry left empty by hand, and a role spelt through TagName.
formula_add_example(records records.cpp [==[\(F / \(x_m \* y_m\)\) of Reference = 20000000 Pa.*f_c = 20 MPa, from record Reference \(sample 23, test 3\), entered by hand.*m_r = 139 g. 197 g. 103 g, from record Reference \(sample 23, test 3\), entered by hand.*same TestMethod as this record: 12 for this record, 12 for Reference, satisfied.*same TestMethod as this record: 12 for this record, 13 for Reference, violated.*same MaterialBatch as this record: 4411 for this record, unknown for Reference, not checked.*same TestMethod as this record: 12 for this record, 13 for Reference, violated.*from record Reference \(no record bound\).*f_c = \(entered by hand as empty\), from record Reference.*f_c of second reference.*all checks passed: yes]==])

# Every output block of docs/records.md must be a run of lines the example
# really prints, and every code block must appear in its source.
add_test(NAME docs.records-output
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_EXE=$<TARGET_FILE:formula-cpp-example-records>"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/records.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideOutput.cmake")

add_test(NAME docs.records-snippets
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/records.cpp"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/records.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideSnippets.cmake")

# docs/opaque-and-retry.md quotes this program's output verbatim. Pinned here,
# in the order the program prints them: an opaque call's line saying its
# inside is not shown, with its citation; the page's operation entry; two
# outputs as two runs; the exact slope; a degenerate fit's and an overflowing
# fit's refusals; the slope rounded where it is used, and on fifteen distinct
# denominators; an uncited call; the retry's rendering; its six endings,
# each by its concluded line; the attempt-1 mistake; and the two-sided
# agreement accepted at the third attempt. No literal `;`:
# PASS_REGULAR_EXPRESSION is a list property (see the lookup_tables entry).
formula_add_example(opaque_and_retry opaque_and_retry.cpp [==[series span\(r\(i\)\)\.span.*span = 88 g \[inside not shown\] \[Spread of readings, Example Standard 12, 4\.2\].*operation: series span, outputs: lowest highest span.*operation runs: 2.*slope = 19/28 mm/s \[inside not shown\].*one point: argument outside the domain of the operation.*fifteen distinct denominators: overflow in exact arithmetic.*round\(linear least squares\(t\(i\), L\(i\)\)\.slope, to 4 dp of mm/s\).*4\. linear least squares\(#3\) = intercept, slope: rounded where used \[inside not shown\].*5\. round\(slope of #4, to 4 dp of mm/s\) = 3393/5000 mm/s \[nearest, ties to even\].*fifteen distinct denominators, rounded where used: 116\.232 mm/min.*\[inside not shown\] \(no citation given\).*up to 4 attempts: w\(k\) = 152/25 g \+ w\(k-1\) / 2, starting from w\(0\) = 0 g.*accepted at attempt 4 of 4 = 57/5 g.*exhausted after 3 of 3: repeat the determination.*not judgeable at attempt 1.*d_a = retry: attempt 3 not recorded.*failed at attempt 1: division by zero.*ManuallyEntered after 0 attempt\(s\).*w\(k-1\) = previous attempt: none before attempt 1.*accept from attempt 2 when abs\(d_a\(k\) - d_a\(k-1\)\) <= 127/100 g.*accepted at attempt 3 of 4 = 427/10 g.*all checks passed: yes]==])

# Every output block of docs/opaque-and-retry.md must be a run of lines the
# example really prints, and every code block must appear in its source.
add_test(NAME docs.opaque-and-retry-output
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_EXE=$<TARGET_FILE:formula-cpp-example-opaque_and_retry>"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/opaque-and-retry.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideOutput.cmake")
add_test(NAME docs.opaque-and-retry-snippets
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/opaque_and_retry.cpp"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/opaque-and-retry.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideSnippets.cmake")

# docs/display.md quotes this program's output verbatim. Pinned here, in the
# order the program prints them: a value with no exact decimal kept as its
# fraction, then rounded and marked, then padded; a comparison stated exactly
# beside a rounded input; the typed tare as a decimal in the formula's text;
# number_text of an absent value; the std::format reference's exact decimal,
# fraction and marked rounding; and a spec refused at run time in the
# library's words.
formula_add_example(display display.cpp [==[#3 / #6 = 134/1185.*#3 / #6 = ≈0\.113.*m_d = 144\.0 g.*67/6 % \(at most 12 %\).*\(m_w - m_d\) / \(m_d - 25\.5 g\).*not measured: \(not measured\).*Rational \{ 3, 5 \}\)  +0\.6.*Rational \{ 1, 3 \}\)  +1/3.*Rational \{ 1, 3 \}\)  +≈0\.333.*formula: this number format rounds but names no rounding mode.*all checks passed: yes]==])

# Every ```text block of docs/display.md must be lines the example really
# prints, and every ```cpp block lines of its source. CALL_TABLES also judges
# the guide's std::format table: each row's call and output must be a line the
# example prints.
add_test(NAME docs.display-output
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_EXE=$<TARGET_FILE:formula-cpp-example-display>"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/display.md"
            -D "CALL_TABLES=ON"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideOutput.cmake")
# README.md's ```text blocks are judged against this example too: its
# std::format block is this program's output, and any ```text block added to
# README later must be as well.
add_test(NAME docs.readme-display-output
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_EXE=$<TARGET_FILE:formula-cpp-example-display>"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/README.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideOutput.cmake")
add_test(NAME docs.display-snippets
    COMMAND "${CMAKE_COMMAND}"
            -D "EXAMPLE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/display.cpp"
            -D "GUIDE=${PROJECT_SOURCE_DIR}/docs/display.md"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGuideSnippets.cmake")
