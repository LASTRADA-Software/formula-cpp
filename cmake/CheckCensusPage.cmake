# SPDX-License-Identifier: Apache-2.0
# docs/numeric-headroom.md's figures are generated, never typed: each table
# sits between `<!-- census:NAME -->` and `<!-- /census:NAME -->` and is
# rebuilt here from what the census programs print --
#
#  - statistics, resolution, cylinder, least-squares: formula-cpp-census-tests' own
#    `@census:NAME:` lines, in order;
#  - examples: the `overflow census:` line each census twin of an example,
#    and of the gallery generator, prints when it exits;
#  - exact: what tools/census/exact_sizes.py prints, when a Python
#    interpreter was found at configure time (without one that block is left
#    as it is, and said so).
#
# Without UPDATE the page must already be what regenerating it gives, or this
# fails -- the way gallery.is-current holds docs/gallery.md. One exception:
# the examples table is cl's (CENSUS_COMPILER MSVC). clang and gcc evaluate a
# `const` local's constant initialiser at compile time, where cl runs it, so
# they can see fewer integers: built with them, no row may leave less
# headroom than the page's, and the page's table is kept. With UPDATE=ON
# (the formula-cpp-census-page target) the page is rewritten in place.
#
# Nothing here becomes a CMake list of page text: the rows hold `|`, and a `;`
# or `[` would be mangled (CheckGuideOutput.cmake says what that has cost).
# The rows are joined with string(APPEND) only; CENSUS_PROGRAMS, the one list,
# is `name=path` pairs joined with `|`, which neither a name nor a path holds.
cmake_minimum_required(VERSION 3.23)

foreach(variable CENSUS_EXE CENSUS_PROGRAMS GALLERY_EXE PAGE BINARY_DIR CENSUS_COMPILER)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "CheckCensusPage.cmake: ${variable} is not set")
    endif()
endforeach()

# ---- The census program's own tables ------------------------------------------
execute_process(
    COMMAND "${CENSUS_EXE}"
    RESULT_VARIABLE censusResult
    OUTPUT_VARIABLE censusOut
    ERROR_VARIABLE censusErr)
if(NOT censusResult EQUAL 0)
    message(FATAL_ERROR
        "docs.numeric-headroom: the census program failed (exit ${censusResult}).\n"
        "--- stdout ---\n${censusOut}\n--- stderr ---\n${censusErr}")
endif()
string(REPLACE "\r\n" "\n" censusOut "${censusOut}")

function(census_block name outVariable)
    set(block "")
    set(rest "${censusOut}")
    set(prefix "@census:${name}:")
    string(LENGTH "${prefix}" prefixLength)
    while(TRUE)
        string(FIND "${rest}" "${prefix}" at)
        if(at EQUAL -1)
            break()
        endif()
        math(EXPR from "${at} + ${prefixLength}")
        string(SUBSTRING "${rest}" ${from} -1 rest)
        string(FIND "${rest}" "\n" lineEnd)
        string(SUBSTRING "${rest}" 0 ${lineEnd} line)
        string(APPEND block "${line}\n")
    endwhile()
    if(block STREQUAL "")
        message(FATAL_ERROR "docs.numeric-headroom: the census program printed no ${name} table")
    endif()
    set(${outVariable} "${block}" PARENT_SCOPE)
endfunction()

census_block(statistics statisticsTable)
census_block(resolution resolutionTable)
census_block(cylinder cylinderTable)
census_block(least-squares leastSquaresTable)

# ---- The census twins ---------------------------------------------------------------
set(examplesTable "| program | numerator bits | denominator bits | intermediate bits | headroom |\n|---|---|---|---|---|\n")
set(censusLine "overflow census: numerator ([0-9]+) bits, denominator ([0-9]+) bits, intermediate ([0-9]+) bits, unsigned ([0-9]+) bits; headroom ([0-9]+) of 63")

function(twin_row label program outVariable)
    execute_process(
        COMMAND ${program} ${ARGN}
        RESULT_VARIABLE twinResult
        OUTPUT_VARIABLE twinOut
        ERROR_VARIABLE twinErr)
    if(NOT twinResult EQUAL 0)
        message(FATAL_ERROR "docs.numeric-headroom: ${label} failed (exit ${twinResult}).\n${twinOut}\n${twinErr}")
    endif()
    if(NOT twinOut MATCHES "${censusLine}")
        message(FATAL_ERROR "docs.numeric-headroom: ${label} printed no overflow census line.\n${twinOut}")
    endif()
    set(${outVariable}
        "| ${label} | ${CMAKE_MATCH_1} | ${CMAKE_MATCH_2} | ${CMAKE_MATCH_3} | ${CMAKE_MATCH_5} |\n"
        PARENT_SCOPE)
endfunction()

string(REPLACE "|" ";" programs "${CENSUS_PROGRAMS}")
foreach(entry IN LISTS programs)
    string(FIND "${entry}" "=" split)
    string(SUBSTRING "${entry}" 0 ${split} name)
    math(EXPR pathFrom "${split} + 1")
    string(SUBSTRING "${entry}" ${pathFrom} -1 path)
    twin_row("example `${name}`" "${path}" row)
    string(APPEND examplesTable "${row}")
endforeach()
twin_row("the gallery generator" "${GALLERY_EXE}" row "${BINARY_DIR}/census-gallery.md")
string(APPEND examplesTable "${row}")

# ---- The exact companion --------------------------------------------------------------
set(haveExact FALSE)
if(DEFINED PYTHON AND DEFINED EXACT_SCRIPT)
    execute_process(
        COMMAND "${PYTHON}" "${EXACT_SCRIPT}"
        RESULT_VARIABLE exactResult
        OUTPUT_VARIABLE exactOut
        ERROR_VARIABLE exactErr)
    if(NOT exactResult EQUAL 0)
        message(FATAL_ERROR "docs.numeric-headroom: ${EXACT_SCRIPT} failed (exit ${exactResult}).\n${exactErr}")
    endif()
    string(REPLACE "\r\n" "\n" exactOut "${exactOut}")
    set(exactTable "```text\n${exactOut}```\n")
    set(haveExact TRUE)
else()
    message(STATUS "docs.numeric-headroom: no Python interpreter; the exact block is not checked")
endif()

# ---- The page ----------------------------------------------------------------------------
file(READ "${PAGE}" page)
string(REPLACE "\r\n" "\n" page "${page}")
set(regenerated "${page}")

function(replace_block name content)
    set(open "<!-- census:${name} -->\n")
    set(close "<!-- /census:${name} -->")
    string(FIND "${regenerated}" "${open}" openAt)
    string(FIND "${regenerated}" "${close}" closeAt)
    if(openAt EQUAL -1 OR closeAt EQUAL -1 OR closeAt LESS openAt)
        message(FATAL_ERROR "docs.numeric-headroom: ${PAGE} has no census:${name} block")
    endif()
    string(LENGTH "${open}" openLength)
    math(EXPR innerFrom "${openAt} + ${openLength}")
    string(SUBSTRING "${regenerated}" 0 ${innerFrom} before)
    string(SUBSTRING "${regenerated}" ${closeAt} -1 after)
    set(regenerated "${before}\n${content}\n${after}" PARENT_SCOPE)
endfunction()

# ---- The examples table under another compiler than cl -----------------------------
if(NOT CENSUS_COMPILER STREQUAL "MSVC")
    set(rowPattern "(\\| [^\n]*) \\| [0-9]+ \\| [0-9]+ \\| [0-9]+ \\| ([0-9]+) \\|\n")
    string(REGEX MATCHALL "${rowPattern}" measuredRows "${examplesTable}")
    foreach(measuredRow IN LISTS measuredRows)
        string(REGEX MATCH "${rowPattern}" matched "${measuredRow}")
        set(label "${CMAKE_MATCH_1}")
        set(measuredHeadroom "${CMAKE_MATCH_2}")
        string(FIND "${page}" "${label} |" labelAt)
        if(labelAt EQUAL -1)
            message(FATAL_ERROR "docs.numeric-headroom: the page's examples table has no row `${label}`")
        endif()
        string(SUBSTRING "${page}" ${labelAt} -1 fromLabel)
        if(NOT fromLabel MATCHES "^[^\n]* \\| ([0-9]+) \\|\n")
            message(FATAL_ERROR "docs.numeric-headroom: the page's row `${label}` is malformed")
        endif()
        if(measuredHeadroom LESS CMAKE_MATCH_1)
            message(FATAL_ERROR
                "docs.numeric-headroom: `${label}` leaves ${measuredHeadroom} bits of headroom under "
                "${CENSUS_COMPILER}, fewer than the page's ${CMAKE_MATCH_1}, measured with cl")
        endif()
    endforeach()
    # Keep the page's table. The block holds a blank line, the table, a blank line.
    set(examplesOpenText "<!-- census:examples -->\n\n")
    string(FIND "${page}" "${examplesOpenText}" examplesOpen)
    string(FIND "${page}" "\n<!-- /census:examples -->" examplesClose)
    if(examplesOpen EQUAL -1 OR examplesClose EQUAL -1)
        message(FATAL_ERROR "docs.numeric-headroom: ${PAGE} has no census:examples block")
    endif()
    string(LENGTH "${examplesOpenText}" openLength)
    math(EXPR tableFrom "${examplesOpen} + ${openLength}")
    math(EXPR tableLength "${examplesClose} - ${tableFrom}")
    string(SUBSTRING "${page}" ${tableFrom} ${tableLength} examplesTable)
    message(STATUS "docs.numeric-headroom: the examples table is cl's; under ${CENSUS_COMPILER} no row leaves less headroom")
endif()

replace_block(statistics "${statisticsTable}")
replace_block(resolution "${resolutionTable}")
replace_block(cylinder "${cylinderTable}")
replace_block(least-squares "${leastSquaresTable}")
replace_block(examples "${examplesTable}")
if(haveExact)
    replace_block(exact "${exactTable}")
endif()

if(UPDATE)
    file(WRITE "${PAGE}" "${regenerated}")
    message(STATUS "docs/numeric-headroom.md regenerated")
elseif(NOT regenerated STREQUAL page)
    file(WRITE "${BINARY_DIR}/numeric-headroom-regenerated.md" "${regenerated}")
    message(FATAL_ERROR
        "docs/numeric-headroom.md is stale: its census tables are not what the census programs print now.\n"
        "Regenerate it and commit the result: build the target formula-cpp-census-page.\n"
        "The regenerated page is at ${BINARY_DIR}/numeric-headroom-regenerated.md")
else()
    message(STATUS "docs/numeric-headroom.md is current")
endif()
