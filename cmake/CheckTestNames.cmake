# SPDX-License-Identifier: Apache-2.0
# Catch2 test names must contain balanced square brackets.
#
# catch_discover_tests registers tests by walking the output of
# `<test-exe> --list-tests` as a CMake list. CMake stops splitting a list at an
# unbalanced "[" or "]", so such a test -- and EVERY test discovered after it --
# collapses into a single unmatchable entry and is silently never run. The
# failure is invisible: ctest reports no error, the tests simply do not exist.
#
# Parentheses are harmless here. `foreach(... IN LISTS ...)` does not help: the
# behaviour is in CMake's list-element semantics, not in the argument lexer.
#
# This script neutralises brackets BEFORE the file contents ever become a CMake
# list, because otherwise the check would be defeated by the very defect it
# exists to catch.

file(GLOB_RECURSE sources "${SOURCE_DIR}/test/*.cpp")

list(LENGTH sources total)

if(total EQUAL 0)
    message(FATAL_ERROR
        "test-name check examined no files. SOURCE_DIR=\"${SOURCE_DIR}\" is wrong, or the glob "
        "matches nothing. A check that examines nothing is a check that lies.")
endif()

set(offenders "")
set(offenderCount 0)

foreach(file IN LISTS sources)
    file(READ "${file}" contents)
    string(REPLACE "[" "@LB@" contents "${contents}")
    string(REPLACE "]" "@RB@" contents "${contents}")
    string(REPLACE ";" "@SEMI@" contents "${contents}")

    string(REGEX MATCHALL "(TEST_CASE|TEMPLATE_TEST_CASE|SCENARIO)[ \t]*\\([ \t]*\"[^\"]*\""
           declarations "${contents}")

    foreach(declaration IN LISTS declarations)
        string(REGEX MATCH "\"([^\"]*)\"" matched "${declaration}")
        set(name "${CMAKE_MATCH_1}")

        string(REGEX MATCHALL "@LB@" opens "${name}")
        string(REGEX MATCHALL "@RB@" closes "${name}")
        list(LENGTH opens openCount)
        list(LENGTH closes closeCount)

        if(NOT openCount EQUAL closeCount)
            string(REPLACE "@LB@" "[" readable "${name}")
            string(REPLACE "@RB@" "]" readable "${readable}")
            string(REPLACE "@SEMI@" ";" readable "${readable}")
            file(RELATIVE_PATH rel "${SOURCE_DIR}" "${file}")
            string(APPEND offenders "\n  ${rel}: ${readable}")
            math(EXPR offenderCount "${offenderCount}+1")
        endif()
    endforeach()
endforeach()

if(offenderCount GREATER 0)
    message(FATAL_ERROR
        "Catch2 test name with unbalanced square brackets. ctest would silently skip this test "
        "and every test discovered after it:${offenders}")
endif()

message(STATUS "balanced brackets in every test name across ${total} files")
