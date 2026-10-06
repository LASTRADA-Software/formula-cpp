# SPDX-License-Identifier: Apache-2.0
# User documentation describes the library as it is now. What it did before,
# what changed and in which release belongs in CHANGELOG.md, and nowhere else:
# a reader who meets "no longer compiles" or "new in 0.4.0" in a guide is told
# about a library they never used, and the sentence goes stale with the next
# release. This check refuses that wording.
#
# Scanned: README.md, every docs/**/*.md except docs/superpowers/ (plans, not
# user documentation), and the comment lines of include/**/*.hpp -- a line
# whose first non-blank characters are `//`, `/*` or `*`. Code lines are not
# scanned: an identifier is not prose.
#
# Each scanned line is lower-cased and searched for these phrases, each as
# whole words:
#
#  - no longer, previously, formerly, once meant, was renamed, deprecated,
#    new in;
#  - used to, except after is, are, was, were, be, been or being: "the factor
#    is used to round" is the passive, not history;
#  - before ... existed, within one sentence;
#  - a release reference: since or as of and a version of two or three parts
#    ("since 0.3"), or in, until, before or after and a version of three parts
#    ("in 0.4.0"). A two-part number after "in" is a measurement, "in 2.5 mm",
#    and is not matched.
#
# Every match is reported as `<path>:<line>: <phrase>`, the path relative to
# SOURCE_DIR with forward slashes, sorted by file and then by line.
#
# The allow-list, cmake/docs-current-state-allowlist.txt (or the file ALLOWLIST
# names), holds the lines that match a phrase but describe data rather than
# history -- "the two determinations no longer agree" -- one per line:
#
#   <path relative to SOURCE_DIR>|<the line, exactly, trimmed>|<why it is not history>
#
# Lines starting with `#` and blank lines are ignored; an entry without a
# reason is an error. An absent allow-list is an empty one. An entry allows the
# line it spells, every phrase in it, and nothing else.
#
# A scan that examined no file fails: a check that examines nothing is a check
# that lies.
#
# Nothing here becomes a CMake list: the lines hold `[`, `]` and `;`, each of
# which CMake's list handling mangles. Text is walked with string(FIND) and
# string(SUBSTRING) only, as CheckGuideOutput.cmake does.

# A script run with `cmake -P` starts with every policy at its OLD setting; the
# project's own minimum sets them all (see CheckGuideOutput.cmake for what the
# old CMP0012 did to `while(TRUE)`).
cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "CheckDocsCurrentState.cmake: SOURCE_DIR is not set")
endif()
get_filename_component(SOURCE_DIR "${SOURCE_DIR}" ABSOLUTE)
if(NOT DEFINED ALLOWLIST)
    set(ALLOWLIST "${SOURCE_DIR}/cmake/docs-current-state-allowlist.txt")
endif()

set(files "")
if(EXISTS "${SOURCE_DIR}/README.md")
    list(APPEND files "README.md")
endif()
file(GLOB_RECURSE docs RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/docs/*.md")
list(FILTER docs EXCLUDE REGEX "^docs/superpowers/")
file(GLOB_RECURSE headers RELATIVE "${SOURCE_DIR}" "${SOURCE_DIR}/include/*.hpp")
list(APPEND files ${docs} ${headers})
list(SORT files)
list(LENGTH files scanned)
if(scanned EQUAL 0)
    message(FATAL_ERROR
        "docs current-state check examined no files. SOURCE_DIR=\"${SOURCE_DIR}\" is wrong, or it holds no "
        "README.md, docs/**/*.md or include/**/*.hpp. A check that examines nothing is a check that lies.")
endif()

# The allowed lines, each as `<path>|<line>` followed by a newline, after a
# leading newline: one string, searched with string(FIND).
set(allowed "\n")
if(EXISTS "${ALLOWLIST}")
    file(READ "${ALLOWLIST}" rest)
    string(REPLACE "\r\n" "\n" rest "${rest}")
    set(allowLineNumber 0)
    while(NOT rest STREQUAL "")
        string(FIND "${rest}" "\n" newline)
        if(newline EQUAL -1)
            set(entry "${rest}")
            set(rest "")
        else()
            string(SUBSTRING "${rest}" 0 ${newline} entry)
            math(EXPR next "${newline} + 1")
            string(SUBSTRING "${rest}" ${next} -1 rest)
        endif()
        math(EXPR allowLineNumber "${allowLineNumber} + 1")
        string(STRIP "${entry}" entry)
        if(entry STREQUAL "" OR entry MATCHES "^#")
            continue()
        endif()
        # The line itself may hold `|` (a table row): the path ends at the
        # first `|`, the reason starts after the last.
        string(FIND "${entry}" "|" first)
        string(FIND "${entry}" "|" last REVERSE)
        set(reason "")
        if(NOT first EQUAL last)
            math(EXPR reasonStart "${last} + 1")
            string(SUBSTRING "${entry}" ${reasonStart} -1 reason)
            string(STRIP "${reason}" reason)
        endif()
        if(reason STREQUAL "")
            message(FATAL_ERROR
                "docs current-state check: ${ALLOWLIST}:${allowLineNumber} has no reason; write it as "
                "`<path>|<the line, exactly, trimmed>|<why it is not history>`.")
        endif()
        string(SUBSTRING "${entry}" 0 ${first} allowedPath)
        string(STRIP "${allowedPath}" allowedPath)
        math(EXPR lineStart "${first} + 1")
        math(EXPR lineLength "${last} - ${lineStart}")
        string(SUBSTRING "${entry}" ${lineStart} ${lineLength} allowedLine)
        string(STRIP "${allowedLine}" allowedLine)
        string(APPEND allowed "${allowedPath}|${allowedLine}\n")
    endwhile()
endif()

# The phrases, as one alternation, each bounded by a character that cannot
# continue a word: "new in" is not "new instance", "used to" is not "used
# together". The whole file is searched at once, so nothing may cross a line.
set(phrases "no longer|previously|formerly|once meant|was renamed|deprecated|new in|used to")
string(APPEND phrases "|before [^.\n]* existed")
string(APPEND phrases "|(since|as of) v?[0-9]+\\.[0-9]+(\\.[0-9]+)?")
string(APPEND phrases "|(in|until|before|after) v?[0-9]+\\.[0-9]+\\.[0-9]+")
set(phrasePattern "[^a-z0-9_](${phrases})[^a-z0-9_]")

set(offences "")
set(offenceCount 0)
foreach(relative IN LISTS files)
    file(READ "${SOURCE_DIR}/${relative}" original)
    string(REPLACE "\r\n" "\n" original "${original}")
    # A newline either side, so that a phrase at the start or end of the file
    # has a character to bound it, and so that the newlines before a phrase
    # count its line from one.
    set(original "\n${original}\n")
    string(TOLOWER "${original}" rest)
    set(isHeader FALSE)
    if(relative MATCHES "\\.hpp$")
        set(isHeader TRUE)
    endif()

    # `rest` is the lower-cased text from offset `consumed` of `original`;
    # lower-casing leaves every offset where it was.
    set(consumed 0)
    set(lineNumber 0)
    set(countedTo 0)
    while(rest MATCHES "${phrasePattern}")
        set(phrase "${CMAKE_MATCH_1}")
        string(FIND "${rest}" "${CMAKE_MATCH_0}" at)
        math(EXPR next "${at} + 1")
        string(SUBSTRING "${rest}" ${next} -1 rest)
        math(EXPR consumed "${consumed} + ${next}")
        # `rest` now starts at the phrase, at offset `consumed` of `original`.

        math(EXPR span "${consumed} - ${countedTo}")
        string(SUBSTRING "${original}" ${countedTo} ${span} between)
        string(REGEX REPLACE "[^\n]+" "" between "${between}")
        string(LENGTH "${between}" newlines)
        math(EXPR lineNumber "${lineNumber} + ${newlines}")
        set(countedTo ${consumed})

        string(SUBSTRING "${original}" 0 ${consumed} head)
        string(FIND "${head}" "\n" lineStart REVERSE)
        math(EXPR lineStart "${lineStart} + 1")
        string(FIND "${rest}" "\n" lineEnd)
        math(EXPR lineLength "${consumed} + ${lineEnd} - ${lineStart}")
        string(SUBSTRING "${original}" ${lineStart} ${lineLength} line)

        if(isHeader AND NOT line MATCHES "^[ \t]*(//|/\\*|\\*)")
            continue()
        endif()
        if(phrase STREQUAL "used to")
            math(EXPR column "${consumed} - ${lineStart}")
            string(SUBSTRING "${line}" 0 ${column} before)
            string(TOLOWER "${before}" before)
            if(before MATCHES "(^|[^a-z0-9_])(is|are|was|were|be|been|being)[ \t]+$")
                continue()
            endif()
        endif()
        string(STRIP "${line}" trimmed)
        string(FIND "${allowed}" "\n${relative}|${trimmed}\n" allowedAt)
        if(NOT allowedAt EQUAL -1)
            continue()
        endif()

        string(APPEND offences "\n  ${relative}:${lineNumber}: ${phrase}")
        math(EXPR offenceCount "${offenceCount} + 1")
    endwhile()
endforeach()

if(offenceCount GREATER 0)
    message(FATAL_ERROR
        "docs current-state check: ${offenceCount} history phrases in user documentation:${offences}\n"
        "User documentation describes the library as it is now; say what it does today, or record the change in "
        "CHANGELOG.md. A line that describes data rather than history goes in cmake/docs-current-state-allowlist.txt "
        "with its reason.")
endif()

message(STATUS "docs current-state check: ${scanned} files scanned, no history wording")
