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

# The README and the tutorial tell a consumer which release to fetch with CPM.
# A version bump that leaves either behind sends every new user to an old
# release, so each must name exactly this version, at least once, and never
# another.
foreach(document IN ITEMS README TUTORIAL)
    if(NOT DEFINED ${document})
        message(FATAL_ERROR "CheckVersionConsistency.cmake: ${document} is not set")
    endif()
    file(READ "${${document}}" text)
    string(REGEX MATCHALL "gh:LASTRADA-Software/formula-cpp@[0-9]+\\.[0-9]+\\.[0-9]+" tags "${text}")
    if(NOT tags)
        message(FATAL_ERROR "version drift: ${${document}} names no CPM tag gh:LASTRADA-Software/formula-cpp@<version>")
    endif()
    foreach(tag IN LISTS tags)
        string(REGEX REPLACE "^.*@" "" tagVersion "${tag}")
        if(NOT tagVersion STREQUAL fromHeader)
            message(FATAL_ERROR "version drift: ${${document}} fetches ${tagVersion} with CPM, the project is ${fromHeader}")
        endif()
    endforeach()
endforeach()

message(STATUS "version consistent: ${fromHeader}")
