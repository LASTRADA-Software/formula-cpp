# SPDX-License-Identifier: Apache-2.0
# The current-state check must fail on history wording, name every offence,
# and accept a line its allow-list names. This builds a small tree in WORK_DIR
# and runs the check on it, so the check is tested on text written to break it,
# not only on a tree that happens to pass.
cmake_minimum_required(VERSION 3.23)

foreach(variable CHECK_SCRIPT WORK_DIR)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "TestDocsCurrentState.cmake: ${variable} is not set")
    endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(WRITE "${WORK_DIR}/README.md" "A Bounds written positionally no longer compiles.\n")
file(WRITE "${WORK_DIR}/docs/guide.md"
    "This was previously a warning.\n"
    "The two determinations no longer agree.\n"
    "New in 0.4.0: literals.\n"
    "The factor is used to round the strength, in 2.5 mm steps.\n"
    "It used to return a double.\n")
file(WRITE "${WORK_DIR}/docs/superpowers/plan.md" "This used to be ignored, and is.\n")
file(WRITE "${WORK_DIR}/include/formula-cpp/x.hpp"
    "/// Formerly named old_name.\n"
    "int previously_named = 0; // code, not a comment line: not scanned\n")
file(WRITE "${WORK_DIR}/allow.txt"
    "docs/guide.md|The two determinations no longer agree.|describes data, not history\n")

execute_process(COMMAND "${CMAKE_COMMAND}" -D "SOURCE_DIR=${WORK_DIR}" -D "ALLOWLIST=${WORK_DIR}/allow.txt"
                        -P "${CHECK_SCRIPT}"
                RESULT_VARIABLE exitCode OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(report "${out}${err}")

if(exitCode EQUAL 0)
    message(FATAL_ERROR "the check passed a tree full of history wording:\n${report}")
endif()
foreach(expected IN ITEMS "README.md:1: no longer" "docs/guide.md:1: previously" "docs/guide.md:3: new in"
                          "docs/guide.md:3: in 0.4.0" "docs/guide.md:5: used to" "include/formula-cpp/x.hpp:1: formerly")
    string(FIND "${report}" "${expected}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "the check did not report '${expected}':\n${report}")
    endif()
endforeach()
foreach(unexpected IN ITEMS "docs/guide.md:2:" "docs/guide.md:4:" "superpowers" "x.hpp:2:")
    string(FIND "${report}" "${unexpected}" at)
    if(NOT at EQUAL -1)
        message(FATAL_ERROR "the check reported '${unexpected}', which it must not:\n${report}")
    endif()
endforeach()

# With every offending line removed, the same tree passes.
file(WRITE "${WORK_DIR}/README.md" "A Bounds is written with designated initialisers.\n")
file(WRITE "${WORK_DIR}/docs/guide.md" "The two determinations no longer agree.\n")
file(WRITE "${WORK_DIR}/include/formula-cpp/x.hpp" "/// Named new_name.\n")
execute_process(COMMAND "${CMAKE_COMMAND}" -D "SOURCE_DIR=${WORK_DIR}" -D "ALLOWLIST=${WORK_DIR}/allow.txt"
                        -P "${CHECK_SCRIPT}"
                RESULT_VARIABLE exitCode OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT exitCode EQUAL 0)
    message(FATAL_ERROR "the check failed a clean tree:\n${out}${err}")
endif()
message(STATUS "TestDocsCurrentState: the check fails on history wording and passes a clean tree")
