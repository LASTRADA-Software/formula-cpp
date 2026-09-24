// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Version macros. Hand-written on purpose: a generated header would live in the
/// build directory and break the library's most valuable fallback property --
/// copy include/ into a tree and it works. `ctest -R hygiene.version` asserts
/// that these agree with project(VERSION) in CMakeLists.txt.

/// The major version -- bumped for a breaking change.
#define FORMULA_VERSION_MAJOR 0
/// The minor version -- bumped for a compatible feature addition.
#define FORMULA_VERSION_MINOR 1
/// The patch version -- bumped for a compatible fix.
#define FORMULA_VERSION_PATCH 0
/// The full version, as a string literal.
#define FORMULA_VERSION_STRING "0.1.0"
