// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/evaluate.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "effective water content", formula::unit::Litre>
{
};
struct CementVolume: formula::Quantity<CementVolume, "V_c", "cement content", formula::unit::Litre>
{
};
struct TotalVolume: formula::Quantity<TotalVolume, "V", "total volume", formula::unit::Litre>
{
};
struct VolumeDifference: formula::Quantity<VolumeDifference, "dV", "volume difference", formula::unit::Litre>
{
};
struct Ratio: formula::Quantity<Ratio, "w/c", "water/cement ratio", formula::unit::One>
{
};
struct SpecimenMass: formula::Quantity<SpecimenMass, "m", "specimen mass", formula::unit::Gram>
{
};
struct MassInKilogram: formula::Quantity<MassInKilogram, "m", "mass", formula::unit::Kilogram>
{
};
struct DistanceInKilometre: formula::Quantity<DistanceInKilometre, "s", "distance", formula::unit::Kilometre>
{
};
struct TemperatureA: formula::Quantity<TemperatureA, "T1", "first temperature", formula::unit::Celsius>
{
};
struct TemperatureB: formula::Quantity<TemperatureB, "T2", "second temperature", formula::unit::Celsius>
{
};
struct TemperatureDeltaKelvin:
    formula::Quantity<TemperatureDeltaKelvin, "dT_K", "temperature difference", formula::unit::Kelvin>
{
};
struct TemperatureDeltaCelsius:
    formula::Quantity<TemperatureDeltaCelsius, "dT_C", "temperature difference", formula::unit::Celsius>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

constexpr auto ratio = var<WaterVolume> / var<CementVolume>;
constexpr auto total = var<WaterVolume> + var<CementVolume>;

constexpr auto inputs =
    formula::environment(formula::Measured<WaterVolume> { rat(180) }, formula::Measured<CementVolume> { rat(300) });

} // namespace

TEST_CASE("evaluate: a ratio of like quantities is exact and dimensionless", "[evaluate]")
{
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(3, 5));
    STATIC_REQUIRE(computed->source() == formula::ValueSource::Derived);
}

TEST_CASE("evaluate: a sum comes back in the result quantity's own unit", "[evaluate]")
{
    // 180 l + 300 l is 480 l. Evaluated in cubic metres and converted back, it
    // must be exactly 480 -- not 0,48 and not 479,999999.
    constexpr auto computed = formula::checked_evaluate<TotalVolume>(total, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(480));
}

TEST_CASE("evaluate: a difference comes back in the result quantity's own unit", "[evaluate]")
{
    // 180 l - 300 l is -120 l. Nothing else in this suite builds a `-`
    // expression and evaluates it -- rewiring the Subtract arm to add instead
    // left every other test green.
    constexpr auto difference = var<WaterVolume> - var<CementVolume>;
    constexpr auto computed = formula::checked_evaluate<VolumeDifference>(difference, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(-120));
}

TEST_CASE("evaluate: the double representation adds, subtracts, multiplies and negates", "[evaluate]")
{
    // The only `double` evaluation elsewhere in this suite is a division (the
    // ratio below); add, subtract, multiply and negate are otherwise only
    // exercised through the exact `Rational` representation.
    // checked_evaluate_si answers in the coherent SI unit, cubic metres, not
    // litres: 180 l and 300 l are 0,18 m3 and 0,3 m3 there. Bounded rather
    // than compared for exact equality, like the rest of this suite's double
    // arithmetic: binary floating point owes no promise of landing on the
    // same bit pattern as a decimal literal.
    constexpr formula::Evaluated<double> summed = formula::checked_evaluate_si<double>(total, inputs);
    STATIC_REQUIRE(summed.has_value() && summed->has_value());
    CHECK(**summed > 0.4799999);
    CHECK(**summed < 0.4800001);

    constexpr auto difference = var<WaterVolume> - var<CementVolume>;
    constexpr formula::Evaluated<double> subtracted = formula::checked_evaluate_si<double>(difference, inputs);
    STATIC_REQUIRE(subtracted.has_value() && subtracted->has_value());
    CHECK(**subtracted > -0.1200001);
    CHECK(**subtracted < -0.1199999);

    constexpr auto product = var<WaterVolume> * var<CementVolume>;
    constexpr formula::Evaluated<double> multiplied = formula::checked_evaluate_si<double>(product, inputs);
    STATIC_REQUIRE(multiplied.has_value() && multiplied->has_value());
    CHECK(**multiplied > 0.0539999);
    CHECK(**multiplied < 0.0540001);

    constexpr formula::Evaluated<double> negated = formula::checked_evaluate_si<double>(-total, inputs);
    STATIC_REQUIRE(negated.has_value() && negated->has_value());
    CHECK(**negated > -0.4800001);
    CHECK(**negated < -0.4799999);
}

TEST_CASE("evaluate: the representation-agnostic core agrees with the exact one", "[evaluate]")
{
    constexpr formula::Evaluated<formula::Rational> exact = formula::checked_evaluate_si<formula::Rational>(ratio, inputs);
    constexpr formula::Evaluated<double> approximate = formula::checked_evaluate_si<double>(ratio, inputs);

    STATIC_REQUIRE(exact.has_value() && exact->has_value());
    STATIC_REQUIRE(**exact == rat(3, 5));
    STATIC_REQUIRE(approximate.has_value() && approximate->has_value());
    CHECK(**approximate == 0.6);
}

TEST_CASE("evaluate: an absent input makes the whole result empty, not zero", "[evaluate]")
{
    constexpr auto partial =
        formula::environment(formula::Measured<WaterVolume>::absent(), formula::Measured<CementVolume> { rat(300) });
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, partial);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_empty());
    STATIC_REQUIRE_FALSE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().is_absent());
}

TEST_CASE("evaluate: division by zero is an error, never a number", "[evaluate]")
{
    constexpr auto zeroed =
        formula::environment(formula::Measured<WaterVolume> { rat(180) }, formula::Measured<CementVolume> { rat(0) });
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, zeroed);

    STATIC_REQUIRE_FALSE(computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("evaluate: the double representation also reports division by zero, not infinity", "[evaluate]")
{
    // RepTraits<double>::divide has its own zero guard, a deliberate
    // divergence from this header's "a double says inf" doctrine (the doc
    // comment above RepTraits<double> is about its ordinary arithmetic, not
    // this one refusal). Falling through to lhs / 0.0 would silently answer
    // with an infinity instead.
    constexpr auto zeroed =
        formula::environment(formula::Measured<WaterVolume> { rat(180) }, formula::Measured<CementVolume> { rat(0) });
    constexpr formula::Evaluated<double> computed = formula::checked_evaluate_si<double>(ratio, zeroed);

    STATIC_REQUIRE_FALSE(computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("evaluate: the throwing spelling throws what the checked one reports", "[evaluate]")
{
    auto const zeroed =
        formula::environment(formula::Measured<WaterVolume> { rat(180) }, formula::Measured<CementVolume> { rat(0) });

    CHECK_THROWS_AS(formula::evaluate<Ratio>(ratio, zeroed), formula::ArithmeticException);
    CHECK(formula::evaluate<Ratio>(ratio, inputs).measurement().value() == rat(3, 5));
}

TEST_CASE("evaluate: an entered result replaces the formula and says so", "[evaluate]")
{
    constexpr auto overridden = formula::environment(formula::Measured<WaterVolume> { rat(180) },
                                                     formula::Measured<CementVolume> { rat(300) },
                                                     formula::entered(formula::Measured<Ratio> { rat(45, 100) }));
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, overridden);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_value());
    STATIC_REQUIRE(computed->is_overridden());
    STATIC_REQUIRE(computed->source() == formula::ValueSource::ManuallyEntered);
    // The formula would have produced 3/5; the entered value wins outright.
    STATIC_REQUIRE(computed->measurement().value() == rat(45, 100));
}

TEST_CASE("evaluate: an entered result short-circuits an otherwise failing formula", "[evaluate]")
{
    // The formula divides by zero. The override must be returned anyway -- proof
    // that the expression is not evaluated at all, rather than evaluated and
    // discarded.
    constexpr auto overridden = formula::environment(formula::Measured<WaterVolume> { rat(180) },
                                                     formula::Measured<CementVolume> { rat(0) },
                                                     formula::entered(formula::Measured<Ratio> { rat(45, 100) }));
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, overridden);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(45, 100));
}

TEST_CASE("evaluate: an entered input is still just an input", "[evaluate]")
{
    // `entered` on an *input* changes where the number came from, not how the
    // formula is evaluated: the result is still derived.
    constexpr auto mixed = formula::environment(formula::entered(formula::Measured<WaterVolume> { rat(180) }),
                                                formula::Measured<CementVolume> { rat(300) });
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, mixed);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->source() == formula::ValueSource::Derived);
    STATIC_REQUIRE(computed->measurement().value() == rat(3, 5));
}

TEST_CASE("evaluate: a constant enters the arithmetic in its own unit", "[evaluate]")
{
    // half a cubic metre, added to 180 l + 300 l, is 980 l.
    constexpr auto withConstant = total + formula::constant<formula::unit::CubicMetre>(rat(1, 2));
    constexpr auto computed = formula::checked_evaluate<TotalVolume>(withConstant, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(980));
}

TEST_CASE("evaluate: a constant declared in a non-coherent unit still enters in its own unit", "[evaluate]")
{
    // CubicMetre, used by the test above, happens to be the coherent SI unit
    // for volume, so that test cannot tell a constant evaluated in its own
    // unit apart from one evaluated as though it were already stated in the
    // coherent unit. Litre is not coherent (its magnitude is 1/1000), so
    // this one can: 180 l + 300 l + 20 l is exactly 500 l. Under the
    // coherent-unit mutation, the 20 would enter as 20 m3 -- 20000 l -- and
    // the total would be 20480 l, not 500.
    constexpr auto withConstant = total + formula::constant<formula::unit::Litre>(rat(20));
    constexpr auto computed = formula::checked_evaluate<TotalVolume>(withConstant, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(500));
}

TEST_CASE("evaluate: negation negates", "[evaluate]")
{
    constexpr auto computed = formula::checked_evaluate<TotalVolume>(-total, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(-480));
}

TEST_CASE("evaluate: a value in a different unit converts exactly", "[evaluate]")
{
    // The input is in grams; the formula is dimensionally a mass; and the
    // result quantity declares kilograms.
    constexpr auto grams = formula::environment(formula::Measured<SpecimenMass> { rat(2500) });
    constexpr auto computed = formula::checked_evaluate<MassInKilogram>(var<SpecimenMass>, grams);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(5, 2));
}

TEST_CASE("evaluate: an overflowing computation is reported, not wrapped", "[evaluate]")
{
    // Anything above the square root of the representable range cannot be
    // squared: sqrt(INT64_MAX) is about 3,04e9, so 4e9 divided by 1/4e9 -- a
    // cross-reduction-proof 1,6e19 -- overflows outright. (3e9 does not: see
    // the companion test below, which pins exactly where the edge is.)
    constexpr std::int64_t huge = 4'000'000'000LL;
    auto const big =
        formula::environment(formula::Measured<WaterVolume> { rat(huge) }, formula::Measured<CementVolume> { rat(1, huge) });
    auto const computed = formula::checked_evaluate<Ratio>(ratio, big);

    REQUIRE_FALSE(computed.has_value());
    CHECK(computed.error() == formula::ArithmeticError::Overflow);
}

TEST_CASE("evaluate: the double representation still reports an overflow from the leaf conversion", "[evaluate]")
{
    // RepTraits<double>'s own arithmetic cannot overflow (the doc comment
    // above it says so), but detail::in_si converts every leaf in exact
    // Rational before handing it to RepTraits<Rep>::from, and that conversion
    // can overflow on its own -- IntMax kilometres times a magnitude of 1000
    // overflows the exact multiply long before any double arithmetic runs.
    constexpr std::int64_t huge = formula::detail::IntMax;
    auto const farInputs = formula::environment(formula::Measured<DistanceInKilometre> { rat(huge) });
    auto const computed = formula::checked_evaluate_si<double>(var<DistanceInKilometre>, farInputs);

    REQUIRE_FALSE(computed.has_value());
    CHECK(computed.error() == formula::ArithmeticError::Overflow);
}

TEST_CASE("evaluate: a result at the edge of the range is computed, not refused", "[evaluate]")
{
    // 3e9 litres over 1/3e9 litres is exactly 9e18, which fits in a 64-bit
    // integer with room to spare. It fits only because `checked_mul`
    // cross-reduces before multiplying; a naive implementation would overflow
    // on the way to a representable answer. This is the companion to the
    // overflow test above: together they say where the edge actually is.
    constexpr std::int64_t large = 3'000'000'000LL;
    auto const edgeInputs = formula::environment(formula::Measured<WaterVolume> { rat(large) },
                                                 formula::Measured<CementVolume> { rat(1, large) });
    auto const computed = formula::checked_evaluate<Ratio>(ratio, edgeInputs);

    REQUIRE(computed.has_value());
    REQUIRE(computed->is_value());
    CHECK(computed->measurement().value() == rat(9'000'000'000'000'000'000LL));
}

TEST_CASE("evaluate: a measured result is not mistaken for an override", "[evaluate]")
{
    // Only an `entered` value for the result quantity short-circuits the
    // formula. A plain `Measured<Ratio>` supplied alongside it is a value the
    // environment merely provides, not one a person typed in place of the
    // formula's answer, and it must not silently win.
    constexpr auto measuredResult = formula::environment(formula::Measured<WaterVolume> { rat(180) },
                                                         formula::Measured<CementVolume> { rat(300) },
                                                         formula::Measured<Ratio> { rat(45, 100) });
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, measuredResult);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->source() == formula::ValueSource::Derived);
    STATIC_REQUIRE(computed->measurement().value() == rat(3, 5));
}

TEST_CASE("evaluate: an arithmetic error on one side outranks absence on the other", "[evaluate]")
{
    // The left side is never measured; the right side, entirely on its own,
    // divides by zero. Both sides are asked regardless of order, so the error
    // from the side that IS present must win -- an absent left side must not
    // silently swallow an error the right side already found.
    constexpr auto nested = var<WaterVolume> / (var<CementVolume> / var<TotalVolume>);
    constexpr auto mixedEnv = formula::environment(formula::Measured<WaterVolume>::absent(),
                                                   formula::Measured<CementVolume> { rat(300) },
                                                   formula::Measured<TotalVolume> { rat(0) });
    constexpr formula::Evaluated<formula::Rational> computed =
        formula::checked_evaluate_si<formula::Rational>(nested, mixedEnv);

    STATIC_REQUIRE_FALSE(computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("evaluate: an offset unit converts a point, not a difference", "[evaluate]")
{
    // checked_convert converts a POINT on the scale (unit.hpp says so): 20
    // degC and 15 degC become 293,15 K and 288,15 K on the way in, so their
    // difference in the coherent SI unit -- where the subtraction actually
    // happens -- is exactly 5 K, and a result quantity declared in kelvin
    // reports that. A result quantity declared in degrees Celsius instead
    // asks a different question: it converts the computed 5 K as a point too,
    // landing on -268,15, not on the 5-degree swing a reader might expect.
    // This documents that behaviour rather than judging it -- it is this
    // layer's documented semantics, faithfully propagated.
    constexpr auto temperatures =
        formula::environment(formula::Measured<TemperatureA> { rat(20) }, formula::Measured<TemperatureB> { rat(15) });
    constexpr auto difference = var<TemperatureA> - var<TemperatureB>;

    constexpr auto inKelvin = formula::checked_evaluate<TemperatureDeltaKelvin>(difference, temperatures);
    STATIC_REQUIRE(inKelvin.has_value());
    STATIC_REQUIRE(inKelvin->measurement().value() == rat(5));

    constexpr auto inCelsius = formula::checked_evaluate<TemperatureDeltaCelsius>(difference, temperatures);
    STATIC_REQUIRE(inCelsius.has_value());
    STATIC_REQUIRE(inCelsius->measurement().value() == rat(-26815, 100));
}
