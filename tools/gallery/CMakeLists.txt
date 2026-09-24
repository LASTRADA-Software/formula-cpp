# SPDX-License-Identifier: Apache-2.0
#
# The gallery generator -- not an example. It does not go through
# examples/CMakeLists.txt's formula_add_example: that function registers an
# example.* ctest whose PASS_REGULAR_EXPRESSION checks the program's stdout,
# and this program's job is the FILE it writes, not anything it prints. But it
# runs unattended in CI exactly like an example does, so it links
# support/fail_without_dialogs.cpp for the same reason every example does --
# see that file's own comment.
include(PedanticCompiler)

add_executable(formula-cpp-gallery main.cpp "${PROJECT_SOURCE_DIR}/support/fail_without_dialogs.cpp")
target_link_libraries(formula-cpp-gallery PRIVATE formula-cpp::formula-cpp)
formula_apply_warnings(formula-cpp-gallery)

# Regenerates the gallery into a temporary file and compares it against the
# checked-in docs/gallery.md. Bare enable_testing() at the top level (see the
# comment above it in the root CMakeLists.txt) means this registers regardless
# of FORMULA_BUILD_TESTS, the same way examples/CMakeLists.txt's tests do.
add_test(NAME gallery.is-current
    COMMAND "${CMAKE_COMMAND}"
            -D "GALLERY_EXE=$<TARGET_FILE:formula-cpp-gallery>"
            -D "EXPECTED_FILE=${PROJECT_SOURCE_DIR}/docs/gallery.md"
            -D "BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
            -P "${PROJECT_SOURCE_DIR}/cmake/CheckGalleryIsCurrent.cmake")
