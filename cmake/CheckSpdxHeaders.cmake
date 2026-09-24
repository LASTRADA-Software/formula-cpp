# SPDX-License-Identifier: Apache-2.0
# Every source and build file carries an SPDX identifier on its first line.

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

set(offenders "")
foreach(file ${sources})
    file(READ "${file}" contents LIMIT 200)
    if(NOT contents MATCHES "SPDX-License-Identifier: Apache-2\\.0")
        file(RELATIVE_PATH rel "${SOURCE_DIR}" "${file}")
        list(APPEND offenders "${rel}")
    endif()
endforeach()

list(LENGTH sources total)

if(total EQUAL 0)
    message(FATAL_ERROR
        "SPDX check examined no files. SOURCE_DIR=\"${SOURCE_DIR}\" is wrong, or the globs match "
        "nothing. A check that examines nothing is a check that lies.")
endif()

if(offenders)
    list(JOIN offenders "\n  " pretty)
    message(FATAL_ERROR "files without an SPDX identifier:\n  ${pretty}")
endif()

message(STATUS "SPDX identifier present in all ${total} files")
