# SPDX-License-Identifier: Apache-2.0
# No tracked in-tree source contains NOLINT. Suppressions belong in
# .clang-tidy, where every exception is reviewable in one place.

file(GLOB_RECURSE sources
    "${SOURCE_DIR}/include/*.hpp"
    "${SOURCE_DIR}/test/*.cpp"
    "${SOURCE_DIR}/test/*.hpp"
    "${SOURCE_DIR}/test/*CMakeLists.txt"
    "${SOURCE_DIR}/examples/*.cpp"
    "${SOURCE_DIR}/examples/*CMakeLists.txt"
    "${SOURCE_DIR}/cmake/*.cmake"
    "${SOURCE_DIR}/cmake/*.cmake.in")

if(EXISTS "${SOURCE_DIR}/CMakeLists.txt")
    list(APPEND sources "${SOURCE_DIR}/CMakeLists.txt")
endif()

# Deliberately the same source set CheckSpdxHeaders.cmake globs: it stays
# inside include/, test/, examples/ and cmake/, so it can never wander into
# out/build/*/_deps, where Catch2's own sources do contain NOLINT.
#
# This script itself is excluded: it has to describe, in its own comments and
# error messages, the very string it is checking for, so left in, it would
# always report itself as an offender. It is the only exclusion; every other
# file the glob above finds -- including every other .cmake file -- is still
# scanned.
list(REMOVE_ITEM sources "${CMAKE_CURRENT_LIST_FILE}")

set(offenders "")
foreach(file ${sources})
    file(READ "${file}" contents)
    if(contents MATCHES "NOLINT")
        file(RELATIVE_PATH rel "${SOURCE_DIR}" "${file}")
        list(APPEND offenders "${rel}")
    endif()
endforeach()

list(LENGTH sources total)

if(total EQUAL 0)
    message(FATAL_ERROR
        "NOLINT check examined no files. SOURCE_DIR=\"${SOURCE_DIR}\" is wrong, or the globs "
        "match nothing. A check that examines nothing is a check that lies.")
endif()

if(offenders)
    list(JOIN offenders "\n  " pretty)
    message(FATAL_ERROR "NOLINT found in tracked source (suppressions belong in .clang-tidy):\n  ${pretty}")
endif()

message(STATUS "no NOLINT in any of ${total} files")
