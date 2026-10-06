// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/evaluate.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace
{

struct Rise: formula::Quantity<Rise, "h", "height gained", formula::unit::Millimetre>
{
};
struct Run: formula::Quantity<Run, "L", "horizontal distance covered", formula::unit::Millimetre>
{
};
struct TotalLength: formula::Quantity<TotalLength, "T", "total length", formula::unit::Millimetre>
{
};
struct LengthDifference: formula::Quantity<LengthDifference, "dL", "length difference", formula::unit::Millimetre>
{
};
struct Ratio: formula::Quantity<Ratio, "s", "road gradient", formula::unit::One>
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
struct HeaterPower: formula::Quantity<HeaterPower, "P", "heater power", formula::unit::Kilowatt>
{
};
struct RunTime: formula::Quantity<RunTime, "t", "run time", formula::unit::Hour>
{
};
struct HeaterEnergy: formula::Quantity<HeaterEnergy, "E", "heater energy", formula::unit::KilowattHour>
{
};
struct BodyTemperatureFahrenheit:
    formula::Quantity<BodyTemperatureFahrenheit, "T_F", "body temperature", formula::unit::Fahrenheit>
{
};
struct BodyTemperatureCelsius:
    formula::Quantity<BodyTemperatureCelsius, "T_C", "body temperature", formula::unit::Celsius>
{
};

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

using formula::var;

constexpr auto ratio = var<Rise> / var<Run>;
constexpr auto total = var<Rise> + var<Run>;

constexpr auto inputs = formula::environment(formula::Measured<Rise> { rat(180) }, formula::Measured<Run> { rat(300) });

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
    // 180 mm + 300 mm is 480 mm. Evaluated in metres and converted back, it
    // must be exactly 480 -- not 0.48 and not 479.999999.
    constexpr auto computed = formula::checked_evaluate<TotalLength>(total, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(480));
}

TEST_CASE("evaluate: a difference comes back in the result quantity's own unit", "[evaluate]")
{
    // 180 mm - 300 mm is -120 mm. Nothing else in this suite builds a `-`
    // expression and evaluates it -- rewiring the Subtract arm to add instead
    // left every other test green.
    constexpr auto difference = var<Rise> - var<Run>;
    constexpr auto computed = formula::checked_evaluate<LengthDifference>(difference, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(-120));
}

TEST_CASE("evaluate: the double representation adds, subtracts, multiplies and negates", "[evaluate]")
{
    // The only `double` evaluation elsewhere in this suite is a division (the
    // ratio below); add, subtract, multiply and negate are otherwise only
    // exercised through the exact `Rational` representation.
    // checked_evaluate_si answers in the coherent SI unit, metres, not
    // millimetres: 180 mm and 300 mm are 0.18 m and 0.3 m there. Bounded rather
    // than compared for exact equality, like the rest of this suite's double
    // arithmetic: binary floating point owes no promise of landing on the
    // same bit pattern as a decimal literal.
    constexpr formula::Evaluated<double> summed = formula::checked_evaluate_si<double>(total, inputs);
    STATIC_REQUIRE(summed.has_value() && summed->has_value());
    CHECK(**summed > 0.4799999);
    CHECK(**summed < 0.4800001);

    constexpr auto difference = var<Rise> - var<Run>;
    constexpr formula::Evaluated<double> subtracted = formula::checked_evaluate_si<double>(difference, inputs);
    STATIC_REQUIRE(subtracted.has_value() && subtracted->has_value());
    CHECK(**subtracted > -0.1200001);
    CHECK(**subtracted < -0.1199999);

    constexpr auto product = var<Rise> * var<Run>;
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
    constexpr auto partial = formula::environment(formula::Measured<Rise>::absent(), formula::Measured<Run> { rat(300) });
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, partial);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->is_empty());
    STATIC_REQUIRE_FALSE(computed->is_value());
    STATIC_REQUIRE(computed->measurement().is_absent());
}

TEST_CASE("evaluate: division by zero is an error, never a number", "[evaluate]")
{
    constexpr auto zeroed = formula::environment(formula::Measured<Rise> { rat(180) }, formula::Measured<Run> { rat(0) });
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
    constexpr auto zeroed = formula::environment(formula::Measured<Rise> { rat(180) }, formula::Measured<Run> { rat(0) });
    constexpr formula::Evaluated<double> computed = formula::checked_evaluate_si<double>(ratio, zeroed);

    STATIC_REQUIRE_FALSE(computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("evaluate: the throwing spelling throws what the checked one reports", "[evaluate]")
{
    auto const zeroed = formula::environment(formula::Measured<Rise> { rat(180) }, formula::Measured<Run> { rat(0) });

    CHECK_THROWS_AS(formula::evaluate<Ratio>(ratio, zeroed), formula::ArithmeticException);
    CHECK(formula::evaluate<Ratio>(ratio, inputs).measurement().value() == rat(3, 5));
}

TEST_CASE("evaluate: an entered result replaces the formula and says so", "[evaluate]")
{
    constexpr auto overridden = formula::environment(formula::Measured<Rise> { rat(180) },
                                                     formula::Measured<Run> { rat(300) },
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
    constexpr auto overridden = formula::environment(formula::Measured<Rise> { rat(180) },
                                                     formula::Measured<Run> { rat(0) },
                                                     formula::entered(formula::Measured<Ratio> { rat(45, 100) }));
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, overridden);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(45, 100));
}

TEST_CASE("evaluate: an entered input is still just an input", "[evaluate]")
{
    // `entered` on an *input* changes where the number came from, not how the
    // formula is evaluated: the result is still derived.
    constexpr auto mixed =
        formula::environment(formula::entered(formula::Measured<Rise> { rat(180) }), formula::Measured<Run> { rat(300) });
    constexpr auto computed = formula::checked_evaluate<Ratio>(ratio, mixed);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->source() == formula::ValueSource::Derived);
    STATIC_REQUIRE(computed->measurement().value() == rat(3, 5));
}

TEST_CASE("evaluate: a constant enters the arithmetic in its own unit", "[evaluate]")
{
    // half a metre, added to 180 mm + 300 mm, is 980 mm.
    constexpr auto withConstant = total + formula::constant<formula::unit::Metre>(rat(1, 2));
    constexpr auto computed = formula::checked_evaluate<TotalLength>(withConstant, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(980));
}

TEST_CASE("evaluate: a constant declared in a non-coherent unit still enters in its own unit", "[evaluate]")
{
    // Metre, used by the test above, happens to be the coherent SI unit
    // for length, so that test cannot tell a constant evaluated in its own
    // unit apart from one evaluated as though it were already stated in the
    // coherent unit. Millimetre is not coherent (its magnitude is 1/1000), so
    // this one can: 180 mm + 300 mm + 20 mm is exactly 500 mm. Under the
    // coherent-unit mutation, the 20 would enter as 20 m -- 20000 mm -- and
    // the total would be 20480 mm, not 500.
    constexpr auto withConstant = total + formula::constant<formula::unit::Millimetre>(rat(20));
    constexpr auto computed = formula::checked_evaluate<TotalLength>(withConstant, inputs);

    STATIC_REQUIRE(computed.has_value());
    STATIC_REQUIRE(computed->measurement().value() == rat(500));
}

TEST_CASE("evaluate: negation negates", "[evaluate]")
{
    constexpr auto computed = formula::checked_evaluate<TotalLength>(-total, inputs);

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
    // squared: sqrt(2^127) is about 1.3e19, so 2e19 divided by 1/2e19 -- a
    // cross-reduction-proof 4e38 -- overflows outright.
    constexpr formula::Rational::Int huge = formula::Rational::Int { 10'000'000'000'000'000'000ULL } * 2;
    auto const big = formula::environment(formula::Measured<Rise> { formula::Rational { huge } },
                                          formula::Measured<Run> { formula::Rational { 1, huge } });
    auto const computed = formula::checked_evaluate<Ratio>(ratio, big);

    REQUIRE_FALSE(computed.has_value());
    CHECK(computed.error() == formula::ArithmeticError::Overflow);

    // 4e9 over 1/4e9, which overflowed 64 bits, is 1.6e19.
    constexpr std::int64_t huge64 = 4'000'000'000LL;
    auto const big64 =
        formula::environment(formula::Measured<Rise> { rat(huge64) }, formula::Measured<Run> { rat(1, huge64) });
    auto const computed64 = formula::checked_evaluate<Ratio>(ratio, big64);
    REQUIRE(computed64.has_value());
    REQUIRE(computed64->is_value());
    CHECK(computed64->measurement().value() == formula::Rational { formula::Rational::Int { huge64 } * huge64 });
}

TEST_CASE("evaluate: the double representation still reports an overflow from the leaf conversion", "[evaluate]")
{
    // RepTraits<double>'s own arithmetic cannot overflow (the doc comment
    // above it says so), but detail::in_si converts every leaf in exact
    // Rational before handing it to RepTraits<Rep>::from, and that conversion
    // can overflow on its own -- the largest Rational::Int of kilometres times
    // a magnitude of 1000 overflows the exact multiply long before any double
    // arithmetic runs.
    constexpr formula::Rational::Int huge = std::numeric_limits<formula::Rational::Int>::max();
    auto const farInputs = formula::environment(formula::Measured<DistanceInKilometre> { formula::Rational { huge } });
    auto const computed = formula::checked_evaluate_si<double>(var<DistanceInKilometre>, farInputs);

    REQUIRE_FALSE(computed.has_value());
    CHECK(computed.error() == formula::ArithmeticError::Overflow);
}

TEST_CASE("evaluate: a result at the edge of the range is computed, not refused", "[evaluate]")
{
    // 1.3e19 millimetres over 1/1.3e19 millimetres is exactly 1.69e38, which fits below
    // 2^127 (1.70e38). It fits only because `checked_mul` cross-reduces before
    // multiplying; a naive implementation would overflow on the way to a
    // representable answer. This is the companion to the overflow test above,
    // at 2e19: together they say where the edge actually is, at the square
    // root of 2^127, about 1.30e19.
    constexpr formula::Rational::Int large { 13'000'000'000'000'000'000ULL };
    auto const edgeInputs = formula::environment(formula::Measured<Rise> { formula::Rational { large } },
                                                 formula::Measured<Run> { formula::Rational { 1, large } });
    auto const computed = formula::checked_evaluate<Ratio>(ratio, edgeInputs);

    REQUIRE(computed.has_value());
    REQUIRE(computed->is_value());
    CHECK(computed->measurement().value() == formula::Rational { large * large });
}

TEST_CASE("evaluate: a measured result is not mistaken for an override", "[evaluate]")
{
    // Only an `entered` value for the result quantity short-circuits the
    // formula. A plain `Measured<Ratio>` supplied alongside it is a value the
    // environment merely provides, not one a person typed in place of the
    // formula's answer, and it must not silently win.
    constexpr auto measuredResult = formula::environment(formula::Measured<Rise> { rat(180) },
                                                         formula::Measured<Run> { rat(300) },
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
    constexpr auto nested = var<Rise> / (var<Run> / var<TotalLength>);
    constexpr auto mixedEnv = formula::environment(
        formula::Measured<Rise>::absent(), formula::Measured<Run> { rat(300) }, formula::Measured<TotalLength> { rat(0) });
    constexpr formula::Evaluated<formula::Rational> computed =
        formula::checked_evaluate_si<formula::Rational>(nested, mixedEnv);

    STATIC_REQUIRE_FALSE(computed.has_value());
    STATIC_REQUIRE(computed.error() == formula::ArithmeticError::DivisionByZero);
}

TEST_CASE("evaluate: an offset unit converts a point, not a difference", "[evaluate]")
{
    // checked_convert converts a POINT on the scale (unit.hpp says so): 20
    // degC and 15 degC become 293.15 K and 288.15 K on the way in, so their
    // difference in the coherent SI unit -- where the subtraction actually
    // happens -- is exactly 5 K, and a result quantity declared in kelvin
    // reports that. A result quantity declared in degrees Celsius instead
    // asks a different question: it converts the computed 5 K as a point too,
    // landing on -268.15, not on the 5-degree swing a reader might expect.
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

TEST_CASE("evaluate: a power over a time is an energy, exact in kilowatt-hours", "[evaluate]")
{
    // 3/2 kW for 4 h is exactly 6 kWh. Evaluated in joules -- 1500 W times
    // 14400 s is 21600000 J -- and converted back through the 3600000 that a
    // kilowatt-hour is, so a wrong magnitude on the power, the hour or the
    // energy unit lands on something other than 6.
    constexpr auto heater = formula::environment(formula::Measured<HeaterPower> { rat(3, 2) },
                                                 formula::Measured<RunTime> { rat(4) });
    constexpr auto consumed = var<HeaterPower> * var<RunTime>;

    constexpr auto energy = formula::checked_evaluate<HeaterEnergy>(consumed, heater);
    STATIC_REQUIRE(energy.has_value());
    STATIC_REQUIRE(energy->measurement().value() == rat(6));
}

TEST_CASE("evaluate: a Fahrenheit reading converts to Celsius and back exactly", "[evaluate]")
{
    // 98.6 degF is 493/5, and lands on exactly 37 degC rather than on a rounded
    // 37.0000001: the factor is 5/9 and the offset 45967/180, both exact.
    constexpr auto fever = formula::environment(formula::Measured<BodyTemperatureFahrenheit> { rat(493, 5) });
    constexpr auto reading = var<BodyTemperatureFahrenheit>;

    constexpr auto inCelsius = formula::checked_evaluate<BodyTemperatureCelsius>(reading, fever);
    STATIC_REQUIRE(inCelsius.has_value());
    STATIC_REQUIRE(inCelsius->measurement().value() == rat(37));

    // And the other way: 37 degC is 493/5 degF.
    constexpr auto normal = formula::environment(formula::Measured<BodyTemperatureCelsius> { rat(37) });
    constexpr auto inFahrenheit =
        formula::checked_evaluate<BodyTemperatureFahrenheit>(var<BodyTemperatureCelsius>, normal);
    STATIC_REQUIRE(inFahrenheit.has_value());
    STATIC_REQUIRE(inFahrenheit->measurement().value() == rat(493, 5));
}

namespace
{
// Money, with this file's own units -- the library ships no currencies. Euros
// and yen are named base dimensions, a cent is a hundredth of a euro, and an
// exchange rate is data: a quantity in yen per euro, not a conversion factor.
constexpr formula::Dimension EuroAmount = formula::base_dimension("EUR");
constexpr formula::Dimension YenAmount = formula::base_dimension("JPY");

constexpr formula::Unit Euro { .dimension = EuroAmount, .symbolText = formula::symbol("EUR"), .decimals = 2 };
constexpr formula::Unit EuroCent { .dimension = EuroAmount,
                                   .magnitudeNumerator = 1,
                                   .magnitudeDenominator = 100,
                                   .symbolText = formula::symbol("ct"),
                                   .decimals = 0 };
constexpr formula::Unit Yen { .dimension = YenAmount, .symbolText = formula::symbol("JPY"), .decimals = 0 };
constexpr formula::Unit EuroPerKilowattHour { .dimension = EuroAmount / formula::dim::Energy,
                                              .magnitudeNumerator = 1,
                                              .magnitudeDenominator = 3600000,
                                              .symbolText = formula::symbol("EUR/kWh"),
                                              .decimals = 4 };
constexpr formula::Unit YenPerEuro { .dimension = YenAmount / EuroAmount,
                                     .symbolText = formula::symbol("JPY/EUR"),
                                     .decimals = 2 };

struct Energy: formula::Quantity<Energy, "E", "energy consumed", formula::unit::KilowattHour>
{
};
struct Tariff: formula::Quantity<Tariff, "c", "energy tariff", EuroPerKilowattHour>
{
};
struct EnergyCost: formula::Quantity<EnergyCost, "C", "cost of the energy", Euro>
{
};
struct Fee: formula::Quantity<Fee, "F", "testing fee", Euro>
{
};
struct Surcharge: formula::Quantity<Surcharge, "S", "surcharge", EuroCent>
{
};
struct Charged: formula::Quantity<Charged, "T", "amount charged", Euro>
{
};
struct Amount: formula::Quantity<Amount, "A", "amount in euros", Euro>
{
};
struct ExchangeRate: formula::Quantity<ExchangeRate, "r", "exchange rate", YenPerEuro>
{
};
struct AmountInYen: formula::Quantity<AmountInYen, "A_y", "amount in yen", Yen>
{
};
} // namespace

TEST_CASE("evaluate: energy at a tariff costs euros, exactly", "[evaluate][money]")
{
    // The product of an energy and a tariff in euros per kilowatt-hour is a
    // dimension of euros, decided while the formula is compiled.
    constexpr auto cost = var<Energy> * var<Tariff>;
    STATIC_REQUIRE(decltype(cost)::dimension == EuroAmount);

    // 150 kWh at 3/10 EUR/kWh is exactly 45 EUR. Evaluated as 540000000 J
    // times 1/12000000 EUR/J, so a wrong factor on one of the two -- the
    // kilowatt-hour or the tariff -- lands on something other than 45.
    constexpr auto consumption =
        formula::environment(formula::Measured<Energy> { rat(150) }, formula::Measured<Tariff> { rat(3, 10) });
    constexpr auto charged = formula::checked_evaluate<EnergyCost>(cost, consumption);
    STATIC_REQUIRE(charged.has_value());
    STATIC_REQUIRE(charged->measurement().value() == rat(45));
}

TEST_CASE("evaluate: euros and cents add up in euros", "[evaluate][money]")
{
    // 10 EUR + 250 ct is 25/2 EUR: the cents converted, not added as euros.
    constexpr auto bill =
        formula::environment(formula::Measured<Fee> { rat(10) }, formula::Measured<Surcharge> { rat(250) });
    constexpr auto billed = formula::checked_evaluate<Charged>(var<Fee> + var<Surcharge>, bill);
    STATIC_REQUIRE(billed.has_value());
    STATIC_REQUIRE(billed->measurement().value() == rat(25, 2));
}

TEST_CASE("evaluate: an exchange rate supplied as a quantity turns euros into yen", "[evaluate][money]")
{
    // 100 EUR at 16235/100 JPY/EUR is 16235 JPY. The rate is an input like any
    // other, which is the only way euros become yen.
    constexpr auto exchange = formula::environment(formula::Measured<Amount> { rat(100) },
                                                   formula::Measured<ExchangeRate> { rat(16235, 100) });
    constexpr auto inYen = formula::checked_evaluate<AmountInYen>(var<Amount> * var<ExchangeRate>, exchange);
    STATIC_REQUIRE(inYen.has_value());
    STATIC_REQUIRE(inYen->measurement().value() == rat(16235));
}

namespace
{
/// What every test-local environment below answers from `get<Q>()`: 1009 mm,
/// never what its other hooks answer, so that a result tells which one the
/// evaluator read. 1009 mm over 1009 mm is 1, not the fixture's 3/5.
inline constexpr std::int64_t fromGet = 1009;

/// The fixture's 180 mm of rise, and 300 mm for any other quantity.
template <typename Q>
[[nodiscard]] constexpr formula::Measured<Q> fixtureValue() noexcept
{
    return formula::Measured<Q> { std::is_same_v<Q, Rise> ? rat(180) : rat(300) };
}

/// An environment of a consumer's own that works its values out rather than
/// holding them, and can fail to: `checked_get<Q>()` answers the fixture's
/// values, except for @p Failing, whose read fails with `DomainError` -- an
/// error no arithmetic in these formulas produces, so one seen is this one.
/// It has no `source_of`, and says through `is_entered` that nothing was
/// typed in.
template <typename Failing>
struct FailingEnvironment
{
    template <formula::Described Q>
    static constexpr bool is_entered = false;

    template <formula::Described Q>
    [[nodiscard]] constexpr formula::Measured<Q> get() const noexcept
    {
        return formula::Measured<Q> { rat(fromGet) };
    }

    template <formula::Described Q>
    [[nodiscard]] constexpr std::expected<formula::Measured<Q>, formula::ArithmeticError> checked_get() const noexcept
    {
        if constexpr (std::is_same_v<Q, Failing>)
            return std::unexpected { formula::ArithmeticError::DomainError };
        else
            return fixtureValue<Q>();
    }
};

/// An environment that says at run time where its values came from:
/// `source_of<Q>()` answers @p answered, whatever the static `is_entered`
/// says -- which is that every value was typed in. It holds the fixture's
/// rise, and no run.
struct SourceEnvironment
{
    formula::ValueSource answered;

    template <formula::Described Q>
    static constexpr bool is_entered = true;

    template <formula::Described Q>
    [[nodiscard]] constexpr formula::Measured<Q> get() const noexcept
    {
        if constexpr (std::is_same_v<Q, Rise>)
            return fixtureValue<Q>();
        else
            return formula::Measured<Q>::absent();
    }

    template <formula::Described Q>
    [[nodiscard]] constexpr formula::ValueSource source_of() const noexcept
    {
        return answered;
    }
};

/// An environment whose members are named as the hooks are but return
/// something else. Its `source_of` answers an `int`; its `checked_get` is
/// one of the two below. Neither is the hook, so the evaluator reads `get`
/// -- the fixture's values -- and asks `is_entered`, which says the rise was
/// typed in and the run was not.
struct Lookalike
{
    template <formula::Described Q>
    static constexpr bool is_entered = std::is_same_v<Q, Rise>;

    template <formula::Described Q>
    [[nodiscard]] constexpr formula::Measured<Q> get() const noexcept
    {
        return fixtureValue<Q>();
    }

    template <formula::Described Q>
    [[nodiscard]] constexpr int source_of() const noexcept
    {
        return 2;
    }
};

/// A `checked_get` whose error is an `int`, and which always fails.
struct ErrorOfAnotherType: Lookalike
{
    template <formula::Described Q>
    [[nodiscard]] constexpr std::expected<formula::Measured<Q>, int> checked_get() const noexcept
    {
        return std::unexpected { 7 };
    }
};

/// A `checked_get` answering a plain measurement -- one the hook's type
/// converts from, so that only an exact match refuses it -- of 1009 mm.
struct PlainMeasurement: Lookalike
{
    template <formula::Described Q>
    [[nodiscard]] constexpr formula::Measured<Q> checked_get() const noexcept
    {
        return formula::Measured<Q> { rat(fromGet) };
    }
};

/// A `checked_get` answering a reference to the hook's own type, holding a
/// failure: the hook's type exactly, but not by value.
struct ReferenceToFailure: Lookalike
{
    template <formula::Described Q>
    static constexpr std::expected<formula::Measured<Q>, formula::ArithmeticError> failed =
        std::unexpected { formula::ArithmeticError::DomainError };

    template <formula::Described Q>
    [[nodiscard]] constexpr std::expected<formula::Measured<Q>, formula::ArithmeticError> const&
    checked_get() const noexcept
    {
        return failed<Q>;
    }
};

/// Converts to `ValueSource::Derived`.
struct DerivedOnConversion
{
    [[nodiscard]] constexpr operator formula::ValueSource() const noexcept
    {
        return formula::ValueSource::Derived;
    }
};

/// A `source_of` answering a type that converts to `ValueSource` -- to
/// `Derived`, which contradicts both of `is_entered`'s answers -- so that
/// only an exact match refuses it.
struct ConvertibleSource: Lookalike
{
    template <formula::Described Q>
    [[nodiscard]] constexpr DerivedOnConversion source_of() const noexcept
    {
        return DerivedOnConversion {};
    }
};

/// A `source_of` answering a reference to `ValueSource::Derived`: the hook's
/// type exactly, but not by value.
struct ReferenceToSource: Lookalike
{
    static constexpr formula::ValueSource calculated = formula::ValueSource::Derived;

    template <formula::Described Q>
    [[nodiscard]] constexpr formula::ValueSource const& source_of() const noexcept
    {
        return calculated;
    }
};

[[nodiscard]] std::string source_name(formula::ValueSource answered)
{
    switch (answered)
    {
        case formula::ValueSource::Measured:
            return "measured";
        case formula::ValueSource::ManuallyEntered:
            return "entered";
        case formula::ValueSource::Derived:
            return "derived";
    }
    return "unknown";
}

[[nodiscard]] std::string held_text(formula::Evaluated<formula::Rational> const& evaluated)
{
    if (!evaluated.has_value())
        return std::string { formula::describe(evaluated.error()) };
    return evaluated->has_value() ? "a value" : "absent";
}

/// A sink that writes down, in order, what the evaluator tells it of each
/// variable -- where its value came from, and what it produced -- and what
/// every other node produced.
struct VariableLog
{
    std::vector<std::string>* heard;

    template <formula::Node N>
    void entered(N const&) const
    {
    }

    template <formula::Node N, typename V>
    void produced(N const&, V const& evaluated) const
    {
        if constexpr (std::is_same_v<V, formula::Evaluated<formula::Rational>>)
            heard->push_back("an operation produced " + held_text(evaluated));
    }

    template <formula::Described Q>
    void produced(formula::VarNode<Q> const&, formula::Evaluated<formula::Rational> const& evaluated) const
    {
        heard->push_back(std::string { formula::Describe<Q>::symbol } + " produced " + held_text(evaluated));
    }

    template <formula::Described Q>
    void input_source(formula::VarNode<Q> const&, formula::ValueSource answered) const
    {
        heard->push_back(std::string { formula::Describe<Q>::symbol } + " was " + source_name(answered));
    }
};
} // namespace

TEST_CASE("evaluate: an environment that can fail a read is read through checked_get", "[evaluate]")
{
    // 180 mm over 300 mm is 3/5, read through checked_get; get's 1009 mm over
    // 1009 mm would be 1.
    STATIC_REQUIRE(formula::checked_evaluate<Ratio>(ratio, FailingEnvironment<void> {})->measurement().value()
                   == rat(3, 5));
    // A failed read fails the formula with the environment's own error, on
    // either side; read through get it would have been a value.
    STATIC_REQUIRE(formula::checked_evaluate<Ratio>(ratio, FailingEnvironment<Rise> {}).error()
                   == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(formula::checked_evaluate<Ratio>(ratio, FailingEnvironment<Run> {}).error()
                   == formula::ArithmeticError::DomainError);
}

TEST_CASE("evaluate: a failed read is the variable's failure, relayed as an inline one is", "[evaluate]")
{
    // The variable is told its source and then that it failed; the sum
    // relays the failure. A failure on the left ends the sum there, so the
    // run is never read -- as a left-hand division by zero would end it.
    std::vector<std::string> heard;
    (void) formula::checked_evaluate_si<formula::Rational>(total, FailingEnvironment<Rise> {}, VariableLog { &heard });
    CHECK(heard
          == std::vector<std::string> { "h was measured",
                                        "h produced argument outside the domain of the operation",
                                        "an operation produced argument outside the domain of the operation" });

    heard.clear();
    (void) formula::checked_evaluate_si<formula::Rational>(total, FailingEnvironment<Run> {}, VariableLog { &heard });
    CHECK(heard
          == std::vector<std::string> { "h was measured",
                                        "h produced a value",
                                        "L was measured",
                                        "L produced argument outside the domain of the operation",
                                        "an operation produced argument outside the domain of the operation" });
}

TEST_CASE("evaluate: a branch not taken never reads a value whose read fails", "[evaluate]")
{
    // 180 mm is over 100 mm. The run's read fails: in the branch not taken
    // it is never read, and in the branch taken it fails the formula.
    constexpr auto overHundred = var<Rise> > formula::constant<formula::unit::Millimetre>(rat(100));
    constexpr auto runUntaken = formula::when(overHundred, var<Rise>, var<Run>);
    constexpr auto runTaken = formula::when(overHundred, var<Run>, var<Rise>);
    STATIC_REQUIRE(formula::checked_evaluate<TotalLength>(runUntaken, FailingEnvironment<Run> {})->measurement().value()
                   == rat(180));
    STATIC_REQUIRE(formula::checked_evaluate<TotalLength>(runTaken, FailingEnvironment<Run> {}).error()
                   == formula::ArithmeticError::DomainError);
}

TEST_CASE("evaluate: an environment's run-time source is preferred over its static is_entered", "[evaluate]")
{
    // is_entered says typed in; source_of says what it is told, at run time,
    // of a present value and of an absent one alike.
    std::vector<std::string> heard;
    (void) formula::checked_evaluate_si<formula::Rational>(
        total, SourceEnvironment { formula::ValueSource::Derived }, VariableLog { &heard });
    CHECK(heard
          == std::vector<std::string> {
              "h was derived", "h produced a value", "L was derived", "L produced absent", "an operation produced absent" });

    heard.clear();
    (void) formula::checked_evaluate_si<formula::Rational>(
        total, SourceEnvironment { formula::ValueSource::Measured }, VariableLog { &heard });
    CHECK(
        heard
        == std::vector<std::string> {
            "h was measured", "h produced a value", "L was measured", "L produced absent", "an operation produced absent" });
}

TEST_CASE("evaluate: a checked_get or a source_of of another return type is not the hook", "[evaluate]")
{
    // No checked_get of these is read, so the ratio is the fixture's 3/5: not
    // an error, and not 1009 mm over 1009 mm.
    STATIC_REQUIRE(formula::checked_evaluate<Ratio>(ratio, ErrorOfAnotherType {})->measurement().value() == rat(3, 5));
    STATIC_REQUIRE(formula::checked_evaluate<Ratio>(ratio, PlainMeasurement {})->measurement().value() == rat(3, 5));
    STATIC_REQUIRE(formula::checked_evaluate<Ratio>(ratio, ReferenceToFailure {})->measurement().value() == rat(3, 5));
    // Their source_of answers an int: not asked, so is_entered decides.
    std::vector<std::string> heard;
    (void) formula::checked_evaluate_si<formula::Rational>(total, ErrorOfAnotherType {}, VariableLog { &heard });
    CHECK(heard
          == std::vector<std::string> { "h was entered",
                                        "h produced a value",
                                        "L was measured",
                                        "L produced a value",
                                        "an operation produced a value" });
}

TEST_CASE("evaluate: a source_of answering a reference or a type converting to ValueSource is not the hook",
          "[evaluate]")
{
    // Both answer Derived, if asked; an int, as above, would not convert.
    STATIC_REQUIRE(static_cast<formula::ValueSource>(ConvertibleSource {}.source_of<Rise>())
                   == formula::ValueSource::Derived);
    STATIC_REQUIRE(ReferenceToSource {}.source_of<Rise>() == formula::ValueSource::Derived);

    // Neither is asked, so is_entered decides: the rise typed in, the run
    // measured -- where the hook would have said both were derived.
    std::vector<std::string> const decidedByIsEntered {
        "h was entered", "h produced a value", "L was measured", "L produced a value", "an operation produced a value"
    };
    std::vector<std::string> heard;
    (void) formula::checked_evaluate_si<formula::Rational>(total, ConvertibleSource {}, VariableLog { &heard });
    CHECK(heard == decidedByIsEntered);

    heard.clear();
    (void) formula::checked_evaluate_si<formula::Rational>(total, ReferenceToSource {}, VariableLog { &heard });
    CHECK(heard == decidedByIsEntered);
}
