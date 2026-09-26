# SPDX-License-Identifier: Apache-2.0
#
# Passes only when BOTH hold: the build failed, AND the failure text contains the
# library's own static_assert message. Either half alone is a test that lies --
# "it failed" also passes on a typo, and "the text appeared" also passes on a
# warning.
#
# EXPECT is matched as a LITERAL substring (string(FIND), not MATCHES): a
# static_assert message is free to contain "(", ")", "." and other regex
# metacharacters -- the dimensional-analysis messages already will -- and a
# regex would either mismatch on them or, worse, silently match text it
# should not (a lone "." matches anything). The check exists to catch a wrong
# reason, so it must not be the thing that quietly stops working.
#
# REJECT, when supplied, is one or more further literal substrings, and none of
# them may appear. With them the harness answers a stricter question: did the
# build fail for our reason, AND not also report any of THESE strings? It does
# not answer whether the compiler added a cascade of its own under some other
# name -- only a string named here is looked for. EXPECT alone cannot tell a
# clean refusal from one with a pile of "no such member" errors after it, since
# the expected text is found either way. That is what makes an ordering guard
# testable at all: a guard that exists to stop a later rule being asked must be
# pinned by rejecting that rule's name, because its absence is the only
# observable difference.
#
# EXPECT and the REJECT values arrive in EXPECTATIONS_FILE, which
# formula_add_negative_test in test/CMakeLists.txt writes as `EXPECT`,
# `REJECT_COUNT` and `REJECT_0`, `REJECT_1`, ... -- see that function for why
# not `-D`. The file must define `EXPECT` and `REJECT_COUNT` itself; nothing
# here defaults either, so an empty or half-written file fails the test
# rather than reading as "no REJECT".
#
# Both EXPECT and REJECT are matched against the COMBINED build output, which
# is more than the compiler's diagnostics: it includes the build tool's own
# lines, such as ninja's echo of a failed command, and so the target name and
# every file path on that command line. A REJECT that happens to occur in a
# path or target name -- "method", say, or "negative" -- fails every build,
# for no reason of the compiler's. Choose text that only a diagnostic can
# contain, such as a template name or a message of the library's own.

if(NOT DEFINED EXPECTATIONS_FILE OR NOT EXISTS "${EXPECTATIONS_FILE}")
    message(FATAL_ERROR
        "negative test ${TARGET}: no expectations file was given, or it does not exist: "
        "${EXPECTATIONS_FILE}")
endif()
include("${EXPECTATIONS_FILE}")
if(NOT DEFINED EXPECT OR "${EXPECT}" STREQUAL "" OR NOT DEFINED REJECT_COUNT)
    message(FATAL_ERROR
        "negative test ${TARGET}: ${EXPECTATIONS_FILE} does not define a non-empty EXPECT and a "
        "REJECT_COUNT, so it is empty or half-written; reconfigure to regenerate it.")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --config "${CONFIG}" --target "${TARGET}"
    RESULT_VARIABLE buildResult
    OUTPUT_VARIABLE buildOut
    ERROR_VARIABLE buildErr)

set(combined "${buildOut}${buildErr}")

# Compilers do not agree on how to quote a name. GCC uses typographic quotes
# (U+2018/U+2019) where cl and clang use plain apostrophes, so an expectation
# written with either spelling silently fails on the others -- measured: the
# quantity_wrong_type case passed on cl, clang-cl and AppleClang and failed the
# Linux GCC leg alone, with the build failing correctly and only the text
# comparison disagreeing. Normalise both spellings to the plain one before
# matching, so an expectation is written once and means the same thing
# everywhere.
string(REPLACE "‘" "'" combined "${combined}")
string(REPLACE "’" "'" combined "${combined}")
string(REPLACE "‘" "'" EXPECT "${EXPECT}")
string(REPLACE "’" "'" EXPECT "${EXPECT}")

if(REJECT_COUNT GREATER 0)
    math(EXPR _lastReject "${REJECT_COUNT} - 1")
    foreach(_index RANGE 0 ${_lastReject})
        if(NOT DEFINED REJECT_${_index})
            message(FATAL_ERROR
                "negative test ${TARGET}: ${EXPECTATIONS_FILE} declares ${REJECT_COUNT} REJECT values but "
                "does not define REJECT_${_index}.")
        endif()
        string(REPLACE "‘" "'" REJECT_${_index} "${REJECT_${_index}}")
        string(REPLACE "’" "'" REJECT_${_index} "${REJECT_${_index}}")
    endforeach()
endif()

if(buildResult EQUAL 0)
    message(FATAL_ERROR
        "negative test ${TARGET}: the code COMPILED, and it must not.\n"
        "Expected the compiler to report: ${EXPECT}")
endif()

string(FIND "${combined}" "${EXPECT}" _found)
if(_found EQUAL -1)
    message(FATAL_ERROR
        "negative test ${TARGET}: the build failed, but for the WRONG reason.\n"
        "Expected to find: ${EXPECT}\n"
        "--- compiler output ---\n${combined}")
endif()

# EXPECT_COUNT, when the expectations file sets it, is how many times EXPECT
# must occur: one mistake refused once, rather than once per place it was met.
# formula_add_negative_test leaves it unset under cl -- see that function.
if(DEFINED EXPECT_COUNT)
    set(_rest "${combined}")
    set(_occurrences 0)
    string(LENGTH "${EXPECT}" _expectLength)
    string(FIND "${_rest}" "${EXPECT}" _at)
    while(NOT _at EQUAL -1)
        math(EXPR _occurrences "${_occurrences} + 1")
        math(EXPR _after "${_at} + ${_expectLength}")
        string(SUBSTRING "${_rest}" ${_after} -1 _rest)
        string(FIND "${_rest}" "${EXPECT}" _at)
    endwhile()
    if(NOT _occurrences EQUAL EXPECT_COUNT)
        message(FATAL_ERROR
            "negative test ${TARGET}: the build failed for the RIGHT reason, but reported it "
            "${_occurrences} times, not ${EXPECT_COUNT}.\n"
            "Expected to find: ${EXPECT}\n"
            "--- compiler output ---\n${combined}")
    endif()
endif()

# Every value is checked, and every one that is found is reported, not only
# the first. An empty value would be found in every output -- string(FIND) of
# an empty needle returns 0 -- so formula_add_negative_test refuses one at
# configure time rather than this script skipping it here.
if(REJECT_COUNT GREATER 0)
    set(_rejected "")
    foreach(_index RANGE 0 ${_lastReject})
        string(FIND "${combined}" "${REJECT_${_index}}" _position)
        if(NOT _position EQUAL -1)
            string(APPEND _rejected "Must NOT find:    ${REJECT_${_index}}\n")
        endif()
    endforeach()
    if(NOT _rejected STREQUAL "")
        message(FATAL_ERROR
            "negative test ${TARGET}: the build failed for the RIGHT reason, but ALSO reported "
            "something it must not.\n"
            "Expected to find: ${EXPECT}\n"
            "${_rejected}"
            "--- compiler output ---\n${combined}")
    endif()
endif()
