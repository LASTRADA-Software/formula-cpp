# SPDX-License-Identifier: Apache-2.0
#
# Warning flags for the project's OWN targets (tests, examples) -- never for the
# exported INTERFACE, which must not impose a warning policy on consumers.
#
# Keyed on CMAKE_CXX_COMPILER_FRONTEND_VARIANT, not CMAKE_CXX_COMPILER_ID: for
# clang-cl the ID is "Clang" but the frontend is "MSVC", so guarding on the ID
# alone hands clang-cl a GCC-style -Werror it will reject.

include_guard(GLOBAL)

# Clang 22 diagnoses `__COUNTER__` as a C2y extension under -Wpedantic, and
# Catch2's TEST_CASE macro uses it -- so every test file fails under -Werror
# through no fault of ours. Marking Catch2's headers SYSTEM does NOT help:
# measured, the diagnostic is attributed to the expansion site in our file, and
# -isystem leaves it in place.
#
# The flag cannot simply be added, either: an unknown -Wno-* option is itself an
# error under -Werror (-Wunknown-warning-option), which would break the compilers
# that predate the warning. So probe for the POSITIVE spelling -- unknown options
# are diagnosed, while unknown -Wno-* ones are silently tolerated until some other
# warning fires, which makes the negative spelling useless as a feature test.
# Only the GCC-style frontend takes these flags at all; on cl and clang-cl the
# /W4 path never enables -Wpedantic, so the diagnostic does not arise and the
# probe would only add configure-time noise.
if(NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    include(CheckCXXCompilerFlag)
    set(CMAKE_REQUIRED_FLAGS "-Werror")
    check_cxx_compiler_flag("-Wc2y-extensions" FORMULA_HAVE_WC2Y_EXTENSIONS)
    unset(CMAKE_REQUIRED_FLAGS)
endif()

function(formula_apply_warnings target)
    if(NOT FORMULA_PEDANTIC)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /W4)
        if(FORMULA_WERROR)
            target_compile_options(${target} PRIVATE /WX)
        endif()
        # /bigobj: template-heavy test translation units exceed the default
        # section limit long before they look large.
        target_compile_options(${target} PRIVATE /bigobj)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wconversion -Wpedantic -Wshadow -Wnon-virtual-dtor)
        # See the note above the feature test: this silences a diagnostic about
        # Catch2's use of __COUNTER__, not about anything we write.
        if(FORMULA_HAVE_WC2Y_EXTENSIONS)
            target_compile_options(${target} PRIVATE -Wno-c2y-extensions)
        endif()
        if(FORMULA_WERROR)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
