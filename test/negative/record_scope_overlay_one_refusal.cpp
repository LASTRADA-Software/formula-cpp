// SPDX-License-Identifier: Apache-2.0
// EXPECT: formula: this overlay overrides a constant in an expression holding a node kind it cannot see inside
// REJECT: formula: this overlay overrides a quantity that no variant or constraint of the method uses
//
// `with_constant<ShapeFactor>` over a method whose only uses of the shape
// factor sit inside a consumer's own node kind, and that node inside three
// distinct scope types: two roles, and one role with a lineage requirement.
// The overlay sees through every scope, so the one mistake -- the node it
// cannot see into -- is refused once, not once per scope type around it.
// Before the overlay saw through scopes, each scope
// type drew a refusal of its own. `EXPECT_COUNT 1` (test/CMakeLists.txt)
// pins the one message on g++, clang++ and clang-cl; not on cl, which stops
// at the first failed static_assert of the chain and so reads 1 either way.
//
// This must not compile.
#include <formula-cpp/constraint.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/record.hpp>

#include <optional>
#include <tuple>

namespace
{
struct Cube
{
};
struct Reference
{
};
struct PriorTest
{
};
struct MaterialBatch
{
};

struct Force: formula::Quantity<Force, "F", "applied force", formula::unit::Newton>
{
};
struct ShapeFactor: formula::Quantity<ShapeFactor, "k_s", "shape factor", formula::unit::One>
{
};

// A consumer's own node kind, which evaluates to 1 whatever its operand
// holds. The overlay has no way to see the operand inside it.
template <typename Operand>
struct OpaqueNode: formula::NodeBase
{
    Operand operand {};

    static constexpr formula::Dimension dimension = formula::dim::Scalar;
};

template <typename Rep, typename Operand, typename Env>
[[nodiscard]] constexpr formula::Evaluated<Rep> checked_evaluate_si(OpaqueNode<Operand> const&, Env const&) noexcept
{
    return formula::Evaluated<Rep> { std::optional<Rep> { Rep { 1 } } };
}

using formula::var;
using Hidden = OpaqueNode<formula::VarNode<ShapeFactor>>;

inline constexpr auto m = formula::method(
    formula::variants(formula::variant<Cube>(
        formula::from_record<Reference>(Hidden {} * var<Force>) / formula::from_record<PriorTest>(Hidden {} * var<Force>)
        * formula::from_record<Reference>(Hidden {} * var<Force>, formula::same_lineage<MaterialBatch>()))),
    formula::rounding_rule<formula::unit::Newton, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
} // namespace

int main()
{
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::with_constant<ShapeFactor>(formula::Rational { 97, 100 }, formula::Citation { .reference = "Example Standard 14:2022 NA" })), m);
    return std::tuple_size_v<decltype(overlaid.variantSet.cases)> == 0 ? 1 : 0;
}
