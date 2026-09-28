// SPDX-License-Identifier: Apache-2.0
//
// Quantities declared by alias:
//
//     using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", ...>;
//
// The alias names the `Quantity` specialisation itself, where the struct form
// derives a type of its own from it. Everything that keys on a quantity type
// must take either, so every such surface is exercised here with alias
// quantities: evaluation and its trace, rendering and the page, `Describe`,
// vocabularies, overlays and methods, lookups, series, observations and
// curves, statistics and rejection, records and lineage, opaque operations
// and retry, and the environment. Every other test file declares its
// quantities by struct, so both spellings stay covered.
//
// Every number is invented; every citation is an Example Standard.
#include "quantity_alias_cross_tu.hpp"

#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::Rational;
using formula::var;

[[nodiscard]] constexpr Rational rat(std::int64_t numerator, std::int64_t denominator = 1)
{
    return Rational { numerator, denominator };
}

using WaterVolume = formula::Quantity<struct WaterVolumeTag, "V_w", "effective water content", unit::Litre>;
using CementVolume = formula::Quantity<struct CementVolumeTag, "V_c", "cement content", unit::Litre>;
using WaterCementRatio = formula::Quantity<struct WaterCementRatioTag, "w/c", "ratio of water to cement", unit::One>;

// Alike in symbol, description and unit; told apart by the tag alone.
using FirstTwin = formula::Quantity<struct FirstTwinTag, "V", "a volume", unit::Litre>;
using SecondTwin = formula::Quantity<struct SecondTwinTag, "V", "a volume", unit::Litre>;

// One tag, shared: an alias template that declares its tag inside itself
// declares one tag for every instantiation. Each instantiation is still a
// quantity of its own, since its unit differs.
template <formula::Unit U>
using LengthIn = formula::Quantity<struct LengthInTag, "L", "a length", U>;

// One tag per instantiation: the tag depends on what the unit depends on.
template <formula::Unit U>
struct EdgeInTag;
template <formula::Unit U>
using EdgeIn = formula::Quantity<EdgeInTag<U>, "e", "an edge", U>;

// Two aliases that share a tag and differ in symbol: two quantities.
using InnerDiameter = formula::Quantity<struct DiameterPairTag, "d_i", "inner diameter", unit::Millimetre>;
using OuterDiameter = formula::Quantity<struct DiameterPairTag, "d_o", "outer diameter", unit::Millimetre>;

// A quantity declared the other way, to be mixed with the aliases.
struct AdmixtureVolume: formula::Quantity<AdmixtureVolume, "V_a", "admixture content", unit::Litre>
{
};

// 183 l of water to 305 l of cement: a ratio of exactly 3/5.
inline constexpr auto mix =
    formula::environment(formula::Measured<WaterVolume> { rat(183) }, formula::Measured<CementVolume> { rat(305) });
inline constexpr auto ratio =
    formula::documented(var<WaterVolume> / var<CementVolume>,
                        { .title = "Water/cement ratio", .reference = "Example Standard 1:2020", .section = "5.4.2" });

template <typename Q>
[[nodiscard]] Rational valueOf(formula::Outcome<Q> const& outcome)
{
    REQUIRE(outcome.is_value());
    return outcome.measurement().value();
}
} // namespace

// ---- identity and Describe ----

static_assert(!std::is_same_v<FirstTwin, SecondTwin>, "two tags, two types");
static_assert(FirstTwin::symbol == SecondTwin::symbol && FirstTwin::unit == SecondTwin::unit);
static_assert(std::is_same_v<WaterVolume::QuantityTag, WaterVolumeTag>, "the tag an alias names is its QuantityTag");

// The alias IS the specialisation: naming it again with the same arguments is
// the same type -- the collapse `quantity.hpp` documents.
static_assert(std::is_same_v<WaterVolume, formula::Quantity<WaterVolumeTag, "V_w", "effective water content", unit::Litre>>);

static_assert(formula::Described<WaterVolume>);
static_assert(formula::RequireDescribed<WaterVolume>::value);
static_assert(formula::Describe<WaterVolume>::symbol == std::string_view { "V_w" });
static_assert(formula::Describe<WaterVolume>::description == std::string_view { "effective water content" });
static_assert(formula::Describe<WaterVolume>::unit == unit::Litre);
static_assert(formula::Describe<WaterVolume>::dimension == formula::dim::Volume);

static_assert(!std::is_same_v<LengthIn<unit::Metre>, LengthIn<unit::Millimetre>>);
static_assert(std::is_same_v<LengthIn<unit::Metre>::QuantityTag, LengthIn<unit::Millimetre>::QuantityTag>);
static_assert(!std::is_same_v<EdgeIn<unit::Metre>::QuantityTag, EdgeIn<unit::Millimetre>::QuantityTag>);
static_assert(!std::is_same_v<InnerDiameter, OuterDiameter>);

TEST_CASE("quantity alias: quantities that share a tag are distinct while another argument differs", "[quantity][alias]")
{
    constexpr auto inputs = formula::environment(formula::Measured<LengthIn<unit::Metre>> { rat(2) },
                                                 formula::Measured<LengthIn<unit::Millimetre>> { rat(139) },
                                                 formula::Measured<InnerDiameter> { rat(103) },
                                                 formula::Measured<OuterDiameter> { rat(163) });
    auto const total = formula::checked_evaluate<LengthIn<unit::Millimetre>>(
        var<LengthIn<unit::Metre>> + var<LengthIn<unit::Millimetre>>, inputs);
    REQUIRE(total.has_value());
    CHECK(valueOf(*total) == rat(2139));

    auto const wall = formula::checked_evaluate<InnerDiameter>(var<OuterDiameter> - var<InnerDiameter>, inputs);
    REQUIRE(wall.has_value());
    CHECK(valueOf(*wall) == rat(60));
    CHECK(formula::render(var<OuterDiameter> - var<InnerDiameter>) == "d_o - d_i");

    constexpr auto edges = formula::environment(formula::Measured<EdgeIn<unit::Metre>> { rat(1) },
                                                formula::Measured<EdgeIn<unit::Millimetre>> { rat(127) });
    auto const edge =
        formula::checked_evaluate<EdgeIn<unit::Millimetre>>(var<EdgeIn<unit::Metre>> + var<EdgeIn<unit::Millimetre>>, edges);
    REQUIRE(edge.has_value());
    CHECK(valueOf(*edge) == rat(1127));
}

TEST_CASE("quantity alias: a quantity declared by alias is the same type in every translation unit", "[quantity][alias]")
{
    CHECK(cross_alias::symbol_of(cross_alias::WaterVolume {}) == "V_w");
    CHECK(cross_alias::water_and_cement_are_distinct());
    CHECK(cross_alias::address_of_water_volume_dimension() == &cross_alias::WaterVolume::dimension);
}

TEST_CASE("quantity alias: evaluated, checked and explained, with its trace", "[quantity][alias]")
{
    CHECK(valueOf(formula::evaluate<WaterCementRatio>(ratio, mix)) == rat(3, 5));

    auto const checked = formula::checked_evaluate<WaterCementRatio>(ratio, mix);
    REQUIRE(checked.has_value());
    CHECK(valueOf(*checked) == rat(3, 5));
    CHECK(checked->source() == formula::ValueSource::Derived);

    auto const explained = formula::explain<WaterCementRatio>(ratio, mix);
    CHECK(valueOf(explained.outcome) == rat(3, 5));
    CHECK(formula::render_trace(explained.trace, { .maxSteps = 10 })
          == "1. V_w = 183 l\n"
             "2. V_c = 305 l\n"
             "3. #1 / #2 = 3/5\n"
             "4. #3 = 3/5 [Water/cement ratio, Example Standard 1:2020, 5.4.2]\n");
}

TEST_CASE("quantity alias: rendered in every dialect, and documented", "[quantity][alias]")
{
    CHECK(formula::render(ratio) == "V_w / V_c");
    CHECK(formula::render<formula::Dialect::Markdown>(ratio) == "`V_w` / `V_c`");
    CHECK(formula::render<formula::Dialect::LaTeX>(ratio) == "\\frac{V_w}{V_c}");

    formula::Documentation const page = formula::document(ratio);
    REQUIRE(page.symbols.size() == 2);
    CHECK(page.symbols[0].symbol == "V_w");
    CHECK(page.symbols[0].description == "effective water content");
    CHECK(page.symbols[1].symbol == "V_c");
    REQUIRE(page.citations.size() == 1);
    CHECK(page.citations[0].reference == "Example Standard 1:2020");
}

TEST_CASE("quantity alias: an alias and a struct quantity in one formula", "[quantity][alias]")
{
    constexpr auto withAdmixture = (var<WaterVolume> + var<AdmixtureVolume>) / var<CementVolume>;
    constexpr auto inputs = formula::environment(formula::Measured<WaterVolume> { rat(173) },
                                                 formula::Measured<AdmixtureVolume> { rat(10) },
                                                 formula::Measured<CementVolume> { rat(305) });

    CHECK(formula::render(withAdmixture) == "(V_w + V_a) / V_c");
    auto const computed = formula::checked_evaluate<WaterCementRatio>(withAdmixture, inputs);
    REQUIRE(computed.has_value());
    CHECK(valueOf(*computed) == rat(3, 5));
    CHECK(inputs.get<AdmixtureVolume>().value() == rat(10));
    CHECK(inputs.get<WaterVolume>().value() == rat(173));
}

TEST_CASE("quantity alias: Measured, Entered and the environment", "[quantity][alias]")
{
    constexpr formula::Measured<WaterVolume> absent {};
    CHECK_FALSE(absent.has_value());

    constexpr auto typedIn = formula::environment(formula::Measured<WaterVolume> { rat(183) },
                                                  formula::Measured<CementVolume> { rat(305) },
                                                  formula::entered(formula::Measured<WaterCementRatio> { rat(1, 2) }));
    STATIC_REQUIRE(decltype(typedIn)::provides<WaterVolume>);
    STATIC_REQUIRE_FALSE(decltype(mix)::provides<WaterCementRatio>);
    CHECK(typedIn.source_of<WaterCementRatio>() == formula::ValueSource::ManuallyEntered);
    CHECK(typedIn.get<CementVolume>().value() == rat(305));

    auto const overridden = formula::checked_evaluate<WaterCementRatio>(ratio, typedIn);
    REQUIRE(overridden.has_value());
    CHECK(overridden->is_overridden());
    CHECK(valueOf(*overridden) == rat(1, 2));

    constexpr auto noWater =
        formula::environment(formula::Measured<WaterVolume>::absent(), formula::Measured<CementVolume> { rat(305) });
    auto const empty = formula::checked_evaluate<WaterCementRatio>(ratio, noWater);
    REQUIRE(empty.has_value());
    CHECK(empty->is_empty());
}

namespace
{
using Force = formula::Quantity<struct ForceTag, "F", "maximum load at failure", unit::Newton>;
using EdgeA = formula::Quantity<struct EdgeATag, "a", "first loaded edge", unit::Millimetre>;
using EdgeB = formula::Quantity<struct EdgeBTag, "b", "second loaded edge", unit::Millimetre>;
using ShapeFactor = formula::Quantity<struct ShapeFactorTag, "k_s", "shape factor", unit::One>;

struct Cube
{
};
struct Prism
{
};

// clang-format off
inline constexpr auto strength = formula::method(
    formula::variants(formula::variant<Prism>(var<Force> / (var<EdgeA> * var<EdgeA>)),
                      formula::variant<Cube>(var<ShapeFactor> * var<Force> / (var<EdgeA> * var<EdgeB>))),
    formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints(formula::constraint(var<Force> >= formula::constant<unit::Kilonewton>(rat(473, 10)),
                                             formula::Verdict { "the load at failure is below 47.3 kN" })));
// clang-format on

// 89.3 kN on a 163 x 103 mm face, with a shape factor of 1.043: 5.5477... MPa.
inline constexpr auto specimen = formula::environment(formula::Measured<Force> { rat(89'300) },
                                                      formula::Measured<EdgeA> { rat(163) },
                                                      formula::Measured<EdgeB> { rat(103) },
                                                      formula::Measured<ShapeFactor> { rat(1043, 1000) });

inline constexpr formula::Citation annex { .reference = "Example Standard 12:2021 NA", .section = "NA.2" };

[[nodiscard]] Rational pascals(formula::Evaluated<Rational> const& result)
{
    REQUIRE(result.has_value());
    REQUIRE(result->has_value());
    return **result;
}

template <typename Tag, typename M, typename... V>
[[nodiscard]] std::string derivationOf(M const& m, V const&... vocabulary)
{
    formula::Trace<> trace {};
    (void) formula::evaluate_method<Tag>(m, specimen, formula::RecordingSink { trace, vocabulary... });
    return formula::render_trace(trace, { .maxSteps = 30 });
}
} // namespace

TEST_CASE("quantity alias: a method's variants, rounding rule and constraints", "[quantity][alias]")
{
    CHECK(pascals(formula::evaluate_method<Cube>(strength, specimen)) == rat(5'500'000));
    CHECK(derivationOf<Cube>(strength).find("k_s = 1043/1000") != std::string::npos);

    auto const outcomes = formula::check_method(strength, specimen);
    REQUIRE(outcomes.size() == 1);
    CHECK(outcomes[0].is_satisfied());
}

TEST_CASE("quantity alias: a vocabulary renames an alias quantity on the page and in the trace", "[quantity][alias]")
{
    constexpr auto words = formula::vocabulary(formula::renames<EdgeA>("b"), formula::renames<EdgeB>("a"));
    auto const cube = std::get<1>(strength.variantSet.cases).expression;
    CHECK(formula::render(cube, words) == "k_s * F / (b * a)");
    CHECK(derivationOf<Cube>(strength, words).find("b = 163 mm\n") != std::string::npos);

    formula::Documentation const page = formula::document(cube, words);
    bool renamed = false;
    for (formula::SymbolEntry const& row: page.symbols)
        renamed = renamed || (row.symbol == "b" && row.description == "first loaded edge");
    CHECK(renamed);
}

TEST_CASE("quantity alias: overlays fix, derive and replace by alias quantity", "[quantity][alias]")
{
    constexpr auto fixed =
        formula::apply(formula::overlay(formula::with_constant<ShapeFactor>(rat(863, 1000), annex)), strength);
    CHECK(pascals(formula::evaluate_method<Cube>(fixed, specimen)) == rat(4'600'000));
    CHECK(derivationOf<Cube>(fixed).find("fixed by jurisdiction overlay") != std::string::npos);

    constexpr auto derived =
        formula::apply(formula::overlay(formula::add_derived<ShapeFactor>(var<EdgeB> / var<EdgeA>, annex)), strength);
    CHECK(pascals(formula::evaluate_method<Cube>(derived, specimen)) == rat(3'400'000));
    formula::Documentation const page = formula::document(std::get<1>(derived.variantSet.cases).expression);
    REQUIRE_FALSE(page.symbols.empty());
    CHECK(page.symbols.front().derivedAs == std::optional<std::string> { "b / a" });

    constexpr auto replaced = formula::apply(
        formula::overlay(formula::replace_variant<Prism>(var<Force> / (var<EdgeB> * var<EdgeB>), annex)), strength);
    CHECK(derivationOf<Prism>(replaced).find("[replaced by jurisdiction overlay: Example Standard 12:2021 NA, NA.2]")
          != std::string::npos);
}

namespace
{
using Diameter = formula::Quantity<struct DiameterTag, "d", "specimen diameter", unit::Millimetre>;
using SizeFactor = formula::Quantity<struct SizeFactorTag, "k_d", "size factor", unit::Percent>;

inline constexpr formula::BandTable<2> sizeBands { formula::band(0, 1, 127, 1), formula::band(127, 1, 241, 1) };
inline constexpr formula::BreakpointTable<2> sizePoints { formula::breakpoint(103), formula::breakpoint(163) };
} // namespace

TEST_CASE("quantity alias: a lookup keyed by an alias quantity, answered as one", "[quantity][alias]")
{
    constexpr auto banded =
        formula::banded_lookup<unit::Millimetre, sizeBands, unit::Percent>(var<Diameter>, { rat(97), rat(89) });
    constexpr auto interpolated =
        formula::interpolating_lookup<unit::Millimetre, sizePoints, unit::Percent>(var<Diameter>, { rat(97), rat(91) });
    constexpr auto specimenSize = formula::environment(formula::Measured<Diameter> { rat(133) });

    auto const band = formula::checked_evaluate<SizeFactor>(banded, specimenSize);
    REQUIRE(band.has_value());
    CHECK(valueOf(*band) == rat(89));
    auto const read = formula::checked_evaluate<SizeFactor>(interpolated, specimenSize);
    REQUIRE(read.has_value());
    CHECK(valueOf(*read) == rat(94));
    CHECK(formula::render(banded).find("d") != std::string::npos);
}

namespace
{
using Retained = formula::Quantity<struct RetainedTag, "m_r", "mass retained on a screen", unit::Gram>;
using TotalMass = formula::Quantity<struct TotalMassTag, "m_t", "total dry mass", unit::Gram>;
using Share = formula::Quantity<struct ShareTag, "s_r", "share of the total retained", unit::One>;
using Passing = formula::Quantity<struct PassingTag, "p", "percentage passing a screen", unit::Percent>;
using Opening = formula::Quantity<struct OpeningTag, "o", "screen opening", unit::Metre>;
using ParticleSize = formula::Quantity<struct ParticleSizeTag, "s", "particle size", unit::Metre>;
using Count = formula::Quantity<struct CountTag, "n", "particles in a class", unit::One>;

inline constexpr formula::BreakpointTable<3> screens { formula::breakpoint(103),
                                                       formula::breakpoint(127),
                                                       formula::breakpoint(163) };
inline constexpr formula::BandTable<2> sizeClasses { formula::band(0, 1, 127, 1), formula::band(127, 1, 197, 1) };

inline constexpr auto screenings =
    formula::environment(formula::measured_series<Retained>(formula::Measured<Retained> { rat(139) },
                                                            formula::Measured<Retained> { rat(197) },
                                                            formula::Measured<Retained> { rat(103) }),
                         formula::Measured<TotalMass> { rat(439) },
                         formula::measured_series<Passing>(formula::Measured<Passing> { rat(31) },
                                                           formula::Measured<Passing> { rat(47) },
                                                           formula::Measured<Passing> { rat(83) }));
} // namespace

TEST_CASE("quantity alias: series, observations and curves of alias quantities", "[quantity][alias]")
{
    constexpr auto shares = formula::series<Retained, 3> / var<TotalMass>;
    CHECK(formula::render(shares) == "m_r(i) / m_t");
    auto const perScreen = formula::checked_evaluate_series<Share>(shares, screenings);
    REQUIRE(perScreen.has_value());
    CHECK(perScreen->elements()[1].value() == rat(197, 439));

    auto const inAll = formula::checked_evaluate<Retained>(formula::sum(formula::series<Retained, 3>), screenings);
    REQUIRE(inAll.has_value());
    CHECK(valueOf(*inAll) == rat(439));

    constexpr auto grading = formula::curve(formula::domain<unit::Metre, screens>, formula::series<Passing, 3>);
    auto const at139 = formula::checked_evaluate<Passing>(
        formula::interpolate_at(grading, formula::constant<unit::Metre>(rat(139))), screenings);
    REQUIRE(at139.has_value());
    CHECK(valueOf(*at139) == rat(59));

    auto const explained = formula::explain_series<Share>(shares, screenings);
    CHECK(formula::render_trace(explained.trace, { .maxSteps = 10 }).starts_with("1. m_r = 139 g; 197 g; 103 g\n"));

    constexpr auto counted = formula::binned<unit::Metre, sizeClasses>(formula::observations<ParticleSize, 8>);
    constexpr auto sample =
        formula::environment(formula::MeasuredObservations<ParticleSize, 8>(rat(103), rat(163), rat(113), rat(173)));
    auto const classes = formula::checked_evaluate_series<Count>(counted, sample);
    REQUIRE(classes.has_value());
    CHECK(classes->elements()[0].value() == rat(2));
    CHECK(classes->elements()[1].value() == rat(2));
}

namespace
{
using Mass = formula::Quantity<struct MassTag, "m", "mass of a determination", unit::Gram>;

inline constexpr formula::Verdict repeatTest { "discard the determinations and repeat the test" };
inline constexpr formula::Citation rejectionRule { .title = "Outliers",
                                                   .reference = "Example Standard 5:2022",
                                                   .section = "7.4" };

// 40.2, 39.8, 40.5, 44.0, 40.0 and 43.3 g.
inline constexpr auto sixMasses =
    formula::environment(formula::measured_series<Mass>(formula::Measured<Mass> { rat(402, 10) },
                                                        formula::Measured<Mass> { rat(398, 10) },
                                                        formula::Measured<Mass> { rat(405, 10) },
                                                        formula::Measured<Mass> { rat(44) },
                                                        formula::Measured<Mass> { rat(40) },
                                                        formula::Measured<Mass> { rat(433, 10) }));
} // namespace

TEST_CASE("quantity alias: statistics and rejection over an alias quantity", "[quantity][alias]")
{
    auto const mean = formula::checked_evaluate<Mass>(formula::sample_mean(formula::series<Mass, 6>), sixMasses);
    REQUIRE(mean.has_value());
    CHECK(valueOf(*mean) == rat(413, 10));

    constexpr auto withoutOutliers = formula::
        without_outliers<formula::PerPass::MostExtreme, formula::OnLimit::Keep, formula::AtMost<2>, formula::KeepAtLeast<4>>(
            formula::series<Mass, 6>,
            formula::deviation_from_mean(rat(6, 100) * formula::pass_mean<Mass>),
            repeatTest,
            rejectionRule);
    auto const settled = formula::checked_evaluate_rejection<Mass>(withoutOutliers, sixMasses);
    REQUIRE(settled.has_value());
    CHECK(valueOf(settled->outcome()) == rat(321, 8));
    CHECK(settled->rejected().size() == 2);

    auto const observed = formula::checked_evaluate<Mass>(
        formula::sample_mean(formula::observations<Mass, 8>),
        formula::environment(formula::MeasuredObservations<Mass, 8>(rat(402, 10), rat(398, 10), rat(405, 10))));
    REQUIRE(observed.has_value());
    CHECK(valueOf(*observed) == rat(241, 6));
}

namespace
{
using Strength = formula::Quantity<struct StrengthTag, "f_c", "compressive strength", unit::Megapascal>;

struct Reference
{
};
struct MaterialBatch
{
};

inline constexpr auto here = formula::environment(formula::Measured<Strength> { rat(30) });
inline constexpr auto there = formula::environment(formula::entered(formula::Measured<Strength> { rat(20) }));

template <typename Lineage>
[[nodiscard]] constexpr auto recordsWith(Lineage batch)
{
    return formula::record_context(
        formula::record<formula::ThisRecord>(
            formula::record_key(formula::sample_id(17), formula::test_id(5)), here, formula::lineage<MaterialBatch>(4411)),
        formula::record<Reference>(formula::record_key(formula::sample_id(23), formula::test_id(3)), there, batch));
}
} // namespace

TEST_CASE("quantity alias: read from another record, gated on lineage", "[quantity][alias]")
{
    constexpr auto relative = var<Strength> / formula::from_record<Reference>(var<Strength>);
    CHECK(formula::render(relative) == "f_c / (f_c of Reference)");

    constexpr auto sameBatch = recordsWith(formula::lineage<MaterialBatch>(4411));
    auto const ratioOf = formula::checked_evaluate_si<Rational>(relative, sameBatch);
    REQUIRE(ratioOf.has_value());
    REQUIRE(ratioOf->has_value());
    CHECK(**ratioOf == rat(3, 2));

    formula::Trace<> trace {};
    (void) formula::checked_evaluate_si<Rational>(relative, sameBatch, formula::RecordingSink { trace });
    CHECK(formula::render_trace(trace, { .maxSteps = 10 })
              .find("f_c = 20 MPa, from record Reference (sample 23, test 3), entered by hand")
          != std::string::npos);

    constexpr auto gated = formula::from_record<Reference>(var<Strength>, formula::same_lineage<MaterialBatch>());
    CHECK(formula::checked_explain<Strength>(gated, sameBatch).has_value());
    auto const refused = formula::checked_explain<Strength>(gated, recordsWith(formula::lineage<MaterialBatch>(4412)));
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().error == formula::ArithmeticError::DomainError);
}

namespace
{
using Reading = formula::Quantity<struct ReadingTag, "r", "an invented reading", unit::Gram>;
using Spread = formula::Quantity<struct SpreadTag, "r_sp", "the spread of the readings", unit::Gram>;
using Estimate = formula::Quantity<struct EstimateTag, "w", "an invented iterated estimate", unit::Gram>;
using Determination = formula::Quantity<struct DeterminationTag, "d", "an invented determination", unit::Gram>;
using Agreed = formula::Quantity<struct AgreedTag, "d_a", "an invented agreed determination", unit::Gram>;

struct SeriesSpan
{
    static constexpr std::string_view name = "series span";
    static constexpr std::array shapes { formula::InputShape::Series };
    static constexpr std::array<std::string_view, 1> outputs { "span" };

    static consteval std::optional<std::array<formula::Dimension, 1>> output_dimensions(
        std::array<formula::Dimension, 1> declared) noexcept
    {
        return std::array { declared[0] };
    }

    template <typename Rep>
    static constexpr std::expected<std::array<Rep, 1>, formula::ArithmeticError> compute(
        std::span<Rep const> readings) noexcept
    {
        Rep least = readings[0];
        Rep most = readings[0];
        for (Rep const& each: readings)
        {
            if (each < least)
                least = each;
            if (most < each)
                most = each;
        }
        std::expected<Rep, formula::ArithmeticError> const difference = formula::RepTraits<Rep>::subtract(most, least);
        if (!difference.has_value())
            return std::unexpected { difference.error() };
        return std::array { *difference };
    }
};

inline constexpr formula::Citation spanCitation { .title = "Spread of readings",
                                                  .reference = "Example Standard 12",
                                                  .section = "4.2" };
} // namespace

TEST_CASE("quantity alias: an opaque operation over an alias quantity's series", "[quantity][alias]")
{
    constexpr auto span =
        formula::opaque_output<"span">(formula::opaque<SeriesSpan>(spanCitation, formula::series<Reading, 4>));
    constexpr auto readings =
        formula::environment(formula::measured_series<Reading>(formula::Measured<Reading> { rat(127) },
                                                               formula::Measured<Reading> { rat(103) },
                                                               formula::Measured<Reading> { rat(191) },
                                                               formula::Measured<Reading> { rat(139) }));
    auto const spread = formula::explain<Spread>(span, readings);
    CHECK(valueOf(spread.outcome) == rat(88));
    CHECK(formula::render_trace(spread.trace, { .maxSteps = 10 }).find("r = 127 g; 103 g; 191 g; 139 g")
          != std::string::npos);
}

TEST_CASE("quantity alias: a retry of an alias quantity reads its previous attempt", "[quantity][alias]")
{
    // w(k) = 6.08 g + w(k-1) / 2, from 0 g, accepted once it rose by at most 0.76 g.
    constexpr auto halving = formula::constant<unit::Gram>(rat(152, 25)) + formula::previous_attempt<Estimate> / rat(2);
    constexpr auto settled =
        formula::previous_attempt<Estimate> - formula::this_attempt<Estimate> >= formula::constant<unit::Gram>(rat(-19, 25));
    constexpr auto fourAttempts = formula::retry<Estimate, 4, formula::FirstJudged::AtFirstAttempt>(
        formula::starting_from(formula::constant<unit::Gram>(rat(0))),
        halving,
        settled,
        formula::Verdict { "repeat the determination" },
        { .title = "Settled estimate", .reference = "Example Standard 12", .section = "6" });

    CHECK(formula::render(fourAttempts).find("w(k-1)") != std::string::npos);
    auto const accepted = formula::checked_evaluate_retry(fourAttempts, formula::environment());
    REQUIRE(accepted.has_value());
    CHECK(accepted->end() == formula::RetryEnd::Accepted);
    CHECK(valueOf(accepted->outcome()) == rat(57, 5));

    constexpr auto successive = formula::retry<Agreed, 4, formula::FirstJudged::AtSecondAttempt>(
        formula::attempt_input<Determination>,
        formula::abs(formula::this_attempt<Agreed> - formula::previous_attempt<Agreed>)
            <= formula::constant<unit::Gram>(rat(127, 100)),
        formula::Verdict { "repeat the test" },
        { .title = "Agreed determination", .reference = "Example Standard 12", .section = "7" });
    constexpr auto determinations =
        formula::environment(formula::measured_series<Determination>(formula::Measured<Determination> { rat(413, 10) },
                                                                     formula::Measured<Determination> { rat(439, 10) },
                                                                     formula::Measured<Determination> { rat(427, 10) },
                                                                     formula::Measured<Determination> { rat(457, 10) }));
    auto const agreed = formula::checked_evaluate_retry(successive, determinations);
    REQUIRE(agreed.has_value());
    CHECK(agreed->accepted_at() == std::optional<std::size_t> { 2 });
    CHECK(valueOf(agreed->outcome()) == rat(427, 10));
}
