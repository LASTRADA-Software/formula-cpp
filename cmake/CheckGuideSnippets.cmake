# SPDX-License-Identifier: Apache-2.0
# A guide's code must be code that compiles. This checks that every ```cpp
# block of the guide appears in the example's source as a run of consecutive
# lines -- so the example's own build, run and pass regex vouch for it.
#
# Lines are compared after their leading whitespace is stripped, so a snippet
# dedented from inside `main()` still matches; everything else on a line,
# trailing text included, must be identical. A block, not each line: a snippet
# spliced from two places of the source would compile nowhere.
#
# **A snippet that is deliberately not from the example** -- a misuse the page
# shows so that it can say what happens -- is marked by the line
#
#     <!-- snippet: not from the example -->
#
# directly above its opening fence, and is skipped. The guide says near its
# top what the marker means. A guide whose every block is skipped fails as
# checking nothing.
#
# `cmake_minimum_required` for the reason CheckGuideOutput.cmake gives: under
# `cmake -P`'s default OLD policies, CMake 3.28.3 reads `while(TRUE)` as an
# undefined variable. Nothing here becomes a CMake list, for the reason that
# script gives too.
cmake_minimum_required(VERSION 3.23)

foreach(variable EXAMPLE_SOURCE GUIDE)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "CheckGuideSnippets.cmake: ${variable} is not set")
    endif()
endforeach()

file(READ "${EXAMPLE_SOURCE}" source)
string(REPLACE "\r\n" "\n" source "${source}")
string(REGEX REPLACE "\n[ \t]+" "\n" source "\n${source}\n")

file(READ "${GUIDE}" guide)
string(REPLACE "\r\n" "\n" guide "${guide}")

set(opening "```cpp\n")
set(marker "<!-- snippet: not from the example -->\n")
string(LENGTH "${opening}" openingLength)
string(LENGTH "${marker}" markerLength)
set(rest "${guide}")
set(checked 0)
set(skipped 0)
set(offenders "")
while(TRUE)
    string(FIND "${rest}" "${opening}" start)
    if(start EQUAL -1)
        break()
    endif()

    set(isMarked FALSE)
    if(start GREATER_EQUAL markerLength)
        math(EXPR markerStart "${start} - ${markerLength}")
        string(SUBSTRING "${rest}" ${markerStart} ${markerLength} before)
        if(before STREQUAL marker)
            set(isMarked TRUE)
        endif()
    endif()

    math(EXPR bodyStart "${start} + ${openingLength}")
    string(SUBSTRING "${rest}" ${bodyStart} -1 rest)
    string(FIND "${rest}" "```" end)
    if(end EQUAL -1)
        message(FATAL_ERROR "guide snippet check: ${GUIDE} has a ```cpp block that is never closed")
    endif()
    string(SUBSTRING "${rest}" 0 ${end} block)
    string(SUBSTRING "${rest}" ${end} -1 rest)

    if(isMarked)
        math(EXPR skipped "${skipped} + 1")
        continue()
    endif()
    math(EXPR checked "${checked} + 1")

    string(REGEX REPLACE "\n[ \t]+" "\n" block "\n${block}")
    string(FIND "${source}" "${block}" found)
    if(found EQUAL -1)
        string(SUBSTRING "${block}" 1 -1 withoutLead)
        string(FIND "${withoutLead}" "\n" firstNewline)
        string(SUBSTRING "${withoutLead}" 0 ${firstNewline} firstLine)
        string(APPEND offenders "\n  the block beginning: ${firstLine}")
    endif()
endwhile()

if(checked EQUAL 0)
    message(FATAL_ERROR
        "guide snippet check examined no ```cpp block in ${GUIDE} (skipped: ${skipped}). A check that "
        "examines nothing is a check that lies.")
endif()

if(NOT offenders STREQUAL "")
    message(FATAL_ERROR
        "${GUIDE} quotes code that is not in ${EXAMPLE_SOURCE} as quoted. Copy it from the example, or "
        "mark a deliberate exception with <!-- snippet: not from the example -->:${offenders}")
endif()

message(STATUS "guide snippet check: ${checked} blocks found in the example, ${skipped} marked as not from it")
