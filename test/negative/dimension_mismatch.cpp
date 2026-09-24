// SPDX-License-Identifier: Apache-2.0
// Adding a volume to a mass is meaningless, and the library must say so at
// compile time, in its own words, naming the two dimensions. This must not
// compile.
#include <formula-cpp/dimension.hpp>

namespace dim = formula::dim;

using Checked = formula::RequireSameDimension<dim::Volume, dim::Mass>;

int main()
{
    return Checked::value ? 1 : 0;
}
