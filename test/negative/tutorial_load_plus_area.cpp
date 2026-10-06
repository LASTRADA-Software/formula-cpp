// SPDX-License-Identifier: Apache-2.0
// Tutorial, chapter 2: a load plus an area measures nothing, so it does not
// compile. docs/tutorial/02-units-and-dimensions.md includes the line below.
#include <formula-cpp/formula.hpp>

using Load = formula::Quantity<struct LoadTag, "F", "maximum load", formula::unit::Kilonewton>;
using Area = formula::Quantity<struct AreaTag, "A_c", "loaded area", formula::unit::SquareMillimetre>;

// --8<-- [start:mistake]
constexpr auto broken = formula::var<Load> + formula::var<Area>;
// --8<-- [end:mistake]

int main()
{
    return 0;
}
