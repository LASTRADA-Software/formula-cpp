// SPDX-License-Identifier: Apache-2.0
#pragma once

/// Cross-translation-unit identity for a Unit used as a non-type template
/// parameter. Mirrors dimension_cross_tu.hpp/_b.cpp exactly, for Unit rather
/// than Dimension: the function below is DEFINED in unit_cross_tu_b.cpp and
/// CALLED from unit_tests.cpp with a Litre rebuilt field-by-field rather than
/// named from unit::Litre. If the two spellings are not the same type, this
/// does not link.
///
/// Unit nests Symbol (a 16-byte char array) and Bounds inside the NTTP, a
/// strictly richer mangling than Dimension's, and phase 4's Quantity<Unit> is
/// the consumer that will depend on this holding.

#include <formula-cpp/unit.hpp>

namespace formula_test
{

template <formula::Unit U>
struct TaggedUnit
{
    int value {};
};

int consume_litre(TaggedUnit<formula::unit::Litre> tagged);

} // namespace formula_test
