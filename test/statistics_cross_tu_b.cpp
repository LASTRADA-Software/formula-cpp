// SPDX-License-Identifier: Apache-2.0
#include "statistics_cross_tu.hpp"

statistics_cross_tu::Read statistics_mean_in_other_tu() noexcept
{
    return formula::checked_evaluate<statistics_cross_tu::Specimen>(statistics_cross_tu::meanOfSpecimens,
                                                                    statistics_cross_tu::inputs);
}
