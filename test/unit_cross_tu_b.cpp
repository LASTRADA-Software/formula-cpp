// SPDX-License-Identifier: Apache-2.0
#include "unit_cross_tu.hpp"

namespace formula_test
{

int consume_litre(TaggedUnit<formula::unit::Litre> tagged)
{
    return tagged.value + 1;
}

} // namespace formula_test
