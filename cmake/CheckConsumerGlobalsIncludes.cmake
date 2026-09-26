# SPDX-License-Identifier: Apache-2.0
# `test/consumer_globals_tests.cpp` is the guard that no public header
# declares a local or parameter hiding a consumer's global (cl C4459 under
# /W4 /WX). It guards only the headers it includes, so every public header --
# every `.hpp` directly under `include/formula-cpp/` -- must be included there
# by name. A header added later and left off the list would be unguarded
# without any test saying so; this check says so.
#
# Included unconditionally, too: an include inside `#if 0`, or inside any
# other preprocessor conditional, is on the list but may never be compiled,
# and would leave its header as unguarded as one left off the list.

set(guard "${SOURCE_DIR}/test/consumer_globals_tests.cpp")
file(GLOB headers RELATIVE "${SOURCE_DIR}/include" "${SOURCE_DIR}/include/formula-cpp/*.hpp")
list(LENGTH headers total)

if(total EQUAL 0)
    message(FATAL_ERROR
        "consumer-globals check examined no headers. SOURCE_DIR=\"${SOURCE_DIR}\" is wrong, or the glob "
        "matches nothing. A check that examines nothing is a check that lies.")
endif()

if(NOT EXISTS "${guard}")
    message(FATAL_ERROR "consumer-globals check: ${guard} does not exist.")
endif()

file(STRINGS "${guard}" includeLines REGEX "^#include <formula-cpp/[^>]+>")

set(missing "")
foreach(header IN LISTS headers)
    list(FIND includeLines "#include <${header}>" found)
    if(found EQUAL -1)
        list(APPEND missing "${header}")
    endif()
endforeach()

if(missing)
    list(JOIN missing "\n  " missingText)
    message(FATAL_ERROR
        "consumer-globals check: test/consumer_globals_tests.cpp does not include these public headers, so "
        "nothing guards them against hiding a consumer's global:\n  ${missingText}")
endif()

# Every preprocessor line, in order: an include of a library header met while
# a conditional is open is refused, whatever the condition says.
file(STRINGS "${guard}" directiveLines REGEX "^[ \t]*#")
set(depth 0)
set(conditional "")
foreach(line IN LISTS directiveLines)
    if(line MATCHES "^[ \t]*#[ \t]*if")
        math(EXPR depth "${depth} + 1")
    elseif(line MATCHES "^[ \t]*#[ \t]*endif")
        math(EXPR depth "${depth} - 1")
    elseif(line MATCHES "^[ \t]*#[ \t]*include[ \t]*<formula-cpp/" AND depth GREATER 0)
        list(APPEND conditional "${line}")
    endif()
endforeach()

if(conditional)
    list(JOIN conditional "\n  " conditionalText)
    message(FATAL_ERROR
        "consumer-globals check: test/consumer_globals_tests.cpp includes these library headers inside a "
        "preprocessor conditional, which may leave them uncompiled and so unguarded; include every one "
        "unconditionally:\n  ${conditionalText}")
endif()

message(STATUS "consumer-globals check: all ${total} public headers are included by the guard, unconditionally")
