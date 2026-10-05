// SPDX-License-Identifier: Apache-2.0
#pragma once

/// Cross-translation-unit identity for a Dimension used as a non-type template
/// parameter. The equivalent trick with `decltype([]{})` gives each TU its OWN
/// type and fails at link time with a message that never names the cause. This
/// test exists so that a future change to Dimension cannot reintroduce that
/// failure silently: the functions below are DEFINED in
/// dimension_cross_tu_b.cpp and CALLED from dimension_tests.cpp with an equal
/// but differently spelled dimension. If the two spellings are not the same
/// type, this does not link. The two that carry named base dimensions are also
/// defined with a spelling different from their declaration here, so that the
/// declaration, the definition and the call each work out the named bases'
/// canonical order for themselves.

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
int consume_tariff(Tagged<formula::base_dimension("EUR") / formula::dim::Energy> tagged);
int consume_yen_euro(Tagged<formula::base_dimension("JPY") * formula::base_dimension("EUR")> tagged);

} // namespace formula_test
