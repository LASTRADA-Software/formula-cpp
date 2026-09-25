// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// The traced escape hatch: a formula stated over a bare number rather than a
/// quantity.
///
/// Some published rules are fits whose coefficients only work when the input
/// is expressed in one particular unit -- an empirical formula stated over
/// "the numeric value of the strength in MPa" rather than over the strength
/// itself. Such a rule is dimensionally inconsistent by construction: the same
/// physical quantity read in a different unit would feed the rule a different
/// number and the rule does not correct for that, because it was never meant
/// to be evaluated in any other unit. This library cannot make a rule like
/// that consistent, and silently dropping the unit to accommodate it would
/// defeat the entire dimensional layer phases 3 and 4 exist for.
///
/// So the hole is explicit, narrow, and impossible to take quietly:
///
///  - it names the unit the number must be read in;
///  - it carries a justification that cannot be omitted or left empty;
///  - it produces a dimensionless value, honestly -- what comes out really is
///    a bare number, not a quantity wearing one;
///  - and it is its own kind of node, so a trace can show exactly where the
///    type system was stepped around, rather than folding it into an ordinary
///    conversion no reader would think twice about.

#include <formula-cpp/detail/fixed_string.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/unit.hpp>

namespace formula
{

namespace detail
{
    /// Whether @p text actually says something, rather than merely occupying
    /// bytes.
    ///
    /// A plain `size() > 0` check counts bytes, not content, so a single
    /// space and a lone NUL both pass it -- and a justification nobody can
    /// read defeats this escape hatch exactly as thoroughly as no
    /// justification at all. The point of demanding one is that an audit
    /// trail shows why a dimension was deliberately dropped; blank space
    /// shows nothing.
    ///
    /// Deliberately not a general whitespace classifier: `std::isspace` is
    /// locale-dependent and not `constexpr`, and this has to run during
    /// translation. These are the characters a person types by accident,
    /// plus NUL, which `FixedString` can carry because it is byte-oriented.
    [[nodiscard]] constexpr bool saysSomething(std::string_view text) noexcept
    {
        for (char const character: text)
            if (character != ' ' && character != '\t' && character != '\n' && character != '\r' && character != '\f' && character != '\v' && character != '\0')
                return true;
        return false;
    }

    /// Fails to compile when a numeric-value escape hatch names a unit that
    /// does not measure the dimension of the expression it reads a number
    /// from -- "the numeric value of this mass in megapascals" is not a
    /// thing.
    template <Unit U, typename Operand>
    struct RequireEscapeUnitMatches
    {
        static_assert(U.dimension == Operand::dimension,
                      "formula: this numeric_value_of names a unit that does not measure the dimension "
                      "of the expression it reads a number from; the unit's dimension and the operand "
                      "appear in this diagnostic as the template arguments of RequireEscapeUnitMatches");

        static constexpr bool value = true;
    };
} // namespace detail

/// The numeric value of @p Operand, read in @p U, with @p Justification
/// recording why the rule this feeds needs a bare number rather than the
/// quantity itself.
template <Unit U, detail::FixedString Justification, Node Operand>
struct NumericValueNode: NodeBase
{
    static_assert(detail::saysSomething(Justification.view()),
                  "formula: numeric_value_of requires a justification saying why this rule is stated "
                  "over a bare number rather than over a quantity; one that is empty, blank, or only "
                  "NUL bytes defeats the only safeguard this escape hatch has");
    static_assert(detail::RequireEscapeUnitMatches<U, Operand>::value);

    /// The expression whose numeric value is taken.
    Operand operand {};

    /// The unit the number must be read in. The coefficients of the rule this
    /// escape hatch exists for only work for this one unit; that is what makes
    /// the rule dimensionally inconsistent and this node necessary.
    static constexpr Unit unit = U;

    /// Why the dimension is being dropped here. Part of the type, so it cannot
    /// be omitted, cannot be empty, and costs no storage.
    static constexpr std::string_view justification = Justification.view();

    /// Dimensionless, by construction. That is the whole point: what comes out
    /// is a bare number, and the type system now says so honestly rather than
    /// carrying a dimension that the rule downstream will contradict.
    static constexpr Dimension dimension = dim::Scalar;
};

/// `numeric_value_of<unit::Megapascal, "...">(var<Strength>)`: the numeric
/// value of `operand`, read in `U`. Fails to compile if `Justification` is
/// empty or if `U` does not measure `operand`'s dimension -- see
/// `detail::RequireEscapeUnitMatches`.
template <Unit U, detail::FixedString Justification, Node Operand>
[[nodiscard]] constexpr auto numeric_value_of(Operand operand) noexcept
{
    return NumericValueNode<U, Justification, Operand> { {}, operand };
}

/// Evaluates the operand and converts its (coherent-SI) value into `U`. The
/// converted number is returned as-is, in `Rep`, and never converted back --
/// unlike every other node in this library, the whole point here is that the
/// dimension is gone.
template <typename Rep = Rational,
          Unit U,
          detail::FixedString Justification,
          Node Operand,
          typename Env,
          typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(NumericValueNode<U, Justification, Operand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    sink.entered(node);
    Evaluated<Rep> const operand = detail::dispatch<Rep>(node.operand, environment, sink);
    if (!operand.has_value())
    {
        Evaluated<Rep> const failed = std::unexpected { operand.error() };
        sink.produced(node, failed);
        return failed;
    }
    if (!operand->has_value())
    {
        Evaluated<Rep> const absent = detail::nothing<Rep>();
        sink.produced(node, absent);
        return absent;
    }

    std::expected<Rep, ArithmeticError> const converted = checked_convert(**operand, coherent(U.dimension), U);
    Evaluated<Rep> const result =
        converted.has_value() ? detail::present<Rep>(*converted) : Evaluated<Rep> { std::unexpected { converted.error() } };
    sink.produced(node, result);
    return result;
}

} // namespace formula
