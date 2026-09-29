// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: these two dimensions are not the same
//
// Euros are not a bare ratio. A base dimension the application declares is a
// dimension of its own, so requiring euros and a pure number to agree is
// refused in the library's own words, with both dimensions in the diagnostic.
// This must not compile.
#include <formula-cpp/dimension.hpp>

inline constexpr formula::Dimension Euro = formula::base_dimension("EUR");

int main()
{
    return formula::RequireSameDimension<Euro, formula::dim::Scalar>::value ? 1 : 0;
}
