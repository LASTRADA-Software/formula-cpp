// SPDX-License-Identifier: Apache-2.0
#include <formula-cpp/document.hpp>
#include <formula-cpp/formula.hpp>
#include <formula-cpp/render.hpp>
#include <formula-cpp/trace.hpp>
#include <formula-cpp/trace_render.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace
{
namespace unit = formula::unit;
using formula::var;

// The discriminators. Empty tags, never instantiated: what a variant applies
// to is a type, so that selecting one is a compile-time fact the type system
// can state rather than a runtime string nobody checks.
struct Cube
{
};
struct Cylinder
{
};
struct Prism
{
};
struct Core
{
};

// Declared and never defined: a tag is never instantiated, so it need not be
// complete. See the test that uses it.
struct Unfinished;

struct Force: formula::Quantity<Force, "F", "applied force", unit::Newton>
{
};
struct EdgeX: formula::Quantity<EdgeX, "x_m", "measured edge", unit::Millimetre>
{
};
struct EdgeY: formula::Quantity<EdgeY, "y_m", "measured edge", unit::Millimetre>
{
};
struct Strength: formula::Quantity<Strength, "f_c", "compressive strength", unit::Megapascal>
{
};

[[nodiscard]] constexpr auto specimen(long long force, long long edgeX, long long edgeY)
{
    return formula::environment(formula::Measured<Force> { formula::Rational { force } },
                                formula::Measured<EdgeX> { formula::Rational { edgeX } },
                                formula::Measured<EdgeY> { formula::Rational { edgeY } });
}

/// Writes down what `check_method` tells it about a method's constraints, in
/// order: `E` and `P` for the pair around the checks, with whose they are,
/// and `c` for each verdict.
struct AcceptanceWitness: formula::NullSink
{
    std::string* events;

    void acceptance_entered(formula::ConstraintOrigin const& origin) const
    {
        *events += origin.provenance() == formula::ConstraintProvenance::MethodOwn ? "E(own)" : "E(overlay)";
    }
    template <formula::Predicate P>
    void constraint_produced(formula::Constraint<P> const&, formula::ConstraintOutcome const&) const
    {
        *events += "c";
    }
    void acceptance_produced(formula::ConstraintOrigin const&) const
    {
        *events += "P";
    }
};

/// The same, defining only the first of the pair.
struct HalfAcceptanceWitness: formula::NullSink
{
    std::string* events;

    void acceptance_entered(formula::ConstraintOrigin const&) const
    {
        *events += "E";
    }
    template <formula::Predicate P>
    void constraint_produced(formula::Constraint<P> const&, formula::ConstraintOutcome const&) const
    {
        *events += "c";
    }
};

// Two constraints of the method's own: 90 kN holds the first (at least
// 47.3 kN) and not the second (at least 97.3 kN).
inline constexpr auto twoOwnConstraints = formula::method(
    formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) )),
    formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints(formula::constraint(var<Force> >= formula::constant<unit::Newton>(formula::Rational { 47'300 }),
                                             formula::Verdict { "the load is below the minimum" }),
                         formula::constraint(var<Force> >= formula::constant<unit::Newton>(formula::Rational { 97'300 }),
                                             formula::Verdict { "the load is below the upper minimum" })));
} // namespace

TEST_CASE("a variants pack holds structurally different expressions", "[method]")
{
    constexpr auto pack = formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                            formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ));

    STATIC_REQUIRE(std::tuple_size_v<decltype(pack.cases)> == 2);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<0, decltype(pack.cases)>::tag, Cube>);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<1, decltype(pack.cases)>::tag, Cylinder>);

    // The point of the pack, and the reason it is a tuple rather than an
    // array: the two variants really are different TYPES. An array would
    // require one type for both and so could not hold the spec's own pair.
    // Without this line a guard that demanded the variants be the same type
    // would pass every other assertion in this file.
    STATIC_REQUIRE(
        !std::is_same_v<std::tuple_element_t<0, decltype(pack.cases)>, std::tuple_element_t<1, decltype(pack.cases)>>);
}

TEST_CASE("a variant carries its tag, its expression and the dimension it reports", "[method]")
{
    constexpr auto only = formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) );

    STATIC_REQUIRE(std::is_same_v<decltype(only)::tag, Cube>);
    STATIC_REQUIRE(decltype(only)::dimension == formula::dim::Pressure);

    // A variant is not a `Node`: it names an expression, it is not one. D1 of
    // the phase's design decisions turns on this -- a method selects among
    // formulas rather than standing where a number stands -- and the cheapest
    // way for that to stop being true is for someone to give `VariantCase` a
    // `NodeBase` it does not need.
    STATIC_REQUIRE(!formula::Node<decltype(only)>);
}

TEST_CASE("a variant's tag need not be a complete type", "[method]")
{
    // The tag rule refuses what can never be selected -- `void`, `int`, a
    // reference, a cv-qualified class -- and deliberately stops short of
    // `std::is_empty_v`, which would demand a complete type. This is what
    // pins that choice: a rule tightened to `is_empty_v` refuses this file --
    // measured on cl 19.51, clang-cl 22, clang++ 20 and g++ 13, each of which
    // rejects `is_empty_v` of the incomplete `Unfinished` outright.
    constexpr auto only = formula::variant<Unfinished>(var<Force> / (var<EdgeX> * var<EdgeY>) );

    STATIC_REQUIRE(std::is_same_v<decltype(only)::tag, Unfinished>);
}

TEST_CASE("the expression a variant was given is the expression it stores", "[method]")
{
    // A REACHABILITY PROBE, and judged as one. The other tests here inspect
    // types; this is the only one that takes a variant back out of the pack
    // and evaluates it, which is what says the pack holds usable formulas
    // rather than merely well-formed types.
    //
    // It kills no mutation uniquely, and that is characteristic of the kind
    // rather than a defect in this one: dropping the expression in `variant()`
    // leaves its parameter unreferenced and `/W4 /WX` rejects the build before
    // any test runs. What a reachability probe catches is ABSENCE -- phase 9
    // shipped an overload that worked, was tested, and no user could call;
    // phase 10 shipped a `document()` walk that compiled for nothing. The
    // spike compiled this pack and never ran it, so nothing until now had
    // established that a stored variant still evaluates at all.
    //
    // `ConstantNode` because it is the node with runtime state reachable from
    // the operators THIS fixture uses: a `VarNode` holds no bytes, and a
    // `BinaryNode`'s bytes are only its children. That is a property of this
    // fixture and not of node kinds in general -- lookups carry their
    // corrections as runtime state for the same reason `ConstantNode` carries
    // its number, and they are exactly what a method's variants are likely to
    // hold, so a round-trip probe over one of those would not be pointless.
    constexpr auto pack = formula::variants(
        formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
        formula::variant<Cylinder>(var<Force> / (formula::number(formula::Rational { 1, 2 }) * var<EdgeX> * var<EdgeX>) ));

    // 1000 N over a 100 mm square face is 100 kPa, which is 0.1 MPa.
    constexpr auto cube = formula::checked_evaluate<Strength>(std::get<0>(pack.cases).expression, specimen(1000, 100, 100));
    REQUIRE(cube.has_value());
    CHECK(cube->measurement().value() == formula::Rational { 1, 10 });

    // The same load over half that area is twice the strength -- and that
    // factor of a half exists only in the stored expression.
    constexpr auto cylinder =
        formula::checked_evaluate<Strength>(std::get<1>(pack.cases).expression, specimen(1000, 100, 100));
    REQUIRE(cylinder.has_value());
    CHECK(cylinder->measurement().value() == formula::Rational { 1, 5 });
}

TEST_CASE("four variants that agree in dimension without agreeing in type are accepted", "[method]")
{
    // The positive twin of `negative/method_variants_disagree.cpp`: the same
    // four-variant shape, and a third variant that measures what the other
    // three measure. Without this, a guard that refused every four-variant
    // pack -- or refused whenever the variants' types differ at all -- would
    // be indistinguishable from the one this library wants.
    constexpr auto pack = formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                            formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ),
                                            formula::variant<Prism>(var<Force> / (var<EdgeY> * var<EdgeY>) ),
                                            formula::variant<Core>(var<Force> / (var<EdgeY> * var<EdgeX>) ));

    STATIC_REQUIRE(std::tuple_size_v<decltype(pack.cases)> == 4);
    STATIC_REQUIRE(std::is_same_v<std::tuple_element_t<2, decltype(pack.cases)>::tag, Prism>);
    STATIC_REQUIRE(std::tuple_element_t<0, decltype(pack.cases)>::dimension
                   == std::tuple_element_t<2, decltype(pack.cases)>::dimension);
}

TEST_CASE("duplicate variant tags are refused", "[method]")
{
    // A well-formed pack is accepted -- the control, so this test is shown
    // able to pass as well as able to fail.
    constexpr auto fine = formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                                            formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) ));
    STATIC_REQUIRE(formula::detail::tags_are_distinct(fine));
}

TEST_CASE("a pack that repeats a tag is not a well-formed method's variants", "[method]")
{
    // Asked of the TYPES, which never completes the pack, so a repeat can be
    // answered false here rather than refused. `evaluate_method` gates its
    // body on `IsWellFormedMethod`, whose doc says it asks every rule the
    // variants ask -- this is what holds the distinct-tag rule to that.
    //
    // The repeat is at (0, 1) on purpose, a pair every narrowing of the
    // pairwise comparison still makes, so that this test does not overlap
    // `negative/method_duplicate_tag.cpp`, which is what the narrowings die
    // against.
    using Pressure = decltype(var<Force> / (var<EdgeX> * var<EdgeY>) );
    using Repeated = formula::Variants<formula::VariantCase<Cube, Pressure>, formula::VariantCase<Cube, Pressure>>;
    using Distinct = formula::Variants<formula::VariantCase<Cube, Pressure>, formula::VariantCase<Cylinder, Pressure>>;
    using Rule =
        formula::RoundingRule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>;
    using NoConstraints = decltype(formula::constraints());

    STATIC_REQUIRE(!formula::detail::VariantTagsAreDistinct<Repeated>::value);
    STATIC_REQUIRE(formula::detail::VariantTagsAreDistinct<Distinct>::value);
    STATIC_REQUIRE(!formula::detail::IsWellFormedMethod<formula::Method<Repeated, Rule, NoConstraints>>::value);
    STATIC_REQUIRE(formula::detail::IsWellFormedMethod<formula::Method<Distinct, Rule, NoConstraints>>::value);
}

TEST_CASE("a repeated tag is reported by the later of its two positions", "[method]")
{
    // `[Cube, Cylinder, Cylinder, Cube]` holds two repeats, and the two ways
    // of ordering them disagree: by the later position the first repeat is
    // Cylinder at (1, 2), because 2 comes before 3; by the earlier position
    // it is Cube at (0, 3). `first_repeated_pair` documents the former, and
    // `RequireTagDeclaredOnce<First, Second, Tag>` reports what it answers.
    constexpr auto repeated = formula::detail::first_repeated_pair<Cube, Cylinder, Cylinder, Cube>();
    STATIC_REQUIRE(repeated.first == 1);
    STATIC_REQUIRE(repeated.second == 2);

    // The control: nothing repeated answers no pair at all.
    constexpr auto distinct = formula::detail::first_repeated_pair<Cube, Cylinder, Prism, Core>();
    STATIC_REQUIRE(distinct.first == distinct.second);
}

TEST_CASE("a method selects the variant matching the tag", "[method]")
{
    constexpr auto m = formula::method(
        formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                          formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
        formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());

    constexpr auto inputs = formula::environment(formula::Measured<Force> { formula::Rational { 90'000 } },
                                                 formula::Measured<EdgeX> { formula::Rational { 150 } },
                                                 formula::Measured<EdgeY> { formula::Rational { 100 } });

    // `evaluate_method` answers in the coherent SI unit, as every
    // `Evaluated<Rep>` in this library does -- so 6 MPa is 6'000'000 Pa.
    //
    // Cube: 90000 N / (150 mm * 100 mm) = 6 MPa
    constexpr auto cube = formula::evaluate_method<Cube>(m, inputs);
    STATIC_REQUIRE(cube.has_value());
    STATIC_REQUIRE(cube->has_value());
    STATIC_REQUIRE(cube->value() == formula::Rational { 6'000'000 });

    // Cylinder uses EdgeX twice: 90000 / (150*150) = 4 MPa -- a DIFFERENT
    // number, so this test cannot pass if selection picked the wrong variant.
    constexpr auto cylinder = formula::evaluate_method<Cylinder>(m, inputs);
    STATIC_REQUIRE(cylinder.has_value());
    STATIC_REQUIRE(cylinder->has_value());
    STATIC_REQUIRE(cylinder->value() == formula::Rational { 4'000'000 });

    // The constraint set is held as it was given, not unpacked -- see
    // `ConstraintSet` in `constraint.hpp` for why `check_all()` wants it whole.
    STATIC_REQUIRE(std::is_same_v<decltype(m.constraintSet), formula::ConstraintSet<>>);
}

TEST_CASE("a method applies its own rounding rule to the variant it selects", "[method]")
{
    // The selection test above lands on 6 and 4 MPa, which rounding to one
    // decimal leaves alone, so it cannot tell a method that rounds from one
    // that does not. This one can, with two points on the same variant.
    //
    // 60500 N over 100 mm * 100 mm is 6.05 MPa, a tie at one place. Ignored,
    // the rule leaves it 6.05; rounded in pascals rather than megapascals,
    // likewise; to zero or two places, 6 or 6.05; half-even,
    // half-toward-zero, floor or toward zero, 6.0. That leaves three modes
    // giving 6.1: the declared half-away-from-zero, and ceiling and
    // away-from-zero, which round every non-zero remainder up.
    //
    // 60400 N is 6.04 MPa, not a tie, and separates those three: ceiling and
    // away-from-zero give 6.1, half-away-from-zero 6.0. Of the seven modes,
    // only half-away-from-zero gives 6.1 on the first point AND 6.0 on the
    // second.
    constexpr auto m = formula::method(
        formula::variants(formula::variant<Cube>(var<Force> / (var<EdgeX> * var<EdgeY>) ),
                          formula::variant<Cylinder>(var<Force> / (var<EdgeX> * var<EdgeX>) )),
        formula::rounding_rule<unit::Megapascal, formula::DecimalPlaces { 1 }, formula::RoundingMode::HalfAwayFromZero>(),
        formula::constraints());

    // The unrounded figure, measured through the variant's own expression, so
    // that the fixture's premise is checked rather than asserted in a comment.
    constexpr auto unrounded =
        formula::checked_evaluate<Strength>(std::get<1>(m.variantSet.cases).expression, specimen(60'500, 100, 999));
    STATIC_REQUIRE(unrounded->measurement().value() == formula::Rational { 605, 100 });

    // Cylinder, the second variant rather than the first, so that a method
    // rounding only its first variant would not pass either.
    constexpr auto cylinder = formula::evaluate_method<Cylinder>(m, specimen(60'500, 100, 999));
    STATIC_REQUIRE(cylinder.has_value());
    STATIC_REQUIRE(cylinder->has_value());
    STATIC_REQUIRE(cylinder->value() == formula::Rational { 6'100'000 });

    constexpr auto notATie = formula::evaluate_method<Cylinder>(m, specimen(60'400, 100, 999));
    STATIC_REQUIRE(notATie.has_value());
    STATIC_REQUIRE(notATie->has_value());
    STATIC_REQUIRE(notATie->value() == formula::Rational { 6'000'000 });
}

TEST_CASE("a sink is told whose constraints they are around the checks, or not at all", "[method][constraint]")
{
    // Both of the pair: told once before the verdicts and once after, and
    // that these are the method's own.
    std::string both;
    (void) formula::check_method(twoOwnConstraints, specimen(90'000, 150, 100), AcceptanceWitness { {}, &both });
    CHECK(both == "E(own)ccP");

    // Only the first of the pair: told nothing of it, rather than told of a
    // beginning it will never see the end of. The verdicts still reach it.
    std::string half;
    (void) formula::check_method(twoOwnConstraints, specimen(90'000, 150, 100), HalfAcceptanceWitness { {}, &half });
    CHECK(half == "cc");
}

// ------------------------------------------- phase 13: a precision check

namespace
{
struct PairTag
{
};
struct FirstMass: formula::Quantity<FirstMass, "x_A", "first determination", unit::Gram>
{
};
struct SecondMass: formula::Quantity<SecondMass, "x_B", "second determination", unit::Gram>
{
};
struct MeanMass: formula::Quantity<MeanMass, "x_m", "mean of the determinations", unit::Gram>
{
};
/// A coefficient of the limit a jurisdiction may fix: invented, as every
/// coefficient here is.
struct LevelCoefficient: formula::Quantity<LevelCoefficient, "k_r", "level coefficient", unit::One>
{
};

[[nodiscard]] constexpr formula::Rational ratio(std::int64_t numerator, std::int64_t denominator = 1)
{
    return formula::Rational::make(numerator, denominator).value();
}

inline constexpr auto pairMean = (var<FirstMass> + var<SecondMass>) / ratio(2);

/// Fixture P's check, its coefficient an input: r(level) = 0.1 g + k_r * level.
inline constexpr auto pairAgreement = formula::constraint(
    formula::abs(var<FirstMass> - var<SecondMass>)
        <= formula::precision_limit<formula::PrecisionKind::Repeatability>(
            pairMean, formula::constant<unit::Gram>(ratio(1, 10)) + var<LevelCoefficient> * formula::precision_level<FirstMass>),
    formula::Verdict { "repeat the determinations" });

inline constexpr auto pairMethod = formula::method(
    formula::variants(formula::variant<PairTag>(pairMean)),
    formula::rounding_rule<unit::Gram, formula::DecimalPlaces { 3 }, formula::RoundingMode::HalfAwayFromZero>(),
    formula::constraints(pairAgreement));

/// 40 g and 40.905 g. With k_r = 1/50, r = 0.90905 g and d = 0.905 g:
/// satisfied. With k_r = 1/60, r = 0.77420... g: violated.
[[nodiscard]] constexpr auto pairInputs(formula::Rational coefficient)
{
    return formula::environment(formula::Measured<FirstMass> { ratio(40) },
                                formula::Measured<SecondMass> { ratio(40905, 1000) },
                                formula::Measured<LevelCoefficient> { coefficient });
}
} // namespace

TEST_CASE("a precision check joins a method's constraints and is checked by check_method", "[method][precision]")
{
    STATIC_REQUIRE(formula::check_method(pairMethod, pairInputs(ratio(1, 50)))[0].is_satisfied());
    STATIC_REQUIRE(formula::check_method(pairMethod, pairInputs(ratio(1, 60)))[0].is_violated());

    // The AcceptanceChecked step's operand is the constraint's verdict.
    formula::Trace<> trace {};
    (void) formula::check_method(pairMethod, pairInputs(ratio(1, 50)), formula::RecordingSink<> { trace });
    formula::Step<> const& acceptance = trace.steps.back();
    REQUIRE(acceptance.kind == formula::StepKind::AcceptanceChecked);
    REQUIRE(acceptance.operands.size() == 1);
    CHECK(trace.steps[acceptance.operands[0]].kind == formula::StepKind::Constraint);
}

TEST_CASE("with_constant reaches a coefficient inside a precision limit's limit expression", "[method][precision][overlay]")
{
    // A jurisdiction fixes k_r at 1/60. The environment holds no k_r at all,
    // so this compiles only if the rewrite reached inside the limit; and the
    // verdict flips from satisfied (1/50, an input nobody reads now) to
    // violated.
    constexpr auto overlaid =
        formula::apply(formula::overlay(formula::with_constant<LevelCoefficient>(
                           ratio(1, 60), formula::Citation { .reference = "Example Standard 1:2020 NA", .section = "NA.3" })), pairMethod);
    constexpr auto withoutCoefficient = formula::environment(formula::Measured<FirstMass> { ratio(40) },
                                                             formula::Measured<SecondMass> { ratio(40905, 1000) });
    STATIC_REQUIRE(formula::check_method(overlaid, withoutCoefficient)[0].is_violated());
}

TEST_CASE("a vocabulary renames the results in a precision check on every surface, and the level stays level",
          "[method][precision][vocabulary]")
{
    constexpr auto south = formula::vocabulary(formula::renames<FirstMass>("m_1"), formula::renames<SecondMass>("m_2"));
    CHECK(formula::render(pairAgreement, south)
          == "require abs(m_1 - m_2) <= r(1/10 g + k_r * level; level = (m_1 + m_2) / 2)");
    CHECK(formula::render<formula::Dialect::Markdown>(pairAgreement, south)
          == "require abs(`m_1` - `m_2`) <= r(1/10 g + `k_r` * level; level = (`m_1` + `m_2`) / 2)");
    CHECK(formula::render<formula::Dialect::LaTeX>(pairAgreement, south).find("\\text{level} = \\frac{m_1 + m_2}{2}")
          != std::string::npos);

    formula::Documentation const page = formula::document(pairAgreement, south);
    REQUIRE(page.symbols.size() == 3);
    CHECK(page.symbols[0].symbol == std::string_view { "m_1" });
    CHECK(page.symbols[1].symbol == std::string_view { "m_2" });
    CHECK(page.symbols[2].symbol == std::string_view { "k_r" });

    formula::Trace<> trace {};
    (void) formula::check(pairAgreement, pairInputs(ratio(1, 50)), formula::RecordingSink { trace, south });
    std::string const text = formula::render_trace(trace, { .maxSteps = 40 });
    CHECK(text.starts_with("1. m_1 = 40 g\n2. m_2 = 8181/200 g\n"));
    CHECK(text.find("level = 16181/400 g [bound by #") != std::string::npos);
    CHECK(text.find("x_A") == std::string::npos);
}
