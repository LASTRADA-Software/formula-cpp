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

# Runs the check on `sourceDir` with the allow-list `allowList` (none when
# empty), and sets `exitCode` and `report`, and `flatReport`: the report with
# every run of whitespace made one space, because CMake wraps a message's prose
# wherever the line grows long, and the paths in it differ in length per tree.
function(run_check sourceDir allowList)
    set(arguments -D "SOURCE_DIR=${sourceDir}")
    if(NOT allowList STREQUAL "")
        list(APPEND arguments -D "ALLOWLIST=${allowList}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" ${arguments} -P "${CHECK_SCRIPT}"
                    RESULT_VARIABLE result OUTPUT_VARIABLE out ERROR_VARIABLE err)
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${out}${err}")
    set(exitCode "${result}" PARENT_SCOPE)
    set(report "${out}${err}" PARENT_SCOPE)
    set(flatReport "${flat}" PARENT_SCOPE)
endfunction()

file(REMOVE_RECURSE "${WORK_DIR}")
# CRLF line endings, and a first line holding `;`, `[` and `]`, which a CMake
# list would mangle: the phrase must still be reported on line 2.
file(WRITE "${WORK_DIR}/README.md" "x; [a]\r\nIt no longer compiles.\r\n")
file(WRITE "${WORK_DIR}/docs/guide.md"
    "This was previously a warning.\n"
    "The two determinations no longer agree.\n"
    "New in 0.4.0: literals.\n"
    "The factor is used to round the strength, in 2.5 mm steps.\n"
    "It used to return a double.\n"
    "The flag once meant the opposite.\n"
    "The type was renamed.\n"
    "The overload is deprecated.\n"
    "Text written before the option existed.\n"
    "Since 0.3 and as of v0.2 it rounds; until 1.2.3 it did not.\n"
    "This is _no longer_ true.\n"
    "A new instance, used together, within 1.2.3 of it.\n"
    "The value is\tused to round.\n")
file(WRITE "${WORK_DIR}/docs/superpowers/plan.md" "This used to be ignored, and is.\n")
file(WRITE "${WORK_DIR}/include/formula-cpp/x.hpp"
    "/// Formerly named old_name.\n"
    "int previously_named = 0; // code, not a comment line: not scanned\n")
file(WRITE "${WORK_DIR}/allow.txt"
    "docs/guide.md|The two determinations no longer agree.|describes data, not history\n")

run_check("${WORK_DIR}" "${WORK_DIR}/allow.txt")
if(exitCode EQUAL 0)
    message(FATAL_ERROR "the check passed a tree full of history wording:\n${report}")
endif()
foreach(expected IN ITEMS "README.md:2: no longer" "docs/guide.md:1: previously" "docs/guide.md:3: new in"
                          "docs/guide.md:3: in 0.4.0" "docs/guide.md:5: used to" "docs/guide.md:6: once meant"
                          "docs/guide.md:7: was renamed" "docs/guide.md:8: deprecated"
                          "docs/guide.md:9: before the option existed" "docs/guide.md:10: since 0.3"
                          "docs/guide.md:10: as of v0.2" "docs/guide.md:10: until 1.2.3"
                          "docs/guide.md:11: no longer" "include/formula-cpp/x.hpp:1: formerly")
    string(FIND "${report}" "${expected}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR "the check did not report '${expected}':\n${report}")
    endif()
endforeach()
foreach(unexpected IN ITEMS "README.md:1:" "docs/guide.md:2:" "docs/guide.md:4:" "docs/guide.md:12:"
                            "docs/guide.md:13:" "docs/superpowers/" "x.hpp:2:")
    string(FIND "${report}" "${unexpected}" at)
    if(NOT at EQUAL -1)
        message(FATAL_ERROR "the check reported '${unexpected}', which it must not:\n${report}")
    endif()
endforeach()

# An allow-list entry without a reason is refused.
file(WRITE "${WORK_DIR}/no-reason.txt" "docs/guide.md|x\n")
run_check("${WORK_DIR}" "${WORK_DIR}/no-reason.txt")
string(FIND "${flatReport}" "has no reason" at)
if(exitCode EQUAL 0 OR at EQUAL -1)
    message(FATAL_ERROR "the check did not refuse an allow-list entry without a reason:\n${report}")
endif()

# A directory with nothing to scan fails: a check that examines nothing lies.
file(MAKE_DIRECTORY "${WORK_DIR}/empty")
run_check("${WORK_DIR}/empty" "")
string(FIND "${flatReport}" "examined no files" at)
if(exitCode EQUAL 0 OR at EQUAL -1)
    message(FATAL_ERROR "the check did not fail on a directory with nothing to scan:\n${report}")
endif()

# With every offending line removed, the same tree passes.
file(WRITE "${WORK_DIR}/README.md" "A Bounds is written with designated initialisers.\n")
file(WRITE "${WORK_DIR}/docs/guide.md" "The two determinations no longer agree.\n")
file(WRITE "${WORK_DIR}/include/formula-cpp/x.hpp" "/// Named new_name.\n")

# An allow-list entry that allows no reported line -- a reworded line, a wrong
# path -- fails the clean tree, and is named; the entry still in use is not.
file(WRITE "${WORK_DIR}/stale.txt"
    "docs/guide.md|The two determinations no longer agree.|describes data, not history\n"
    "docs/gide.md|The two determinations no longer agree.|a typo in the path\n")
run_check("${WORK_DIR}" "${WORK_DIR}/stale.txt")
if(exitCode EQUAL 0)
    message(FATAL_ERROR "the check passed an allow-list entry that allows no line:\n${report}")
endif()
string(FIND "${report}" "  docs/gide.md|The two determinations no longer agree." at)
if(at EQUAL -1)
    message(FATAL_ERROR "the check did not name the allow-list entry that allows no line:\n${report}")
endif()
string(FIND "${report}" "  docs/guide.md|" at)
if(NOT at EQUAL -1)
    message(FATAL_ERROR "the check named an allow-list entry that is in use:\n${report}")
endif()

run_check("${WORK_DIR}" "${WORK_DIR}/allow.txt")
if(NOT exitCode EQUAL 0)
    message(FATAL_ERROR "the check failed a clean tree:\n${report}")
endif()
message(STATUS "TestDocsCurrentState: the check fails on history wording and passes a clean tree")
