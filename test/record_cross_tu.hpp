// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// One overlaid method that reads from another record, one vocabulary and
/// one context, `inline` in a header and used from two translation units:
/// `record_join_tests.cpp` and `record_cross_tu_b.cpp`.
///
/// **What crosses is the method's type.** `record_join_evaluate_in_other_tu`
/// takes the method as a parameter and is defined in the other unit, so its
/// mangled name spells the whole type -- the scope node, its role, its
/// lineage requirement and the rewritten operand. A split in which one unit
/// sees a different type fails to link. Measured by compiling the other unit
/// against a copy of this header with the lineage requirement dropped: the
/// link fails on cl, g++ and clang++. A function merely *returning* the type
/// would not do on g++ and clang++, where a return type is not part of the
/// mangled name.
///
/// **What cannot be caught here:** a split that changes only a value and not
/// a type -- a citation's text, say -- is an ODR violation no link sees. The
/// value itself is compared, so a split that changes the result is caught at
/// run time instead.
///
/// **Everything shared lives in a named namespace**, never an anonymous one,
/// for the reason `method_cross_tu.hpp` gives.

#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <string>
#include <type_traits>

namespace record_cross_tu
{
namespace unit = formula::unit;
using formula::var;

struct Cube
{
};
struct Cylinder
{
};
struct Reference
{
};
struct MaterialBatch
{
};

struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", unit::Megapascal>
{
};
struct Force: formula::Quantity<Force, "F", "applied force", unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", unit::Millimetre>
{
};
struct ShapeFactor: formula::Quantity<ShapeFactor, "k_s", "shape factor", unit::One>
{
};

/// The Cube reports this specimen's strength over the reference's, the
/// reference computed from its own shape factor and measurements, and read
/// only when the two records share a batch. Rounded to 0.1 by the method's
/// own rule.
inline constexpr auto baseMethod = formula::method(
    formula::variants(
        formula::variant<Cube>(var<Strength>
                               / formula::from_record<Reference>(var<ShapeFactor> * var<Force> / (var<EdgeX> * var<EdgeY>),
                                                                 formula::same_lineage<MaterialBatch>())),
        formula::variant<Cylinder>(var<Strength> / var<Strength>)),
    formula::rounding_rule<unit::One, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints(formula::constraint(formula::from_record<Reference>(var<Force>)
                                                 >= formula::constant<unit::Newton>(formula::Rational { 70'000 }),
                                             formula::Verdict { "reference load too low" })));

inline constexpr formula::Citation annex { .reference = "Example Standard 14:2022 NA", .section = "NA.1" };

/// A jurisdiction fixing the shape factor -- which the method reads only
/// inside the scope -- and rounding to 0.01 instead.
inline constexpr auto overlaid = formula::apply(
    formula::overlay(
        formula::with_constant<ShapeFactor>(formula::Rational { 97, 100 }, annex),
        formula::with_rounding<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(annex)),
    baseMethod);

/// The page's words: the force is P, the shape factor k.
inline constexpr auto north = formula::vocabulary(formula::renames<Force>("P"), formula::renames<ShapeFactor>("k"));

/// This record: 6 MPa, 90 000 N over 197 by 103 mm; and the reference:
/// 57 268 N over 139 by 103 mm, its shape factor 1 typed in by hand; both
/// batch 4411. The edges differ, so a read from the wrong record gives a
/// different value.
inline constexpr auto here = formula::environment(formula::Measured<Strength> { formula::Rational { 6 } },
                                                  formula::Measured<Force> { formula::Rational { 90'000 } },
                                                  formula::Measured<EdgeX> { formula::Rational { 197 } },
                                                  formula::Measured<EdgeY> { formula::Rational { 103 } },
                                                  formula::Measured<ShapeFactor> { formula::Rational { 1 } });
inline constexpr auto there =
    formula::environment(formula::Measured<Force> { formula::Rational { 57'268 } },
                         formula::Measured<EdgeX> { formula::Rational { 139 } },
                         formula::Measured<EdgeY> { formula::Rational { 103 } },
                         formula::entered(formula::Measured<ShapeFactor> { formula::Rational { 1 } }));

/// The shared context.
[[nodiscard]] constexpr auto context()
{
    return formula::record_context(
        formula::record<formula::ThisRecord>(
            formula::record_key(formula::sample_id(17), formula::test_id(5)), here, formula::lineage<MaterialBatch>(4411)),
        formula::record<Reference>(
            formula::record_key(formula::sample_id(23), formula::test_id(3)), there, formula::lineage<MaterialBatch>(4411)));
}

/// The overlaid Cube's page, in `north`'s words.
inline std::string page()
{
    return formula::document(std::get<0>(overlaid.variantSet.cases).expression, north).formula;
}

/// The overlaid Cube's trace over `context()`, in `north`'s words.
inline std::string trace()
{
    formula::Trace<> recorded {};
    (void) formula::evaluate_method<Cube>(overlaid, context(), formula::RecordingSink { recorded, north });
    return formula::render_trace(recorded, { .maxSteps = 40 });
}
} // namespace record_cross_tu

/// The Cube of @p method over `context()`, evaluated in
/// `record_cross_tu_b.cpp`. Taking the method is what makes its type part of
/// this function's mangled name; see the file comment.
formula::Evaluated<formula::Rational>
    record_join_evaluate_in_other_tu(std::remove_cvref_t<decltype(record_cross_tu::overlaid)> const& method);
