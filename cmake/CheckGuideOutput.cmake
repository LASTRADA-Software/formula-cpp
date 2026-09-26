# SPDX-License-Identifier: Apache-2.0
# A guide that quotes its example's output must quote what the example really
# prints. This runs the example and checks that every one of the guide's
# output blocks -- fenced as ```text -- appears in that output as a run of
# whole, consecutive lines, exactly as quoted.
#
# The example's own pass regex (examples/CMakeLists.txt) pins the spellings a
# change is most likely to break, in order. It cannot pin every quoted line
# without restating the whole output, and a quoted line nothing checks goes
# stale silently: a trace line whose format changed, a symbol table whose order
# changed, a rational approximation of pi that became a different rational.
# This is the check that makes "every output block is copied from the program"
# a fact rather than a promise.
#
# Only ```text blocks are judged. A compiler diagnostic the guide quotes is a
# plain ``` block: it comes from a compiler, not from the example, and
# CheckDocumentedDiagnostics.cmake is what judges those.
#
# A block, not only each line in it: a first draft checked line by line, and
# swapping one symbol-table row for a row printed elsewhere by the same program
# -- `b: second loaded edge` in place of `b: first loaded edge`, a different
# claim about what `b` means -- passed, because that line does appear in the
# output, in another table. A guide may still quote an excerpt: any run of
# consecutive lines is a block. What it may not do is splice lines from two
# places into one block. When a block does not match, the lines in it that the
# program never prints at all are listed, to say where to look.
#
# Nothing here becomes a CMake list: the lines hold `[`, `]` and `;`, each of
# which CMake's list handling mangles (see CheckTestNames.cmake and
# CheckDocumentedDiagnostics.cmake for what that has cost this repository).
# The text is walked with string(FIND) and string(SUBSTRING) only.

# A script run with `cmake -P` starts with every policy at its OLD setting, and
# under the old CMP0012 `while(TRUE)` reads TRUE as an undefined variable: the
# loop below never ran on CMake 3.28.3, where it had run on 4.3.1, and only the
# "examined nothing" refusal said so. The project's own minimum sets them all.
cmake_minimum_required(VERSION 3.23)

foreach(variable EXAMPLE_EXE GUIDE)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "CheckGuideOutput.cmake: ${variable} is not set")
    endif()
endforeach()

execute_process(
    COMMAND "${EXAMPLE_EXE}"
    RESULT_VARIABLE exampleResult
    OUTPUT_VARIABLE exampleOutput
    ERROR_VARIABLE exampleError)
if(NOT exampleResult EQUAL 0)
    message(FATAL_ERROR
        "guide output check: the example itself failed (exit ${exampleResult}).\n"
        "--- stdout ---\n${exampleOutput}\n--- stderr ---\n${exampleError}")
endif()

string(REPLACE "\r\n" "\n" exampleOutput "${exampleOutput}")
set(haystack "\n${exampleOutput}\n")

file(READ "${GUIDE}" guide)
string(REPLACE "\r\n" "\n" guide "${guide}")

set(opening "```text\n")
string(LENGTH "${opening}" openingLength)
set(rest "${guide}")
set(blocks 0)
set(lines 0)
set(offenders "")
while(TRUE)
    string(FIND "${rest}" "${opening}" start)
    if(start EQUAL -1)
        break()
    endif()
    math(EXPR bodyStart "${start} + ${openingLength}")
    string(SUBSTRING "${rest}" ${bodyStart} -1 rest)
    string(FIND "${rest}" "```" end)
    if(end EQUAL -1)
        message(FATAL_ERROR "guide output check: ${GUIDE} has a ```text block that is never closed")
    endif()
    string(SUBSTRING "${rest}" 0 ${end} block)
    string(SUBSTRING "${rest}" ${end} -1 rest)
    math(EXPR blocks "${blocks} + 1")

    # The block's text ends in the newline before the closing fence, so this
    # asks for whole lines from the start of one to the end of another.
    string(FIND "${haystack}" "\n${block}" blockFound)
    if(blockFound EQUAL -1)
        string(FIND "${block}" "\n" firstNewline)
        string(SUBSTRING "${block}" 0 ${firstNewline} firstLine)
        string(APPEND offenders "\n  the block beginning: ${firstLine}")
    endif()

    while(NOT block STREQUAL "")
        string(FIND "${block}" "\n" newline)
        if(newline EQUAL -1)
            set(line "${block}")
            set(block "")
        else()
            string(SUBSTRING "${block}" 0 ${newline} line)
            math(EXPR next "${newline} + 1")
            string(SUBSTRING "${block}" ${next} -1 block)
        endif()
        if(line STREQUAL "")
            continue()
        endif()
        math(EXPR lines "${lines} + 1")
        string(FIND "${haystack}" "\n${line}\n" found)
        if(found EQUAL -1)
            string(APPEND offenders "\n    a line it never prints: ${line}")
        endif()
    endwhile()
endwhile()

if(blocks EQUAL 0 OR lines EQUAL 0)
    message(FATAL_ERROR
        "guide output check examined no output lines in ${GUIDE} (blocks: ${blocks}). A check that "
        "examines nothing is a check that lies.")
endif()

if(NOT offenders STREQUAL "")
    message(FATAL_ERROR
        "${GUIDE} quotes output the example does not print as quoted. Re-run the example and copy its output:"
        "${offenders}")
endif()

message(STATUS "guide output check: ${blocks} blocks of ${lines} quoted lines, each printed by the example as quoted")
