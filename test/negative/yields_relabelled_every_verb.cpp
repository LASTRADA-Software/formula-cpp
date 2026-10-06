// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this formula names its result quantity with yields
// REJECT: no matching
//
// Every verb that names a result, each asked for a quantity other than the
// one its bound formula names -- a different one at each call, so that each
// call is its own refusal and the case counts ten messages, one per verb.
// Each quantity measures what its formula computes, so nothing but the
// Yields could refuse it, and a verb that stopped refusing lowers the count.
#include <formula-cpp/formula.hpp>
#include <formula-cpp/trace.hpp>

using Rise = formula::Quantity<struct RiseTag, "h", "height gained", formula::unit::Millimetre>;
using Run = formula::Quantity<struct RunTag, "L", "horizontal distance covered", formula::unit::Millimetre>;
using Gradient = formula::Quantity<struct GradientTag, "s", "road gradient", formula::unit::One>;
using Retained = formula::Quantity<struct RetainedTag, "m_r", "mass retained on a screen", formula::unit::Gram>;
using Mass = formula::Quantity<struct MassTag, "m", "mass of a determination", formula::unit::Gram>;

// Ten quantities none of the formulas names: six ratios, two masses of a
// screen and two masses of a determination.
using Efficiency = formula::Quantity<struct EfficiencyTag, "eta", "drivetrain efficiency", formula::unit::One>;
using RollingCoefficient =
    formula::Quantity<struct RollingCoefficientTag, "C_rr", "rolling resistance coefficient", formula::unit::One>;
using Drafting = formula::Quantity<struct DraftingTag, "k_d", "drafting factor", formula::unit::One>;
using MoistureContent = formula::Quantity<struct MoistureTag, "u", "moisture content", formula::unit::One>;
using Shrinkage = formula::Quantity<struct ShrinkageTag, "e_s", "shrinkage strain", formula::unit::One>;
using Saturation = formula::Quantity<struct SaturationTag, "S_r", "degree of saturation", formula::unit::One>;
using Passing = formula::Quantity<struct PassingTag, "m_p", "mass passing a screen", formula::unit::Gram>;
using Sieved = formula::Quantity<struct SievedTag, "m_s", "mass sieved", formula::unit::Gram>;
using Tare = formula::Quantity<struct TareTag, "m_0", "tare of a determination", formula::unit::Gram>;
using DryMass = formula::Quantity<struct DryMassTag, "m_d", "dry mass of a determination", formula::unit::Gram>;

inline constexpr auto inputs = formula::environment(formula::Measured<Rise> { 163 },
                                                    formula::Measured<Run> { 307 },
                                                    formula::measured_series<Retained>(131, 211),
                                                    formula::measured_series<Mass>(41, 43, 47, 53, 59));

int main()
{
    constexpr auto ratio = formula::yields<Gradient>(formula::var<Rise> / formula::var<Run>);
    constexpr auto retained = formula::yields<Retained>(formula::series<Retained, 2>);
    constexpr auto mostExtreme = formula::PerPass::MostExtreme;
    constexpr auto keep = formula::OnLimit::Keep;
    constexpr auto settled =
        formula::yields<Mass>(formula::without_outliers<mostExtreme, keep, formula::AtMost<1>, formula::KeepAtLeast<3>>(
            formula::series<Mass, 5>,
            formula::deviation_from_mean(formula::Rational { 6, 100 } * formula::pass_mean<Mass>),
            formula::Verdict { "repeat the determinations" },
            formula::Citation { .title = "Example Standard" }));

    auto const evaluated = formula::evaluate<Efficiency>(ratio, inputs);
    auto const checked = formula::checked_evaluate<RollingCoefficient>(ratio, inputs);
    auto const explained = formula::explain<Drafting>(ratio, inputs);
    auto const checkedExplained = formula::checked_explain<MoistureContent>(ratio, inputs);
    auto const traced = formula::trace_of<Saturation>(ratio, inputs);
    auto const defined = formula::define<Shrinkage>(ratio);
    auto const series = formula::checked_evaluate_series<Passing>(retained, inputs);
    auto const explainedSeries = formula::explain_series<Sieved>(retained, inputs);
    auto const rejection = formula::checked_evaluate_rejection<Tare>(settled, inputs);
    auto const explainedRejection = formula::explain_rejection<DryMass>(settled, inputs);
    return evaluated.is_value() && checked.has_value() && explained.outcome.is_value() && checkedExplained.has_value()
                   && !traced.empty() && decltype(defined)::valid && series.has_value() && explainedSeries.outcome.has_value()
                   && rejection.has_value() && explainedRejection.outcome.has_value()
               ? 0
               : 1;
}
