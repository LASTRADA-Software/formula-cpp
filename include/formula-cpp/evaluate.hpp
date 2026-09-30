// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Turning a formula and a set of inputs into an outcome.
///
/// Every leaf is converted to the **coherent unit** of its dimension -- the SI
/// unit, times one of each named base dimension it has -- on the way in, the
/// tree is evaluated there, and the result is converted once at the end into
/// the declared unit of the quantity it was asked to produce. Both
/// conversions are the exact multiply-then-divide of the unit layer, so 180 l
/// plus 300 l is exactly 480 l and not 479.999999.
///
/// Two entry points, deliberately different:
///
///  - `checked_evaluate_si<Rep>` is the representation-agnostic core. It answers
///    in the coherent unit and in whatever `Rep` the caller asked for --
///    exact `Rational` by default, `double` when a formula needs values exact
///    rationals cannot hold and no audit trail is wanted. A formula that states
///    the precision such a value is reported at -- `rounded_sqrt`, or
///    `rounded_output` of an opaque operation that computes in wider integers,
///    as `linear_least_squares` does -- is evaluated exactly instead, by
///    `checked_evaluate` and by `checked_evaluate_si` in `Rational`; in
///    `double` it does not compile, as no rounding node does.
///  - `checked_evaluate<Result>` is the auditable one. It is always exact,
///    because a result that goes into an audit trail as binary floating point
///    would have to explain itself, and it returns an `Outcome<Result>` in
///    `Result`'s own unit.

#include <formula-cpp/environment.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

#include <concepts>
#include <expected>
#include <optional>

namespace formula
{

/// The coherent unit of a dimension: magnitude one, offset zero, no symbol --
/// the SI unit, times one of each named base dimension it has: the unit named
/// after a base, which by convention has magnitude one.
///
/// Every `Unit` already states its own exact conversion to this one, so it is
/// the single scale on which values from different units can meet.
[[nodiscard]] constexpr Unit coherent(Dimension dimensionOfUnit) noexcept
{
    return Unit { .dimension = dimensionOfUnit };
}

/// How arithmetic is done for one representation.
///
/// The primary template is deliberately undefined: a representation that has
/// not been taught to the library fails at the point of use, naming itself,
/// rather than silently selecting something plausible.
///
/// This is a **public extension point**, which is why it lives here rather
/// than in `detail`. A consumer who wants the evaluator to work in a
/// representation the library does not ship -- an arbitrary-precision
/// rational, a fixed-point type, an interval -- specialises this and the
/// evaluator uses it with no further change. The specialisations below for
/// `Rational` and `double` are the two the library ships, not the two it
/// permits.
template <typename Rep>
struct RepTraits;

/// Exact rational arithmetic -- the default, and the only one an `Outcome` is
/// built from.
template <>
struct RepTraits<Rational>
{
    /// A value already in `Rational`, unchanged -- present so `detail::in_si`
    /// can call `RepTraits<Rep>::from` uniformly for every representation.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> from(Rational exact) noexcept
    {
        return exact;
    }
    /// Exact addition; an overflowing sum is reported, never wrapped.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> add(Rational leftOperand,
                                                                                Rational rightOperand) noexcept
    {
        return checked_add(leftOperand, rightOperand);
    }
    /// Exact subtraction; an overflowing difference is reported, never wrapped.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> subtract(Rational leftOperand,
                                                                                     Rational rightOperand) noexcept
    {
        return checked_sub(leftOperand, rightOperand);
    }
    /// Exact multiplication; an overflowing product is reported, never wrapped.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> multiply(Rational leftOperand,
                                                                                     Rational rightOperand) noexcept
    {
        return checked_mul(leftOperand, rightOperand);
    }
    /// Exact division; division by zero is reported, never a trap or an infinity.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> divide(Rational leftOperand,
                                                                                   Rational rightOperand) noexcept
    {
        return checked_div(leftOperand, rightOperand);
    }
    /// Exact negation.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> negate(Rational operandValue) noexcept
    {
        return checked_negate(operandValue);
    }
};

/// Binary floating point, for formulas whose values exact rationals cannot
/// hold. This arithmetic itself never reports `Overflow`: a `double` says
/// `inf` and the caller asked for `double`. That is not the last word on
/// `Overflow` for this representation, though -- `detail::in_si` converts
/// every leaf in exact `Rational` before handing it to `RepTraits<Rep>::from`,
/// so a leaf whose conversion to the coherent unit overflows still fails
/// the whole evaluation, `double` included.
template <>
struct RepTraits<double>
{
    /// Converts an exact `Rational` (already in the coherent unit) to `double`.
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> from(Rational exact) noexcept
    {
        return exact.to_double();
    }
    /// Ordinary floating-point addition.
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> add(double leftOperand,
                                                                              double rightOperand) noexcept
    {
        return leftOperand + rightOperand;
    }
    /// Ordinary floating-point subtraction.
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> subtract(double leftOperand,
                                                                                   double rightOperand) noexcept
    {
        return leftOperand - rightOperand;
    }
    /// Ordinary floating-point multiplication.
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> multiply(double leftOperand,
                                                                                   double rightOperand) noexcept
    {
        return leftOperand * rightOperand;
    }
    /// Floating-point division, except that division by zero is reported as
    /// `ArithmeticError::DivisionByZero` rather than becoming `inf` or `nan`.
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> divide(double leftOperand,
                                                                                 double rightOperand) noexcept
    {
        if (rightOperand == 0.0)
            return std::unexpected { ArithmeticError::DivisionByZero };
        return leftOperand / rightOperand;
    }
    /// Ordinary floating-point negation.
    [[nodiscard]] static constexpr std::expected<double, ArithmeticError> negate(double operandValue) noexcept
    {
        return -operandValue;
    }
};

/// The result of evaluating a subtree: a number, nothing (an input was never
/// measured), or an arithmetic error. The two layers are different questions --
/// "there is no value" is a fact about the specimen, "the arithmetic failed" is
/// a fact about the library -- and collapsing them would lose one of them.
template <typename Rep>
using Evaluated = std::expected<std::optional<Rep>, ArithmeticError>;

namespace detail
{
    /// Fails to compile when the result quantity does not measure what the
    /// expression computes.
    template <typename Result, typename Expression>
    struct RequireResultDimension
    {
        static_assert(refused_already<Expression>() || Describe<Result>::dimension == Expression::dimension,
                      "formula: this result quantity does not measure the dimension this expression "
                      "computes; the quantity and the expression appear in this diagnostic as the "
                      "template arguments of RequireResultDimension");

        static constexpr bool value = true;
    };

    /// Fails to compile when a series (`series.hpp`) is handed to something
    /// that answers with one value -- `checked_evaluate`, `evaluate` or
    /// `variant<Tag>` (`method.hpp`). A series is not a `Node`, so without
    /// this each would refuse it in the compiler's words, as a constraint
    /// nobody satisfied. Named so the expression prints.
    template <typename Expression>
    struct RequireSingleValueExpression
    {
        // A series already refused (`refused`, `series.hpp`) is not asked
        // again: its own refusal is the one message for the mistake.
        static_assert(
            !SeriesNode<Expression> || requires { requires detail::refused_already<Expression>(); },
            "formula: this expression is a series, not a single value; evaluate it with "
            "checked_evaluate_series, or reduce it to one value first (sum, interpolate_at)");

        static constexpr bool value = true;
    };

    /// Wraps a bare `Rep` as a present value.
    template <typename Rep>
    [[nodiscard]] constexpr Evaluated<Rep> present(Rep value) noexcept
    {
        return Evaluated<Rep> { std::optional<Rep> { value } };
    }

    /// The absent result, which every operator propagates.
    template <typename Rep>
    [[nodiscard]] constexpr Evaluated<Rep> nothing() noexcept
    {
        return Evaluated<Rep> { std::optional<Rep> {} };
    }

    /// Tells @p sink that @p node failed with @p arithmeticFailure, and
    /// returns that failure -- how a node relays an operand's failure, and
    /// how a lookup reports its miss, its key conversion failure or its
    /// interpolation failure. A node's own arithmetic result, failed or not,
    /// is still recorded and returned as the named value it computed.
    ///
    /// The result is built twice rather than held in a named local: GCC 13
    /// reports a false -Wmaybe-uninitialized, at -O2 and above, on returning a
    /// named `std::expected` that has been passed to the sink by reference,
    /// and which nodes it reports depends on what a consumer instantiates.
    /// The error is a plain enumerator, so constructing it twice costs
    /// nothing.
    template <typename Rep, typename N, typename Sink>
    [[nodiscard]] constexpr Evaluated<Rep> report_failure(N const& node,
                                                          Sink& sink,
                                                          ArithmeticError arithmeticFailure) noexcept
    {
        sink.produced(node, Evaluated<Rep> { std::unexpected { arithmeticFailure } });
        return Evaluated<Rep> { std::unexpected { arithmeticFailure } };
    }

    /// A `Rational` stated in @p from, converted to the coherent unit and
    /// then into @p Rep.
    template <typename Rep>
    [[nodiscard]] constexpr Evaluated<Rep> in_si(Rational value, Unit from) noexcept
    {
        std::expected<Rational, ArithmeticError> const inCoherentUnit =
            checked_convert(value, from, coherent(from.dimension));
        if (!inCoherentUnit.has_value())
            return std::unexpected { inCoherentUnit.error() };

        std::expected<Rep, ArithmeticError> const represented = RepTraits<Rep>::from(*inCoherentUnit);
        if (!represented.has_value())
            return std::unexpected { represented.error() };
        return present<Rep>(*represented);
    }
} // namespace detail

namespace detail
{
    /// Whether @p Env says at run time where its value for @p Q came from,
    /// through a member `source_of<Q>()` returning exactly `ValueSource`.
    /// `Environment` has one, and so do the library's own wrappers of an
    /// environment whenever what they wrap has one. A member of that name
    /// returning anything else -- a reference, or a type that merely converts
    /// to `ValueSource` -- is not this hook, as `ReportsReadFailure` says of
    /// its own: such an environment is asked `is_entered<Q>` instead, as one
    /// with no `source_of` is.
    template <typename Env, typename Q>
    concept RunTimeSource = requires(Env const& asked) {
        { asked.template source_of<Q>() } -> std::same_as<ValueSource>;
    };

    /// Whether @p Env can say where its value for @p Q came from at all: at
    /// run time (`RunTimeSource`), or statically, through `Env::is_entered<Q>`.
    template <typename Env, typename Q>
    concept KnowsSource = RunTimeSource<Env, Q> || requires { Env::template is_entered<Q>; };

    /// Where @p environment says its value for `Q` came from: its
    /// `source_of<Q>()` when it has one, and otherwise `ManuallyEntered` or
    /// `Measured` as `Env::is_entered<Q>` says.
    ///
    /// The run-time answer is preferred because it can say more than the
    /// static one: an environment whose values change after it is built can
    /// say `Derived` of a value it calculated, or change whether a value was
    /// typed in. For every environment this library builds -- `Environment`,
    /// a record context, and the environments a precision limit, a rejection
    /// and a retry evaluate in -- the two answers agree for every quantity it
    /// holds.
    template <Described Q, typename Env>
        requires KnowsSource<Env, Q>
    [[nodiscard]] constexpr ValueSource known_source(Env const& environment) noexcept
    {
        if constexpr (RunTimeSource<Env, Q>)
            return environment.template source_of<Q>();
        else
            return Env::template is_entered<Q> ? ValueSource::ManuallyEntered : ValueSource::Measured;
    }

    /// Whether @p Env can report that reading @p Q failed, through a member
    /// `checked_get<Q>()` whose return type is exactly
    /// `std::expected<Measured<Q>, ArithmeticError>`. A member of that name
    /// returning anything else -- another error type, a reference, a plain
    /// `Measured<Q>` -- is not this hook, and the variable evaluator reads
    /// such an environment through `get<Q>()` as it reads any other.
    template <typename Env, typename Q>
    concept ReportsReadFailure = requires(Env const& asked) {
        { asked.template checked_get<Q>() } -> std::same_as<std::expected<Measured<Q>, ArithmeticError>>;
    };

    /// Tells @p sink where the value @p environment holds for `Q` came from,
    /// through the optional hook `sink.input_source(node, source)` -- only
    /// when the sink defines it and the environment can say
    /// (`known_source`). A consumer's own environment type with neither
    /// `source_of` nor `is_entered` still evaluates, and its trace then
    /// records no source rather than a guessed one.
    template <Described Q, typename Env, typename Sink>
    constexpr void report_input_source(VarNode<Q> const& node, Env const& environment, Sink& sink) noexcept
    {
        if constexpr (requires { sink.input_source(node, ValueSource::Measured); } && KnowsSource<Env, Q>)
            sink.input_source(node, known_source<Q>(environment));
    }

    /// What a variable produces once the value it read is in hand: nothing
    /// when @p measured is absent, and otherwise that value converted to the
    /// coherent unit. Either way the sink is told where the value came
    /// from (`report_input_source`) just before `produced`.
    template <typename Rep, Described Q, typename Env, typename Sink>
    [[nodiscard]] constexpr Evaluated<Rep> produce_read(VarNode<Q> const& node,
                                                        Measured<Q> const& measured,
                                                        Env const& environment,
                                                        Sink& sink) noexcept
    {
        if (measured.is_absent())
        {
            Evaluated<Rep> const absent = nothing<Rep>();
            report_input_source<Q>(node, environment, sink);
            sink.produced(node, absent);
            return absent;
        }
        Evaluated<Rep> const evaluated = in_si<Rep>(*measured.stored(), Describe<Q>::unit);
        report_input_source<Q>(node, environment, sink);
        sink.produced(node, evaluated);
        return evaluated;
    }
} // namespace detail

/// Looks `Q` up in `environment` and, if present, converts it to the coherent
/// unit of its dimension.
///
/// Just before `produced`, and whether or not the value is present, a sink
/// that asks is told where the value came from -- see
/// `detail::report_input_source`.
///
/// **An environment that can fail a read** says so through a member
/// `checked_get<Q>()` returning exactly `std::expected<Measured<Q>,
/// ArithmeticError>` (`detail::ReportsReadFailure`), which is then read
/// instead of `get<Q>()`. A failed read is this variable's failure: the sink
/// is told the source and then that the variable failed, and the error
/// travels up the formula as any operand's failure does -- a parent relays
/// it, and an untaken `when()` branch never reads it. `Environment` has no
/// such member, since none of its reads can fail.
template <typename Rep = Rational, Described Q, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(VarNode<Q> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    if constexpr (detail::ReportsReadFailure<Env, Q>)
    {
        std::expected<Measured<Q>, ArithmeticError> const checkedRead = environment.template checked_get<Q>();
        if (!checkedRead.has_value())
        {
            detail::report_input_source<Q>(node, environment, sink);
            return detail::report_failure<Rep>(node, sink, checkedRead.error());
        }
        return detail::produce_read<Rep>(node, *checkedRead, environment, sink);
    }
    else
        return detail::produce_read<Rep>(node, environment.template get<Q>(), environment, sink);
}

/// A literal coefficient is always present; converts it to the coherent unit.
template <typename Rep = Rational, Unit U, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(ConstantNode<U> const& node,
                                                           Env const&,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const evaluated = detail::in_si<Rep>(node.number, U);
    sink.produced(node, evaluated);
    return evaluated;
}

/// Evaluates the operand, then applies `Op` -- absence and arithmetic errors
/// both propagate without applying the operator.
template <typename Rep = Rational, UnaryOperator Op, Node Operand, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(UnaryNode<Op, Operand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const evaluatedOperand = detail::dispatch<Rep>(node.operand, environment, sink);
    if (!evaluatedOperand.has_value())
    {
        return detail::report_failure<Rep>(node, sink, evaluatedOperand.error());
    }
    if (!evaluatedOperand->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    static_assert(Op == UnaryOperator::Negate, "formula: unknown unary operator");
    std::expected<Rep, ArithmeticError> const negated = RepTraits<Rep>::negate(**evaluatedOperand);
    Evaluated<Rep> const evaluated =
        negated.has_value() ? detail::present<Rep>(*negated) : Evaluated<Rep> { std::unexpected { negated.error() } };
    sink.produced(node, evaluated);
    return evaluated;
}

/// Evaluates the left operand, then the right, and only then considers
/// absence -- so an arithmetic error is never hidden behind the other side's
/// being absent. An error on the **left** returns at once: the right side
/// cannot change an answer that is already an error, and evaluating it anyway
/// would only choose which of two errors to report.
template <typename Rep = Rational, BinaryOperator Op, Node Left, Node Right, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(BinaryNode<Op, Left, Right> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const leftResult = detail::dispatch<Rep>(node.lhs, environment, sink);
    if (!leftResult.has_value())
    {
        return detail::report_failure<Rep>(node, sink, leftResult.error());
    }
    Evaluated<Rep> const rightResult = detail::dispatch<Rep>(node.rhs, environment, sink);
    if (!rightResult.has_value())
    {
        return detail::report_failure<Rep>(node, sink, rightResult.error());
    }

    // Absence wins over arithmetic, but only after both sides have been asked:
    // an arithmetic error in the side that *is* present is still an error, and
    // hiding it behind the other side's absence would lose it.
    if (!leftResult->has_value() || !rightResult->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    std::expected<Rep, ArithmeticError> const combined = [&] {
        if constexpr (Op == BinaryOperator::Add)
            return RepTraits<Rep>::add(**leftResult, **rightResult);
        else if constexpr (Op == BinaryOperator::Subtract)
            return RepTraits<Rep>::subtract(**leftResult, **rightResult);
        else if constexpr (Op == BinaryOperator::Multiply)
            return RepTraits<Rep>::multiply(**leftResult, **rightResult);
        else
            return RepTraits<Rep>::divide(**leftResult, **rightResult);
    }();
    Evaluated<Rep> const evaluated =
        combined.has_value() ? detail::present<Rep>(*combined) : Evaluated<Rep> { std::unexpected { combined.error() } };
    sink.produced(node, evaluated);
    return evaluated;
}

/// Evaluates @p expression for quantity @p Result.
///
/// `Result` is never deduced. An expression's dimension does not name a
/// quantity -- a mass over a volume is *a* density, but which one is the
/// author's decision -- and a library that picked one would eventually label a
/// number with someone else's symbol and description.
///
/// When the environment holds an `entered` value for `Result`, that value is
/// returned with `ValueSource::ManuallyEntered` and the expression is not
/// evaluated at all. That is what an override is; a number a person typed in
/// must never be reported as though the library derived it.
template <Described Result, Node Expression, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<Outcome<Result>, ArithmeticError> checked_evaluate(Expression const& expression,
                                                                                         Env const& environment,
                                                                                         Sink sink = {}) noexcept
{
    static_assert(detail::RequireResultDimension<Result, Expression>::value);

    if constexpr (Env::template is_entered<Result>)
    {
        return Outcome<Result>::value(environment.template get<Result>(), ValueSource::ManuallyEntered);
    }
    else
    {
        Evaluated<Rational> const computed = detail::dispatch<Rational>(expression, environment, sink);
        if (!computed.has_value())
            return std::unexpected { computed.error() };
        if (!computed->has_value())
            return Outcome<Result>::empty();

        std::expected<Rational, ArithmeticError> const inDeclaredUnit =
            checked_convert(**computed, coherent(Expression::dimension), Describe<Result>::unit);
        if (!inDeclaredUnit.has_value())
            return std::unexpected { inDeclaredUnit.error() };

        return Outcome<Result>::value(Measured<Result> { *inDeclaredUnit }, ValueSource::Derived);
    }
}

/// A series handed to `checked_evaluate`: fails to compile, in this library's
/// words, pointing at `checked_evaluate_series` (`series.hpp`). The body is the
/// refusal and nothing else; what it returns is never seen.
template <Described Result, SeriesNode Expression, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<Outcome<Result>, ArithmeticError> checked_evaluate(Expression const&,
                                                                                         Env const&,
                                                                                         Sink = {}) noexcept
{
    static_assert(detail::RequireSingleValueExpression<Expression>::value);
    return Outcome<Result>::empty();
}

/// Throwing spelling of `checked_evaluate`, for callers who would only rethrow.
template <Described Result, Node Expression, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Outcome<Result> evaluate(Expression const& expression, Env const& environment, Sink sink = {})
{
    return detail::or_throw(checked_evaluate<Result>(expression, environment, sink));
}

/// A series handed to `evaluate`: refused as `checked_evaluate` refuses it. A
/// series has no throwing spelling at all -- an exception would have to drop
/// the position of the element that failed, which is the most useful fact a
/// series failure carries.
template <Described Result, SeriesNode Expression, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Outcome<Result> evaluate(Expression const&, Env const&, Sink = {}) noexcept
{
    static_assert(detail::RequireSingleValueExpression<Expression>::value);
    return Outcome<Result>::empty();
}

} // namespace formula
