// SPDX-License-Identifier: Apache-2.0
/// The shortest complete formula this library can express: two measured inputs,
/// one formula, one traceable result.

#include <formula-cpp/formula.hpp>

#include <print>

/// A quantity is a type. It carries its own symbol, its own description and the
/// unit its values are stated in, and it is distinct from every other quantity
/// even when the unit is the same.
using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", formula::unit::Litre>;
using CementVolume = formula::Quantity<struct CementVolumeTag, "V_c", "cement content", formula::unit::Litre>;
using WaterCementRatio =
    formula::Quantity<struct WaterCementRatioTag, "w/c", "ratio of water to cement", formula::unit::One>;

/// The formula is written once, with ordinary operators, and is a compile-time
/// entity: this line builds a type, not a computation.
inline constexpr auto waterCementRatio = formula::var<WaterVolume> / formula::var<CementVolume>;

int main()
{
    auto const batch = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                            formula::Measured<CementVolume> { formula::Rational { 300 } });

    formula::Outcome<WaterCementRatio> const result = formula::evaluate<WaterCementRatio>(waterCementRatio, batch);

    std::println("{} = {:f} ({})",
                 formula::Describe<WaterCementRatio>::symbol,
                 result.measurement().value().to_double(),
                 result.is_value() ? "computed" : "no value");
    return 0;
}
