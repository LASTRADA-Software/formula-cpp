# SPDX-License-Identifier: Apache-2.0
# No real standard is named anywhere in this repository: every citation in
# the sources, tests, documentation and examples is an invented "Example
# Standard". This check scans every tracked file for one of the bodies listed
# in `bodies` below, in these forms, and fails on any match not on the
# allow-list, naming the file, the line and the match:
#
#  - the body's name, not the tail of a longer word, and case-sensitive:
#    `ISO` and `ÖNORM`, never `iso`;
#  - then any run of separators -- a space, a tab, a no-break space (U+00A0),
#    a narrow no-break space (U+202F), a thin or figure space (U+2009,
#    U+2007), a non-breaking hyphen (U+2011), an en dash (U+2013), `.`, `/`,
#    `_` or `-` -- or none;
#  - or one line break among them, with the next line's indentation and a
#    comment leader (`//`, `///`, `#`, `*`), so that a body at the end of one
#    line and its number at the start of the next are one identifier, as
#    wrapped prose and wrapped comments put them;
#  - then an optional capital letter (and separators), where a body numbers
#    its standards that way, `ASTM C39`;
#  - then a digit.
#
# Each file is read whole, as bytes, so a body spelt outside ASCII (`ÖNORM`)
# and a separator outside ASCII are matched as written.
#
# The files are git's tracked files when SOURCE_DIR is a git work tree, and
# otherwise every file under it outside build and site output, so that a
# source tarball is scanned too. A scan that examined no file fails: a check
# that examines nothing is a check that lies.
#
# The allow-list, cmake/real-standards-allowlist.txt, holds one identifier per
# line exactly as it may appear, then `|` and the reason it is allowed. Each
# allowed identifier is removed from a file's text before the file is
# scanned, so it allows exactly the text it spells and nothing around it. The
# allow-list is not itself scanned, and neither is this file.

set(allowListPath "${SOURCE_DIR}/cmake/real-standards-allowlist.txt")
set(selfPath "cmake/CheckNoRealStandards.cmake")
set(allowListRelative "cmake/real-standards-allowlist.txt")

# The characters outside ASCII that the rule names, built from their UTF-8
# bytes so that none of them sits invisibly in this file.
string(ASCII 195 150 capitalOWithDiaeresis)
string(ASCII 194 160 noBreakSpace)
string(ASCII 226 128 175 narrowNoBreakSpace)
string(ASCII 226 128 137 thinSpace)
string(ASCII 226 128 135 figureSpace)
string(ASCII 226 128 145 nonBreakingHyphen)
string(ASCII 226 128 147 enDash)
set(unicodeSeparators "${noBreakSpace};${narrowNoBreakSpace};${thinSpace};${figureSpace};${nonBreakingHyphen};${enDash}")

# The bodies, as an alternation. The two-part names first, so that each is
# read whole.
set(bodies "CEN/TS|ISO/TR|ISO/TS|prEN|DIN|EN|ISO|IEC|IEEE|ASTM|AASHTO|BS|NF|${capitalOWithDiaeresis}NORM|OENORM|SN|UNI|TL|TP|ZTV")
# Separators within a line, and one line break with the next line's
# indentation and comment leader. Every separator outside ASCII has been
# replaced by a space before this is applied.
set(separators "[ \t./_-]*(\r?\n[ \t]*(///?|#+|\\*)?[ \t]*)?[ \t./_-]*")
# A body name that is not the tail of a longer word, then separators, an
# optional letter (and separators), and a digit.
set(identifier "(^|[^A-Za-z0-9_])(${bodies})${separators}([A-Z][ \t./_-]*)?[0-9]+")

if(EXISTS "${SOURCE_DIR}/.git")
    execute_process(COMMAND git -c core.quotepath=off -C "${SOURCE_DIR}" ls-files
                    OUTPUT_VARIABLE tracked
                    RESULT_VARIABLE gitResult
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NOT gitResult EQUAL 0)
        message(FATAL_ERROR "no-real-standards check: `git ls-files` failed in ${SOURCE_DIR} (exit ${gitResult}).")
    endif()
    string(REPLACE "\n" ";" files "${tracked}")
else()
    file(GLOB_RECURSE files RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/*")
    list(FILTER files EXCLUDE REGEX "^(out|build[^/]*|site|\\.git|\\.superpowers)/")
endif()

list(REMOVE_ITEM files "${selfPath}" "${allowListRelative}")
list(FILTER files EXCLUDE REGEX "\\.(png|jpg|jpeg|gif|ico|pdf|woff2?|ttf|zip)$")
list(LENGTH files scanned)
if(scanned EQUAL 0)
    message(FATAL_ERROR
        "no-real-standards check examined no files. SOURCE_DIR=\"${SOURCE_DIR}\" is wrong, or it holds nothing. "
        "A check that examines nothing is a check that lies.")
endif()

set(allowed "")
if(EXISTS "${allowListPath}")
    file(STRINGS "${allowListPath}" allowLines)
    foreach(allowLine IN LISTS allowLines)
        if(allowLine MATCHES "^[ \t]*(#|$)")
            continue()
        endif()
        if(NOT allowLine MATCHES "^([^|]+[^| \t])[ \t]*\\|[ \t]*[^ \t].*$")
            message(FATAL_ERROR
                "no-real-standards check: allow-list entry \"${allowLine}\" has no reason; write it as "
                "`<identifier> | <why it is allowed>`.")
        endif()
        list(APPEND allowed "${CMAKE_MATCH_1}")
    endforeach()
endif()

set(found "")
foreach(relative IN LISTS files)
    set(path "${SOURCE_DIR}/${relative}")
    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
        continue()
    endif()
    file(READ "${path}" content)
    foreach(entry IN LISTS allowed)
        string(REPLACE "${entry}" "" content "${content}")
    endforeach()
    foreach(separator IN LISTS unicodeSeparators)
        string(REPLACE "${separator}" " " content "${content}")
    endforeach()
    string(REGEX MATCHALL "${identifier}" matches "${content}")
    foreach(match IN LISTS matches)
        string(REGEX REPLACE "^[^A-Za-z0-9_]" "" match "${match}")
        # The line it starts on, counted from one, for the report.
        string(FIND "${content}" "${match}" matchAt)
        string(SUBSTRING "${content}" 0 ${matchAt} before)
        string(REGEX MATCHALL "\n" newlines "${before}")
        list(LENGTH newlines lineIndex)
        math(EXPR lineNumber "${lineIndex} + 1")
        string(REGEX REPLACE "\r?\n" "<line break>" shownMatch "${match}")
        list(APPEND found "${relative}:${lineNumber}: ${shownMatch}")
    endforeach()
endforeach()

if(found)
    list(JOIN found "\n  " foundText)
    message(FATAL_ERROR
        "no-real-standards check: these files name a real standard, which this repository never does -- cite an "
        "invented \"Example Standard\" instead, or, if the identifier is not a citation at all, add it to "
        "cmake/real-standards-allowlist.txt with its reason:\n  ${foundText}")
endif()

message(STATUS "no-real-standards check: ${scanned} files scanned, no real standard named")
