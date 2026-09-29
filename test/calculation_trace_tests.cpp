// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/citation.hpp>
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include "household_bill.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace
{
namespace unit = formula::unit;
using formula::var;

constexpr formula::Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational { numerator, denominator };
}

// Invented quantities; every value below is invented too.
struct Factor: formula::Quantity<Factor, "k", "an invented factor", unit::One>
{
};
struct Other: formula::Quantity<Other, "k_o", "another invented factor", unit::One>
{
};
struct Share: formula::Quantity<Share, "s", "an invented share", unit::One>
{
};
struct Halved: formula::Quantity<Halved, "s_h", "an invented share, halved", unit::One>
{
};
struct Doubled: formula::Quantity<Doubled, "s_2", "an invented share, doubled", unit::One>
{
};
struct Width: formula::Quantity<Width, "b", "an invented width", unit::Millimetre>
{
};
/// A share whose declared symbol ends in a newline, as no symbol should.
struct AwkwardShare: formula::Quantity<AwkwardShare, "s\n", "an invented share, awkwardly written", unit::One>
{
};
struct Depth: formula::Quantity<Depth, "d", "an invented depth", unit::Millimetre>
{
};

/// A share of two factors, which fails when the other factor is zero; a
/// value that reads the share only where the other factor is not zero, and
/// the factor where it is; and a value that reads the share whatever happens.
inline constexpr auto guarded =
    formula::calculation(formula::define<Share>(var<Factor> / var<Other>),
                         formula::define<Halved>(
                             formula::when(var<Other> > formula::constant<unit::One>(rat(0)), var<Share>, var<Factor>)),
                         formula::define<Doubled>(var<Share> * rat(2)));

/// `guarded`'s worksheet over @p factor and @p other.
inline auto guarded_sheet(formula::Rational factor, formula::Rational other)
{
    return formula::worksheet(
        guarded, formula::environment(formula::Measured<Factor> { factor }, formula::Measured<Other> { other }));
}

constexpr formula::Citation clause { .reference = "Example Standard 7:2024", .section = "2" };

struct Plain
{
};

/// A method whose one formula multiplies the factor by the other factor, and
/// two overlays of it: one fixing the factor, one deriving it from the width
/// and the depth.
inline constexpr auto shareMethod = formula::method(
    formula::variants(formula::variant<Plain>(var<Factor> * var<Other>)),
    formula::rounding_rule<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints());
inline constexpr auto fixedFactor =
    formula::apply(formula::overlay(formula::with_constant<Factor>(rat(103, 100), clause)), shareMethod);
inline constexpr auto derivedFactor =
    formula::apply(formula::overlay(formula::add_derived<Factor>(var<Width> / var<Depth>, clause)), shareMethod);

/// The symbols of @p explained's blocks, in order.
template <typename Explained>
std::vector<std::string_view> block_symbols(Explained const& explained)
{
    std::vector<std::string_view> symbols;
    for (formula::WorksheetEntry const& shown: explained.entries)
        symbols.push_back(shown.symbol);
    return symbols;
}

/// The block for @p symbol in @p explained, or null.
template <typename Explained>
formula::WorksheetEntry const* block_of(Explained const& explained, std::string_view symbol)
{
    for (formula::WorksheetEntry const& shown: explained.entries)
        if (shown.symbol == symbol)
            return &shown;
    return nullptr;
}

/// The `Variable` step for @p symbol in @p shown's trace, or null.
formula::Step<formula::Rational> const* variable_step(formula::WorksheetEntry const& shown, std::string_view symbol)
{
    for (formula::Step<formula::Rational> const& recorded: shown.trace.steps)
        if (recorded.kind == formula::StepKind::Variable && recorded.symbol == symbol)
            return &recorded;
    return nullptr;
}

/// The block for @p symbol in @p explained, which must have one.
template <typename Explained>
formula::WorksheetEntry const& named_block(Explained const& explained, std::string_view symbol)
{
    INFO("the block for " << symbol);
    formula::WorksheetEntry const* const found = block_of(explained, symbol);
    REQUIRE(found != nullptr);
    return *found;
}

/// The `Variable` step for @p symbol in @p shown's trace, which must have one.
formula::Step<formula::Rational> const& named_step(formula::WorksheetEntry const& shown, std::string_view symbol)
{
    INFO("the step reading " << symbol << " in the block for " << shown.symbol);
    formula::Step<formula::Rational> const* const found = variable_step(shown, symbol);
    REQUIRE(found != nullptr);
    return *found;
}

/// The first step of @p stepKind in @p shown's trace, or null.
formula::Step<formula::Rational> const* step_of_kind(formula::WorksheetEntry const& shown, formula::StepKind stepKind)
{
    for (formula::Step<formula::Rational> const& recorded: shown.trace.steps)
        if (recorded.kind == stepKind)
            return &recorded;
    return nullptr;
}

/// @p shown's root, in the unit @p shown states its value in: empty when the
/// root has no value or it does not convert.
std::optional<formula::Rational> root_in_declared_unit(formula::WorksheetEntry const& shown)
{
    formula::Step<formula::Rational> const& root = shown.trace.steps[shown.trace.root()];
    if (!root.value.has_value())
        return std::nullopt;
    std::expected<formula::Rational, formula::ArithmeticError> const converted =
        formula::checked_convert(*root.value, formula::coherent(shown.unit.dimension), shown.unit);
    if (!converted.has_value())
        return std::nullopt;
    return *converted;
}
} // namespace

TEST_CASE("a worksheet's derivation gives the result first, then what it was reached through, then the inputs",
          "[calculation][worksheet][trace]")
{
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    auto const explained = formula::explain_worksheet<Total>(sheet);

    REQUIRE(explained.outcome.has_value());
    CHECK(explained.outcome->measurement() == formula::Measured<Total> { rat(591311, 5000) });
    CHECK(explained.outcome == sheet.checked_calculate<Total>());

    // Every calculated value, each before the values it reads -- the reverse
    // of the order they are calculated in -- then the ten inputs in the order
    // the calculation numbers them.
    CHECK(block_symbols(explained)
          == std::vector<std::string_view> {
              "total",      "vat",        "subtotal", "energy_cost", "feed_in_credit",
              "grid_cost",  "net_draw",   "exported", "self_used",   "monthly_load",
              "daily_load", "heater_kwh", "oven_kwh", "fridge_kwh",  "fridge_kw",
              "fridge_w",   "fridge_h",   "oven_kw",  "oven_h",      "heater_kw",
              "heater_h",   "solar",      "price",    "feed_in",     "base_fee",
          });
    for (std::size_t position = 0; position < explained.entries.size(); ++position)
        CHECK(explained.entries[position].kind
              == (position < 15 ? formula::WorksheetEntryKind::Calculated : formula::WorksheetEntryKind::Input));
    CHECK(explained.entries.front().slot == BillGraph::slot_of<Total>);
    CHECK(explained.entries.back().slot == BillGraph::slot_of<BaseFee>);

    // Asking calculated the fifteen values; recording the blocks calculated
    // nothing again, and neither does recording them a second time.
    CHECK(sheet.recomputed() == 15);
    auto const again = formula::explain_worksheet<Total>(sheet);
    CHECK(block_symbols(again) == block_symbols(explained));
    CHECK(sheet.recomputed() == 15);
    CHECK(sheet.reused() == 0);
}

TEST_CASE("a calculated value read in a derivation is one step, marked calculated", "[calculation][worksheet][trace]")
{
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    auto const explained = formula::explain_worksheet<Total>(sheet);

    formula::WorksheetEntry const* const total = block_of(explained, "total");
    REQUIRE(total != nullptr);
    // The subtotal plus the tax: three steps, the two reads and the sum.
    CHECK(total->trace.steps.size() == 3);
    formula::Step<formula::Rational> const* const subtotal = variable_step(*total, "subtotal");
    formula::Step<formula::Rational> const* const vat = variable_step(*total, "vat");
    REQUIRE(subtotal != nullptr);
    REQUIRE(vat != nullptr);
    CHECK(subtotal->inputSource == formula::ValueSource::Derived);
    CHECK(vat->inputSource == formula::ValueSource::Derived);

    // The fridge's energy reads its power, calculated, and its hours, an
    // input.
    formula::WorksheetEntry const* const fridgeKwh = block_of(explained, "fridge_kwh");
    REQUIRE(fridgeKwh != nullptr);
    CHECK(named_step(*fridgeKwh, "fridge_kw").inputSource == formula::ValueSource::Derived);
    CHECK(named_step(*fridgeKwh, "fridge_h").inputSource == formula::ValueSource::Measured);

    // An input's own block is one step, as the worksheet holds it.
    formula::WorksheetEntry const* const price = block_of(explained, "price");
    REQUIRE(price != nullptr);
    REQUIRE(price->trace.steps.size() == 1);
    CHECK(price->trace.steps.front().inputSource == formula::ValueSource::Measured);
    CHECK(price->value == rat(8, 25));
}

TEST_CASE("each block's root is the value the worksheet holds", "[calculation][worksheet][trace]")
{
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    auto const explained = formula::explain_worksheet<Total>(sheet);

    REQUIRE(explained.entries.size() == 25);
    for (formula::WorksheetEntry const& shown: explained.entries)
    {
        INFO(shown.symbol);
        REQUIRE_FALSE(shown.trace.empty());
        REQUIRE(shown.value.has_value());
        CHECK_FALSE(shown.error.has_value());
        CHECK(root_in_declared_unit(shown) == shown.value);
    }
    CHECK(named_block(explained, "total").value == rat(591311, 5000));
    CHECK(named_block(explained, "net_draw").value == rat(279));
    CHECK(named_block(explained, "fridge_kw").value == rat(1, 5));
    CHECK(named_block(explained, "fridge_kw").unit == Kilowatt);
    CHECK(named_block(explained, "fridge_w").value == rat(200));
}

TEST_CASE("a derivation after an early cutoff describes the current inputs", "[calculation][worksheet][trace]")
{
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    REQUIRE(sheet.calculate<Total>().measurement() == formula::Measured<Total> { rat(591311, 5000) });

    // Twice the fridge's power for half the hours: its energy a day comes out
    // the same, so the eight values after it are reused, not calculated
    // again -- and their derivation still reads 400 W for 12 h.
    sheet.set(formula::Measured<FridgeW> { rat(400) }, formula::Measured<FridgeH> { rat(12) });
    auto const explained = formula::explain_worksheet<Total>(sheet);
    CHECK(sheet.recomputed() == 17);
    CHECK(sheet.reused() == 8);

    formula::WorksheetEntry const* const fridgeKw = block_of(explained, "fridge_kw");
    formula::WorksheetEntry const* const fridgeKwh = block_of(explained, "fridge_kwh");
    REQUIRE(fridgeKw != nullptr);
    REQUIRE(fridgeKwh != nullptr);
    // In the coherent SI unit: 400 W, and 12 h is 43,200 s.
    CHECK(named_step(*fridgeKw, "fridge_w").value == rat(400));
    CHECK(named_step(*fridgeKwh, "fridge_kw").value == rat(400));
    CHECK(named_step(*fridgeKwh, "fridge_h").value == rat(43200));
    CHECK(fridgeKw->value == rat(2, 5));
    CHECK(fridgeKwh->value == rat(24, 5));
    CHECK(named_block(explained, "fridge_w").value == rat(400));
    CHECK(named_block(explained, "fridge_h").value == rat(12));

    // Nowhere the old 200 W, nor the old 24 h.
    for (formula::WorksheetEntry const& shown: explained.entries)
        for (formula::Step<formula::Rational> const& recorded: shown.trace.steps)
        {
            INFO(shown.symbol);
            if (recorded.symbol == "fridge_w" || recorded.symbol == "fridge_kw")
                CHECK(recorded.value == rat(400));
            if (recorded.symbol == "fridge_h")
                CHECK(recorded.value == rat(43200));
        }
    for (formula::WorksheetEntry const& shown: explained.entries)
    {
        INFO(shown.symbol);
        CHECK(root_in_declared_unit(shown) == shown.value);
    }
}

TEST_CASE("an override is a block of its own, and a value read only through it gets none",
          "[calculation][worksheet][trace]")
{
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    sheet.set(formula::entered(formula::Measured<NetDraw> { rat(250) }));
    auto const explained = formula::explain_worksheet<Total>(sheet);
    REQUIRE(explained.outcome.has_value());
    CHECK(explained.outcome->measurement() == formula::Measured<Total> { rat(107219, 1000) });

    CHECK(block_symbols(explained)
          == std::vector<std::string_view> { "total",
                                             "vat",
                                             "subtotal",
                                             "energy_cost",
                                             "feed_in_credit",
                                             "grid_cost",
                                             "net_draw",
                                             "exported",
                                             "self_used",
                                             "solar",
                                             "price",
                                             "feed_in",
                                             "base_fee" });
    formula::WorksheetEntry const* const netDraw = block_of(explained, "net_draw");
    REQUIRE(netDraw != nullptr);
    CHECK(netDraw->kind == formula::WorksheetEntryKind::Overridden);
    CHECK(netDraw->value == rat(250));
    REQUIRE(netDraw->trace.steps.size() == 1);
    CHECK(netDraw->trace.steps.front().kind == formula::StepKind::Variable);
    CHECK(netDraw->trace.steps.front().inputSource == formula::ValueSource::ManuallyEntered);
    // The grid cost reads it as a value typed in, not calculated.
    CHECK(named_step(named_block(explained, "grid_cost"), "net_draw").inputSource
          == formula::ValueSource::ManuallyEntered);

    // Asked for itself, the override is the one block.
    auto const alone = formula::explain_worksheet<NetDraw>(sheet);
    CHECK(block_symbols(alone) == std::vector<std::string_view> { "net_draw" });
    CHECK(alone.entries.front().kind == formula::WorksheetEntryKind::Overridden);
}

TEST_CASE("an input asked for is its one block", "[calculation][worksheet][trace]")
{
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    sheet.set(formula::entered(formula::Measured<Price> { rat(1, 4) }));
    auto const explained = formula::explain_worksheet<Price>(sheet);
    REQUIRE(explained.outcome.has_value());
    CHECK(explained.outcome->source() == formula::ValueSource::ManuallyEntered);
    REQUIRE(explained.entries.size() == 1);
    CHECK(explained.entries.front().kind == formula::WorksheetEntryKind::Input);
    CHECK(explained.entries.front().value == rat(1, 4));
    CHECK(explained.entries.front().trace.steps.front().inputSource == formula::ValueSource::ManuallyEntered);
    CHECK(sheet.recomputed() == 0);
}

TEST_CASE("a value only a when() branch not taken reads has no block", "[calculation][worksheet][trace]")
{
    // The other factor is zero: the halved value reads the factor, and the
    // share -- which fails -- is not read, and has no block.
    auto sheet = guarded_sheet(rat(3), rat(0));
    auto const untaken = formula::explain_worksheet<Halved>(sheet);
    REQUIRE(untaken.outcome.has_value());
    CHECK(untaken.outcome->measurement() == formula::Measured<Halved> { rat(3) });
    CHECK(block_symbols(untaken) == std::vector<std::string_view> { "s_h", "k", "k_o" });

    // Not zero: the share is read, and has its block.
    sheet.set(formula::Measured<Other> { rat(2) });
    auto const taken = formula::explain_worksheet<Halved>(sheet);
    CHECK(block_symbols(taken) == std::vector<std::string_view> { "s_h", "s", "k", "k_o" });
    CHECK(named_block(taken, "s").value == rat(3, 2));
}

TEST_CASE("a value to the right of an operand that failed has no block", "[calculation][worksheet][trace]")
{
    // The doubled share adds the halved factor to the share. The other
    // factor is zero, so the share fails, and the sum stops there: the
    // halved factor is calculated, but the answer is not reached through it,
    // and it has no block.
    constexpr auto rightOfFailure = formula::calculation(formula::define<Share>(var<Factor> / var<Other>),
                                                         formula::define<Halved>(var<Factor> * rat(1, 2)),
                                                         formula::define<Doubled>(var<Share> + var<Halved>));
    auto sheet = formula::worksheet(
        rightOfFailure, formula::environment(formula::Measured<Factor> { rat(3) }, formula::Measured<Other> { rat(0) }));
    auto const failed = formula::explain_worksheet<Doubled>(sheet);
    REQUIRE_FALSE(failed.outcome.has_value());
    CHECK(failed.outcome.error() == formula::ArithmeticError::DivisionByZero);
    CHECK(sheet.recomputed() == 3);
    CHECK(block_symbols(failed) == std::vector<std::string_view> { "s_2", "s", "k", "k_o" });
    CHECK(variable_step(named_block(failed, "s_2"), "s_h") == nullptr);

    // Once the share holds a value, the sum reads the halved factor too.
    sheet.set(formula::Measured<Other> { rat(2) });
    auto const reached = formula::explain_worksheet<Doubled>(sheet);
    REQUIRE(reached.outcome.has_value());
    CHECK(reached.outcome->measurement() == formula::Measured<Doubled> { rat(3) });
    CHECK(block_symbols(reached) == std::vector<std::string_view> { "s_2", "s_h", "s", "k", "k_o" });
    CHECK(named_block(reached, "s_h").value == rat(3, 2));
}

TEST_CASE("a failed value is a block like any other", "[calculation][worksheet][trace]")
{
    auto sheet = guarded_sheet(rat(3), rat(0));
    auto const explained = formula::explain_worksheet<Doubled>(sheet);
    REQUIRE_FALSE(explained.outcome.has_value());
    CHECK(explained.outcome.error() == formula::ArithmeticError::DivisionByZero);

    CHECK(block_symbols(explained) == std::vector<std::string_view> { "s_2", "s", "k", "k_o" });
    for (std::string_view const failed: { "s_2", "s" })
    {
        INFO(failed);
        formula::WorksheetEntry const* const shown = block_of(explained, failed);
        REQUIRE(shown != nullptr);
        CHECK(shown->kind == formula::WorksheetEntryKind::Calculated);
        CHECK_FALSE(shown->value.has_value());
        CHECK(shown->error == formula::ArithmeticError::DivisionByZero);
        CHECK(shown->trace.steps[shown->trace.root()].error == formula::ArithmeticError::DivisionByZero);
    }
    // The doubled share reads the share's failure as its own read's.
    formula::Step<formula::Rational> const* const shareRead = variable_step(named_block(explained, "s_2"), "s");
    REQUIRE(shareRead != nullptr);
    CHECK(shareRead->error == formula::ArithmeticError::DivisionByZero);
    CHECK(shareRead->inputSource == formula::ValueSource::Derived);
}

TEST_CASE("a derivation writes each symbol as its vocabulary says", "[calculation][worksheet][trace][vocabulary]")
{
    using namespace household;
    constexpr auto words =
        formula::vocabulary(formula::renames<NetDraw>("E_grid"), formula::renames<Total>("C_bill"));
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    auto const explained = formula::explain_worksheet<Total>(sheet, words);
    CHECK(explained.entries.front().symbol == "C_bill");
    CHECK(block_of(explained, "E_grid") != nullptr);
    CHECK(block_of(explained, "net_draw") == nullptr);
    CHECK(variable_step(named_block(explained, "grid_cost"), "E_grid") != nullptr);
    // It keeps the calculation and the vocabulary its blocks were written in.
    CHECK(formula::calculation_order(explained.calculation) == formula::calculation_order(bill));
    CHECK(formula::symbol_of<NetDraw>(explained.vocabulary) == "E_grid");
}

TEST_CASE("a value an overlay replaced is reported only where the definition reads it itself",
          "[calculation][worksheet][trace][overlay]")
{
    constexpr auto fixedShare = std::get<0>(fixedFactor.variantSet.cases).expression;
    constexpr auto derivedShare = std::get<0>(derivedFactor.variantSet.cases).expression;

    // The worksheet holds the factor, 97/100, which the doubled share reads.
    // The share's formula fixes its own factor at 103/100 and reads nothing
    // of the worksheet's: its step names no value it replaced.
    constexpr auto fixedCalculation =
        formula::calculation(formula::define<Share>(fixedShare), formula::define<Doubled>(var<Factor> * rat(2)));
    auto fixedSheet = formula::worksheet(
        fixedCalculation,
        formula::environment(formula::Measured<Factor> { rat(97, 100) }, formula::Measured<Other> { rat(139, 100) }));
    auto const fixedExplained = formula::explain_worksheet<Share>(fixedSheet);
    REQUIRE(fixedExplained.outcome.has_value());
    CHECK(fixedExplained.outcome->measurement() == formula::Measured<Share> { rat(103 * 139, 100 * 100) });
    CHECK(block_symbols(fixedExplained) == std::vector<std::string_view> { "s", "k_o" });
    formula::Step<formula::Rational> const* const fixedStep =
        step_of_kind(fixedExplained.entries.front(), formula::StepKind::OverriddenConstant);
    REQUIRE(fixedStep != nullptr);
    CHECK_FALSE(fixedStep->inputSource.has_value());
    CHECK_FALSE(fixedStep->replacedEntryEmpty);

    // The same for a factor the overlay derives from the width and the depth.
    constexpr auto derivedCalculation =
        formula::calculation(formula::define<Share>(derivedShare), formula::define<Doubled>(var<Factor> * rat(2)));
    auto derivedSheet = formula::worksheet(derivedCalculation,
                                           formula::environment(formula::Measured<Width> { rat(103) },
                                                                formula::Measured<Depth> { rat(127) },
                                                                formula::Measured<Other> { rat(139, 100) },
                                                                formula::Measured<Factor> { rat(97, 100) }));
    auto const derivedExplained = formula::explain_worksheet<Share>(derivedSheet);
    CHECK(block_symbols(derivedExplained) == std::vector<std::string_view> { "s", "b", "d", "k_o" });
    formula::Step<formula::Rational> const* const derivedStep =
        step_of_kind(derivedExplained.entries.front(), formula::StepKind::DerivedQuantity);
    REQUIRE(derivedStep != nullptr);
    CHECK_FALSE(derivedStep->inputSource.has_value());

    // A definition that reads the factor itself as well: the value it reads
    // is the one the fixed constant replaced, and its step says so.
    constexpr auto readingCalculation = formula::calculation(formula::define<Share>(var<Factor> + fixedShare));
    auto readingSheet = formula::worksheet(
        readingCalculation,
        formula::environment(formula::Measured<Factor> { rat(97, 100) }, formula::Measured<Other> { rat(139, 100) }));
    auto const readingExplained = formula::explain_worksheet<Share>(readingSheet);
    CHECK(block_symbols(readingExplained) == std::vector<std::string_view> { "s", "k", "k_o" });
    formula::Step<formula::Rational> const* const readStep =
        step_of_kind(readingExplained.entries.front(), formula::StepKind::OverriddenConstant);
    REQUIRE(readStep != nullptr);
    CHECK(readStep->inputSource == formula::ValueSource::Measured);
    CHECK_FALSE(readStep->replacedEntryEmpty);
}

TEST_CASE("a calculated value an overlay replaced is reported only once the worksheet brought it up to date",
          "[calculation][worksheet][trace][overlay]")
{
    constexpr auto fixedShare = std::get<0>(fixedFactor.variantSet.cases).expression;
    auto const inputs = formula::environment(formula::Measured<Width> { rat(97) },
                                             formula::Measured<Depth> { rat(100) },
                                             formula::Measured<Other> { rat(139, 100) });

    // The factor is calculated from the width and the depth, and the share
    // reads only the fixed constant standing for it: asking for the share
    // never calculates the factor, and the constant's step says nothing of
    // it.
    constexpr auto unreadCalculation = formula::calculation(formula::define<Factor>(var<Width> / var<Depth>),
                                                            formula::define<Share>(fixedShare));
    auto unreadSheet = formula::worksheet(unreadCalculation, inputs);
    auto const unread = formula::explain_worksheet<Share>(unreadSheet);
    REQUIRE(unread.outcome.has_value());
    CHECK(unread.outcome->measurement() == formula::Measured<Share> { rat(103 * 139, 100 * 100) });
    CHECK(unreadSheet.recomputed() == 1);
    CHECK(block_symbols(unread) == std::vector<std::string_view> { "s", "k_o" });
    formula::Step<formula::Rational> const* const unreadStep =
        step_of_kind(unread.entries.front(), formula::StepKind::OverriddenConstant);
    REQUIRE(unreadStep != nullptr);
    CHECK_FALSE(unreadStep->inputSource.has_value());

    // The share reads the factor itself as well, 97/100 plus 103/100 times
    // 139/100: the factor is calculated first, and the constant's step says
    // the value it replaced was calculated.
    constexpr auto readingCalculation = formula::calculation(formula::define<Factor>(var<Width> / var<Depth>),
                                                             formula::define<Share>(var<Factor> + fixedShare));
    auto sheet = formula::worksheet(readingCalculation, inputs);
    auto const calculated = formula::explain_worksheet<Share>(sheet);
    REQUIRE(calculated.outcome.has_value());
    CHECK(calculated.outcome->measurement() == formula::Measured<Share> { rat(24017, 10000) });
    CHECK(sheet.recomputed() == 2);
    CHECK(block_symbols(calculated) == std::vector<std::string_view> { "s", "k", "b", "d", "k_o" });
    CHECK(named_block(calculated, "k").kind == formula::WorksheetEntryKind::Calculated);
    formula::Step<formula::Rational> const* const calculatedStep =
        step_of_kind(calculated.entries.front(), formula::StepKind::OverriddenConstant);
    REQUIRE(calculatedStep != nullptr);
    CHECK(calculatedStep->inputSource == formula::ValueSource::Derived);
    CHECK_FALSE(calculatedStep->replacedEntryEmpty);

    // The factor overridden by hand, 101/100: the value the constant
    // replaced was typed in, and what the factor was calculated from is no
    // longer read.
    sheet.set(formula::entered(formula::Measured<Factor> { rat(101, 100) }));
    auto const overridden = formula::explain_worksheet<Share>(sheet);
    REQUIRE(overridden.outcome.has_value());
    CHECK(overridden.outcome->measurement() == formula::Measured<Share> { rat(24417, 10000) });
    CHECK(block_symbols(overridden) == std::vector<std::string_view> { "s", "k", "k_o" });
    CHECK(named_block(overridden, "k").kind == formula::WorksheetEntryKind::Overridden);
    formula::Step<formula::Rational> const* const overriddenStep =
        step_of_kind(overridden.entries.front(), formula::StepKind::OverriddenConstant);
    REQUIRE(overriddenStep != nullptr);
    CHECK(overriddenStep->inputSource == formula::ValueSource::ManuallyEntered);
    CHECK_FALSE(overriddenStep->replacedEntryEmpty);
}

TEST_CASE("an empty entry an overlay replaced is reported as empty, and has a block even from a branch not taken",
          "[calculation][worksheet][trace][overlay]")
{
    constexpr auto fixedShare = std::get<0>(fixedFactor.variantSet.cases).expression;

    // The factor is an input left empty. The share reads the fixed constant
    // standing for it on the branch taken, and the factor itself only on the
    // branch not taken: the constant's step says the entry it replaced held
    // no value, and saying so read the factor, which has its block.
    constexpr auto branchCalculation = formula::calculation(formula::define<Share>(
        formula::when(var<Other> > formula::constant<unit::One>(rat(0)), fixedShare, var<Factor>)));
    auto sheet = formula::worksheet(
        branchCalculation,
        formula::environment(formula::Measured<Factor>::absent(), formula::Measured<Other> { rat(139, 100) }));
    auto const explained = formula::explain_worksheet<Share>(sheet);
    REQUIRE(explained.outcome.has_value());
    CHECK(explained.outcome->measurement() == formula::Measured<Share> { rat(103 * 139, 100 * 100) });
    formula::Step<formula::Rational> const* const emptyStep =
        step_of_kind(explained.entries.front(), formula::StepKind::OverriddenConstant);
    REQUIRE(emptyStep != nullptr);
    CHECK(emptyStep->replacedEntryEmpty);
    CHECK(emptyStep->inputSource == formula::ValueSource::Measured);
    CHECK(block_symbols(explained) == std::vector<std::string_view> { "s", "k_o", "k" });
    formula::WorksheetEntry const& factor = named_block(explained, "k");
    CHECK(factor.kind == formula::WorksheetEntryKind::Input);
    CHECK_FALSE(factor.value.has_value());
}

TEST_CASE("a derivation renders each block under its header, and the inputs last", "[calculation][worksheet][trace]")
{
    auto sheet = guarded_sheet(rat(3), rat(2));
    auto const explained = formula::explain_worksheet<Doubled>(sheet);
    std::string const everything = "s_2 = s * 2 = 3\n"
                                   "  1. s = 3/2, calculated\n"
                                   "  2. 2\n"
                                   "  3. #1 * #2 = 3\n"
                                   "s = k / k_o = 3/2\n"
                                   "  1. k = 3\n"
                                   "  2. k_o = 2\n"
                                   "  3. #1 / #2 = 3/2\n"
                                   "inputs\n"
                                   "  k = 3\n"
                                   "  k_o = 2\n";
    // Ten lines: two headers, six steps, two inputs.
    CHECK(formula::render_derivation(explained, { .maxSteps = 10 }) == everything);
    CHECK(formula::render_derivation(explained, { .maxSteps = 100 }) == everything);
}

TEST_CASE("a derivation's one budget counts every header, step and input line", "[calculation][worksheet][trace]")
{
    auto sheet = guarded_sheet(rat(3), rat(2));
    auto const explained = formula::explain_worksheet<Doubled>(sheet);

    // One short: the last input.
    CHECK(formula::render_derivation(explained, { .maxSteps = 9 }) == "s_2 = s * 2 = 3\n"
                                                                      "  1. s = 3/2, calculated\n"
                                                                      "  2. 2\n"
                                                                      "  3. #1 * #2 = 3\n"
                                                                      "s = k / k_o = 3/2\n"
                                                                      "  1. k = 3\n"
                                                                      "  2. k_o = 2\n"
                                                                      "  3. #1 / #2 = 3/2\n"
                                                                      "inputs\n"
                                                                      "  k = 3\n"
                                                                      "... 1 further step not shown\n");
    // Out after the second header: its three steps and both inputs are left
    // out, and so is the line naming the inputs.
    CHECK(formula::render_derivation(explained, { .maxSteps = 5 }) == "s_2 = s * 2 = 3\n"
                                                                      "  1. s = 3/2, calculated\n"
                                                                      "  2. 2\n"
                                                                      "  3. #1 * #2 = 3\n"
                                                                      "s = k / k_o = 3/2\n"
                                                                      "... 5 further steps not shown\n");
    // Out inside the first block.
    CHECK(formula::render_derivation(explained, { .maxSteps = 2 }) == "s_2 = s * 2 = 3\n"
                                                                      "  1. s = 3/2, calculated\n"
                                                                      "... 8 further steps not shown\n");
    CHECK(formula::render_derivation(explained, { .maxSteps = 0 }) == "... 10 further steps not shown\n");
}

TEST_CASE("a derivation's header states a failure, an empty value and an override", "[calculation][worksheet][trace]")
{
    // The share fails, and the doubled share with it.
    auto failing = guarded_sheet(rat(3), rat(0));
    std::string const failed = formula::render_derivation(formula::explain_worksheet<Doubled>(failing), { 100 });
    CHECK(failed.starts_with("s_2 = s * 2 = division by zero\n"
                             "  1. s = division by zero, calculated\n"));
    CHECK(failed.find("\ns = k / k_o = division by zero\n") != std::string::npos);
    CHECK(failed.ends_with("inputs\n"
                           "  k = 3\n"
                           "  k_o = 0\n"));

    // The factor not measured: nothing calculated from it has a value.
    auto dark = formula::worksheet(guarded,
                                   formula::environment(formula::Measured<Factor>::absent(),
                                                        formula::Measured<Other> { rat(2) }));
    CHECK(formula::render_derivation(formula::explain_worksheet<Share>(dark), { 100 }) == "s = k / k_o = (no value)\n"
                                                                                          "  1. k = (not measured)\n"
                                                                                          "  2. k_o = 2\n"
                                                                                          "  3. #1 / #2 = (not measured)\n"
                                                                                          "inputs\n"
                                                                                          "  k = (not measured)\n"
                                                                                          "  k_o = 2\n");

    // An override is its header alone, and says what it stands in place of.
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    sheet.set(formula::entered(formula::Measured<NetDraw> { rat(250) }));
    CHECK(formula::render_derivation(formula::explain_worksheet<NetDraw>(sheet), { 100 })
          == "net_draw = 250 kWh, entered by hand in place of monthly_load - self_used\n");
    sheet.set(formula::entered(formula::Measured<NetDraw>::absent()));
    CHECK(formula::render_derivation(formula::explain_worksheet<NetDraw>(sheet), { 100 })
          == "net_draw = (entered by hand as empty) in place of monthly_load - self_used\n");
}

TEST_CASE("a bill's derivation, in its vocabulary, cut short", "[calculation][worksheet][trace][vocabulary]")
{
    using namespace household;
    constexpr auto words = formula::vocabulary(formula::renames<Total>("C_bill"));
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    sheet.set(formula::entered(formula::Measured<Price> { rat(1, 4) }));
    auto const explained = formula::explain_worksheet<Total>(sheet, words);

    // Fifteen headers, 45 steps and ten inputs: seventy lines, four shown.
    CHECK(formula::render_derivation(explained, { .maxSteps = 4 }) == "C_bill = subtotal + vat = 190043/2000 EUR\n"
                                                                      "  1. subtotal = 1597/20 EUR, calculated\n"
                                                                      "  2. vat = 30343/2000 EUR, calculated\n"
                                                                      "  3. #1 + #2 = 190043/2000\n"
                                                                      "... 66 further steps not shown\n");

    // In full: the inputs close it, the typed-in price saying so.
    std::string const full = formula::render_derivation(explained, { .maxSteps = 70 });
    CHECK(full.find("further step") == std::string::npos);
    CHECK(full.find("\nfridge_kw = fridge_w = 1/5 kW\n  1. fridge_w = 200 W\n") != std::string::npos);
    CHECK(full.ends_with("inputs\n"
                         "  fridge_w = 200 W\n"
                         "  fridge_h = 24 h\n"
                         "  oven_kw = 5/2 kW\n"
                         "  oven_h = 1 h\n"
                         "  heater_kw = 3/2 kW\n"
                         "  heater_h = 4 h\n"
                         "  solar = 150 kWh\n"
                         "  price = 1/4 EUR/kWh, entered by hand\n"
                         "  feed_in = 2/25 EUR/kWh\n"
                         "  base_fee = 25/2 EUR\n"));
}

TEST_CASE("author text in a derivation's header cannot end its line", "[calculation][worksheet][trace]")
{
    // A share whose declared symbol ends in a newline: every line it appears
    // on shows the newline escaped, and no line is broken by it.
    constexpr auto awkward = formula::calculation(formula::define<AwkwardShare>(var<Factor> / var<Other>),
                                                  formula::define<Doubled>(var<AwkwardShare> * rat(2)));
    auto sheet = formula::worksheet(
        awkward, formula::environment(formula::Measured<Factor> { rat(3) }, formula::Measured<Other> { rat(2) }));
    CHECK(formula::render_derivation(formula::explain_worksheet<Doubled>(sheet), { 100 })
          == "s_2 = s\\n * 2 = 3\n"
             "  1. s\\n = 3/2, calculated\n"
             "  2. 2\n"
             "  3. #1 * #2 = 3\n"
             "s\\n = k / k_o = 3/2\n"
             "  1. k = 3\n"
             "  2. k_o = 2\n"
             "  3. #1 / #2 = 3/2\n"
             "inputs\n"
             "  k = 3\n"
             "  k_o = 2\n");
}
