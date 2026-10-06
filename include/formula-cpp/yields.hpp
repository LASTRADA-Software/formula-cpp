// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A formula bound to the quantity it computes, named once where the formula
/// is written: `constexpr auto ratio = yields<Gradient>(var<Rise> / var<Run>);`
/// then `evaluate(ratio, environment)`. The author still names the result --
/// nothing is deduced from the expression, whose dimension does not name a
/// quantity (`evaluate.hpp`) -- but only once. A `Yields` is not a node: a
/// verb takes it as the top of a formula.
///
/// Used as part of another formula, a bound formula stands for the formula it
/// holds: `var<Load> / loadedArea` is `var<Load> / loadedArea.expression`, of
/// that type and value, on either side of every arithmetic operator and
/// comparison, and so is a bound formula handed to any function that builds a
/// formula -- `sqrt`, `rounded`, `documented`, `sum`, a lookup's key, ... --
/// each of which has an overload for one that hands on `.expression`. The
/// outer formula renders, traces and documents as that spelling does: the
/// bound quantity's name is not part of it, and neither is the binding, so
/// `documented(boundFormula, citation)` documents the formula and is not
/// bound itself.
///
/// Every verb that is told a result quantity takes a `Yields` in place of the
/// expression and the quantity: `evaluate` and `checked_evaluate` here,
/// `checked_evaluate_series` (`series.hpp`), `checked_evaluate_rejection`
/// (`rejection.hpp`), `explain`, `checked_explain`, `trace_of`,
/// `explain_series` and `explain_rejection` (`trace.hpp`), and `define`
/// (`calculation.hpp`). Each
/// returns what it returns for `boundFormula.expression` and the quantity the
/// `Yields` names. A result named at the call as well is accepted when it is
/// that quantity, and refused when it is another
/// (`detail::RequireYieldsResult`). `render` (`render.hpp`) and `document`
/// (`document.hpp`) take one too, and write the formula it holds: they name
/// no result.
///
/// **Refused where the `Yields` is written:**
///  - a formula bound already -- a `Yields` around a `Yields`, even for the
///    same quantity (`detail::RequireFormulaNotBound`);
///  - a quantity that does not measure the dimension the expression
///    computes, in `checked_evaluate`'s words
///    (`detail::RequireResultDimension`).
///
/// A verb given a refused `Yields` adds no second message (`Yields::valid`).
///
/// **Refused where it is evaluated:** a bound series, rejection of outliers,
/// retry or whole opaque call handed to a verb that answers with one value --
/// `evaluate`, `checked_evaluate`, `explain`, `checked_explain`, `trace_of` or
/// `define` -- in words naming the verbs that take it
/// (`detail::RequireSingleValueBound`), once. `define` refuses a series in
/// its own words, as `define<Q>` does.

#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/sink.hpp>

#include <expected>
#include <type_traits>

namespace formula
{

template <Described Q, typename E>
struct Yields;

namespace detail
{
    /// The result a verb is asked for when a `Yields` supplies it: the
    /// default of every verb's `Result` for a `Yields`, which no quantity is.
    struct ResultOfYields
    {
    };

    /// Whether @p T is a `Yields`: a formula bound to its result quantity.
    template <typename T>
    inline constexpr bool is_yields = false;

    template <Described Q, typename E>
    inline constexpr bool is_yields<Yields<Q, E>> = true;

    /// Fails to compile when a `Yields` is given a formula bound already. A
    /// verb takes a `Yields` as the top of a formula; around another, every
    /// verb would forward to the inner one's answer, for the inner one's
    /// quantity, where the outer one's was promised.
    template <typename E>
    struct RequireFormulaNotBound
    {
        static_assert(!is_yields<E>,
                      "formula: this formula is bound to its result quantity already; bind the formula it holds "
                      "(.expression), or use it as it is -- the bound formula appears in this diagnostic as the "
                      "template argument of RequireFormulaNotBound");

        /// Always true: the refusal is the `static_assert` above.
        static constexpr bool value = true;
    };

    /// Whether a `Yields` of @p E for @p Q passes its checks: false for a
    /// formula bound already; otherwise whether @p E computes @p Q's
    /// dimension, true for an expression that publishes none, which its verb
    /// checks instead, and for one refused already, whose dimension is a
    /// stand-in.
    template <Described Q, typename E>
    [[nodiscard]] consteval bool yields_measures() noexcept
    {
        if constexpr (is_yields<E>)
            return false;
        else if constexpr (requires { E::dimension; })
            return refused_already<E>() || E::dimension == Describe<Q>::dimension;
        else
            return true;
    }

    /// Whether @p Result is the quantity @p Q a `Yields` names, or none: what
    /// every verb gates on, so that a refused call adds no second message.
    ///
    /// Asked apart from `RequireYieldsResult`, never through its `value`:
    /// once that check has failed, clang-cl 22.1.8 compiles both branches of
    /// a gate that asks the value -- `define`'s, whose two branches return
    /// different types, so asking it there would add an error of its own.
    template <typename Result, typename Q>
    inline constexpr bool names_yields_result = std::is_same_v<Result, ResultOfYields> || std::is_same_v<Result, Q>;

    /// Fails to compile when a `Yields` is evaluated for another quantity than
    /// the one it names.
    template <typename Result, Described Q>
    struct RequireYieldsResult
    {
        static_assert(names_yields_result<Result, Q>,
                      "formula: this formula names its result quantity with yields; evaluate it for that quantity, or "
                      "name none -- the two quantities appear in this diagnostic as the template arguments of "
                      "RequireYieldsResult");

        /// Always true: the refusal is the `static_assert` above.
        static constexpr bool value = true;
    };

    /// Fails to compile when a bound formula that is not an expression of one
    /// value is handed to a verb that answers with one: `evaluate`,
    /// `checked_evaluate`, `explain`, `checked_explain`, `trace_of` or
    /// `define`. A series is refused below; a rejection of outliers, a retry
    /// and a whole opaque call where each is declared (`rejection.hpp`,
    /// `retry.hpp`, `opaque.hpp`), each naming the verbs that take it.
    template <typename E>
    struct RequireSingleValueBound
    {
        static_assert(Node<E>,
                      "formula: this bound formula is not an expression of one value, which is what this verb "
                      "evaluates -- the formula appears in this diagnostic as the template argument of "
                      "RequireSingleValueBound");

        /// Always true: the refusal is the `static_assert` above.
        static constexpr bool value = true;
    };

    /// A bound series: refused as `checked_evaluate` refuses a series
    /// (`RequireSingleValueExpression`, `evaluate.hpp`).
    template <SeriesNode S>
    struct RequireSingleValueBound<S>: RequireSingleValueExpression<S>
    {
    };

    /// Whether a `Yields<Q, E>` is asked for its own quantity (@p Result) and
    /// passes its checks: one that is not has had its one message.
    template <typename Result, typename Q, typename E>
    inline constexpr bool names_valid_bound = names_yields_result<Result, Q> && Yields<Q, E>::valid;

    /// `RequireSingleValueBound<E>`, asked only of a `Yields<Q, E>` for which
    /// `names_valid_bound` holds.
    template <typename Result, typename Q, typename E>
    using SingleValueBoundCheck =
        std::conditional_t<names_valid_bound<Result, Q, E>, RequireSingleValueBound<E>, std::true_type>;

    /// Whether a verb that answers with one value evaluates a `Yields<Q, E>`
    /// asked for @p Result: `names_valid_bound`, and it holds an expression of
    /// one value. What those verbs gate on, so that a refused call adds no
    /// second message.
    template <typename Result, typename Q, typename E>
    inline constexpr bool evaluates_bound_value = names_valid_bound<Result, Q, E> && Node<E>;
} // namespace detail

/// A formula and the quantity it computes -- built by `yields<Q>(expression)`.
///
/// A public aggregate: its checks sit in the class body, so one spelled
/// without `yields` is refused as well. It claims nothing a verb could not
/// be told directly: its `Q` is held to the dimension the expression
/// computes, as `checked_evaluate<Q>` holds the result it is given. It holds
/// no `Yields`: a formula bound already is refused, even for the same
/// quantity.
template <Described Q, typename E>
struct Yields
{
    static_assert(detail::RequireFormulaNotBound<E>::value);

    // `RequireResultDimension` is named only as the type `conditional_t`
    // picks, so an expression that publishes no dimension -- a retry, say --
    // never instantiates it; behind a `||`, its `::value` would instantiate
    // it whatever the left side said. Asked through a `consteval` helper
    // instead, clang-cl 22.1.8 adds two errors to the one message: the helper's
    // call does not evaluate once the check has failed, and neither does
    // the `yields` call.
    static_assert(
        std::conditional_t<requires { E::dimension; }, detail::RequireResultDimension<Q, E>, std::true_type>::value);

    /// The quantity this formula computes.
    using quantity = Q;

    /// Whether the checks above hold, asked without firing them, so that a
    /// verb given a refused `Yields` adds no second message.
    static constexpr bool valid = detail::yields_measures<Q, E>();

    /// The formula. Deliberately no `{}` default member initialiser -- see
    /// `Corrections` (`lookup.hpp`).
    E expression;
};

/// @p formulaExpression, bound to the quantity @p Q it computes.
template <Described Q, typename E>
[[nodiscard]] constexpr Yields<Q, E> yields(E formulaExpression) noexcept
{
    return Yields<Q, E> { formulaExpression };
}

/// `checked_evaluate<Q>(boundFormula.expression, environmentGiven, recordingSink)`,
/// `Q` taken from the `Yields`. `Result` is `Q`'s place for a caller who
/// names it anyway; any other quantity is refused.
template <typename Result = detail::ResultOfYields, Described Q, typename E, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr std::expected<Outcome<Q>, ArithmeticError> checked_evaluate(Yields<Q, E> const& boundFormula,
                                                                                    Env const& environmentGiven,
                                                                                    Sink recordingSink = {}) noexcept
{
    static_assert(detail::RequireYieldsResult<Result, Q>::value);
    static_assert(detail::SingleValueBoundCheck<Result, Q, E>::value);
    if constexpr (!detail::evaluates_bound_value<Result, Q, E>)
        return Outcome<Q>::empty(); // refused already, where the mistake is
    else
        return checked_evaluate<Q>(boundFormula.expression, environmentGiven, recordingSink);
}

/// Throwing spelling of the overload above, for callers who would only rethrow.
template <typename Result = detail::ResultOfYields, Described Q, typename E, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Outcome<Q> evaluate(Yields<Q, E> const& boundFormula,
                                            Env const& environmentGiven,
                                            Sink recordingSink = {})
{
    return detail::or_throw(checked_evaluate<Result>(boundFormula, environmentGiven, recordingSink));
}

namespace detail
{
    /// Whether any of @p Ts is a bound formula: what every function that
    /// builds a formula asks before it takes the formula a bound one holds in
    /// its place.
    template <typename... Ts>
    concept AnyBound = (is_yields<Ts> || ...);

    /// @p operand as part of another formula: the formula it holds, for a
    /// bound formula, and anything else as it is.
    template <typename T>
    [[nodiscard]] constexpr auto const& as_operand(T const& operand) noexcept
    {
        if constexpr (is_yields<T>)
            return operand.expression;
        else
            return operand;
    }

    /// The type `as_operand` gives for @p T: the formula a bound formula
    /// holds, and @p T itself otherwise.
    template <typename T>
    struct OperandOf
    {
        using type = T;
    };

    template <Described Q, typename E>
    struct OperandOf<Yields<Q, E>>
    {
        using type = E;
    };

    /// `OperandOf<T>::type`.
    template <typename T>
    using operand_t = typename OperandOf<T>::type;

    /// Whether @p T, as an operand, is refused by operators of its own that
    /// see through a bound formula: a retry (`retry.hpp`). The operators below
    /// step aside for those, so that a bound retry, and a retry beside a bound
    /// formula, are refused as the formula held would be, and asking whether
    /// one can be compared answers no rather than firing the refusal.
    template <typename T>
    inline constexpr bool refused_by_own_operators = false;

    /// Whether the operators below take @p L and @p R: one is a bound
    /// formula, and neither is, or holds, an operand refused by operators of
    /// its own.
    template <typename L, typename R>
    concept ForwardsBound =
        AnyBound<L, R> && !refused_by_own_operators<operand_t<L>> && !refused_by_own_operators<operand_t<R>>;

    /// What `==` and `!=` over a bound formula require, spelled once so that
    /// the two operators keep equivalent declarations: a comparison of the
    /// formulas held that both operators over formulas take.
    template <typename L, typename R>
    concept ComparesBound = ForwardsBound<L, R> && requires(L const& lhs, R const& rhs) {
        as_operand(lhs) == as_operand(rhs);
        as_operand(lhs) != as_operand(rhs);
    };
} // namespace detail

/// A bound formula in arithmetic, on either side of `+`, `-`, `*` or `/`, or
/// negated: the formula it holds stands in its place, so the result is the
/// one `.expression` gives, of its type, and refused where that one is, in
/// its words. Only an operand that is a `Yields` reaches these, so
/// arithmetic over formulas is untouched.
///
/// Each takes only what the operator over the formulas held takes, so asking
/// whether a bound formula combines with something those do not -- as a
/// concept does -- answers no, as it does for the formula held. The return
/// type is deduced, as the operators over formulas deduce theirs.
template <typename L, typename R>
    requires detail::ForwardsBound<L, R>
             && requires(L const& lhs, R const& rhs) { detail::as_operand(lhs) + detail::as_operand(rhs); }
[[nodiscard]] constexpr auto operator+(L lhs, R rhs) noexcept
{
    return detail::as_operand(lhs) + detail::as_operand(rhs);
}

/// See `operator+` over a bound formula.
template <typename L, typename R>
    requires detail::ForwardsBound<L, R>
             && requires(L const& lhs, R const& rhs) { detail::as_operand(lhs) - detail::as_operand(rhs); }
[[nodiscard]] constexpr auto operator-(L lhs, R rhs) noexcept
{
    return detail::as_operand(lhs) - detail::as_operand(rhs);
}

/// See `operator+` over a bound formula.
template <typename L, typename R>
    requires detail::ForwardsBound<L, R>
             && requires(L const& lhs, R const& rhs) { detail::as_operand(lhs) * detail::as_operand(rhs); }
[[nodiscard]] constexpr auto operator*(L lhs, R rhs) noexcept
{
    return detail::as_operand(lhs) * detail::as_operand(rhs);
}

/// See `operator+` over a bound formula.
template <typename L, typename R>
    requires detail::ForwardsBound<L, R>
             && requires(L const& lhs, R const& rhs) { detail::as_operand(lhs) / detail::as_operand(rhs); }
[[nodiscard]] constexpr auto operator/(L lhs, R rhs) noexcept
{
    return detail::as_operand(lhs) / detail::as_operand(rhs);
}

/// See `operator+` over a bound formula.
template <typename Operand>
    requires detail::ForwardsBound<Operand, Operand> && requires(Operand const& operand) { -detail::as_operand(operand); }
[[nodiscard]] constexpr auto operator-(Operand operand) noexcept
{
    return -detail::as_operand(operand);
}

/// A bound formula compared, on either side of `<`, `<=`, `>`, `>=`, `==`
/// or `!=`: the formula it holds is compared, as arithmetic over it takes
/// that formula -- so an acceptance or a constraint over a bound formula
/// checks what one over `.expression` checks. Only an operand that is a
/// `Yields` reaches these, so comparisons of formulas are untouched; and, as
/// for arithmetic, each takes only what the comparison of the formulas held
/// takes.
template <typename L, typename R>
    requires detail::ForwardsBound<L, R>
             && requires(L const& lhs, R const& rhs) { detail::as_operand(lhs) < detail::as_operand(rhs); }
[[nodiscard]] constexpr auto operator<(L lhs, R rhs) noexcept
{
    return detail::as_operand(lhs) < detail::as_operand(rhs);
}

/// See `operator<` over a bound formula.
template <typename L, typename R>
    requires detail::ForwardsBound<L, R>
             && requires(L const& lhs, R const& rhs) { detail::as_operand(lhs) <= detail::as_operand(rhs); }
[[nodiscard]] constexpr auto operator<=(L lhs, R rhs) noexcept
{
    return detail::as_operand(lhs) <= detail::as_operand(rhs);
}

/// See `operator<` over a bound formula.
template <typename L, typename R>
    requires detail::ForwardsBound<L, R>
             && requires(L const& lhs, R const& rhs) { detail::as_operand(lhs) > detail::as_operand(rhs); }
[[nodiscard]] constexpr auto operator>(L lhs, R rhs) noexcept
{
    return detail::as_operand(lhs) > detail::as_operand(rhs);
}

/// See `operator<` over a bound formula.
template <typename L, typename R>
    requires detail::ForwardsBound<L, R>
             && requires(L const& lhs, R const& rhs) { detail::as_operand(lhs) >= detail::as_operand(rhs); }
[[nodiscard]] constexpr auto operator>=(L lhs, R rhs) noexcept
{
    return detail::as_operand(lhs) >= detail::as_operand(rhs);
}

/// See `operator<` over a bound formula. `==` and `!=` share their
/// constraint, `detail::ComparesBound`, so that the two declarations stay
/// equivalent and `==` is never weighed reversed.
template <typename L, typename R>
    requires detail::ComparesBound<L, R>
[[nodiscard]] constexpr auto operator==(L lhs, R rhs) noexcept
{
    return detail::as_operand(lhs) == detail::as_operand(rhs);
}

/// See `operator==` over a bound formula.
template <typename L, typename R>
    requires detail::ComparesBound<L, R>
[[nodiscard]] constexpr auto operator!=(L lhs, R rhs) noexcept
{
    return detail::as_operand(lhs) != detail::as_operand(rhs);
}

} // namespace formula
