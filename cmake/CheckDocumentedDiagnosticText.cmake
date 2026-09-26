# SPDX-License-Identifier: Apache-2.0
#
# Every library refusal quoted in docs/ must still be the library's refusal,
# word for word. `CheckDocumentedDiagnostics.cmake` checks the header and line
# a quoted diagnostic names; it never reads the message, and a quote that names
# no location -- g++'s plain `static assertion failed: formula: ...` -- it does
# not examine at all. That is how `docs/methods-and-overlays.md` came to quote
# the overlay repeat rule without the words "two replacements of the
# constraints" for a whole rebase after the header had gained them, with every
# test passing.
#
# So this checks the text. In a fenced block, a line that reports a static
# assertion failure and carries a `formula: ` message must quote a message some
# header states, verbatim. The three spellings in use are covered:
#
#     static assertion failed: formula: <message>                  (g++)
#     ...static assertion failed due to requirement '...': formula: <message>  (clang)
#     ...error C2338: static assertion failed: 'formula: <message>'  (MSVC)
#
# The quote runs from `formula: ` to the end of the line; MSVC's closing `'` is
# dropped when its opening one stands before `formula:`. A quote must be the
# whole message or a prefix of it: a message cut short with `...` is not a
# verbatim quote, and is reported as one that does not match.
#
# Not checked: a `formula: ` message outside a static-assertion line -- a code
# comment paraphrasing one, or program output that happens to contain the
# words. Those are not compiler output, and judging them as such would make
# this fire on text it has no business judging.
#
# The headers' messages are string literals split across lines, so the headers
# are read with adjacent literals joined (`"a " "b"` reads `a b`) and `\"` and
# `\\` unescaped. Joining is by pattern, not by a C++ lexer, and can join two
# literals that are not one message; that can only make a stale quote match
# text nobody wrote as one message, which a quote of a whole sentence does not
# plausibly do.
#
# Nothing here becomes a CMake list, for the reason
# `CheckDocumentedDiagnostics.cmake` gives: the texts hold `;`, `[` and `]`.
# `cmake_minimum_required` for the reason `CheckGuideOutput.cmake` gives.
cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "CheckDocumentedDiagnosticText.cmake: SOURCE_DIR is not set")
endif()

file(GLOB_RECURSE headers "${SOURCE_DIR}/include/formula-cpp/*.hpp")
if(headers STREQUAL "")
    message(FATAL_ERROR "CheckDocumentedDiagnosticText.cmake: found no header under ${SOURCE_DIR}/include")
endif()
set(library "")
foreach(header IN LISTS headers)
    file(READ "${header}" headerText)
    string(APPEND library "${headerText}\n")
endforeach()
string(REPLACE "\r\n" "\n" library "${library}")
string(REPLACE "\\\\" "@FORMULA_BACKSLASH@" library "${library}")
string(REPLACE "\\\"" "@FORMULA_QUOTE@" library "${library}")
string(REGEX REPLACE "\"[ \t\n]*\"" "" library "${library}")
string(REPLACE "@FORMULA_QUOTE@" "\"" library "${library}")
string(REPLACE "@FORMULA_BACKSLASH@" "\\" library "${library}")

# Prove the join on a message known to span several literals, before trusting
# it: an instrument that cannot find a real message finds every quote stale,
# and one reading the wrong text finds none.
string(FIND "${library}"
       "formula: this overlay lists the same operation twice; two constants or definitions of one quantity"
       selfTest)
if(selfTest EQUAL -1)
    message(FATAL_ERROR
        "CheckDocumentedDiagnosticText.cmake: the header reader is broken -- it cannot find the overlay "
        "repeat rule's message, which overlay.hpp states across several string literals.")
endif()

set(marker "static assertion failed")
set(opening "formula: ")
set(checked 0)
set(problems "")

file(GLOB documents "${SOURCE_DIR}/docs/*.md")
foreach(document IN LISTS documents)
    file(RELATIVE_PATH documentName "${SOURCE_DIR}" "${document}")
    file(READ "${document}" rest)
    string(REPLACE "\r\n" "\n" rest "${rest}")
    string(APPEND rest "\n")
    set(insideFence FALSE)
    set(lineNumber 0)
    while(NOT rest STREQUAL "")
        string(FIND "${rest}" "\n" newline)
        string(SUBSTRING "${rest}" 0 ${newline} line)
        math(EXPR afterNewline "${newline} + 1")
        string(SUBSTRING "${rest}" ${afterNewline} -1 rest)
        math(EXPR lineNumber "${lineNumber} + 1")

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
        string(FIND "${line}" "${marker}" markerAt)
        string(FIND "${line}" "${opening}" quoteAt)
        if(markerAt EQUAL -1 OR quoteAt EQUAL -1)
            continue()
        endif()

        string(SUBSTRING "${line}" ${quoteAt} -1 quote)
        if(quoteAt GREATER 0)
            math(EXPR beforeQuote "${quoteAt} - 1")
            string(SUBSTRING "${line}" ${beforeQuote} 1 openingMark)
            if(openingMark STREQUAL "'" AND quote MATCHES "'$")
                string(REGEX REPLACE "'$" "" quote "${quote}")
            endif()
        endif()

        math(EXPR checked "${checked} + 1")
        string(FIND "${library}" "${quote}" found)
        if(found EQUAL -1)
            string(SUBSTRING "${quote}" 0 100 shown)
            string(APPEND problems "\n  ${documentName}:${lineNumber}: ${shown}...")
        endif()
    endwhile()
endforeach()

if(checked EQUAL 0)
    message(FATAL_ERROR
        "CheckDocumentedDiagnosticText.cmake: found no quoted library refusal in docs/ at all. A check "
        "that examines nothing is a check that lies: either docs/ no longer quotes one, in which case "
        "delete this check, or the patterns above stopped matching the ones it does.")
endif()

if(NOT problems STREQUAL "")
    message(FATAL_ERROR
        "docs/ quotes a library refusal that no header states, word for word:${problems}\n"
        "Recapture the block from the negative test it comes from, rather than editing it by hand.")
endif()

message(STATUS "documented diagnostic text: ${checked} quoted refusal(s), each stated by a header verbatim")
