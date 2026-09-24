# SPDX-License-Identifier: Apache-2.0
# Public headers must not pull in <string>, <vector>, <format> or
# <iostream>: a consumer that only evaluates numbers must not compile those
# in every translation unit. Those belong in opt-in headers.
#
# A header earns a place on `exemptHeaders` only when it is deliberately
# outside that guarantee: it is not part of the umbrella (formula.hpp), and a
# consumer must ask for it by name to pay for what it pulls in. render.hpp
# and document.hpp qualify -- they need <string>, <string_view> and, for
# document.hpp, <vector>, to turn a formula into text. trace.hpp qualifies
# too -- it needs <vector> for its arena, but deliberately not <string>.
#
# Being named here does not, by itself, permit anything: check 2 below
# enforces that no other header may reach one of these, which is what stops
# the umbrella acquiring <string> transitively through them. Growing this
# list only ever grows what check 2 must keep unreachable.
set(exemptHeaders
    "${SOURCE_DIR}/include/formula-cpp/render.hpp"
    "${SOURCE_DIR}/include/formula-cpp/document.hpp"
    "${SOURCE_DIR}/include/formula-cpp/trace.hpp"
)

file(GLOB_RECURSE headers "${SOURCE_DIR}/include/*.hpp")
list(LENGTH headers total)

if(total EQUAL 0)
    message(FATAL_ERROR
        "header include check examined no files. SOURCE_DIR=\"${SOURCE_DIR}\" is wrong, or the "
        "glob matches nothing. A check that examines nothing is a check that lies.")
endif()

set(scannedHeaders "${headers}")
list(REMOVE_ITEM scannedHeaders ${exemptHeaders})
list(LENGTH scannedHeaders scannedTotal)

if(scannedTotal EQUAL 0)
    message(FATAL_ERROR
        "header include check has nothing left to scan once exemptHeaders is removed from "
        "${total} discovered headers -- exemptHeaders no longer matches the tree.")
endif()

# --- Check 1: no non-exempt header pulls a banned standard header directly. ----
#
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
foreach(header IN LISTS scannedHeaders)
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

# --- Check 2: no non-exempt header reaches an exempt one either. ---------------
#
# Check 1 alone used to be the whole guarantee only by accident: while no
# header included <string>, the umbrella could not acquire it transitively
# either, simply because nothing else was there to carry it in. That stopped
# being true the moment an opt-in header existed. Confirmed by experiment:
# adding "#include <formula-cpp/render.hpp>" to formula.hpp builds clean and
# check 1 keeps passing, because render.hpp itself is excluded from the scan
# and formula.hpp includes no banned header *by name*. Check 1 is blind to the
# path that actually matters. This check closes that path directly: a
# non-exempt header may not include an exempt one, so an exempt header's
# <string> can never ride into the umbrella one hop removed.
set(exemptNames "")
foreach(exempt IN LISTS exemptHeaders)
    get_filename_component(exemptName "${exempt}" NAME)
    list(APPEND exemptNames "${exemptName}")
endforeach()
list(JOIN exemptNames "|" exemptNamePattern)

set(leaks "")
set(leakCount 0)
foreach(header IN LISTS scannedHeaders)
    file(STRINGS "${header}" exemptLines
         REGEX "^[ \t]*#[ \t]*include[ \t]*<formula-cpp/(${exemptNamePattern})>")
    foreach(line IN LISTS exemptLines)
        file(RELATIVE_PATH rel "${SOURCE_DIR}" "${header}")
        string(APPEND leaks "\n  ${rel}: ${line}")
        math(EXPR leakCount "${leakCount}+1")
    endforeach()
endforeach()

if(leakCount GREATER 0)
    message(FATAL_ERROR
        "public header includes an exempt, opt-in header -- this reintroduces the standard "
        "headers this check exists to keep out of the umbrella, one hop removed from where "
        "check 1 looks:${leaks}")
endif()

# --- Check 3: an exempt header is exempt for the includes it needs, not for all.
#
# Naming a header on `exemptHeaders` skips it in check 1 entirely, which is one
# blunt instrument too many: render.hpp needs <string>, but nothing should let
# it quietly acquire <iostream> as well. Each exempt header therefore declares
# exactly which banned includes it may use, and using any other one fails here.
#
# Written as "<file name>=<comma-separated includes>" because a CMake list is
# semicolon-separated and cannot nest.
set(exemptAllowances
    "render.hpp=string"
    "document.hpp=string,vector"
    "trace.hpp=vector"
)

set(overreaches "")
set(overreachCount 0)
foreach(exempt IN LISTS exemptHeaders)
    get_filename_component(exemptName "${exempt}" NAME)

    set(allowed "")
    set(declared FALSE)
    foreach(allowance IN LISTS exemptAllowances)
        if(allowance MATCHES "^${exemptName}=(.*)$")
            string(REPLACE "," ";" allowed "${CMAKE_MATCH_1}")
            set(declared TRUE)
        endif()
    endforeach()

    if(NOT declared)
        message(FATAL_ERROR
            "${exemptName} is on exemptHeaders but declares no allowance in exemptAllowances. "
            "An exemption without a declared list is an exemption from everything, which is "
            "what this check exists to prevent.")
    endif()

    # Asked one banned header at a time, rather than by extracting "which one
    # did this line use" from a capture group. A greedy `^.*<(...)>` picks the
    # LAST match on a line, so a line naming two banned headers would be judged
    # by the wrong one and could pass while the other is forbidden. Measured:
    # a line reading `#include <iostream>` followed by `#include <string>` was
    # reported as `string`, which is allowed, and the check passed.
    foreach(banned IN ITEMS string vector format iostream)
        list(FIND allowed "${banned}" allowedIndex)
        if(allowedIndex EQUAL -1)
            file(STRINGS "${exempt}" bannedLines
                 REGEX "^[ 	]*#[ 	]*include[ 	]*<${banned}>")
            foreach(line IN LISTS bannedLines)
                file(RELATIVE_PATH rel "${SOURCE_DIR}" "${exempt}")
                string(APPEND overreaches "
  ${rel}: ${line}")
                math(EXPR overreachCount "${overreachCount}+1")
            endforeach()
        endif()
    endforeach()
endforeach()

if(overreachCount GREATER 0)
    message(FATAL_ERROR
        "an opt-in header includes a banned standard header it was not exempted for -- add it "
        "to that header's entry in exemptAllowances only if it is genuinely "
        "needed:${overreaches}")
endif()

message(STATUS
    "no <string>, <vector>, <format> or <iostream> in any of ${scannedTotal} public headers, "
    "none of them reach ${exemptNamePattern} either, and each opt-in header uses none of those "
    "same four banned headers beyond what it declares in exemptAllowances -- check 3 does not "
    "examine any other standard header, so an opt-in header may also reach one undeclared, as "
    "render.hpp does with <string_view>")
