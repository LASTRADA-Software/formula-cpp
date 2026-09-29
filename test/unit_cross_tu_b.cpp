// SPDX-License-Identifier: Apache-2.0
#include "unit_cross_tu.hpp"

namespace formula_test
{

int consume_litre(TaggedUnit<formula::unit::Litre> tagged)
{
    return tagged.value + 1;
}

// The dimension spelt as the reciprocal of energy times euros, unlike the
// declaration: the same unit, so this defines the declared function.
int consume_tariff_unit(TaggedUnit<formula::Unit {
                            .dimension = formula::dim::Scalar / formula::dim::Energy * formula::base_dimension("EUR"),
                            .magnitudeNumerator = 1,
                            .magnitudeDenominator = 3600000,
                            .symbolText = formula::symbol("EUR/kWh"),
                            .decimals = 4 }> tagged)
{
    return tagged.value + 2;
}

} // namespace formula_test
