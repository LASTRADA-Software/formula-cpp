# SPDX-License-Identifier: Apache-2.0
#
# The fallback of CheckNoRealStandards.cmake, pinned: a tree whose `.git` is a
# file naming a git directory git cannot resolve -- as a worktree created by
# git for Windows is, read from WSL -- must still be scanned, not refused.
#
# The fixture is built fresh under WORK_DIR on every run:
#
#  - `.git`, a file reading `gitdir: ` and a path that does not exist, so that
#    `git ls-files` fails (or git is missing, which the fallback also covers);
#  - `docs/clean.md`, naming only an invented Example Standard;
#  - `out/planted.md`, naming a real body -- inside build output, which the
#    fallback leaves out, so it must not be reported.
#
# Then the check must pass, say that git failed and that it scanned the files
# itself, and have scanned exactly one file. With a second planted file
# outside build output, `docs/planted.md`, it must fail and name that file.
#
# The planted identifier is assembled here from two halves, so that this file
# does not itself name a body the check would report.
cmake_minimum_required(VERSION 3.23)

foreach(variable CHECK_SCRIPT WORK_DIR)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "TestNoRealStandardsFallback.cmake: ${variable} is not set")
    endif()
endforeach()

set(plantedBody "DI")
string(APPEND plantedBody "N 9999")

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/docs" "${WORK_DIR}/out")
file(WRITE "${WORK_DIR}/.git" "gitdir: ${WORK_DIR}/no-such-git-directory/worktrees/fixture\n")
file(WRITE "${WORK_DIR}/docs/clean.md" "Cited as Example Standard 7:2020, clause 5.1.\n")
file(WRITE "${WORK_DIR}/out/planted.md" "Build output that cites ${plantedBody}.\n")

function(run_check outResult outText)
    execute_process(COMMAND "${CMAKE_COMMAND}" -D "SOURCE_DIR=${WORK_DIR}" -P "${CHECK_SCRIPT}"
                    RESULT_VARIABLE result
                    OUTPUT_VARIABLE out
                    ERROR_VARIABLE err)
    set(${outResult} "${result}" PARENT_SCOPE)
    set(${outText} "${out}${err}" PARENT_SCOPE)
endfunction()

run_check(cleanResult cleanText)
if(NOT cleanResult EQUAL 0)
    message(FATAL_ERROR
        "no-real-standards fallback: the check refused a clean tree whose .git git cannot read, rather "
        "than scanning it (exit ${cleanResult}):\n${cleanText}")
endif()
foreach(expected "`git ls-files` failed" "scanning every file outside build and site output instead"
                 "1 files scanned, no real standard named")
    string(FIND "${cleanText}" "${expected}" at)
    if(at EQUAL -1)
        message(FATAL_ERROR
            "no-real-standards fallback: on a clean tree whose .git git cannot read, the check's output "
            "lacks \"${expected}\":\n${cleanText}")
    endif()
endforeach()

file(WRITE "${WORK_DIR}/docs/planted.md" "A citation of ${plantedBody}.\n")
run_check(plantedResult plantedText)
if(plantedResult EQUAL 0)
    message(FATAL_ERROR
        "no-real-standards fallback: the check passed a tree naming a real body in docs/planted.md:\n"
        "${plantedText}")
endif()
string(FIND "${plantedText}" "docs/planted.md:1: ${plantedBody}" at)
if(at EQUAL -1)
    message(FATAL_ERROR
        "no-real-standards fallback: the check failed, but not by naming docs/planted.md:\n${plantedText}")
endif()
string(FIND "${plantedText}" "out/planted.md" at)
if(NOT at EQUAL -1)
    message(FATAL_ERROR
        "no-real-standards fallback: the check reported a file inside build output:\n${plantedText}")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
message(STATUS "no-real-standards fallback: a tree git cannot read is scanned, passes clean and fails when planted")
