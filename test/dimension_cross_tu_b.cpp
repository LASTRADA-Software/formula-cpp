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

// Declared as euros divided by energy; defined as the reciprocal of energy
// times euros. The same dimension, so this defines the declared function
// rather than an overload the test's call could never reach.
int consume_tariff(Tagged<formula::dim::Scalar / formula::dim::Energy * formula::base_dimension("EUR")> tagged)
{
    return tagged.value + 3;
}

// Declared as yen times euros; defined in the other order.
int consume_yen_euro(Tagged<formula::base_dimension("EUR") * formula::base_dimension("JPY")> tagged)
{
    return tagged.value + 4;
}

} // namespace formula_test
