// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/formula.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace
{
namespace unit = formula::unit;
using formula::Rational;
using formula::var;

/// How many determinations a sample holds. A plain quantity for now: task 5's
/// `sample_count` does not exist yet, and a critical value does not care where
/// its count comes from.
struct Specimens: formula::Quantity<Specimens, "n", "number of determinations", unit::One>
{
};
/// The critical value read from the table, a bare number.
struct CriticalLimit: formula::Quantity<CriticalLimit, "c", "critical value", unit::One>
{
};
/// The same, stated in percent, so that the table's result unit is not the
/// coherent one.
struct CriticalPercent: formula::Quantity<CriticalPercent, "c_p", "critical value in percent", unit::Percent>
{
};
/// A mass whose number of grams an invented rule reads as its count.
struct CountedMass: formula::Quantity<CountedMass, "m_n", "mass read as a count", unit::Kilogram>
{
};
/// A limit that has a dimension, so that the node's dimension can be seen to
/// come from the table's result unit and not from its count.
struct StrengthLimit: formula::Quantity<StrengthLimit, "f_lim", "strength limit", unit::Megapascal>
{
};

// The shared fixtures' critical-value table. **Invented, and deliberately
// unrealistic -- no published table holds values like these, or dips as this
// one does -- so that nobody mistakes it for one or "corrects" it toward
// one.** Sizes 3, 4, 5, 6 and 8, with no 7, so a seven-element pass misses;
// values 10, 30, 20, 50 and 40, every neighbour distinct from the next, and
// the mean of the two rows around the hole (45) in no row.
inline constexpr formula::SampleSizeTable<5> DeviationSizes { 3, 4, 5, 6, 8 };

template <formula::Unit ResultUnit = unit::One>
[[nodiscard]] constexpr auto deviationLimit()
{
    return formula::critical_value<DeviationSizes, ResultUnit>(
        var<Specimens>, { Rational { 10 }, Rational { 30 }, Rational { 20 }, Rational { 50 }, Rational { 40 } });
}

/// The critical value at @p count, or the error or absence that came instead.
[[nodiscard]] constexpr auto limitAt(Rational count)
{
    return formula::checked_evaluate<CriticalLimit>(deviationLimit(),
                                                    formula::environment(formula::Measured<Specimens> { count }));
}
} // namespace

TEST_CASE("critical_value reads the row whose size is the count", "[critical-value]")
{
    // 3 is the first row: an off-by-one row would give 30.
    STATIC_REQUIRE(limitAt(Rational { 3 })->measurement().value() == Rational { 10 });
    // 5 is in the middle of values that are not monotone: a search that
    // assumed ascending values could land beside it, on 30 or 50.
    STATIC_REQUIRE(limitAt(Rational { 5 })->measurement().value() == Rational { 20 });
    STATIC_REQUIRE(limitAt(Rational { 6 })->measurement().value() == Rational { 50 });
    // 8 is the last row, after the hole: a row read one position early would
    // give 50.
    STATIC_REQUIRE(limitAt(Rational { 8 })->measurement().value() == Rational { 40 });
}

TEST_CASE("critical_value's dimension is its result unit's, never its count's", "[critical-value]")
{
    // The same table in megapascals: 50 MPa at 6, 5 * 10^7 in the coherent
    // unit, and a pressure -- a node taking its dimension from the count
    // would call it a bare number.
    constexpr auto inMegapascal = deviationLimit<unit::Megapascal>();
    STATIC_REQUIRE(decltype(inMegapascal)::dimension == formula::dim::Pressure);
    constexpr auto inputs = formula::environment(formula::Measured<Specimens> { Rational { 6 } });
    STATIC_REQUIRE(formula::checked_evaluate<StrengthLimit>(inMegapascal, inputs)->measurement().value() == Rational { 50 });
    STATIC_REQUIRE(formula::checked_evaluate_si(inMegapascal, inputs)->value() == Rational { 50'000'000 });
}

TEST_CASE("critical_value accepts numeric_value_of as its count, whatever unit it reads its operand in", "[critical-value]")
{
    // numeric_value_of yields a bare number: the gram it names is the unit
    // it reads its operand in, not the unit of its result, so the count is
    // not "stated in grams". 6/1000 kg read in grams is 6, the row giving 50;
    // read in the coherent unit it would be 6/1000, no row at all.
    constexpr auto gramsCounted =
        formula::numeric_value_of<unit::Gram, "an invented rule counts the grams">(var<CountedMass>);
    constexpr auto limitByGrams = formula::critical_value<DeviationSizes, unit::One>(
        gramsCounted, { Rational { 10 }, Rational { 30 }, Rational { 20 }, Rational { 50 }, Rational { 40 } });
    STATIC_REQUIRE(
        formula::checked_evaluate<CriticalLimit>(
            limitByGrams, formula::environment(formula::Measured<CountedMass> { Rational::make(6, 1000).value() }))
            ->measurement()
            .value()
        == Rational { 50 });
}

TEST_CASE("critical_value misses a count the table does not declare, and never guesses", "[critical-value]")
{
    // 7 is the hole: the nearest row would give 50 or 40, interpolation 45.
    STATIC_REQUIRE(limitAt(Rational { 7 }).error() == formula::ArithmeticError::DomainError);
    // Below the first row and above the last: clamping would give 10 and 40.
    STATIC_REQUIRE(limitAt(Rational { 2 }).error() == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(limitAt(Rational { 9 }).error() == formula::ArithmeticError::DomainError);
    STATIC_REQUIRE(limitAt(Rational { 0 }).error() == formula::ArithmeticError::DomainError);
    // 2^32 + 3 is a whole count, far above every row. Narrowed to a 32-bit
    // size it would be 3, and read 10; compared as the count it is, it misses.
    STATIC_REQUIRE(limitAt(Rational { (std::int64_t { 1 } << 32) + 3 }).error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("critical_value refuses a count that is not a whole, non-negative number", "[critical-value]")
{
    // 5.5: truncation would give 20 and rounding 50.
    STATIC_REQUIRE(limitAt(Rational { 11, 2 }).error() == formula::ArithmeticError::DomainError);
    // -3: a magnitude taken of it would hit the first row.
    STATIC_REQUIRE(limitAt(Rational { -3 }).error() == formula::ArithmeticError::DomainError);
}

TEST_CASE("critical_value of an absent count is absent", "[critical-value]")
{
    constexpr auto outcome = formula::checked_evaluate<CriticalLimit>(
        deviationLimit(), formula::environment(formula::Measured<Specimens>::absent()));
    STATIC_REQUIRE(outcome.has_value());
    STATIC_REQUIRE(outcome->is_empty());
}

TEST_CASE("critical_value states its values in its own result unit", "[critical-value]")
{
    // The same table in percent: row 6 gives 50 %, which is 1/2 in the
    // coherent unit. A value taken as coherent would read 5000 %.
    constexpr auto inPercent = deviationLimit<unit::Percent>();
    constexpr auto inputs = formula::environment(formula::Measured<Specimens> { Rational { 6 } });
    STATIC_REQUIRE(formula::checked_evaluate<CriticalPercent>(inPercent, inputs)->measurement().value() == Rational { 50 });
    STATIC_REQUIRE(formula::checked_evaluate<CriticalLimit>(inPercent, inputs)->measurement().value() == Rational { 1, 2 });
}

TEST_CASE("sample_size_table_is_well_formed accepts ascending sizes of at least one, and nothing else", "[critical-value]")
{
    STATIC_REQUIRE(formula::sample_size_table_is_well_formed(DeviationSizes));
    STATIC_REQUIRE(formula::sample_size_table_is_well_formed(formula::SampleSizeTable<0> {}));
    STATIC_REQUIRE(formula::sample_size_table_is_well_formed(formula::SampleSizeTable<1> { 1 }));
    // A size of zero names no sample, anywhere in the table.
    STATIC_REQUIRE(!formula::sample_size_table_is_well_formed(formula::SampleSizeTable<1> { 0 }));
    STATIC_REQUIRE(!formula::sample_size_table_is_well_formed(formula::SampleSizeTable<3> { 0, 3, 4 }));
    // A repeated size makes its second row unreachable; a descending pair,
    // anywhere, breaks the order.
    STATIC_REQUIRE(!formula::sample_size_table_is_well_formed(formula::SampleSizeTable<3> { 3, 4, 4 }));
    STATIC_REQUIRE(!formula::sample_size_table_is_well_formed(formula::SampleSizeTable<3> { 3, 5, 4 }));
    // The runtime check and the declared-table refusal share one loop, and
    // that loop names the row the refusal reports: the second size of the
    // first pair that breaks the order.
    STATIC_REQUIRE(formula::detail::first_bad_sample_size(formula::SampleSizeTable<4> { 3, 5, 4, 2 }) == 2);
    STATIC_REQUIRE(formula::detail::first_bad_sample_size(DeviationSizes) == 5);
    // The very first pair is a pair too: a loop starting its pairs one row
    // late accepts this table.
    STATIC_REQUIRE(!formula::sample_size_table_is_well_formed(formula::SampleSizeTable<3> { 4, 3, 5 }));
    STATIC_REQUIRE(formula::detail::first_bad_sample_size(formula::SampleSizeTable<3> { 4, 3, 5 }) == 1);
}

TEST_CASE("critical_value reads its table at runtime too", "[critical-value]")
{
    // The cases above are evaluated by the compiler; these run, so that a
    // sanitizer sees the lookup execute.
    struct Case
    {
        Rational count;
        bool hits;
        Rational expected;
    };
    std::array<Case, 7> const cases { {
        { Rational { 3 }, true, Rational { 10 } },
        { Rational { 5 }, true, Rational { 20 } },
        { Rational { 8 }, true, Rational { 40 } },
        { Rational { 7 }, false, Rational {} },
        { Rational { 9 }, false, Rational {} },
        { Rational { 11, 2 }, false, Rational {} },
        { Rational { (std::int64_t { 1 } << 32) + 3 }, false, Rational {} },
    } };
    for (Case const& each: cases)
    {
        auto const outcome = limitAt(each.count);
        REQUIRE(outcome.has_value() == each.hits);
        if (each.hits)
            CHECK(outcome->measurement().value() == each.expected);
        else
            CHECK(outcome.error() == formula::ArithmeticError::DomainError);
    }
}
