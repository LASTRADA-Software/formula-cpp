# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.23)

# Deliberately NO project-level CMAKE_CXX_STANDARD: if cxx_std_23 ever stops
# propagating from the exported target, main.cpp must fail to compile.
project(formula-cpp-package-test LANGUAGES CXX)

enable_testing()

# CONFIG, so that a stray Findformula-cpp.cmake module cannot satisfy this.
find_package(formula-cpp CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE formula-cpp::formula-cpp)

add_test(NAME consumer COMMAND consumer)
