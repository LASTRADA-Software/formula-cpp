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
