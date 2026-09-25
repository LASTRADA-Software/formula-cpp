# SPDX-License-Identifier: Apache-2.0
#
# Every compiler diagnostic quoted in docs/ names a header and a line number,
# and a line number goes stale the moment anyone inserts a line above it. This
# checks the ones that can be checked crisply, against the headers as they are
# now.
#
# **Bought with two real defects, one of them a commit old.**
# `docs/lookup-tables.md` quoted a clang diagnostic at `band.hpp(202,19)`,
# captured from a real compile and correct when written. The next commit on the
# same branch added seventeen lines of comment to `band.hpp` -- 17 insertions,
# 0 deletions -- and the quoted block silently became a description of a line
# that no longer exists, while still being labelled verbatim. Nothing in the
# build noticed. The second was found by this script's own first run:
# `docs/expressions.md` had four numbers stale in a block `docs/citations.md`
# quotes correctly, so two guides disagreed with each other about one
# compiler's output.
#
# What is checked, and what deliberately is not:
#
#  1. **A quoted static-assertion error must name a `static_assert(` line.**
#     Both spellings are covered -- clang's `band.hpp(219,19): error: static
#     assertion failed` and MSVC's `expression.hpp(135): error C2338: static
#     assertion failed`. This one is exact rather than a heuristic: the line a
#     static-assertion error points at is the `static_assert` itself, always.
#  2. **A clang source-excerpt line must match the header's own line.** clang
#     prints the offending source under its diagnostic as `  219 |     <text>`;
#     where a quote carries one, the header's line 219 must really be `<text>`.
#     This is the strongest check here -- it verifies the number *means* what
#     the quote says, not merely that something is there.
#  3. **Every other quoted line must be neither blank nor comment-only.** The
#     `note: see reference to ...` cascade points at instantiation sites, and
#     no invariant this script can state says which line one of those must be.
#     But drift lands them on blank lines and in the middle of prose comments,
#     which is exactly how both real instances showed up -- one `(197)` had
#     become a blank line and one `(128)` the middle of a sentence. A weak
#     check that catches the observed failure mode beats no check; it is called
#     out as weak here so nobody mistakes it for a proof.
#
# Only fenced blocks are scanned. Prose mentioning a header and a number in
# passing is not a quoted diagnostic, and judging it as one would make this
# script fire on text it has no business judging.
#
# **Nothing here is ever turned into a CMake list, and that is not a style
# preference -- two drafts of this script were wrong before this one.**
# `file(STRINGS)` reported `band.hpp` as 32 lines where it has 320; replacing
# the newlines by hand and splitting on `;` reported the same 32. The
# collapse begins at the file's first line carrying a double quote, and
# `list(GET)` then returns the whole remainder of the file as one element while
# `list(LENGTH)` cheerfully reports a number. Both drafts would have judged
# line numbers against text that is not on those lines. So this script walks
# the raw text with `string(FIND)` and `string(SUBSTRING)`, which have no list
# semantics to be confused by, and proves the walker works on a known file
# before judging anything with it.

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "CheckDocumentedDiagnostics.cmake: SOURCE_DIR is not set")
endif()

# Line @p wanted (1-based) of @p text, exactly as written, or the sentinel
# `@@FORMULA_NO_SUCH_LINE@@` when the text has no such line. Walks the text; a
# line's content is never routed through CMake's list machinery.
function(formula_line_at text wanted outVar)
    set(remaining "${text}")
    set(index 1)
    # A flag rather than a bare boolean literal in the condition: under CMake
    # 3.28 -- which is what Ubuntu 24.04, and therefore CI, ships -- a literal
    # there needs policy CMP0012 and otherwise misbehaves with a developer
    # warning, while CMake 4.x, which this was first written against, accepts
    # it. That is the same fault as the Doxygen and the MathJax ones this
    # branch already found: verified against a newer tool than the one that
    # runs it, and caught here only because the self-test below refused to
    # read band.hpp. The real exits are the `return()`s.
    set(scanning YES)
    while(scanning)
        string(FIND "${remaining}" "\n" newlinePos)
        if(index EQUAL wanted)
            if(newlinePos EQUAL -1)
                set(${outVar} "${remaining}" PARENT_SCOPE)
            else()
                string(SUBSTRING "${remaining}" 0 ${newlinePos} line)
                string(REGEX REPLACE "\r$" "" line "${line}")
                set(${outVar} "${line}" PARENT_SCOPE)
            endif()
            return()
        endif()
        if(newlinePos EQUAL -1)
            set(${outVar} "@@FORMULA_NO_SUCH_LINE@@" PARENT_SCOPE)
            return()
        endif()
        math(EXPR afterNewline "${newlinePos} + 1")
        string(SUBSTRING "${remaining}" ${afterNewline} -1 remaining)
        math(EXPR index "${index} + 1")
    endwhile()
endfunction()

# How many lines @p text has, counted the same way `formula_line_at` walks it,
# so the two can never disagree about what a line is.
function(formula_count_lines text outVar)
    set(remaining "${text}")
    set(count 0)
    # A flag in the condition for the portability reason `formula_line_at`
    # gives just above.
    set(scanning YES)
    while(scanning)
        math(EXPR count "${count} + 1")
        string(FIND "${remaining}" "\n" newlinePos)
        if(newlinePos EQUAL -1)
            set(${outVar} "${count}" PARENT_SCOPE)
            return()
        endif()
        math(EXPR afterNewline "${newlinePos} + 1")
        string(SUBSTRING "${remaining}" ${afterNewline} -1 remaining)
    endwhile()
endfunction()

# ---- Prove the walker before trusting it -------------------------------------
#
# An instrument that cannot report a difference will report no difference, and
# two earlier drafts of this script shipped exactly that fault. So the reader is
# checked against a file whose first line is known, whose length is known to be
# far more than a list-based reader managed, and -- the case that broke both
# drafts -- against a line that really does contain a `;`.

file(READ "${SOURCE_DIR}/include/formula-cpp/band.hpp" formulaSelfTestText)
formula_count_lines("${formulaSelfTestText}" formulaSelfTestCount)
formula_line_at("${formulaSelfTestText}" 1 formulaSelfTestFirst)

if(formulaSelfTestCount LESS 200)
    message(FATAL_ERROR
        "CheckDocumentedDiagnostics.cmake: the line reader is broken -- it read band.hpp as "
        "${formulaSelfTestCount} lines. Every check below would be judging line numbers against "
        "text that is not on those lines, so this fails instead.")
endif()
if(NOT formulaSelfTestFirst MATCHES "^// SPDX-License-Identifier")
    message(FATAL_ERROR
        "CheckDocumentedDiagnostics.cmake: the line reader is broken -- band.hpp line 1 came "
        "back as '${formulaSelfTestFirst}' rather than its SPDX header.")
endif()

set(formulaSemicolonSeen FALSE)
set(formulaProbeRest "${formulaSelfTestText}")
set(formulaProbe 0)
while(formulaProbe LESS formulaSelfTestCount)
    math(EXPR formulaProbe "${formulaProbe} + 1")
    string(FIND "${formulaProbeRest}" "\n" formulaProbeNewline)
    if(formulaProbeNewline EQUAL -1)
        set(formulaProbeLine "${formulaProbeRest}")
        set(formulaProbeRest "")
    else()
        string(SUBSTRING "${formulaProbeRest}" 0 ${formulaProbeNewline} formulaProbeLine)
        math(EXPR formulaProbeAfter "${formulaProbeNewline} + 1")
        string(SUBSTRING "${formulaProbeRest}" ${formulaProbeAfter} -1 formulaProbeRest)
    endif()
    string(FIND "${formulaProbeLine}" ";" formulaProbeSemicolon)
    if(NOT formulaProbeSemicolon EQUAL -1)
        # Found one -- and cross-check it against the random-access reader, so
        # the walk and `formula_line_at` are proved to agree about which line
        # this is rather than each being trusted on its own.
        formula_line_at("${formulaSelfTestText}" ${formulaProbe} formulaProbeDirect)
        if(NOT formulaProbeDirect STREQUAL formulaProbeLine)
            message(FATAL_ERROR
                "CheckDocumentedDiagnostics.cmake: the two readers disagree about band.hpp line "
                "${formulaProbe} -- the walk says '${formulaProbeLine}' and formula_line_at says "
                "'${formulaProbeDirect}'.")
        endif()
        set(formulaSemicolonSeen TRUE)
        break()
    endif()
endwhile()
if(NOT formulaSemicolonSeen)
    message(FATAL_ERROR
        "CheckDocumentedDiagnostics.cmake: the line reader lost every semicolon in band.hpp, "
        "which is the exact fault that made two earlier drafts of this script read a 320-line "
        "header as 32 lines.")
endif()

# ---- The checks ---------------------------------------------------------------

set(problems "")
set(checked 0)

file(GLOB documents "${SOURCE_DIR}/docs/*.md")
if(documents STREQUAL "")
    message(FATAL_ERROR "CheckDocumentedDiagnostics.cmake: found no documents under ${SOURCE_DIR}/docs")
endif()

foreach(document ${documents})
    file(RELATIVE_PATH documentName "${SOURCE_DIR}" "${document}")
    file(READ "${document}" documentText)
    formula_count_lines("${documentText}" documentLineCount)

    set(insideFence FALSE)
    set(lastHeaderText "")
    set(lastHeaderName "")
    set(lastHeaderLineCount 0)
    set(lineNumber 0)
    set(remainingDocument "${documentText}")

    # A cursor rather than `formula_line_at` per line: calling the random-access
    # reader once per line makes the scan quadratic, and on this repository's
    # docs that was nine seconds of a twenty-second test run. The cursor
    # advances exactly once per iteration, before any `continue()` below, so
    # every early exit in the body is still safe.
    while(lineNumber LESS documentLineCount)
        math(EXPR lineNumber "${lineNumber} + 1")
        string(FIND "${remainingDocument}" "\n" documentNewline)
        if(documentNewline EQUAL -1)
            set(line "${remainingDocument}")
            set(remainingDocument "")
        else()
            string(SUBSTRING "${remainingDocument}" 0 ${documentNewline} line)
            math(EXPR documentAfterNewline "${documentNewline} + 1")
            string(SUBSTRING "${remainingDocument}" ${documentAfterNewline} -1 remainingDocument)
        endif()
        string(REGEX REPLACE "\r$" "" line "${line}")

        # A fence opens and closes with three backticks. Tracked rather than
        # assumed, so prose outside a block is never judged as captured output.
        if(line MATCHES "^ *```")
            if(insideFence)
                set(insideFence FALSE)
            else()
                set(insideFence TRUE)
            endif()
            continue()
        endif()
        if(NOT insideFence)
            continue()
        endif()

        # A clang source excerpt: "  219 |     static_assert(...)". Attributed
        # to whichever header the error line just above named, which is how
        # clang itself attributes it. The caret line under an excerpt carries
        # markers rather than source, and is skipped.
        if(line MATCHES "^ *([0-9]+) \\|(.*)$")
            set(excerptNumber "${CMAKE_MATCH_1}")
            set(excerptText "${CMAKE_MATCH_2}")
            string(STRIP "${excerptText}" excerptText)
            if(NOT lastHeaderName STREQUAL "" AND NOT excerptText STREQUAL ""
               AND NOT excerptText MATCHES "^[\\^~]+$")
                if(excerptNumber GREATER lastHeaderLineCount)
                    list(APPEND problems
                         "${documentName}:${lineNumber} quotes line ${excerptNumber} of ${lastHeaderName}, which has only ${lastHeaderLineCount} lines")
                else()
                    formula_line_at("${lastHeaderText}" ${excerptNumber} headerLine)
                    string(STRIP "${headerLine}" headerLine)
                    math(EXPR checked "${checked} + 1")
                    if(NOT headerLine STREQUAL excerptText)
                        list(APPEND problems
                             "${documentName}:${lineNumber} quotes ${lastHeaderName}:${excerptNumber} as '${excerptText}', but that line is '${headerLine}'")
                    endif()
                endif()
            endif()
            continue()
        endif()

        # A diagnostic location: MSVC's "…expression.hpp(135): error …" or
        # clang's "…band.hpp(219,19): error …". Only this library's own headers
        # are resolvable; a reference to a test or an example file is skipped
        # rather than guessed at.
        if(NOT line MATCHES "([A-Za-z0-9_]+\\.hpp)\\(([0-9]+)[,)]")
            continue()
        endif()
        set(headerName "${CMAKE_MATCH_1}")
        set(quotedNumber "${CMAKE_MATCH_2}")
        set(header "${SOURCE_DIR}/include/formula-cpp/${headerName}")
        if(NOT EXISTS "${header}")
            continue()
        endif()

        file(READ "${header}" lastHeaderText)
        set(lastHeaderName "${headerName}")
        formula_count_lines("${lastHeaderText}" lastHeaderLineCount)

        if(quotedNumber GREATER lastHeaderLineCount)
            list(APPEND problems
                 "${documentName}:${lineNumber} quotes line ${quotedNumber} of ${headerName}, which has only ${lastHeaderLineCount} lines")
            continue()
        endif()

        formula_line_at("${lastHeaderText}" ${quotedNumber} headerLine)
        string(STRIP "${headerLine}" headerLine)
        math(EXPR checked "${checked} + 1")

        if(line MATCHES "static assertion failed")
            if(NOT headerLine MATCHES "^static_assert\\(")
                list(APPEND problems
                     "${documentName}:${lineNumber} quotes a static-assertion error at ${headerName}:${quotedNumber}, but that line is not a static_assert -- it is '${headerLine}'")
            endif()
        elseif(headerLine STREQUAL "")
            list(APPEND problems
                 "${documentName}:${lineNumber} quotes ${headerName}:${quotedNumber}, which is a blank line")
        elseif(headerLine MATCHES "^(///|//|\\*)")
            list(APPEND problems
                 "${documentName}:${lineNumber} quotes ${headerName}:${quotedNumber}, which is a comment line -- '${headerLine}'")
        endif()

    endwhile()
endforeach()

if(checked EQUAL 0)
    message(FATAL_ERROR
        "CheckDocumentedDiagnostics.cmake: found no quoted diagnostic to check at all.\n"
        "Passing while checking nothing is the failure this script exists to prevent, so it "
        "fails instead. Either docs/ no longer quotes a compiler diagnostic, in which case "
        "delete this check, or the patterns above stopped matching the ones it does.")
endif()

list(LENGTH problems problemCount)
if(problemCount GREATER 0)
    string(REPLACE ";" "\n  - " rendered "${problems}")
    message(FATAL_ERROR
        "Documentation quotes a compiler diagnostic that no longer matches the source:\n"
        "  - ${rendered}\n\n"
        "Recapture the block by compiling the negative test it comes from, rather than editing "
        "the numbers by hand -- a hand-edited 'verbatim' block is exactly what this check "
        "exists to catch.")
endif()

message(STATUS "documented diagnostics: ${checked} quoted source line(s) still match their header")
