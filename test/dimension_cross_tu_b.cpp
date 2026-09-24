// SPDX-License-Identifier: Apache-2.0
#include "dimension_cross_tu.hpp"

namespace formula_test
{

int consume_volume(Tagged<formula::dim::Volume> tagged)
{
    return tagged.value + 1;
}

// Spelled as a COMPUTED dimension here, called as a literal one in the test.
int consume_root_of_area(Tagged<formula::nth_root(formula::dim::Area, 2)> tagged)
{
    return tagged.value + 2;
}

} // namespace formula_test
