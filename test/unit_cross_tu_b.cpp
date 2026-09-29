// SPDX-License-Identifier: Apache-2.0
#include "unit_cross_tu.hpp"

namespace formula_test
{

int consume_litre(TaggedUnit<formula::unit::Litre> tagged)
{
    return tagged.value + 1;
}

// The dimension spelt unlike the declaration, and built differently: euros
// squared over euros times energy -- two equal names the merge combines -- then
// raised to the first power, which builds every slot again outside the merge.
// The same unit, so this defines the declared function; a representation that
// differed between the merge's slots and power's would make it an overload
// instead, and the call in unit_tests.cpp would not link.
int consume_tariff_unit(TaggedUnit<formula::Unit {
                            .dimension = formula::power(formula::base_dimension("EUR") * formula::base_dimension("EUR")
                                                            / (formula::base_dimension("EUR") * formula::dim::Energy),
                                                        1),
                            .magnitudeNumerator = 1,
                            .magnitudeDenominator = 3600000,
                            .symbolText = formula::symbol("EUR/kWh"),
                            .decimals = 4 }> tagged)
{
    return tagged.value + 2;
}

} // namespace formula_test
