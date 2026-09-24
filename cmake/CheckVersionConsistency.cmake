# SPDX-License-Identifier: Apache-2.0
# version.hpp is hand-written, so something must assert it has not drifted from
# project(VERSION). This is that something.

file(READ "${VERSION_HEADER}" header)

foreach(part MAJOR MINOR PATCH)
    if(NOT header MATCHES "#define FORMULA_VERSION_${part} ([0-9]+)")
        message(FATAL_ERROR "version.hpp: FORMULA_VERSION_${part} not found")
    endif()
    set(header_${part} "${CMAKE_MATCH_1}")
endforeach()

set(fromHeader "${header_MAJOR}.${header_MINOR}.${header_PATCH}")

if(NOT fromHeader STREQUAL PROJECT_VERSION)
    message(FATAL_ERROR
        "version drift: version.hpp says ${fromHeader}, project(VERSION) says ${PROJECT_VERSION}")
endif()

if(NOT header MATCHES "#define FORMULA_VERSION_STRING \"([^\"]+)\"")
    message(FATAL_ERROR "version.hpp: FORMULA_VERSION_STRING not found")
endif()
if(NOT CMAKE_MATCH_1 STREQUAL fromHeader)
    message(FATAL_ERROR
        "version drift: FORMULA_VERSION_STRING is \"${CMAKE_MATCH_1}\", macros say ${fromHeader}")
endif()

message(STATUS "version consistent: ${fromHeader}")
