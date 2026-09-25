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
