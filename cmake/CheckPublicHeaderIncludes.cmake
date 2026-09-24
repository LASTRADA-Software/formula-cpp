# SPDX-License-Identifier: Apache-2.0
# Public headers must not pull in <string>, <vector>, <format> or
# <iostream>: a consumer that only evaluates numbers must not compile those
# in every translation unit. Those belong in opt-in headers.
#
# A header earns a place on `exemptHeaders` only when it is deliberately
# outside that guarantee: it is not part of the umbrella (formula.hpp), and a
# consumer must ask for it by name to pay for what it pulls in. render.hpp
# and document.hpp qualify -- they need <string>, <string_view> and, for
# document.hpp, <vector>, to turn a formula into text.
#
# Being named here does not, by itself, permit anything: check 2 below
# enforces that no other header may reach one of these, which is what stops
# the umbrella acquiring <string> transitively through them. Growing this
# list only ever grows what check 2 must keep unreachable.
set(exemptHeaders
    "${SOURCE_DIR}/include/formula-cpp/render.hpp"
    "${SOURCE_DIR}/include/formula-cpp/document.hpp"
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

message(STATUS
    "no <string>, <vector>, <format> or <iostream> in any of ${scannedTotal} public headers, "
    "and none of them reach ${exemptNamePattern} either")
