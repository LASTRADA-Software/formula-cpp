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
# The REJECT values arrive in REJECT_FILE, which formula_add_negative_test in
# test/CMakeLists.txt writes as `REJECT_COUNT` plus `REJECT_0`, `REJECT_1`, ...
# -- see that function for why not `-D`.
#
# Both EXPECT and REJECT are matched against the COMBINED build output, which
# is more than the compiler's diagnostics: it includes the build tool's own
# lines, such as ninja's echo of a failed command, and so the target name and
# every file path on that command line. A REJECT that happens to occur in a
# path or target name -- "method", say, or "negative" -- fails every build,
# for no reason of the compiler's. Choose text that only a diagnostic can
# contain, such as a template name or a message of the library's own.

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

set(REJECT_COUNT 0)
if(DEFINED REJECT_FILE)
    include("${REJECT_FILE}")
endif()
if(REJECT_COUNT GREATER 0)
    math(EXPR _lastReject "${REJECT_COUNT} - 1")
    foreach(_index RANGE 0 ${_lastReject})
        if(NOT DEFINED REJECT_${_index})
            message(FATAL_ERROR
                "negative test ${TARGET}: ${REJECT_FILE} declares ${REJECT_COUNT} REJECT values but "
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
