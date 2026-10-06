# SPDX-License-Identifier: Apache-2.0
# A program whose output a page includes must print exactly that output. The
# page includes `<program>.expected.txt` verbatim (pymdownx.snippets), so this
# is the check that makes the published output the program's real output: it
# runs the program and compares its standard output with that file, whole,
# byte for byte.
#
# Line endings are normalised on both sides first: std::println writes CRLF on
# Windows, and .gitattributes checks text files out with LF. Nothing else is
# normalised -- a trailing space, a missing final newline or a changed line
# is a difference.
#
# On a difference it fails naming the file, and prints both texts, so the fix
# -- the program or the expected file -- can be chosen by reading them.
#
# `cmake_minimum_required` for the reason CheckGuideOutput.cmake gives: under
# `cmake -P`'s default OLD policies, CMake 3.28.3 misreads `if()` constructs
# the project's own minimum reads correctly.
cmake_minimum_required(VERSION 3.23)

foreach(variable EXAMPLE_EXE EXPECTED)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "CheckExpectedOutput.cmake: ${variable} is not set")
    endif()
endforeach()

if(NOT EXISTS "${EXPECTED}")
    message(FATAL_ERROR "CheckExpectedOutput.cmake: ${EXPECTED} does not exist")
endif()

execute_process(COMMAND "${EXAMPLE_EXE}"
                OUTPUT_VARIABLE actual
                ERROR_VARIABLE errors
                RESULT_VARIABLE exitCode)
if(NOT exitCode EQUAL 0)
    message(FATAL_ERROR "CheckExpectedOutput.cmake: ${EXAMPLE_EXE} exited with ${exitCode}:\n${errors}")
endif()

file(READ "${EXPECTED}" expected)
if(expected STREQUAL "")
    message(FATAL_ERROR
        "CheckExpectedOutput.cmake: ${EXPECTED} is empty. An empty expected output matches only a "
        "program that prints nothing, which no page includes.")
endif()

string(REPLACE "\r\n" "\n" actual "${actual}")
string(REPLACE "\r\n" "\n" expected "${expected}")

if(NOT actual STREQUAL expected)
    message(FATAL_ERROR
        "CheckExpectedOutput.cmake: the output of ${EXAMPLE_EXE} differs from ${EXPECTED}.\n"
        "--- expected\n${expected}--- actual\n${actual}--- end")
endif()

message(STATUS "CheckExpectedOutput.cmake: output matches ${EXPECTED}")
