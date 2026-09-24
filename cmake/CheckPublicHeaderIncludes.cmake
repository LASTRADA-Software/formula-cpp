# SPDX-License-Identifier: Apache-2.0
# Public headers must not pull in <string>, <vector>, <format> or
# <iostream>: a consumer that only evaluates numbers must not compile those
# in every translation unit. Those belong in opt-in headers.
#
# Exception: render.hpp is deliberately not in the umbrella (formula.hpp) and
# must include <string>.

file(GLOB_RECURSE headers "${SOURCE_DIR}/include/*.hpp")
list(FILTER headers EXCLUDE REGEX "render\\.hpp$")

list(LENGTH headers total)

if(total EQUAL 0)
    message(FATAL_ERROR
        "header include check examined no files. SOURCE_DIR=\"${SOURCE_DIR}\" is wrong, or the "
        "glob matches nothing. A check that examines nothing is a check that lies.")
endif()

# Matched against the include directive itself, anchored to the start of the
# line, so a mention inside a comment (e.g. "// see <string> too") does not
# trip it.
#
# The filtering happens during the read, deliberately. Reading every line into a
# list and walking it is defeated by an unbalanced "[" anywhere earlier in the
# file: CMake stops splitting the list there, every later line is swallowed into
# one element that no longer matches the anchored regex, and the check passes
# while having examined almost nothing. Measured: 0 of 2 banned includes found
# that way, 2 of 2 this way. `foreach(... IN LISTS ...)` does not help -- the
# behaviour is in list-element semantics, not in the argument lexer.
set(offenders "")
set(offenderCount 0)
foreach(header IN LISTS headers)
    file(STRINGS "${header}" bannedLines
         REGEX "^[ \t]*#[ \t]*include[ \t]*<(string|vector|format|iostream)>")
    foreach(line IN LISTS bannedLines)
        file(RELATIVE_PATH rel "${SOURCE_DIR}" "${header}")
        string(APPEND offenders "\n  ${rel}: ${line}")
        math(EXPR offenderCount "${offenderCount}+1")
    endforeach()
endforeach()

if(offenderCount GREATER 0)
    message(FATAL_ERROR "public header pulls in a banned standard header:${offenders}")
endif()

message(STATUS "no <string>, <vector>, <format> or <iostream> in any of ${total} public headers")
