# SPDX-License-Identifier: Apache-2.0
# CPM.cmake bootstrap -- downloads CPM on first use and caches it.
# https://github.com/cpm-cmake/CPM.cmake
#
# Used for TEST dependencies only. Nothing fetched here may reach the installed
# package config.

include_guard(GLOBAL)

set(CPM_DOWNLOAD_VERSION 0.40.5)
set(CPM_HASH_SUM "c46b876ae3b9f994b4f05a4c15553e0485636862064f1fcc9d8b4f832086bc5d")
set(CPM_DOWNLOAD_URL
    "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake")

if(CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

# Quoted, and ABSOLUTE: a cache directory under a path containing a space would
# otherwise split here and land the ABSOLUTE keyword in the wrong argument slot.
get_filename_component(CPM_DOWNLOAD_LOCATION "${CPM_DOWNLOAD_LOCATION}" ABSOLUTE)

if(NOT EXISTS "${CPM_DOWNLOAD_LOCATION}")
    # INACTIVITY_TIMEOUT, not TIMEOUT: the bound is on silence, so a slow but
    # progressing download still completes however long it takes.
    file(DOWNLOAD
        "${CPM_DOWNLOAD_URL}"
        "${CPM_DOWNLOAD_LOCATION}"
        EXPECTED_HASH "SHA256=${CPM_HASH_SUM}"
        INACTIVITY_TIMEOUT 120
        STATUS cpmDownloadStatus)
    list(GET cpmDownloadStatus 0 cpmDownloadCode)
    if(NOT cpmDownloadCode EQUAL 0)
        message(FATAL_ERROR
            "could not download the CPM.cmake bootstrap: ${cpmDownloadStatus}\n"
            "  from: ${CPM_DOWNLOAD_URL}\n"
            "  into: ${CPM_DOWNLOAD_LOCATION}\n"
            "Re-run the configure, or point CPM_SOURCE_CACHE at a directory that "
            "already holds the bootstrap.")
    endif()
endif()

# Prefer a system/vcpkg Catch2 over fetching source when one is present.
set(CPM_USE_LOCAL_PACKAGES ON)

include("${CPM_DOWNLOAD_LOCATION}")
