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
#  - `0 to under 100 mm gives 95 %` -- the ONE spelling of a half-open
#    interval in this library. It exists because `[0, 100)` is Markdown link
#    syntax, which silently dropped an operand from a published page in phase
#    8; a guard test already forbids `](` and a bare `[` in any Markdown
#    rendering, and this regex additionally pins that the replacement wording
#    itself does not drift.
#  - `at 100 mm gives 95 %` -- the interpolating table's point spelling, which
#    must NOT read as an interval: a breakpoint is a row, not a boundary.
#  - `key 7 gives 97 %` -- an exact key renders as its underlying value, not
#    its name (an enumerator has none at run time) and not its row index. The
#    example numbers its enumerators 3, 7, 11 and 13 precisely so a value and
#    an index cannot be confused, which is why this pins 7 and not 1.
#  - `[between 100 and 150 mm]`, `[90 to under 100 %]` and
#    `[on the row at 200 mm]` -- the trace clauses that say where an answer
#    came from, in the order the program prints them. Three rather than two:
#    an interpolating lookup has a clause for the two rows it drew on AND a
#    different one for a value sitting exactly on a row, and those say
#    genuinely different things, so pinning only the first would leave the
#    row-hit spelling free to drift. (Order is load bearing here -- `.*`
#    between the clauses means this asserts a sequence, and the first draft of
#    this regex listed them in the wrong order and failed, which is how the
#    sequencing was confirmed to be doing work rather than decorating.)
#  - `[in no band ... the bands cover 0 to under 200 mm]` -- the clause that
#    keeps a miss from being an opaque DomainError. Written as two halves
#    joined by `.*` rather than quoting the literal `;` between them:
#    PASS_REGULAR_EXPRESSION is a LIST property, so a `;` would split this
#    into two alternative regexes, either of which passing would satisfy the
#    test -- silently weakening it to a fraction of what it says.
#
# A `[==[ ... ]==]` bracket argument for the reason the constraints entry
# gives: the regex needs literal backslash escapes around `[` and `]`.
formula_add_example(lookup_tables lookup_tables.cpp [==[0 to under 100 mm gives 95 %.*at 100 mm gives 95 %.*key 7 gives 97 %.*\[between 100 and 150 mm\].*\[90 to under 100 %\].*\[on the row at 200 mm\].*\[in no band.*the bands cover 0 to under 200 mm\].*all checks passed: yes]==])
