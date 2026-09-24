// SPDX-License-Identifier: Apache-2.0
#pragma once

/// Cross-translation-unit identity for a Dimension used as a non-type template
/// parameter. Phase 1 established that the equivalent trick with
/// `decltype([]{})` gives each TU its OWN type and fails at link time with a
/// message that never names the cause. This test exists so that a future change
/// to Dimension cannot reintroduce that failure silently: the functions below
/// are DEFINED in dimension_cross_tu_b.cpp and CALLED from dimension_tests.cpp
/// with an equal but differently spelled dimension. If the two spellings are not
/// the same type, this does not link.

#include <formula-cpp/dimension.hpp>

namespace formula_test
{

template <formula::Dimension D>
struct Tagged
{
    int value {};
};

int consume_volume(Tagged<formula::dim::Volume> tagged);
int consume_root_of_area(Tagged<formula::nth_root(formula::dim::Area, 2)> tagged);

} // namespace formula_test
