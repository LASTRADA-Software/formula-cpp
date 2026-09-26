// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Precision limits in two declared passes, and the absolute value they are
/// compared with.
///
/// A repeatability limit is often a function of the level of the very results
/// it checks: r = 0.1 g + level / 50, where the level is the mean of the two
/// determinations. That is two passes -- first the level, then the limit at
/// that level -- and the spec asks that they be *declared*, not a coincidence
/// of evaluation order. `precision_limit<Kind>(level, limit)` is that
/// declaration:
///
///  - **Pass 1** evaluates `level`, an ordinary expression -- the mean, or the
///    mean rounded as the method says, or anything else the author writes.
///  - **Pass 2** evaluates `limit` with `precision_level<Q>` standing for the
///    level pass 1 produced. The placeholder is an ordinary `Node`, so the
///    limit is any expression over it: arithmetic, a banded lookup keyed by
///    it (a constant per level, read from a table), a rounding of it.
///
/// Both passes are trace steps, and the limit step names the level it was
/// evaluated at. The limit is itself a `Node`, so a repeatability check is an
/// ordinary `Constraint` -- `abs(x_A - x_B) <= r` -- and joins a method's
/// `ConstraintSet` with nothing new.
///
/// **Why a placeholder, when `0.1 g + (x_A + x_B) / 2 / 50` computes the same
/// number:** the level would then be an anonymous subexpression. It could not
/// be named in the trace, rounded once and read many times, or seen by a
/// reader as what the limit depends on -- which is exactly what makes such a
/// limit hard to check by hand.
///
/// **Refusals, each drawing one message:**
///  - `precision_level` evaluated outside any `precision_limit`'s limit
///    expression (`RequireLevelBound`);
///  - `precision_level` inside a level expression (`RequireLevelWithoutPlaceholder`)
///    -- a level cannot depend on a level, its own or an enclosing limit's;
///  - `precision_level<Q>` whose `Q` does not measure the level's dimension
///    (`RequireLevelDimensionMatches`);
///  - `precision_level<Q>` whose `Q` has a unit with an offset
///    (`RequireLevelUnitWithoutOffset`).
///
/// The second and third are seen through every node kind this library ships,
/// an overlay's derived quantity and a method's rounding included, so an
/// overlay that puts a placeholder into a level is refused where it is
/// applied. **What they cannot see:** the inside of a consumer's own node
/// kind. A placeholder there is still refused at evaluation where no limit
/// binds it, and where it would read a level of another dimension; but one in
/// a nested limit's *level* reads the enclosing limit's level, silently.
///
/// Nesting one limit inside another's limit expression is allowed; the inner
/// binding shadows the outer, as an inner scope's name does.
///
/// **What is not modelled:** reproducibility across laboratories changes only
/// the name (`R` rather than `r`) and the trace's words, never the arithmetic.
/// The other laboratory's result is an ordinary input here; reading another
/// test's record is a later phase's.

#include <formula-cpp/binning.hpp>
#include <formula-cpp/citation.hpp>
#include <formula-cpp/conditional.hpp>
#include <formula-cpp/critical_value.hpp>
#include <formula-cpp/curve.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/escape.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/lookup.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounded_root.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/series.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/snap.hpp>
#include <formula-cpp/statistics.hpp>
#include <formula-cpp/unit.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <tuple>
#include <type_traits>

namespace formula
{

// ------------------------------------------------------------------ abs

/// The absolute value of @p Operand. Its dimension is the operand's.
template <Node Operand>
struct AbsoluteValueNode: NodeBase
{
    /// The expression whose magnitude is taken.
    ///
    /// Deliberately no `{}` default member initialiser -- see `Corrections`
    /// (`lookup.hpp`).
    Operand operand;

    /// A magnitude measures what its operand measures.
    static constexpr Dimension dimension = Operand::dimension;
};

/// The absolute value of `operand`: `abs(var<ResultA> - var<ResultB>)`.
///
/// Named `abs` after task 1's spike: beside `<cmath>` and `<cstdlib>`, under
/// `using namespace std;`, found by ADL and as a consumer's local name, it drew
/// no ambiguity and no warning on cl 19.51, clang-cl and clang++ 22.1.3, or
/// g++ 13.3 and 14.2.
template <Node Operand>
[[nodiscard]] constexpr auto abs(Operand operand) noexcept
{
    return AbsoluteValueNode<Operand> { {}, operand };
}

/// Evaluates the operand and takes its magnitude. Absence and errors pass
/// through; negating the one value `Rational` cannot negate is `Overflow`.
template <typename Rep = Rational, Node Operand, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(AbsoluteValueNode<Operand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const evaluatedOperand = detail::dispatch<Rep>(node.operand, environment, sink);
    if (!evaluatedOperand.has_value())
    {
        Evaluated<Rep> const failed = std::unexpected { evaluatedOperand.error() };
        sink.produced(node, failed);
        return failed;
    }
    if (!evaluatedOperand->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    std::expected<Rep, ArithmeticError> const magnitude = **evaluatedOperand < Rep {}
                                                              ? RepTraits<Rep>::negate(**evaluatedOperand)
                                                              : std::expected<Rep, ArithmeticError> { **evaluatedOperand };
    Evaluated<Rep> const evaluated =
        magnitude.has_value() ? detail::present<Rep>(*magnitude) : Evaluated<Rep> { std::unexpected { magnitude.error() } };
    sink.produced(node, evaluated);
    return evaluated;
}

// ----------------------------------------------------- the bound environment

namespace detail
{
    /// The environment an enclosing construct evaluates a subexpression in:
    /// the caller's environment, unchanged, plus the values that construct
    /// binds -- a precision limit's level here, a rejection pass's mean and
    /// count later. Written once, for every such construct.
    ///
    /// It forwards exactly what the shipped nodes ask of an environment --
    /// `provides`, `is_entered`, `get` and `source_of`, measured by task 1's
    /// spike, and phase 12's `is_entered_series` and `get_series` -- so every
    /// node, a series node included, evaluates inside it as it does outside. A
    /// binding it does not hold itself is asked of the environment it wraps,
    /// so an inner construct's binding shadows an outer one's of the same
    /// kind.
    ///
    /// Holds a reference to the environment it wraps: it lives for one
    /// evaluation, inside the call that made it. In `detail`, so that nothing
    /// outside the library can bind a placeholder to a value of its choosing.
    template <typename Env, typename... Bindings>
    class BoundEnvironment
    {
      public:
        /// Wraps @p wrapped, binding @p bindings on top of it.
        constexpr BoundEnvironment(Env const& wrapped, Bindings... bindings) noexcept:
            _wrapped { wrapped },
            _bindings { bindings... }
        {
        }

        /// Whether the wrapped environment holds @p Q.
        template <Described Q>
        static constexpr bool provides = Env::template provides<Q>;

        /// Whether the wrapped environment's value for @p Q was typed in.
        template <Described Q>
        static constexpr bool is_entered = Env::template is_entered<Q>;

        /// The wrapped environment's value for @p Q.
        template <Described Q>
        [[nodiscard]] constexpr Measured<Q> get() const noexcept
        {
            return _wrapped.template get<Q>();
        }

        /// Whether the wrapped environment's series for @p Q was typed in.
        template <Described Q>
        static constexpr bool is_entered_series = Env::template is_entered_series<Q>;

        /// The wrapped environment's series for @p Q, of length @p N.
        template <Described Q, std::size_t N>
        [[nodiscard]] constexpr MeasuredSeries<Q, N> get_series() const noexcept
        {
            return _wrapped.template get_series<Q, N>();
        }

        /// Where the wrapped environment's value for @p Q came from.
        template <Described Q>
        [[nodiscard]] constexpr ValueSource source_of() const noexcept
        {
            return _wrapped.template source_of<Q>();
        }

        /// Whether a binding of type @p Binding is in scope: here, or in an
        /// environment this one wraps.
        template <typename Binding>
        static constexpr bool binds = (std::is_same_v<Binding, Bindings> || ...) || requires {
            Env::template binds<Binding>;
            requires Env::template binds<Binding>;
        };

        /// The innermost binding of type @p Binding.
        template <typename Binding>
        [[nodiscard]] constexpr Binding bound() const noexcept
        {
            if constexpr ((std::is_same_v<Binding, Bindings> || ...))
                return std::get<Binding>(_bindings);
            else
                return _wrapped.template bound<Binding>();
        }

      private:
        Env const& _wrapped;
        std::tuple<Bindings...> _bindings;
    };

    /// A precision limit's level, as pass 1 produced it, in the coherent SI
    /// unit of @p LevelDimension -- the binding `precision_level` reads.
    ///
    /// The dimension is part of the type so that the placeholder can refuse
    /// to read a level of another dimension even where the declaration-time
    /// check cannot see it (a placeholder inside a consumer's own node kind).
    template <typename Rep, Dimension LevelDimension>
    struct LevelBinding
    {
        /// The level.
        Rep levelValue;
    };

} // namespace detail

// ----------------------------------------------------- precision limits

/// Which precision a limit states. Changes the symbol (`r` or `R`) and the
/// trace's words, never the arithmetic.
enum class PrecisionKind : std::uint8_t
{
    /// The same operator, apparatus and laboratory, in a short interval.
    Repeatability,
    /// Different operators, apparatus or laboratories.
    Reproducibility,
};

namespace detail
{
    /// Fails to compile when a placeholder names a quantity whose unit has an
    /// offset. The trace shows the level in that unit, and a level is as
    /// often a difference -- the spread of two temperatures -- as a point on
    /// the scale: a spread of 1 K would be shown as -272.15 degC.
    template <Described Q>
    struct RequireLevelUnitWithoutOffset
    {
        static_assert(Describe<Q>::unit.offsetNumerator == 0,
                      "formula: this precision_level names a quantity whose unit has an offset, such as degrees "
                      "Celsius; a level that is a difference, such as the spread of two temperatures, would be "
                      "shown as a point on that scale -- name a quantity in the offset-free unit of the same "
                      "dimension, such as kelvin; the quantity appears in this diagnostic as the template "
                      "argument of RequireLevelUnitWithoutOffset");

        static constexpr bool value = true;
    };
} // namespace detail

/// The level a precision limit is evaluated at: pass 1's result, read inside
/// the limit expression. `Q` names the quantity the level is a value of --
/// its dimension and the unit the trace shows it in, which may have no
/// offset (`RequireLevelUnitWithoutOffset`).
template <Described Q>
struct PrecisionLevelNode: NodeBase
{
    static_assert(detail::RequireLevelUnitWithoutOffset<Q>::value);

    /// The quantity the level is a value of.
    using quantity = Q;
    /// The level measures what `Q` measures.
    static constexpr Dimension dimension = Describe<Q>::dimension;
};

/// The level of the enclosing precision limit: `precision_level<ResultA>`.
template <Described Q>
inline constexpr PrecisionLevelNode<Q> precision_level {};

namespace detail
{
    /// The child expressions of @p N a placeholder could hide in, as a
    /// `std::tuple` of types -- **one specialisation per node kind this
    /// library ships, leaves included**, each saying `seen = true`. The kinds
    /// declared in headers this one cannot include specialise it there:
    /// `DerivedQuantityNode`, `OverriddenConstantNode` and
    /// `ReplacedVariantNode` in `overlay.hpp`.
    ///
    /// The primary is a consumer's node kind, which cannot be seen inside: it
    /// answers no children and `seen = false`. A library kind that fell to it
    /// would hide a placeholder from every check below without a word, as
    /// `DerivedQuantityNode` once did; `level_check_sees_every_node` finds
    /// such a kind, and the vocabulary's every-kind method is put through it
    /// (`vocabulary_tests.cpp`), so a kind added there without a
    /// specialisation here fails a test rather than a reader.
    ///
    /// For a `PrecisionLimitNode` only its level is listed in `type`: a
    /// placeholder in its limit expression is bound by that limit, and so not
    /// free. Its limit expression is listed in `bound`, which only
    /// `level_check_sees_every_node` walks.
    template <typename N>
    struct LevelChildren
    {
        /// A consumer's node kind: nothing inside it can be seen.
        static constexpr bool seen = false;
        using type = std::tuple<>;
    };

    /// A node kind with no children, which the checks below see whole.
    struct LevelLeaf
    {
        static constexpr bool seen = true;
        using type = std::tuple<>;
    };

    /// A node kind whose children @p Children the checks below look into.
    template <typename... Children>
    struct LevelParent
    {
        static constexpr bool seen = true;
        using type = std::tuple<Children...>;
    };

    template <Described Q>
    struct LevelChildren<VarNode<Q>>: LevelLeaf
    {
    };

    template <Unit U>
    struct LevelChildren<ConstantNode<U>>: LevelLeaf
    {
    };

    template <>
    struct LevelChildren<PiNode>: LevelLeaf
    {
    };

    template <KeyTable Keys, Unit ResultUnit>
    struct LevelChildren<ExactLookupNode<Keys, ResultUnit>>: LevelLeaf
    {
    };

    template <UnaryOperator Op, Node Operand>
    struct LevelChildren<UnaryNode<Op, Operand>>: LevelParent<Operand>
    {
    };

    template <BinaryOperator Op, Node Left, Node Right>
    struct LevelChildren<BinaryNode<Op, Left, Right>>: LevelParent<Left, Right>
    {
    };

    template <int Exponent, Node Operand>
    struct LevelChildren<PowerNode<Exponent, Operand>>: LevelParent<Operand>
    {
    };

    template <int Degree, Node Operand>
    struct LevelChildren<RootNode<Degree, Operand>>: LevelParent<Operand>
    {
    };

    template <Node Inner>
    struct LevelChildren<DocumentedNode<Inner>>: LevelParent<Inner>
    {
    };

    template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct LevelChildren<RoundNode<U, Places, Mode, Operand>>: LevelParent<Operand>
    {
    };

    template <Unit U, SignificantDigits Digits, RoundingMode Mode, Node Operand>
    struct LevelChildren<RoundSignificantNode<U, Digits, Mode, Operand>>: LevelParent<Operand>
    {
    };

    template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Radicand>
    struct LevelChildren<RoundedRootNode<U, Places, Mode, Radicand>>: LevelParent<Radicand>
    {
    };

    template <Unit U, FixedString Justification, Node Operand>
    struct LevelChildren<NumericValueNode<U, Justification, Operand>>: LevelParent<Operand>
    {
    };

    template <Comparison Op, Node Left, Node Right>
    struct LevelChildren<PredicateNode<Op, Left, Right>>: LevelParent<Left, Right>
    {
    };

    template <Predicate P, Node Then, Node Else>
    struct LevelChildren<WhenNode<P, Then, Else>>: LevelParent<P, Then, Else>
    {
    };

    template <Unit KeyUnit, BandTable Bands, Unit ResultUnit, Node Operand>
    struct LevelChildren<BandedLookupNode<KeyUnit, Bands, ResultUnit, Operand>>: LevelParent<Operand>
    {
    };

    template <Unit KeyUnit, BreakpointTable Points, Unit ResultUnit, Node Operand>
    struct LevelChildren<InterpolatingLookupNode<KeyUnit, Points, ResultUnit, Operand>>: LevelParent<Operand>
    {
    };

    template <SampleSizeTable Sizes, Unit ResultUnit, Node Count>
    struct LevelChildren<SampleSizeLookupNode<Sizes, ResultUnit, Count>>: LevelParent<Count>
    {
    };

    template <Node Operand>
    struct LevelChildren<AbsoluteValueNode<Operand>>: LevelParent<Operand>
    {
    };

    /// Required, not a refinement: a partial specialisation never matches a
    /// derived class, so the `RoundNode` entry does not reach this one.
    template <Unit U, DecimalPlaces Places, RoundingMode Mode, Node Operand>
    struct LevelChildren<RoundingRuleNode<U, Places, Mode, Operand>>: LevelParent<Operand>
    {
    };

    template <Described Q>
    struct LevelChildren<PrecisionLevelNode<Q>>: LevelLeaf
    {
    };

    // Phase 12's series kinds. A series is not a `Node`, but a placeholder
    // broadcast into one -- `series<M, 3> + precision_level<A>` under a
    // `sum` -- is inside the level all the same.
    template <Described Q, std::size_t N>
    struct LevelChildren<SeriesVarNode<Q, N>>: LevelLeaf
    {
    };

    template <Unit U, std::size_t N>
    struct LevelChildren<SeriesConstantNode<U, N>>: LevelLeaf
    {
    };

    template <UnaryOperator Op, SeriesNode Operand>
    struct LevelChildren<ElementwiseUnaryNode<Op, Operand>>: LevelParent<Operand>
    {
    };

    template <BinaryOperator Op, typename Left, typename Right>
    struct LevelChildren<ElementwiseBinaryNode<Op, Left, Right>>: LevelParent<Left, Right>
    {
    };

    template <Unit U, auto Places, RoundingMode Mode, SeriesNode S>
    struct LevelChildren<ElementwiseRoundNode<U, Places, Mode, S>>: LevelParent<S>
    {
    };

    template <CumulativeDirection D, SeriesNode S>
    struct LevelChildren<CumulativeNode<D, S>>: LevelParent<S>
    {
    };

    template <SeriesNode S>
    struct LevelChildren<SumNode<S>>: LevelParent<S>
    {
    };

    // Phase 14's snap and curves: a snap reads its operand, a curve its two
    // series, a splice its two curves, an interpolation its curve and the
    // point it is read at. A declared domain is a table of points.
    template <Unit KeyUnit, BreakpointTable Permitted, SnapTie Tie, Node Operand>
    struct LevelChildren<SnapNode<KeyUnit, Permitted, Tie, Operand>>: LevelParent<Operand>
    {
    };

    template <Unit U, BreakpointTable Points>
    struct LevelChildren<DomainNode<U, Points>>: LevelLeaf
    {
    };

    template <SeriesNode D, SeriesNode V>
    struct LevelChildren<CurveNode<D, V>>: LevelParent<D, V>
    {
    };

    template <Monotone M, CurveExpression A, CurveExpression B>
    struct LevelChildren<SpliceNode<M, A, B>>: LevelParent<A, B>
    {
    };

    template <CurveExpression C, Node At>
    struct LevelChildren<InterpolateAlongNode<C, At>>: LevelParent<C, At>
    {
    };

    // Phase 12's raw observations, and the classes they are binned into.
    template <Described Q, std::size_t Capacity>
    struct LevelChildren<ObservationsVarNode<Q, Capacity>>: LevelLeaf
    {
    };

    template <Unit KeyUnit, BandTable Classes, ObservationsNode Obs>
    struct LevelChildren<BinnedNode<KeyUnit, Classes, Obs>>: LevelParent<Obs>
    {
    };

    template <SampleSource S>
    struct LevelChildren<SampleCountNode<S>>: LevelParent<S>
    {
    };

    template <SampleSource S>
    struct LevelChildren<SampleMeanNode<S>>: LevelParent<S>
    {
    };

    /// Whether @p N is a `PrecisionLevelNode`.
    template <typename N>
    inline constexpr bool is_precision_level = false;

    template <Described Q>
    inline constexpr bool is_precision_level<PrecisionLevelNode<Q>> = true;

    template <typename N>
    [[nodiscard]] consteval bool mentions_free_level() noexcept;

    template <typename... Children>
    [[nodiscard]] consteval bool any_mentions_free_level(std::tuple<Children...> const*) noexcept
    {
        return (mentions_free_level<Children>() || ...);
    }

    /// Whether a `precision_level` not bound inside @p N appears anywhere in it.
    template <typename N>
    [[nodiscard]] consteval bool mentions_free_level() noexcept
    {
        using Bare = std::remove_cv_t<N>;
        if constexpr (is_precision_level<Bare>)
            return true;
        else
            return any_mentions_free_level(static_cast<typename LevelChildren<Bare>::type const*>(nullptr));
    }

    template <typename N, Dimension LevelDimension>
    [[nodiscard]] consteval bool free_levels_measure() noexcept;

    template <Dimension LevelDimension, typename... Children>
    [[nodiscard]] consteval bool all_free_levels_measure(std::tuple<Children...> const*) noexcept
    {
        return (free_levels_measure<Children, LevelDimension>() && ...);
    }

    /// Whether every `precision_level` free in @p N measures @p LevelDimension.
    template <typename N, Dimension LevelDimension>
    [[nodiscard]] consteval bool free_levels_measure() noexcept
    {
        using Bare = std::remove_cv_t<N>;
        if constexpr (is_precision_level<Bare>)
            return Bare::dimension == LevelDimension;
        else
            return all_free_levels_measure<LevelDimension>(static_cast<typename LevelChildren<Bare>::type const*>(nullptr));
    }

    template <typename N>
    [[nodiscard]] consteval std::optional<Unit> first_free_level_unit() noexcept;

    template <typename... Children>
    [[nodiscard]] consteval std::optional<Unit> first_free_level_unit_of(std::tuple<Children...> const*) noexcept
    {
        std::optional<Unit> found;
        ((found.has_value() ? void() : void(found = first_free_level_unit<Children>())), ...);
        return found;
    }

    /// The declared unit of the first `precision_level` free in @p N, if any:
    /// the unit the trace shows a limit's level in.
    template <typename N>
    [[nodiscard]] consteval std::optional<Unit> first_free_level_unit() noexcept
    {
        using Bare = std::remove_cv_t<N>;
        if constexpr (is_precision_level<Bare>)
            return Describe<typename Bare::quantity>::unit;
        else
            return first_free_level_unit_of(static_cast<typename LevelChildren<Bare>::type const*>(nullptr));
    }

    /// Fails to compile when a level expression reads `precision_level`: the
    /// level cannot depend on a level -- its own, which does not exist until
    /// the level is evaluated, or an enclosing limit's, which would make one
    /// level a function of another no reader could follow.
    template <typename Level>
    struct RequireLevelWithoutPlaceholder
    {
        static_assert(!mentions_free_level<Level>(),
                      "formula: this precision_limit's level expression reads precision_level; the level cannot "
                      "depend on a level -- write the level from the results themselves; the level expression "
                      "appears in this diagnostic as the template argument of RequireLevelWithoutPlaceholder");

        static constexpr bool value = true;
    };

    /// Fails to compile when a `precision_level<Q>` in a limit expression
    /// names a quantity that does not measure the level's dimension -- the
    /// limit would read a mass as though it were a length.
    template <typename Level, typename Limit>
    struct RequireLevelDimensionMatches
    {
        static_assert(free_levels_measure<Limit, Level::dimension>(),
                      "formula: this precision_limit's limit expression reads a precision_level whose quantity "
                      "does not measure the dimension of the level expression; name a quantity of the level's "
                      "dimension; the level and the limit appear in this diagnostic as the template arguments of "
                      "RequireLevelDimensionMatches");

        static constexpr bool value = true;
    };

    /// Whether the declaration-time checks of a precision limit hold: the
    /// gate that keeps a refused limit from being evaluated as well, so that
    /// the placeholder's own checks never add a second message.
    template <typename Level, typename Limit>
    inline constexpr bool precision_limit_is_valid =
        !mentions_free_level<Level>() && free_levels_measure<Limit, Level::dimension>();
} // namespace detail

/// A precision limit: `limit`, evaluated at the level `level` produces -- see
/// the file comment for the two passes.
///
/// Both checks sit in the class body, so that a node declared without the
/// factory -- it is a public aggregate -- is refused as well.
template <PrecisionKind K, Node Level, Node Limit>
struct PrecisionLimitNode: NodeBase
{
    static_assert(detail::RequireLevelWithoutPlaceholder<Level>::value);
    static_assert(detail::RequireLevelDimensionMatches<Level, Limit>::value);

    /// Pass 1: the expression whose value is the level.
    ///
    /// Deliberately no `{}` default member initialiser -- see `Corrections`
    /// (`lookup.hpp`).
    Level level;

    /// Pass 2: the limit, an expression over `precision_level`.
    ///
    /// Deliberately no `{}` default member initialiser, for the same reason.
    Limit limit;

    /// Which precision this limit states.
    static constexpr PrecisionKind kind = K;
    /// What the limit measures.
    static constexpr Dimension dimension = Limit::dimension;
};

/// A precision limit evaluated at the level of the results it checks:
/// `precision_limit<PrecisionKind::Repeatability>((var<A> + var<B>) / rat(2),
/// constant<unit::Gram>(rat(1, 10)) + rat(1, 50) * precision_level<A>)`.
template <PrecisionKind K, Node Level, Node Limit>
[[nodiscard]] constexpr auto precision_limit(Level levelExpression, Limit limitExpression) noexcept
{
    return PrecisionLimitNode<K, Level, Limit> { {}, levelExpression, limitExpression };
}

namespace detail
{
    template <PrecisionKind K, Node Level, Node Limit>
    struct LevelChildren<PrecisionLimitNode<K, Level, Limit>>: LevelParent<Level>
    {
        /// Where a placeholder is bound by this limit, and so never free.
        using bound = std::tuple<Limit>;
    };

    template <typename N>
    [[nodiscard]] consteval bool level_check_sees_every_node() noexcept;

    template <typename... Children>
    [[nodiscard]] consteval bool level_check_sees_all(std::tuple<Children...> const*) noexcept
    {
        return (level_check_sees_every_node<Children>() && ...);
    }

    /// Whether the checks above see every node in @p N -- whether each node
    /// kind in it, bound limit expressions included, has its own
    /// `LevelChildren` specialisation. False at the first node that fell to
    /// the primary. Asked of a tree of library kinds only, by a test: a
    /// consumer's node kind is unseen by design.
    template <typename N>
    [[nodiscard]] consteval bool level_check_sees_every_node() noexcept
    {
        using Children = LevelChildren<std::remove_cv_t<N>>;
        if constexpr (!Children::seen)
            return false;
        else if constexpr (requires { typename Children::bound; })
            return level_check_sees_all(static_cast<typename Children::type const*>(nullptr))
                   && level_check_sees_all(static_cast<typename Children::bound const*>(nullptr));
        else
            return level_check_sees_all(static_cast<typename Children::type const*>(nullptr));
    }

    /// The unit the level expression @p Level is itself stated in, when it
    /// names a quantity -- a plain `var<Q>`, or what an overlay made of one:
    /// its value is `Q`'s, so `Q`'s unit shows it as the reader wrote it.
    /// Nothing for any other expression.
    template <typename Level>
    [[nodiscard]] consteval std::optional<Unit> level_expression_unit() noexcept
    {
        if constexpr (requires { typename Level::quantity; })
            return Describe<typename Level::quantity>::unit;
        else
            return std::nullopt;
    }
} // namespace detail

namespace detail
{
    /// Fails to compile when `precision_level` is evaluated where no
    /// precision limit binds it -- outside any limit expression.
    template <typename Env>
    struct RequireLevelBound
    {
        static constexpr bool bound = false;
        static_assert(bound,
                      "formula: precision_level is meaningful only inside the limit expression of "
                      "precision_limit; the environment it was evaluated in appears in this diagnostic as the "
                      "template argument of RequireLevelBound");

        static constexpr bool value = true;
    };

    /// Fails to compile when a placeholder reads a level of another
    /// dimension. Reached only where the declaration-time check could not
    /// look: a placeholder inside a consumer's own node kind.
    template <Dimension Wanted, Dimension LevelDimension>
    struct RequireBoundLevelDimension
    {
        static_assert(Wanted == LevelDimension,
                      "formula: this precision_level reads a level of another dimension; name a quantity of the "
                      "level's dimension");

        static constexpr bool value = true;
    };

    /// The first level binding among @p Bindings, and its dimension.
    template <typename Rep, typename... Bindings>
    struct FirstLevelIn
    {
        static constexpr bool found = false;
        static constexpr Dimension dimension {};
    };

    template <typename Rep, Dimension D, typename... Rest>
    struct FirstLevelIn<Rep, LevelBinding<Rep, D>, Rest...>
    {
        static constexpr bool found = true;
        static constexpr Dimension dimension = D;
    };

    template <typename Rep, typename Binding, typename... Rest>
    struct FirstLevelIn<Rep, Binding, Rest...>: FirstLevelIn<Rep, Rest...>
    {
    };

    /// Whether @p Env binds a level, and the innermost one's dimension --
    /// searched from the inside out, so the innermost limit is the one a
    /// placeholder reads. A plain `Environment` binds none.
    template <typename Rep, typename Env>
    struct InnermostLevel
    {
        static constexpr bool found = false;
        static constexpr Dimension dimension {};
    };

    template <typename Rep, typename Env, typename... Bindings>
    struct InnermostLevel<Rep, BoundEnvironment<Env, Bindings...>>:
        std::conditional_t<FirstLevelIn<Rep, Bindings...>::found, FirstLevelIn<Rep, Bindings...>, InnermostLevel<Rep, Env>>
    {
    };
} // namespace detail

/// Reads the level of the innermost enclosing precision limit. Refused, in
/// one message, anywhere no limit binds one.
template <typename Rep = Rational, Described Q, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(PrecisionLevelNode<Q> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    using Innermost = detail::InnermostLevel<Rep, Env>;
    if constexpr (!Innermost::found)
    {
        static_assert(detail::RequireLevelBound<Env>::value);
        return detail::nothing<Rep>();
    }
    else if constexpr (!(Innermost::dimension == Describe<Q>::dimension))
    {
        static_assert(detail::RequireBoundLevelDimension<Describe<Q>::dimension, Innermost::dimension>::value);
        return detail::nothing<Rep>();
    }
    else
    {
        sink.entered(node);
        Evaluated<Rep> const evaluated =
            detail::present<Rep>(environment.template bound<detail::LevelBinding<Rep, Describe<Q>::dimension>>().levelValue);
        sink.produced(node, evaluated);
        return evaluated;
    }
}

/// Evaluates `level` (pass 1), then `limit` at that level (pass 2).
///
/// Pass 1 failing or coming out absent ends the evaluation there: a limit at
/// no level is no limit. A sink that defines `precision_limit_entered` and
/// `precision_limit_produced` hears them around the whole, and one that
/// defines `precision_level_entered` and `precision_level_produced` hears them
/// around pass 1 -- each pair asked for in one `requires`, as
/// `variant_entered` is, so a sink defining half a pair is told nothing.
template <typename Rep = Rational, PrecisionKind K, Node Level, Node Limit, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(PrecisionLimitNode<K, Level, Limit> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    constexpr bool hearsLimit = requires(Evaluated<Rep> const& limitResult) {
        sink.precision_limit_entered(K);
        sink.precision_limit_produced(K, limitResult);
    };
    constexpr bool hearsLevel = requires(Evaluated<Rep> const& levelResult, Unit levelUnit) {
        sink.precision_level_entered(K);
        sink.precision_level_produced(K, levelUnit, levelResult);
    };
    // The unit the level is shown in: the quantity the limit's first
    // placeholder names; else, when the limit reads its level only through
    // a nested limit or a consumer's node, the quantity the level expression
    // names; else the coherent unit, as every computed step is shown in.
    [[maybe_unused]] constexpr Unit levelUnit = detail::first_free_level_unit<Limit>().value_or(
        detail::level_expression_unit<Level>().value_or(coherent(Level::dimension)));

    if constexpr (hearsLimit)
        sink.precision_limit_entered(K);
    sink.entered(node);

    Evaluated<Rep> const evaluated = [&]() -> Evaluated<Rep> {
        if constexpr (!detail::precision_limit_is_valid<Level, Limit>)
        {
            // Already refused where the node was declared; evaluating it too
            // would only add the placeholder's messages to that one.
            return std::unexpected { ArithmeticError::DomainError };
        }
        else
        {
            if constexpr (hearsLevel)
                sink.precision_level_entered(K);
            Evaluated<Rep> const levelResult = detail::dispatch<Rep>(node.level, environment, sink);
            if constexpr (hearsLevel)
                sink.precision_level_produced(K, levelUnit, levelResult);
            if (!levelResult.has_value())
                return std::unexpected { levelResult.error() };
            if (!levelResult->has_value())
                return detail::nothing<Rep>();

            using Binding = detail::LevelBinding<Rep, Level::dimension>;
            detail::BoundEnvironment<Env, Binding> const atLevel { environment, Binding { **levelResult } };
            return detail::dispatch<Rep>(node.limit, atLevel, sink);
        }
    }();

    sink.produced(node, evaluated);
    if constexpr (hearsLimit)
        sink.precision_limit_produced(K, evaluated);
    return evaluated;
}

} // namespace formula
