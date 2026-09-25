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
# REJECT, when supplied, is a second literal substring that must NOT appear.
# With it the harness answers a stricter question: did the build fail for our
# reason, AND not also for a cascade of the compiler's own? EXPECT alone
# cannot tell those apart -- a static_assert of ours can fire and a pile of
# "no such member" errors can follow it, and the expected text is still
# found. That is what makes an ordering guard testable at all: a guard that
# exists to stop a later rule being asked must be pinned by rejecting that
# rule's name, because its absence is the only observable difference.

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
string(REPLACE "‘" "'" REJECT "${REJECT}")
string(REPLACE "’" "'" REJECT "${REJECT}")

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

# Guarded on emptiness, not merely on being defined: string(FIND) of an empty
# needle returns 0 rather than -1, so an unsupplied REJECT would otherwise
# "match" every output and fail every case that never asked for the check.
if(NOT "${REJECT}" STREQUAL "")
    string(FIND "${combined}" "${REJECT}" _rejected)
    if(NOT _rejected EQUAL -1)
        message(FATAL_ERROR
            "negative test ${TARGET}: the build failed for the RIGHT reason, but ALSO reported "
            "something it must not.
"
            "Expected to find: ${EXPECT}
"
            "Must NOT find:    ${REJECT}
"
            "--- compiler output ---
${combined}")
    endif()
endif()
