// SPDX-License-Identifier: Apache-2.0
#pragma once

// The interface between `consumer_globals_tests.cpp`, which declares a
// consumer's ordinary globals and uses every public header under them, and
// `consumer_globals_run_tests.cpp`, which checks what it computed. Two
// translation units because Catch2's own templates, instantiated by its
// assertion macros, declare parameters named `lhs` and `rhs`: under the
// same globals they would be reported too, and they are not this library's
// to rename.

#include <vector>

/// What the probe computed: one entry per property it checks, each true
/// when the entry point it used answered as expected.
struct ConsumerGlobalsProbe
{
    std::vector<bool> checks {};
};

/// Runs every main entry point of the library in a translation unit whose
/// globals are named like common locals.
[[nodiscard]] ConsumerGlobalsProbe probe_consumer_globals();
