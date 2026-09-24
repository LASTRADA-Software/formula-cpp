# SPDX-License-Identifier: Apache-2.0
# Every header under include/ must appear in the install FILE_SET.
#
# The FILE_SET is a hand-written list, deliberately: installing a header is a
# decision about the published surface, and a glob makes that decision silently
# on someone's behalf. The cost of writing it by hand is that it can be
# forgotten -- and it was. Phase 7 added sink.hpp, trace.hpp and
# trace_render.hpp and listed none of them, so `find_package(formula-cpp)`
# produced a package that did not compile at all: evaluate.hpp includes
# sink.hpp, and sink.hpp was not there.
#
# Nothing caught it locally. The four presets and the GCC build compile from
# the source tree, where every header is present whether or not it is
# installed; only the install-and-consume CI leg exercises the staged copy, and
# that runs on push. This check closes the gap where it belongs -- beside the
# other hygiene checks, failing in ctest before anybody pushes.

file(GLOB_RECURSE headerPaths "${SOURCE_DIR}/include/*.hpp")
list(LENGTH headerPaths headerCount)

if(headerCount EQUAL 0)
    message(FATAL_ERROR
        "installed-header check found no headers at all. SOURCE_DIR=\"${SOURCE_DIR}\" is wrong, or the "
        "glob matches nothing. A check that examines nothing is a check that lies.")
endif()

file(READ "${SOURCE_DIR}/CMakeLists.txt" cmakeText)

set(missing "")
set(missingCount 0)
foreach(headerPath IN LISTS headerPaths)
    file(RELATIVE_PATH relative "${SOURCE_DIR}/include" "${headerPath}")
    # Matched against the path as the FILE_SET spells it, so a header listed
    # under a different spelling still counts as missing rather than passing
    # by accident.
    if(NOT cmakeText MATCHES "include/${relative}\"")
        string(APPEND missing "\n  ${relative}")
        math(EXPR missingCount "${missingCount}+1")
    endif()
endforeach()

if(missingCount GREATER 0)
    message(FATAL_ERROR
        "header(s) under include/ are missing from the install FILE_SET in CMakeLists.txt. A consumer "
        "who installs this package and includes the umbrella will not get them, and if anything "
        "already installed includes one, their build fails outright:${missing}")
endif()

message(STATUS "all ${headerCount} headers under include/ appear in the install FILE_SET")
