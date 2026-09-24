// SPDX-License-Identifier: Apache-2.0
/// The shortest complete formula this library can express: two measured inputs,
/// one formula, one traceable result.

#include <formula-cpp/formula.hpp>

#include <cstdio>

/// A quantity is a type. It carries its own symbol, its own description and the
/// unit its values are stated in, and it is distinct from every other quantity
/// even when the unit is the same.
struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct WaterCementRatio: formula::Quantity<WaterCementRatio, "w/c", "ratio of water to cement", formula::unit::One>
{
};

/// The formula is written once, with ordinary operators, and is a compile-time
/// entity: this line builds a type, not a computation.
inline constexpr auto waterCementRatio = formula::var<WaterVolume> / formula::var<CementVolume>;

int main()
{
    auto const batch = formula::environment(formula::Measured<WaterVolume> { formula::Rational { 180 } },
                                            formula::Measured<CementVolume> { formula::Rational { 300 } });

    formula::Outcome<WaterCementRatio> const result = formula::evaluate<WaterCementRatio>(waterCementRatio, batch);

    std::printf("%s = %f (%s)\n",
                formula::Describe<WaterCementRatio>::symbol.data(),
                result.measurement().value().to_double(),
                result.is_value() ? "computed" : "no value");
    return 0;
}
