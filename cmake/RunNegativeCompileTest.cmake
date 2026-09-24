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
