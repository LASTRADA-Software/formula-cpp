// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/outcome.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct Mass: formula::Quantity<Mass, "m", "specimen mass", formula::unit::Kilogram>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

} // namespace

TEST_CASE("outcome: a present measurement is a value", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome =
        formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(3) }, formula::ValueSource::Derived);

    STATIC_REQUIRE(outcome.kind() == formula::OutcomeKind::Value);
    STATIC_REQUIRE(outcome.is_value());
    STATIC_REQUIRE_FALSE(outcome.is_empty());
    STATIC_REQUIRE_FALSE(outcome.is_overridden());
    STATIC_REQUIRE(outcome.measurement().value() == rat(3));
    STATIC_REQUIRE(outcome.source() == formula::ValueSource::Derived);
}

TEST_CASE("outcome: an absent measurement is empty, not a value", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome =
        formula::Outcome<Mass>::value(formula::Measured<Mass>::absent(), formula::ValueSource::Derived);

    STATIC_REQUIRE(outcome.kind() == formula::OutcomeKind::Empty);
    STATIC_REQUIRE(outcome.is_empty());
    STATIC_REQUIRE_FALSE(outcome.is_value());
    STATIC_REQUIRE(outcome.measurement().is_absent());
}

TEST_CASE("outcome: a manually entered value is a value and is overridden", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome =
        formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(7, 2) }, formula::ValueSource::ManuallyEntered);

    STATIC_REQUIRE(outcome.is_value());
    STATIC_REQUIRE(outcome.is_overridden());
    STATIC_REQUIRE(outcome.source() == formula::ValueSource::ManuallyEntered);
    STATIC_REQUIRE(outcome.measurement().value() == rat(7, 2));
}

TEST_CASE("outcome: a verdict carries its label and no measurement", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome = formula::Outcome<Mass>::verdict(formula::Verdict { "repeat the test" });

    STATIC_REQUIRE(outcome.kind() == formula::OutcomeKind::Verdict);
    STATIC_REQUIRE_FALSE(outcome.is_value());
    STATIC_REQUIRE(outcome.verdict_label() == std::string_view { "repeat the test" });
    STATIC_REQUIRE(outcome.measurement().is_absent());
}

TEST_CASE("outcome: an invalid result carries its reason", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome =
        formula::Outcome<Mass>::invalid(formula::InvalidReason { "outlier rejection removed every specimen" });

    STATIC_REQUIRE(outcome.kind() == formula::OutcomeKind::Invalid);
    STATIC_REQUIRE_FALSE(outcome.is_value());
    STATIC_REQUIRE(outcome.reason_label() == std::string_view { "outlier rejection removed every specimen" });
}

TEST_CASE("outcome: two outcomes of the same kind and payload compare equal", "[outcome]")
{
    constexpr formula::Outcome<Mass> left =
        formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(3) }, formula::ValueSource::Measured);
    constexpr formula::Outcome<Mass> right =
        formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(3) }, formula::ValueSource::Measured);
    constexpr formula::Outcome<Mass> other =
        formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(3) }, formula::ValueSource::Derived);

    STATIC_REQUIRE(left == right);
    STATIC_REQUIRE(left != other);
}

TEST_CASE("outcome: a verdict outcome is not overridden", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome = formula::Outcome<Mass>::verdict(formula::Verdict { "specimen rejected" });

    STATIC_REQUIRE_FALSE(outcome.is_overridden());
}

TEST_CASE("outcome: an absent manually entered value is not an override", "[outcome]")
{
    // Nothing was entered, so there is nothing to override with. Only the
    // `is_value()` half of `is_overridden` distinguishes this from a real
    // override, which is exactly what this test exists to pin.
    constexpr formula::Outcome<Mass> outcome =
        formula::Outcome<Mass>::value(formula::Measured<Mass>::absent(), formula::ValueSource::ManuallyEntered);

    STATIC_REQUIRE(outcome.is_empty());
    STATIC_REQUIRE(outcome.source() == formula::ValueSource::ManuallyEntered);
    STATIC_REQUIRE_FALSE(outcome.is_overridden());
}

TEST_CASE("outcome: is_verdict is true only for a verdict outcome", "[outcome]")
{
    constexpr formula::Outcome<Mass> verdict = formula::Outcome<Mass>::verdict(formula::Verdict { "repeat the test" });
    constexpr formula::Outcome<Mass> value =
        formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(3) }, formula::ValueSource::Derived);

    STATIC_REQUIRE(verdict.is_verdict());
    STATIC_REQUIRE_FALSE(value.is_verdict());
}

TEST_CASE("outcome: is_invalid is true only for an invalid outcome", "[outcome]")
{
    constexpr formula::Outcome<Mass> invalid =
        formula::Outcome<Mass>::invalid(formula::InvalidReason { "outlier rejection removed every specimen" });
    constexpr formula::Outcome<Mass> value =
        formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(3) }, formula::ValueSource::Derived);

    STATIC_REQUIRE(invalid.is_invalid());
    STATIC_REQUIRE_FALSE(value.is_invalid());
}

TEST_CASE("outcome: the label of a non-verdict outcome is empty rather than stale", "[outcome]")
{
    constexpr formula::Outcome<Mass> outcome =
        formula::Outcome<Mass>::value(formula::Measured<Mass> { rat(1) }, formula::ValueSource::Derived);

    STATIC_REQUIRE(outcome.verdict_label().empty());
    STATIC_REQUIRE(outcome.reason_label().empty());
}
