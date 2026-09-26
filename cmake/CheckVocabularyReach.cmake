# SPDX-License-Identifier: Apache-2.0
# Every surface that writes a quantity's symbol writes it through the
# vocabulary it was given (`vocabulary.hpp`), never straight off `Describe`.
#
# A symbol read directly is a defect no page shows: the page is rendered in
# the jurisdiction's words and the one surface that read `Describe` is not.
# The trace is the case that made this a check rather than a habit -- its
# symbols are written by the sink at evaluation time, so a renderer threaded
# with the vocabulary says nothing about them -- and a new step kind that
# names a quantity copies the old line as naturally as not. So these four
# headers may not contain a `Describe<...>::symbol` or a `quantity_symbol()`
# outside a comment; `symbol_of<Q>(vocabulary)` is how they ask.

set(surfaces
    "${SOURCE_DIR}/include/formula-cpp/render.hpp"
    "${SOURCE_DIR}/include/formula-cpp/document.hpp"
    "${SOURCE_DIR}/include/formula-cpp/trace.hpp"
    "${SOURCE_DIR}/include/formula-cpp/trace_render.hpp")

set(offenders "")
foreach(file IN LISTS surfaces)
    if(NOT EXISTS "${file}")
        message(FATAL_ERROR
            "vocabulary check: ${file} does not exist. SOURCE_DIR=\"${SOURCE_DIR}\" is wrong, or the "
            "header moved. A check that examines nothing is a check that lies.")
    endif()
    file(READ "${file}" contents)
    # Whole-line comments only: a symbol read never hides behind one, and
    # the doc comments in these headers name `Describe<Q>::symbol` freely.
    string(REGEX REPLACE "\n[ \t]*//[^\n]*" "\n" code "${contents}")
    if(code MATCHES "Describe<[^>]*>::symbol|quantity_symbol[(]")
        file(RELATIVE_PATH rel "${SOURCE_DIR}" "${file}")
        string(APPEND offenders "\n  ${rel}: ${CMAKE_MATCH_0}")
    endif()
endforeach()

if(offenders)
    message(FATAL_ERROR
        "a symbol is read straight off Describe, bypassing the vocabulary -- write "
        "symbol_of<Q>(vocabulary) instead:${offenders}")
endif()
