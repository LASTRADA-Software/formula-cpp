// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/calculation.hpp>
#include <formula-cpp/citation.hpp>
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/document.hpp>
#include <formula-cpp/format.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/number_text.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/precision.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include "household_bill.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
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

struct Third: formula::Quantity<Third, "s_3", "an invented share, a third of it", unit::One>
{
};
struct TypedShare: formula::Quantity<TypedShare, "s_t", "an invented share, typed outright", unit::One>
{
};

/// A share of two factors; a third of it, a typed 1/3 that has no exact
/// decimal; and a share typed outright, which reads nothing.
inline constexpr auto thirds =
    formula::calculation(formula::define<Share>(var<Factor> / var<Other>),
                         formula::define<Third>(var<Share> * rat(1, 3)),
                         formula::define<TypedShare>(formula::constant<unit::One>(rat(2, 3))));

/// `thirds`' worksheet over a factor of 1 and another of 4: a share of 1/4,
/// which is 0.25, and a third of it, 1/12, which has no exact decimal.
inline auto thirds_sheet()
{
    return formula::worksheet(
        thirds, formula::environment(formula::Measured<Factor> { rat(1) }, formula::Measured<Other> { rat(4) }));
}

/// The typed share, read as it is by the halved share -- which is therefore a
/// typed value too -- and that doubled: reads of a typed value, one through
/// another.
inline constexpr auto typedReads =
    formula::calculation(formula::define<TypedShare>(formula::constant<unit::One>(rat(2, 3))),
                         formula::define<Halved>(var<TypedShare>),
                         formula::define<Doubled>(var<Halved> * rat(2)));

/// A computed share and the typed one, added: read side by side, and written
/// alike under `alikeShares`.
inline constexpr auto sharesAdded =
    formula::calculation(formula::define<Share>(var<Factor> / var<Other>),
                         formula::define<TypedShare>(formula::constant<unit::One>(rat(2, 3))),
                         formula::define<Doubled>(var<Share> + var<TypedShare>));

/// A vocabulary writing the typed share as the computed one is written, `s`.
inline constexpr auto alikeShares = formula::vocabulary(formula::renames<TypedShare>("s"));

/// A unit declaring more decimals than `DecimalPlaces` spans: 19. Invented.
inline constexpr formula::Unit OverPrecise { .dimension = formula::dim::Length,
                                             .symbolText = formula::symbol("u"),
                                             .decimals = 19 };
struct OverPreciseLength: formula::Quantity<OverPreciseLength, "l_u", "a length in an over-precise unit", OverPrecise>
{
};

/// A length in the over-precise unit, twice the width, and the depth that
/// reads it.
inline constexpr auto overPrecise = formula::calculation(formula::define<OverPreciseLength>(var<Width> * rat(2)),
                                                         formula::define<Depth>(var<OverPreciseLength> + var<Width>));

struct LimitedShare: formula::Quantity<LimitedShare, "x_l", "an invented share, limited by its precision", unit::One>
{
};
struct CitedShare: formula::Quantity<CitedShare, "x_d", "an invented share, citing a clause", unit::One>
{
};
struct RoundedShare: formula::Quantity<RoundedShare, "x_r", "an invented share, rounded", unit::One>
{
};
struct SharesAdded: formula::Quantity<SharesAdded, "x_t", "the invented shares, added", unit::One>
{
};
struct FixedShare: formula::Quantity<FixedShare, "x_f", "an invented share over a fixed factor", unit::One>
{
};

/// Every node that reads the worksheet, in one calculation: the typed factor
/// 2/3; a share whose derived factor stands for it but computes the width
/// over the depth, plus the factor itself; `when()` taken both ways; a
/// precision limit, whose level reads the factor; a documented value; a
/// rounding; all of those added; and a share whose fixed factor stands for
/// the typed one.
inline constexpr auto everyRead = formula::calculation(
    formula::define<Factor>(formula::constant<unit::One>(rat(2, 3))),
    formula::define<Share>(std::get<0>(derivedFactor.variantSet.cases).expression + var<Factor>),
    formula::define<Halved>(formula::when(var<Other> > formula::constant<unit::One>(rat(0)), var<Factor>, var<Share>)),
    formula::define<Doubled>(formula::when(var<Other> < formula::constant<unit::One>(rat(0)), var<Factor>, var<Share>)
                             + var<Halved>),
    formula::define<LimitedShare>(formula::precision_limit<formula::PrecisionKind::Repeatability>(
        var<Factor>, rat(1, 50) * formula::precision_level<Factor> + var<Halved>)),
    formula::define<CitedShare>(formula::documented(var<Halved>, clause)),
    formula::define<RoundedShare>(
        formula::rounded<unit::One, formula::DecimalPlaces { 2 }, formula::RoundingMode::HalfEven>(var<Factor>
                                                                                                    * var<Other>)),
    formula::define<SharesAdded>(var<Doubled> + var<LimitedShare> + var<CitedShare> + var<RoundedShare>),
    formula::define<FixedShare>(std::get<0>(fixedFactor.variantSet.cases).expression + var<Factor>));

/// `everyRead`'s worksheet over a width of 1, a depth of 3 and another factor
/// of 2: the derived factor computes 1/3, beside the typed 2/3.
inline auto every_read_sheet()
{
    return formula::worksheet(everyRead,
                              formula::environment(formula::Measured<Width> { rat(1) },
                                                   formula::Measured<Depth> { rat(3) },
                                                   formula::Measured<Other> { rat(2) }));
}

/// The symbol of the block among @p entries for the quantity in slot
/// @p slot, or empty when none is.
std::optional<std::string_view> slot_symbol(std::vector<formula::WorksheetEntry> const& entries, std::size_t slot)
{
    for (formula::WorksheetEntry const& shown: entries)
        if (shown.slot == slot)
            return shown.symbol;
    return std::nullopt;
}

/// Whether @p read, a step's entry in `readSlots`, is the slot of the
/// quantity @p symbol writes, among @p entries' blocks.
bool reads_own_slot(std::vector<formula::WorksheetEntry> const& entries, std::size_t read, std::string_view symbol)
{
    return read != formula::NothingRead && slot_symbol(entries, read) == symbol;
}

/// Checks each step of each block of @p entries against what it read: a
/// `Variable` step, its own quantity's slot; an overlay's constant or
/// derived quantity, its own quantity's slot or none; any other step, none.
/// Adds the kind of each step checked to @p kindsSeen.
void check_read_slots(std::vector<formula::WorksheetEntry> const& entries, std::vector<formula::StepKind>& kindsSeen)
{
    for (formula::WorksheetEntry const& shown: entries)
    {
        INFO("the block for " << shown.symbol);
        REQUIRE(shown.readSlots.size() == shown.trace.steps.size());
        for (std::size_t stepIndex = 0; stepIndex < shown.trace.steps.size(); ++stepIndex)
        {
            formula::Step<formula::Rational> const& recorded = shown.trace.steps[stepIndex];
            std::size_t const read = shown.readSlots[stepIndex];
            INFO("step " << (stepIndex + 1) << ", " << recorded.symbol << ", read " << read);
            kindsSeen.push_back(recorded.kind);
            if (recorded.kind == formula::StepKind::Variable)
                CHECK(reads_own_slot(entries, read, recorded.symbol));
            else if (recorded.kind == formula::StepKind::OverriddenConstant
                     || recorded.kind == formula::StepKind::DerivedQuantity)
                CHECK((read == formula::NothingRead || reads_own_slot(entries, read, recorded.symbol)));
            else
                CHECK(read == formula::NothingRead);
        }
    }
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
    // In the coherent unit: 400 W, and 12 h is 43200 s.
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

TEST_CASE("an override between calculated blocks is one line of the budget", "[calculation][worksheet][trace]")
{
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    sheet.set(formula::entered(formula::Measured<NetDraw> { rat(250) }));
    auto const explained = formula::explain_worksheet<EnergyCost>(sheet);

    // Six blocks, the override fourth, and three inputs: 24 lines.
    std::string const everything = "energy_cost = grid_cost - feed_in_credit = 388/5 EUR\n"
                                   "  1. grid_cost = 80 EUR, calculated\n"
                                   "  2. feed_in_credit = 12/5 EUR, calculated\n"
                                   "  3. #1 - #2 = 388/5\n"
                                   "feed_in_credit = exported * feed_in = 12/5 EUR\n"
                                   "  1. exported = 30 kWh, calculated\n"
                                   "  2. feed_in = 2/25 EUR/kWh\n"
                                   "  3. #1 * #2 = 12/5\n"
                                   "grid_cost = net_draw * price = 80 EUR\n"
                                   "  1. net_draw = 250 kWh, entered by hand\n"
                                   "  2. price = 8/25 EUR/kWh\n"
                                   "  3. #1 * #2 = 80\n"
                                   "net_draw = 250 kWh, entered by hand in place of monthly_load - self_used\n"
                                   "exported = solar - self_used = 30 kWh\n"
                                   "  1. solar = 150 kWh\n"
                                   "  2. self_used = 120 kWh, calculated\n"
                                   "  3. #1 - #2 = 108000000 m^2 kg/s^2\n"
                                   "self_used = solar * 4/5 = 120 kWh\n"
                                   "  1. solar = 150 kWh\n"
                                   "  2. 4/5\n"
                                   "  3. #1 * #2 = 432000000 m^2 kg/s^2\n"
                                   "inputs\n"
                                   "  solar = 150 kWh\n"
                                   "  price = 8/25 EUR/kWh\n"
                                   "  feed_in = 2/25 EUR/kWh\n";
    CHECK(formula::render_derivation(explained, { .maxSteps = 24 }) == everything);

    // Fifteen lines: twelve of three blocks, the override's one, the
    // header after it and its first step. Left out: two steps, a block of
    // four and three inputs.
    std::string::size_type const cut = everything.find("  2. self_used = 120 kWh, calculated\n");
    REQUIRE(cut != std::string::npos);
    CHECK(formula::render_derivation(explained, { .maxSteps = 15 })
          == everything.substr(0, cut) + "... 9 further steps not shown\n");
}

TEST_CASE("a bill's derivation, in its vocabulary, cut short", "[calculation][worksheet][trace][vocabulary]")
{
    using namespace household;
    constexpr auto words =
        formula::vocabulary(formula::renames<Total>("C_bill"), formula::renames<Subtotal>("S"));
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    sheet.set(formula::entered(formula::Measured<Price> { rat(1, 4) }));
    auto const explained = formula::explain_worksheet<Total>(sheet, words);

    // Fifteen headers, 45 steps and ten inputs: seventy lines, four shown.
    // The subtotal is renamed in the definition the header states, as in
    // the step reading it.
    CHECK(formula::render_derivation(explained, { .maxSteps = 4 }) == "C_bill = S + vat = 190043/2000 EUR\n"
                                                                      "  1. S = 1597/20 EUR, calculated\n"
                                                                      "  2. vat = 30343/2000 EUR, calculated\n"
                                                                      "  3. #1 + #2 = 190043/2000\n"
                                                                      "... 66 further steps not shown\n");

    // In full: the inputs close it, the typed-in price saying so.
    std::string const full = formula::render_derivation(explained, { .maxSteps = 70 });
    CHECK(full.find("further step") == std::string::npos);
    CHECK(full.find("\nvat = S * 19/100 = 30343/2000 EUR\n  1. S = 1597/20 EUR, calculated\n") != std::string::npos);
    CHECK(full.find("\nfridge_kw = fridge_w = 1/5 kW\n  1. fridge_w = 200 W\n") != std::string::npos);
    // A computed step states its value in the coherent unit, as render_trace
    // does: 24/5 kWh in joules.
    CHECK(full.find("\nfridge_kwh = fridge_kw * fridge_h = 24/5 kWh\n"
                    "  1. fridge_kw = 1/5 kW, calculated\n"
                    "  2. fridge_h = 24 h\n"
                    "  3. #1 * #2 = 17280000 m^2 kg/s^2\n")
          != std::string::npos);
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

TEST_CASE("a derivation spells its headers, steps and inputs in the trace's number style",
          "[calculation][worksheet][trace][decimals]")
{
    auto sheet = thirds_sheet();
    auto const explained = formula::explain_worksheet<Third>(sheet);

    // Fractions unless a style is named, as before.
    CHECK(formula::render_derivation(explained, { .maxSteps = 20 }) == "s_3 = s * 1/3 = 1/12\n"
                                                                       "  1. s = 1/4, calculated\n"
                                                                       "  2. 1/3\n"
                                                                       "  3. #1 * #2 = 1/12\n"
                                                                       "s = k / k_o = 1/4\n"
                                                                       "  1. k = 1\n"
                                                                       "  2. k_o = 4\n"
                                                                       "  3. #1 / #2 = 1/4\n"
                                                                       "inputs\n"
                                                                       "  k = 1\n"
                                                                       "  k_o = 4\n");
    // Exact decimals: 1/4 is 0.25 in its block's header, on its root's line
    // and where the third's block reads it; 1/12 and the typed 1/3 have none.
    CHECK(formula::render_derivation(explained, { .maxSteps = 20, .numbers = formula::NumberStyle::exact_decimal() })
          == "s_3 = s * 1/3 = 1/12\n"
             "  1. s = 0.25, calculated\n"
             "  2. 1/3\n"
             "  3. #1 * #2 = 1/12\n"
             "s = k / k_o = 0.25\n"
             "  1. k = 1\n"
             "  2. k_o = 4\n"
             "  3. #1 / #2 = 0.25\n"
             "inputs\n"
             "  k = 1\n"
             "  k_o = 4\n");
    // Rounded: 1/12 reads ≈0.083 in its header and on its root's line alike,
    // at the 3 places of a unit nobody declared. The typed 1/3 is never
    // rounded, in the header's definition or on its step.
    CHECK(formula::render_derivation(
              explained,
              { .maxSteps = 20, .numbers = formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven) })
          == "s_3 = s * 1/3 = \xe2\x89\x88"
             "0.083\n"
             "  1. s = 0.25, calculated\n"
             "  2. 1/3\n"
             "  3. #1 * #2 = \xe2\x89\x88"
             "0.083\n"
             "s = k / k_o = 0.25\n"
             "  1. k = 1\n"
             "  2. k_o = 4\n"
             "  3. #1 / #2 = 0.25\n"
             "inputs\n"
             "  k = 1\n"
             "  k_o = 4\n");
}

TEST_CASE("a derivation's header states a typed value exactly, as its root's line does",
          "[calculation][worksheet][trace][decimals]")
{
    // The share typed as 2/3: its block's root is the typed constant, so a
    // rounding style rounds neither the root's line nor the header.
    auto sheet = thirds_sheet();
    CHECK(formula::render_derivation(
              formula::explain_worksheet<TypedShare>(sheet),
              { .maxSteps = 20, .numbers = formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven) })
          == "s_t = 2/3 = 2/3\n"
             "  1. 2/3\n");
}

TEST_CASE("an input line with no step of its own is spelled in the trace's number style",
          "[calculation][worksheet][trace][decimals]")
{
    // A derivation edited by hand: the second input's step dropped and its
    // value set to 1/3, so its line falls back to the value the block holds.
    auto sheet = thirds_sheet();
    auto handMade = formula::explain_worksheet<Third>(sheet);
    formula::WorksheetEntry& lastInput = handMade.entries.back();
    REQUIRE(lastInput.kind == formula::WorksheetEntryKind::Input);
    REQUIRE(lastInput.symbol == "k_o");
    lastInput.trace = {};
    lastInput.value = rat(1, 3);
    CHECK(formula::render_derivation(handMade, { .maxSteps = 20 }).ends_with("inputs\n"
                                                                             "  k = 1\n"
                                                                             "  k_o = 1/3\n"));
    CHECK(formula::render_derivation(
              handMade,
              { .maxSteps = 20, .numbers = formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven) })
              .ends_with("inputs\n"
                         "  k = 1\n"
                         "  k_o = \xe2\x89\x88"
                         "0.333\n"));

    // An input in a unit that declares decimals: the fallback line states the
    // value in that unit, with its symbol, and pads it to its three decimals.
    using namespace household;
    auto billSheet = formula::worksheet(bill, bill_environment(billValues));
    billSheet.set(formula::entered(formula::Measured<NetDraw> { rat(250) }));
    auto handMadeBill = formula::explain_worksheet<EnergyCost>(billSheet);
    formula::WorksheetEntry* solarInput = nullptr;
    for (formula::WorksheetEntry& shown: handMadeBill.entries)
        if (shown.kind == formula::WorksheetEntryKind::Input && shown.symbol == "solar")
            solarInput = &shown;
    REQUIRE(solarInput != nullptr);
    solarInput->trace = {};
    CHECK(formula::render_derivation(handMadeBill, { .maxSteps = 30 }).find("inputs\n  solar = 150 kWh\n")
          != std::string::npos);
    CHECK(formula::render_derivation(handMadeBill,
                                     { .maxSteps = 30,
                                       .numbers = formula::NumberStyle::approximate_decimal(
                                           formula::RoundingMode::HalfEven, formula::DecimalPadding::Padded) })
              .find("inputs\n  solar = 150.000 kWh\n")
          != std::string::npos);
}

TEST_CASE("a derivation pads a header as a trace pads the line that reads its value",
          "[calculation][worksheet][trace][decimals]")
{
    // The bill with the energy drawn entered by hand, rendered rounded and
    // padded: each value in a unit that declares decimals is padded to them
    // -- the euro's two, the kilowatt-hour's three, the tariff's four -- in a
    // header and on the line that reads it alike. A computed step, stated in
    // the coherent unit nobody declared, is never padded, and the typed 4/5
    // is written exactly, 0.8, in the definition and on its step.
    using namespace household;
    auto sheet = formula::worksheet(bill, bill_environment(billValues));
    sheet.set(formula::entered(formula::Measured<NetDraw> { rat(250) }));
    CHECK(formula::render_derivation(formula::explain_worksheet<EnergyCost>(sheet),
                                     { .maxSteps = 30,
                                       .numbers = formula::NumberStyle::approximate_decimal(
                                           formula::RoundingMode::HalfEven, formula::DecimalPadding::Padded) })
          == "energy_cost = grid_cost - feed_in_credit = 77.60 EUR\n"
             "  1. grid_cost = 80.00 EUR, calculated\n"
             "  2. feed_in_credit = 2.40 EUR, calculated\n"
             "  3. #1 - #2 = 77.6\n"
             "feed_in_credit = exported * feed_in = 2.40 EUR\n"
             "  1. exported = 30.000 kWh, calculated\n"
             "  2. feed_in = 0.0800 EUR/kWh\n"
             "  3. #1 * #2 = 2.4\n"
             "grid_cost = net_draw * price = 80.00 EUR\n"
             "  1. net_draw = 250.000 kWh, entered by hand\n"
             "  2. price = 0.3200 EUR/kWh\n"
             "  3. #1 * #2 = 80\n"
             "net_draw = 250.000 kWh, entered by hand in place of monthly_load - self_used\n"
             "exported = solar - self_used = 30.000 kWh\n"
             "  1. solar = 150.000 kWh\n"
             "  2. self_used = 120.000 kWh, calculated\n"
             "  3. #1 - #2 = 108000000 m^2 kg/s^2\n"
             "self_used = solar * 0.8 = 120.000 kWh\n"
             "  1. solar = 150.000 kWh\n"
             "  2. 0.8\n"
             "  3. #1 * #2 = 432000000 m^2 kg/s^2\n"
             "inputs\n"
             "  solar = 150.000 kWh\n"
             "  price = 0.3200 EUR/kWh\n"
             "  feed_in = 0.0800 EUR/kWh\n");
}

TEST_CASE("a typed value another block reads is stated as exactly there as in its own block",
          "[calculation][worksheet][trace][decimals]")
{
    // The typed 2/3, read as it is by the halved share and that doubled. A
    // reading line, and the header of a block that only passes the value on,
    // state it exactly, as its own block does; the product, computed, is
    // rounded where the style rounds.
    auto sheet = formula::worksheet(typedReads, formula::environment());
    auto const explained = formula::explain_worksheet<Doubled>(sheet);
    std::string const exactly = "s_2 = s_h * 2 = 4/3\n"
                                "  1. s_h = 2/3, calculated\n"
                                "  2. 2\n"
                                "  3. #1 * #2 = 4/3\n"
                                "s_h = s_t = 2/3\n"
                                "  1. s_t = 2/3, calculated\n"
                                "s_t = 2/3 = 2/3\n"
                                "  1. 2/3\n";
    CHECK(formula::render_derivation(explained, { .maxSteps = 20 }) == exactly);
    CHECK(formula::render_derivation(explained, { .maxSteps = 20, .numbers = formula::NumberStyle::exact_decimal() })
          == exactly);
    CHECK(formula::render_derivation(
              explained,
              { .maxSteps = 20, .numbers = formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven) })
          == "s_2 = s_h * 2 = \xe2\x89\x88"
             "1.333\n"
             "  1. s_h = 2/3, calculated\n"
             "  2. 2\n"
             "  3. #1 * #2 = \xe2\x89\x88"
             "1.333\n"
             "s_h = s_t = 2/3\n"
             "  1. s_t = 2/3, calculated\n"
             "s_t = 2/3 = 2/3\n"
             "  1. 2/3\n");
}

TEST_CASE("a derivation tells a typed value from a computed one written alike", "[calculation][worksheet][trace][decimals]")
{
    // A computed share of 1/3 and the typed 2/3, both written `s`: the line
    // reading the computed one is rounded, the line reading the typed one is
    // not. Only the slot each step read can tell the two apart.
    auto sheet = formula::worksheet(
        sharesAdded, formula::environment(formula::Measured<Factor> { rat(1) }, formula::Measured<Other> { rat(3) }));
    auto const explained = formula::explain_worksheet<Doubled>(sheet, alikeShares);
    std::string const rounded = formula::render_derivation(
        explained,
        { .maxSteps = 20, .numbers = formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven) });
    CHECK(rounded.starts_with("s_2 = s + s = 1\n"
                              "  1. s = \xe2\x89\x88"
                              "0.333, calculated\n"
                              "  2. s = 2/3, calculated\n"
                              "  3. #1 + #2 = 1\n"));
    CHECK(rounded.find("\ns = k / k_o = \xe2\x89\x88"
                       "0.333\n")
          != std::string::npos);
    CHECK(rounded.find("\ns = 2/3 = 2/3\n") != std::string::npos);
}

TEST_CASE("each step of a derivation notes the quantity it read, whatever node read it", "[calculation][worksheet][trace]")
{
    // Over every node that reads the worksheet. `render_derivation` reads
    // only a `Variable` step's entry, and relies on its being the step's own
    // quantity's slot; an overlay's step notes the entry it replaced, its own
    // quantity's too; no other step notes a read.
    auto sheet = every_read_sheet();
    auto const added = formula::explain_worksheet<SharesAdded>(sheet);
    auto const fixed = formula::explain_worksheet<FixedShare>(sheet);
    std::vector<formula::StepKind> kindsSeen;
    check_read_slots(added.entries, kindsSeen);
    check_read_slots(fixed.entries, kindsSeen);
    for (formula::StepKind const reading: { formula::StepKind::Variable,
                                            formula::StepKind::Conditional,
                                            formula::StepKind::PrecisionLevel,
                                            formula::StepKind::PrecisionLimit,
                                            formula::StepKind::Documented,
                                            formula::StepKind::Round,
                                            formula::StepKind::DerivedQuantity,
                                            formula::StepKind::OverriddenConstant })
    {
        INFO("a step of kind " << static_cast<int>(reading));
        CHECK(std::ranges::find(kindsSeen, reading) != kindsSeen.end());
    }
}

TEST_CASE("a derived quantity standing for a typed value is rounded, as the value it computed",
          "[calculation][worksheet][trace][decimals]")
{
    // The share's derived factor stands for the typed factor, 2/3, and reads
    // it to ask whether the worksheet held one; but its value is the width
    // over the depth, 1/3, which it computed. It is rounded where the style
    // rounds, while the typed factor read beside it stays exact.
    auto sheet = every_read_sheet();
    auto const explained = formula::explain_worksheet<Share>(sheet);
    CHECK(formula::render_derivation(
              explained,
              { .maxSteps = 20, .numbers = formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven) })
          == "s = k * k_o + k = \xe2\x89\x88"
             "1.333\n"
             "  1. b = 1 mm\n"
             "  2. d = 3 mm\n"
             "  3. #1 / #2 = \xe2\x89\x88"
             "0.333\n"
             "  4. k = #3 = \xe2\x89\x88"
             "0.333 [derived by jurisdiction overlay: Example Standard 7:2024, 2]\n"
             "  5. k_o = 2\n"
             "  6. #4 * #5 = \xe2\x89\x88"
             "0.667\n"
             "  7. k = 2/3, calculated\n"
             "  8. #6 + #7 = \xe2\x89\x88"
             "1.333\n"
             "k = 2/3 = 2/3\n"
             "  1. 2/3\n"
             "inputs\n"
             "  b = 1 mm\n"
             "  d = 3 mm\n"
             "  k_o = 2\n");
}

TEST_CASE("a derivation's header says a value is not shown where its style cannot spell it",
          "[calculation][worksheet][trace][decimals]")
{
    // A length in a unit declaring 19 decimals, more than a rounding or a
    // padding can take: under a style that pads or rounds, its header and
    // the line reading it say it is not shown, while its root, in metres,
    // spells it. In fractions every value is shown.
    auto sheet = formula::worksheet(overPrecise, formula::environment(formula::Measured<Width> { rat(3) }));
    auto const explained = formula::explain_worksheet<Depth>(sheet);
    std::string const fractions = formula::render_derivation(explained, { .maxSteps = 20 });
    CHECK(fractions.find("\nl_u = b * 2 = 3/500 u\n") != std::string::npos);
    CHECK(fractions.find("\n  1. l_u = 3/500 u, calculated\n") != std::string::npos);
    for (formula::NumberStyle const style :
         { formula::NumberStyle::exact_decimal(formula::DecimalPadding::Padded),
           formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven) })
    {
        std::string const styled = formula::render_derivation(explained, { .maxSteps = 20, .numbers = style });
        CHECK(styled.find("\nl_u = b * 2 = (not shown: overflow in exact arithmetic)\n") != std::string::npos);
        CHECK(styled.find("\n  1. l_u = (not shown: overflow in exact arithmetic), calculated\n") != std::string::npos);
        CHECK(styled.find("\n  3. #1 * #2 = 0.006 m\n") != std::string::npos);
    }
}

TEST_CASE("a derivation's computed price per energy shows its first significant digit, not a zero",
          "[calculation][worksheet][trace][decimals]")
{
    // The price worked out from a cost and the energy it paid for: 80 EUR for
    // 250 kWh is 0.32 EUR/kWh, and the division's step states it in euros per
    // joule, 1/11250000. Rounded at the default 3 places it would read ≈0; it
    // reads its first digit instead, at 8 places.
    using namespace household;
    constexpr auto priced = formula::calculation(formula::define<Price>(var<GridCost> / var<NetDraw>));
    auto sheet = formula::worksheet(
        priced, formula::environment(formula::Measured<GridCost> { rat(80) }, formula::Measured<NetDraw> { rat(250) }));
    auto const explained = formula::explain_worksheet<Price>(sheet);
    CHECK(formula::render_derivation(explained, { .maxSteps = 20 }) == "price = grid_cost / net_draw = 8/25 EUR/kWh\n"
                                                                       "  1. grid_cost = 80 EUR\n"
                                                                       "  2. net_draw = 250 kWh\n"
                                                                       "  3. #1 / #2 = 1/11250000 s^2/(m^2 kg)\n"
                                                                       "inputs\n"
                                                                       "  grid_cost = 80 EUR\n"
                                                                       "  net_draw = 250 kWh\n");
    CHECK(formula::render_derivation(
              explained,
              { .maxSteps = 20, .numbers = formula::NumberStyle::approximate_decimal(formula::RoundingMode::HalfEven) })
          == "price = grid_cost / net_draw = 0.32 EUR/kWh\n"
             "  1. grid_cost = 80 EUR\n"
             "  2. net_draw = 250 kWh\n"
             "  3. #1 / #2 = \xe2\x89\x88"
             "0.00000009 s^2/(m^2 kg)\n"
             "inputs\n"
             "  grid_cost = 80 EUR\n"
             "  net_draw = 250 kWh\n");
}

// ------------------------------------------------------------ money of its own

namespace
{
/// An invented currency, a base dimension of its own as a consumer declares
/// one, with two decimals.
inline constexpr formula::Unit MoneyEuro { .dimension = formula::base_dimension("EUR"),
                                           .symbolText = formula::symbol("EUR"),
                                           .decimals = 2 };
/// That currency per kilowatt-hour: one of it per 3600000 joules.
inline constexpr formula::Unit MoneyEuroPerKwh { .dimension = MoneyEuro.dimension / formula::dim::Energy,
                                                 .magnitudeNumerator = 1,
                                                 .magnitudeDenominator = 3'600'000,
                                                 .symbolText = formula::symbol("EUR/kWh"),
                                                 .decimals = 4 };

struct MeterDraw: formula::Quantity<MeterDraw, "draw", "an invented energy drawn", unit::KilowattHour>
{
};
struct EnergyTariff: formula::Quantity<EnergyTariff, "tariff", "an invented price of energy", MoneyEuroPerKwh>
{
};
struct StandingFee: formula::Quantity<StandingFee, "fee", "an invented standing fee", MoneyEuro>
{
};
struct EnergyCharge: formula::Quantity<EnergyCharge, "charge", "an invented charge for energy", MoneyEuro>
{
};
struct ChargeTotal: formula::Quantity<ChargeTotal, "total", "an invented total", MoneyEuro>
{
};
struct MeanRate: formula::Quantity<MeanRate, "rate", "an invented total per energy drawn", MoneyEuroPerKwh>
{
};

/// A charge for the energy drawn; a total of it, the fee and a quarter of a
/// euro typed as money; and what the total comes to per energy drawn.
inline constexpr auto charges = formula::calculation(
    formula::define<EnergyCharge>(var<MeterDraw> * var<EnergyTariff>),
    formula::define<ChargeTotal>(var<EnergyCharge> + var<StandingFee> + formula::constant<MoneyEuro>(rat(1, 4))),
    formula::define<MeanRate>(var<ChargeTotal> / var<MeterDraw>));
} // namespace

TEST_CASE("money of its own is calculated, derived, documented and spelled in its units",
          "[calculation][worksheet][trace][decimals][money]")
{
    auto sheet = formula::worksheet(charges,
                                    formula::environment(formula::Measured<MeterDraw> { rat(279) },
                                                         formula::Measured<EnergyTariff> { rat(8, 25) },
                                                         formula::Measured<StandingFee> { rat(25, 2) }));
    formula::NumberStyle const decimals = formula::NumberStyle::exact_decimal();

    // The derivation in exact decimals, each value in its quantity's unit. A
    // computed step is in the coherent unit, the rate's in euros per joule,
    // and neither it nor the rate, 3401/9300 EUR/kWh, has an exact decimal.
    auto const explained = formula::explain_worksheet<MeanRate>(sheet);
    CHECK(explained.entries.front().unit == MoneyEuroPerKwh);
    CHECK(formula::render_derivation(explained, { .maxSteps = 30, .numbers = decimals })
          == "rate = total / draw = 3401/9300 EUR/kWh\n"
             "  1. total = 102.03 EUR, calculated\n"
             "  2. draw = 279 kWh\n"
             "  3. #1 / #2 = 3401/33480000000 EUR s^2/(m^2 kg)\n"
             "total = charge + fee + 0.25 EUR = 102.03 EUR\n"
             "  1. charge = 89.28 EUR, calculated\n"
             "  2. fee = 12.5 EUR\n"
             "  3. #1 + #2 = 101.78 EUR\n"
             "  4. 0.25 EUR\n"
             "  5. #3 + #4 = 102.03 EUR\n"
             "charge = draw * tariff = 89.28 EUR\n"
             "  1. draw = 279 kWh\n"
             "  2. tariff = 0.32 EUR/kWh\n"
             "  3. #1 * #2 = 89.28 EUR\n"
             "inputs\n"
             "  draw = 279 kWh\n"
             "  tariff = 0.32 EUR/kWh\n"
             "  fee = 12.5 EUR\n");

    CHECK(formula::describe_graph(charges) == "inputs: draw, tariff, fee\n"
                                              "charge <- draw, tariff\n"
                                              "total  <- fee, charge\n"
                                              "rate   <- draw, total\n");

    // The documentation page: every row in its unit, and each definition in
    // the style asked for -- the quarter of a euro as 0.25 EUR.
    formula::Documentation const page =
        formula::document(charges, formula::DefaultVocabulary {}, { .numbers = decimals });
    CHECK(page.formula == "charge = draw * tariff\n"
                          "total = charge + fee + 0.25 EUR\n"
                          "rate = total / draw");
    REQUIRE(page.symbols.size() == 6);
    CHECK(page.symbols[0].symbol == "charge");
    CHECK(page.symbols[0].unit == MoneyEuro);
    CHECK(page.symbols[0].calculatedAs == "draw * tariff");
    CHECK(page.symbols[1].symbol == "total");
    CHECK(page.symbols[1].unit == MoneyEuro);
    CHECK(page.symbols[1].calculatedAs == "charge + fee + 0.25 EUR");
    CHECK(page.symbols[2].symbol == "rate");
    CHECK(page.symbols[2].unit == MoneyEuroPerKwh);
    CHECK(page.symbols[2].calculatedAs == "total / draw");
    CHECK(page.symbols[3].symbol == "draw");
    CHECK(page.symbols[3].unit == unit::KilowattHour);
    CHECK_FALSE(page.symbols[3].calculatedAs.has_value());
    CHECK(page.symbols[4].symbol == "tariff");
    CHECK(page.symbols[4].unit == MoneyEuroPerKwh);
    CHECK_FALSE(page.symbols[4].calculatedAs.has_value());
    CHECK(page.symbols[5].symbol == "fee");
    CHECK(page.symbols[5].unit == MoneyEuro);
    CHECK_FALSE(page.symbols[5].calculatedAs.has_value());

    // A value in euros, and one in euros per kilowatt-hour, written by
    // std::format and by number_text.
    formula::Measured<ChargeTotal> const chargedTotal = sheet.calculate<ChargeTotal>().measurement();
    formula::Measured<MeanRate> const meanRate = sheet.calculate<MeanRate>().measurement();
    formula::Measured<StandingFee> const standingFee { rat(25, 2) };
    CHECK(std::format("{}", chargedTotal) == "102.03 EUR");
    CHECK(std::format("{:.2HalfEven}", standingFee) == "12.50 EUR");
    CHECK(std::format("{}", meanRate) == "3401/9300 EUR/kWh");
    CHECK(std::format("{:~HalfEven}", meanRate) == "\xe2\x89\x88"
                                                   "0.3657 EUR/kWh");
    CHECK(formula::number_text(standingFee, formula::NumberStyle::exact_decimal(formula::DecimalPadding::Padded))
          == "12.50 EUR");
    CHECK(formula::number_text(chargedTotal, formula::NumberStyle::fraction()) == "10203/100 EUR");
}
