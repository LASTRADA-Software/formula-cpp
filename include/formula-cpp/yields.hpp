// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A formula bound to the quantity it computes, named once where the formula
/// is written: `constexpr auto ratio = yields<WaterCementRatio>(var<WaterVolume> / var<CementVolume>);`
/// then `evaluate(ratio, environment)`. The author still names the result --
/// nothing is deduced from the expression, whose dimension does not name a
/// quantity (`evaluate.hpp`) -- but only once. A `Yields` is not a node: it is
/// the top of a formula. Nest `documented()` inside it, not around it, and
/// reuse the formula inside another through `.expression`.
///
/// Every verb that is told a result quantity takes a `Yields` in place of the
/// expression and the quantity: `evaluate` and `checked_evaluate` here,
/// `checked_evaluate_series` (`series.hpp`), `checked_evaluate_rejection`
/// (`rejection.hpp`), `explain`, `checked_explain`, `explain_series` and
/// `explain_rejection` (`trace.hpp`), and `define` (`calculation.hpp`). Each
/// returns what it returns for `boundFormula.expression` and the quantity the
/// `Yields` names. A result named at the call as well is accepted when it is
/// that quantity, and refused when it is another
/// (`detail::RequireYieldsResult`). `render` (`render.hpp`) and `document`
/// (`document.hpp`) take one too, and write the formula it holds: they name
/// no result.
///
/// **Refused where the `Yields` is written:** a quantity that does not
/// measure the dimension the expression computes, in `checked_evaluate`'s
/// words (`detail::RequireResultDimension`). A verb given a refused `Yields`
/// adds no second message (`Yields::valid`).

#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/quantity.hpp>

#include <expected>
#include <type_traits>

namespace formula
{

namespace detail
{
    /// The result a verb is asked for when a `Yields` supplies it: the
    /// default of every verb's `Result` for a `Yields`, which no quantity is.
    struct ResultOfYields
    {
    };

    /// Whether @p E computes @p Q's dimension; true for an expression that
    /// publishes none, which its verb checks instead, and for one refused
    /// already, whose dimension is a stand-in.
    template <Described Q, typename E>
    [[nodiscard]] consteval bool yields_measures() noexcept
    {
        if constexpr (requires { E::dimension; })
            return refused_already<E>() || E::dimension == Describe<Q>::dimension;
        else
            return true;
    }

    /// Fails to compile when a `Yields` is evaluated for another quantity than
    /// the one it names.
    template <typename Result, Described Q>
    struct RequireYieldsResult
    {
        static_assert(std::is_same_v<Result, ResultOfYields> || std::is_same_v<Result, Q>,
                      "formula: this formula names its result quantity with yields; evaluate it for that quantity, or "
                      "name none -- the two quantities appear in this diagnostic as the template arguments of "
                      "RequireYieldsResult");

        /// Whether the result asked for is the one the `Yields` names, or
        /// none: what every verb gates on, so that a refused call adds no
        /// second message.
        static constexpr bool value = std::is_same_v<Result, ResultOfYields> || std::is_same_v<Result, Q>;
    };
} // namespace detail

/// A formula and the quantity it computes -- built by `yields<Q>(expression)`.
///
/// A public aggregate: its check sits in the class body, so one spelled
/// without `yields` is refused as well. It claims nothing a verb could not
/// be told directly: its `Q` is held to the dimension the expression
/// computes, as `checked_evaluate<Q>` holds the result it is given.
template <Described Q, typename E>
struct Yields
{
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

    /// Whether the check above holds, asked without firing it, so that a verb
    /// given a refused `Yields` adds no second message.
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
    if constexpr (!detail::RequireYieldsResult<Result, Q>::value || !Yields<Q, E>::valid)
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

} // namespace formula
