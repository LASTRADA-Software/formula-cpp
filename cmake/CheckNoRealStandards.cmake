# SPDX-License-Identifier: Apache-2.0
# No real standard is named anywhere in this repository: every citation in
# the sources, tests, documentation and examples is an invented "Example
# Standard". This check scans every tracked file for a real standards body's
# identifier -- the body's name followed by a number, with or without a space,
# a hyphen or a slash between them, and a letter before the number where a
# body numbers its standards that way -- and fails on any match not on the
# allow-list, naming the file, the match and the line it is on.
#
# The files are git's tracked files when SOURCE_DIR is a git work tree, and
# otherwise every file under it outside build and site output, so that a
# source tarball is scanned too. A scan that examined no file fails: a check
# that examines nothing is a check that lies.
#
# The allow-list, cmake/real-standards-allowlist.txt, holds one identifier per
# line exactly as it may appear, then `|` and the reason it is allowed. It is
# not itself scanned, and neither is this file.

set(allowListPath "${SOURCE_DIR}/cmake/real-standards-allowlist.txt")
set(selfPath "cmake/CheckNoRealStandards.cmake")
set(allowListRelative "cmake/real-standards-allowlist.txt")

# The bodies, as an alternation. `CEN/TS` before the bare names, so that it
# is read whole.
set(bodies "CEN/TS|DIN|EN|ISO|IEC|ASTM|AASHTO|BS|NF|ÖNORM|OENORM|SN|UNI|TL|TP|ZTV")
# A body name that is not the tail of a longer word, then an optional
# separator, an optional letter (and space), and a digit.
set(identifier "(^|[^A-Za-z0-9_])(${bodies})[ /_-]?([A-Z] ?)?[0-9]+")

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
    file(STRINGS "${path}" lines REGEX "${identifier}")
    foreach(line IN LISTS lines)
        string(REGEX MATCHALL "${identifier}" matches "${line}")
        foreach(match IN LISTS matches)
            string(REGEX REPLACE "^[^A-Za-z0-9_]" "" match "${match}")
            set(isAllowed FALSE)
            foreach(entry IN LISTS allowed)
                string(FIND "${line}" "${entry}" allowedAt)
                if(NOT allowedAt EQUAL -1)
                    string(FIND "${entry}" "${match}" inEntry)
                    if(NOT inEntry EQUAL -1)
                        set(isAllowed TRUE)
                    endif()
                endif()
            endforeach()
            if(NOT isAllowed)
                string(STRIP "${line}" shownLine)
                list(APPEND found "${relative}: ${match}    (in: ${shownLine})")
            endif()
        endforeach()
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
