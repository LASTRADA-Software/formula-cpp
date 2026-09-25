# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.23)

# The version is a committed literal, not derived from `git describe`.
# vcpkg_from_github extracts a tarball with no .git, where a derived version
# silently becomes 0.0.0 and every find_package(formula-cpp 0.1.0) then fails
# with an error the git tag does not explain.
project(formula-cpp
    VERSION 0.1.0
    DESCRIPTION "Declarative, traceable, self-documenting formulas for C++23"
    HOMEPAGE_URL "https://github.com/LASTRADA-Software/formula-cpp"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(FORMULA_BUILD_TESTS    "Build formula-cpp tests"      ${PROJECT_IS_TOP_LEVEL})
option(FORMULA_BUILD_EXAMPLES "Build formula-cpp examples"   ${PROJECT_IS_TOP_LEVEL})
option(FORMULA_INSTALL        "Generate install rules"       ${PROJECT_IS_TOP_LEVEL})
option(FORMULA_PEDANTIC       "Compile with strict warnings" ON)
option(FORMULA_WERROR         "Treat warnings as errors"     OFF)
# Our own tooling (currently: the gallery generator under tools/gallery). A
# consumer embedding this library as a subproject must not build it -- it
# exists to maintain formula-cpp itself, not to be linked by anyone else.
option(FORMULA_TOOLS          "Build formula-cpp's own tools" ${PROJECT_IS_TOP_LEVEL})
# The Doxygen API reference published under /api by .github/workflows/pages.yml.
# Same reasoning as FORMULA_TOOLS: a consumer embedding this library as a
# subproject has no use for formula-cpp's own documentation target.
option(FORMULA_BUILD_DOCS     "Configure the formula-cpp-docs-api target" ${PROJECT_IS_TOP_LEVEL})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(formula-cpp INTERFACE)
add_library(formula-cpp::formula-cpp ALIAS formula-cpp)

target_compile_features(formula-cpp INTERFACE cxx_std_23)

target_sources(formula-cpp INTERFACE
    FILE_SET HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/detail/checked_int.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/detail/fixed_string.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/detail/type_list.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/citation.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/conditional.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/dimension.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/document.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/environment.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/error.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/escape.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/evaluate.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/expression.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/formula.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/function.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/measured.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/outcome.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/predicate.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/quantity.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/render.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/rational.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/rounding.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/rounding_node.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/sink.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/trace.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/trace_render.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/unit.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/formula-cpp/version.hpp")

# These three are workarounds for cl's non-conformance and are REQUIRED to
# compile this library's headers at all, so they are INTERFACE, not PRIVATE:
# a consumer using cl needs them too. $<CXX_COMPILER_ID:MSVC> matches the real
# cl driver only -- clang-cl reports "Clang" and needs none of them, being
# conforming, UTF-8 by default, and correct about __cplusplus already.
target_compile_options(formula-cpp INTERFACE
    "$<$<CXX_COMPILER_ID:MSVC>:/permissive-;/utf-8;/Zc:__cplusplus>")

if(FORMULA_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    install(TARGETS formula-cpp
        EXPORT formula-cpp-targets
        FILE_SET HEADERS DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    install(EXPORT formula-cpp-targets
        FILE formula-cpp-targets.cmake
        NAMESPACE formula-cpp::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/formula-cpp")

    # ARCH_INDEPENDENT: a header-only package must not refuse a consumer whose
    # pointer width differs from whatever configured the install.
    # SameMinorVersion, not SameMajorVersion: before 1.0 the breaking changes
    # land on the minor, so 0.1 must not silently satisfy a request for 0.2.
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/formula-cpp-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion
        ARCH_INDEPENDENT)

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/formula-cpp-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/formula-cpp-config.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/formula-cpp")

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/formula-cpp-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/formula-cpp-config-version.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/formula-cpp")

    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/formula-cpp")
endif()

# Bare enable_testing() at top level, never include(CTest): examples/CMakeLists.txt
# calls add_test() unconditionally, so with -DFORMULA_BUILD_TESTS=OFF
# -DFORMULA_BUILD_EXAMPLES=ON that add_test needs a testing context to
# register into regardless of FORMULA_BUILD_TESTS, or CMake silently drops it.
# Bare enable_testing() only sets a directory property and writes
# CTestTestfile.cmake into the build tree -- it creates no targets, no cache
# variables and no install() rule, so the installed package config stays
# byte-identical. include(CTest) would not be safe as a substitute: it adds a
# BUILD_TESTING cache option and Nightly/Continuous/Experimental targets into
# any consumer's build, so it must not be introduced here.
enable_testing()

if(FORMULA_BUILD_TESTS)
    add_subdirectory(test)
endif()

if(FORMULA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(FORMULA_TOOLS)
    add_subdirectory(tools/gallery)
endif()

if(FORMULA_BUILD_DOCS)
    # QUIET: Doxygen is not installed on every machine that configures this
    # project, and its absence must not fail configuration -- only whoever
    # actually asks for the formula-cpp-docs-api target (in practice, just
    # .github/workflows/pages.yml) needs it present.
    find_package(Doxygen QUIET)

    if(DOXYGEN_FOUND)
        # A local build gets a build-tree default; the workflow overrides this
        # with -DFORMULA_DOXYGEN_OUTPUT_DIR=<mkdocs site>/api so Doxygen writes
        # its HTML straight into the already-built MkDocs site with no copy
        # step. The default must not point inside the MkDocs site's own docs/
        # directory (mkdocs build would then see stray HTML as source input).
        set(FORMULA_DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs-api" CACHE PATH
            "Where the formula-cpp-docs-api target writes the generated HTML")

        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in"
            "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
            @ONLY)

        # Not part of the default build (`cmake --build --preset ...` does not
        # touch this target): none of the four development presets install
        # Doxygen, and the API reference is only ever built by the publishing
        # workflow, on demand, against the version of Doxygen it installs.
        add_custom_target(formula-cpp-docs-api
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${FORMULA_DOXYGEN_OUTPUT_DIR}"
            COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMENT "formula-cpp: generating the Doxygen API reference"
            VERBATIM)
    endif()
endif()
