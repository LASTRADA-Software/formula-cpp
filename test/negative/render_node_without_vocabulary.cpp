// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: a render_node of this library takes no vocabulary
//
// A node kind of the library's own, as a header included before render.hpp
// would declare it, whose render_node takes no vocabulary: under any
// vocabulary it would render in the declared symbols.
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/expression.hpp>

#include <string>

namespace formula
{
enum class Dialect;

struct UnthreadedNode: NodeBase
{
    static constexpr Dimension dimension = dim::Scalar;
};

template <Dialect D>
[[nodiscard]] std::string render_node(UnthreadedNode const&)
{
    return "unthreaded";
}
} // namespace formula

#include <formula-cpp/render.hpp>

int main()
{
    return formula::render(formula::UnthreadedNode {}).empty() ? 1 : 0;
}
