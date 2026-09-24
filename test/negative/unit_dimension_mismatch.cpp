// SPDX-License-Identifier: Apache-2.0
// Converting a volume into a mass is meaningless. The library must refuse it at
// compile time, in its own words. This must not compile.
#include <formula-cpp/unit.hpp>

namespace unit = formula::unit;

using Checked = formula::RequireSameUnitDimension<unit::Litre, unit::Kilogram>;

int main()
{
    return Checked::value ? 1 : 0;
}
