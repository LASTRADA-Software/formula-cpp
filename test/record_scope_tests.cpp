// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::var;

struct Reference
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

constexpr auto here = formula::environment(formula::Measured<Force> { formula::Rational { 85'902 } },
                                           formula::Measured<EdgeX> { formula::Rational { 139 } },
                                           formula::Measured<EdgeY> { formula::Rational { 103 } });
constexpr auto there = formula::environment(formula::Measured<Force> { formula::Rational { 57'268 } },
                                            formula::Measured<EdgeX> { formula::Rational { 139 } },
                                            formula::Measured<EdgeY> { formula::Rational { 103 } });

constexpr auto ctx = formula::record_context(
    formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
    formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there));
} // namespace

TEST_CASE("one quantity read here and from the reference gives two values", "[record-scope]")
{
    // 6 MPa here over 4 MPa there is 3/2. Reading both from one record gives 1;
    // swapping them gives 2/3. Each wrong implementation has its own number.
    constexpr auto ratio = var<Force> / formula::from_record<Reference>(var<Force>);
    constexpr auto evaluated = formula::checked_evaluate_si<formula::Rational>(ratio, ctx);
    STATIC_REQUIRE(evaluated.has_value());
    STATIC_REQUIRE(**evaluated == formula::Rational { 3, 2 });
}

TEST_CASE("a scope computes over the other record's measurements", "[record-scope]")
{
    constexpr auto referenceStrength = formula::from_record<Reference>(var<Force> / (var<EdgeX> * var<EdgeY>));
    constexpr auto evaluated = formula::checked_evaluate_si<formula::Rational>(referenceStrength, ctx);
    STATIC_REQUIRE(**evaluated == formula::Rational { 4'000'000 }); // Pa: coherent SI, the reference's
}

TEST_CASE("a scope over a record not yet made is absent, never zero", "[record-scope]")
{
    // The reference test has not been done: the role is bound to an unbound
    // record. Its value is absent, and so is anything computed from it. A
    // scope that read an unbound record as zero would give a present 0 for
    // the scope, and a division by zero, not an absence, for the ratio; one
    // that fell back to this record would give 85 902 N and 1.
    constexpr auto notYetTested = formula::record_context(
        formula::record<formula::ThisRecord>(formula::record_key(formula::sample_id(17), formula::test_id(5)), here),
        formula::Record<Reference, std::remove_cv_t<decltype(there)>>::unbound());
    constexpr auto referenceForce =
        formula::checked_evaluate_si<formula::Rational>(formula::from_record<Reference>(var<Force>), notYetTested);
    constexpr auto ratio = formula::checked_evaluate_si<formula::Rational>(
        var<Force> / formula::from_record<Reference>(var<Force>), notYetTested);
    STATIC_REQUIRE(referenceForce.has_value());
    STATIC_REQUIRE(!referenceForce->has_value());
    STATIC_REQUIRE(ratio.has_value());
    STATIC_REQUIRE(!ratio->has_value());
}
