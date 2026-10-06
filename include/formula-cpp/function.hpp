// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Function nodes: integer powers, roots, pi, logarithms and the exponential.
///
/// Powers and roots act on the dimension as well as the number -- the square of
/// a length is an area, the cube root of a volume is a length -- which is why
/// they are nodes rather than free functions a caller applies to an evaluated
/// number. A root may also produce a fractional exponent, which is exactly why
/// `Dimension` carries rational exponents.
///
/// Logarithms and the exponential are nodes for the page's and the trace's
/// sake. They act on no dimension because they accept none -- the argument must
/// be a bare number -- and are exact only where their value is rational.

#include <formula-cpp/dimension.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/yields.hpp>

#include <cmath>
#include <cstdint>
#include <expected>
#include <optional>
#include <string_view>

namespace formula
{

/// @p Operand raised to an integer power. Negative exponents are allowed and
/// invert the dimension with the value.
template <int Exponent, Node Operand>
struct PowerNode: NodeBase
{
    /// The base expression.
    ///
    /// Deliberately no `{}` default member initialiser: with one, a method
    /// holding a lookup under this member fails to compile on clang++,
    /// clang-cl or g++, and cl answers the trait wrongly -- see `Corrections`
    /// (`lookup.hpp`).
    Operand operand;

    /// The power `operand` is raised to.
    static constexpr int exponent = Exponent;
    /// `operand`'s dimension, scaled by `exponent`.
    static constexpr Dimension dimension = power(Operand::dimension, Exponent);
    /// Whether its operand was refused -- see `detail::refused_already`.
    static constexpr detail::RefusedFlag refused = detail::refused_already<Operand>();
};

namespace detail
{
    /// Fails to compile for a root of degree zero, which names no operation.
    template <int Degree>
    struct RequirePositiveRootDegree
    {
        static_assert(Degree > 0,
                      "formula: a root of degree zero describes no operation; the degree appears in "
                      "this diagnostic as the template argument of RequirePositiveRootDegree");

        static constexpr bool value = true;
    };
} // namespace detail

/// The @p Degree-th root of @p Operand.
template <int Degree, Node Operand>
struct RootNode: NodeBase
{
    static_assert(detail::RequirePositiveRootDegree<Degree>::value);

    /// The expression the root is taken of.
    ///
    /// Deliberately no `{}` default member initialiser: with one, a method
    /// holding a lookup under this member fails to compile on clang++,
    /// clang-cl or g++, and cl answers the trait wrongly -- see `Corrections`
    /// (`lookup.hpp`).
    Operand operand;

    /// Which root this is -- 2 for a square root, 3 for a cube root, and so on.
    static constexpr int degree = Degree;
    /// `operand`'s dimension, divided by `degree` -- possibly fractional.
    static constexpr Dimension dimension = nth_root(Operand::dimension, Degree);
    /// Whether its operand was refused -- see `detail::refused_already`.
    static constexpr detail::RefusedFlag refused = detail::refused_already<Operand>();
};

/// Pi, as a node, so that a formula containing it stays a formula.
struct PiNode: NodeBase
{
    /// Pi is dimensionless.
    static constexpr Dimension dimension = dim::Scalar;
};

/// Which function a `TranscendentalNode` takes of its argument.
enum class Transcendental : std::uint8_t
{
    /// The natural logarithm, `ln`: to base e.
    NaturalLogarithm,
    /// The decimal logarithm, `log10`: to base 10.
    DecimalLogarithm,
    /// The exponential, `exp`: e raised to the argument.
    Exponential,
};

namespace detail
{
    /// Fails to compile when the argument of a logarithm or an exponential is not dimensionless: of a
    /// quantity, ln(2 m) would be ln 2 + ln(m), a number that changes with the unit the quantity is read
    /// in.
    ///
    /// Asked only of an operand not refused already (`refused_already`), so that an operand refused for
    /// another reason draws that refusal alone. Here, beside `RequirePositiveRootDegree`, and not in
    /// `expression.hpp`, whose line numbers the guides quote.
    template <typename Operand>
    struct RequireDimensionlessArgument
    {
        static_assert(refused_already<Operand>() || is_dimensionless(Operand::dimension),
                      "formula: the argument of this logarithm or exponential is not dimensionless; ln, log10 "
                      "and exp take a bare number, and of a quantity they would change with the unit it is read "
                      "in -- divide it by a reference value of its own dimension, or read it with "
                      "numeric_value_of; the argument appears in this diagnostic as the template argument of "
                      "RequireDimensionlessArgument");

        static constexpr bool value = true;
    };

    /// How a formula writes @p function -- `ln`, `log10`, `exp` -- in plain text, in Markdown and in a
    /// trace: the one spelling `render.hpp` and `trace_render.hpp` both read, so that a derivation names
    /// the function its formula names.
    [[nodiscard]] constexpr std::string_view transcendental_name(Transcendental function) noexcept
    {
        switch (function)
        {
            case Transcendental::NaturalLogarithm:
                return "ln";
            case Transcendental::DecimalLogarithm:
                return "log10";
            case Transcendental::Exponential:
                return "exp";
        }
        return "unknown function";
    }

    /// k when @p positive is exactly 10^k, and nothing otherwise. Read off the reduced fraction: a power
    /// of ten is a power of ten over 1, or 1 over a power of ten, so k runs from -38 to 38, the powers of
    /// ten `Rational::Int` holds. @pre @p positive is above zero.
    [[nodiscard]] constexpr std::optional<int> power_of_ten_exponent(Rational positive) noexcept
    {
        bool const whole = positive.denominator() == 1;
        if (!whole && positive.numerator() != 1)
            return std::nullopt;
        Rational::Int remaining = whole ? positive.numerator() : positive.denominator();
        int tens = 0;
        while (remaining % 10 == 0)
        {
            remaining /= 10;
            ++tens;
        }
        if (remaining != 1)
            return std::nullopt;
        return whole ? tens : -tens;
    }
} // namespace detail

/// The natural logarithm, the decimal logarithm or the exponential -- @p F -- of @p Operand, a
/// dimensionless expression. The result is dimensionless too.
template <Transcendental F, Node Operand>
struct TranscendentalNode: NodeBase
{
    static_assert(detail::RequireDimensionlessArgument<Operand>::value);

    /// The argument: a bare number.
    ///
    /// Named `operand`, as `ConstantRewriteOperand` (`overlay.hpp`) reads it, and deliberately no `{}`
    /// default member initialiser -- see `Corrections` (`lookup.hpp`).
    Operand operand;

    /// Which function this is. Named `function`: the trace recorder reads a node's `exponent`, `degree`,
    /// `places`, `digits`, `mode`, `unit` and `keyUnit` by name, and this node declares none of them.
    static constexpr Transcendental function = F;
    /// A bare number, as the argument is.
    static constexpr Dimension dimension = dim::Scalar;
    /// Whether its operand was refused -- see `detail::refused_already`. Its own check is not counted:
    /// `ln(L) + m`, over a length and a mass, is two mistakes, and the sum says so too.
    static constexpr detail::RefusedFlag refused = detail::refused_already<Operand>();
};

/// `operand` raised to the integer power `Exponent`: `pow<2>(var<Length>)`.
template <int Exponent, Node Operand>
[[nodiscard]] constexpr auto pow(Operand operand) noexcept
{
    return PowerNode<Exponent, Operand> { {}, operand };
}

/// A bound formula as `pow`'s operand: the formula it holds, in its place
/// (`yields.hpp`).
template <int Exponent, typename Bound>
    requires detail::AnyBound<Bound>
[[nodiscard]] constexpr auto pow(Bound boundFormula) noexcept
{
    return pow<Exponent>(boundFormula.expression);
}

/// The square root of `operand`.
template <Node Operand>
[[nodiscard]] constexpr auto sqrt(Operand operand) noexcept
{
    return RootNode<2, Operand> { {}, operand };
}

/// A bound formula as `sqrt`'s operand: the formula it holds, in its place
/// (`yields.hpp`).
template <typename Bound>
    requires detail::AnyBound<Bound>
[[nodiscard]] constexpr auto sqrt(Bound boundFormula) noexcept
{
    return sqrt(boundFormula.expression);
}

/// The cube root of `operand`.
template <Node Operand>
[[nodiscard]] constexpr auto cbrt(Operand operand) noexcept
{
    return RootNode<3, Operand> { {}, operand };
}

/// A bound formula as `cbrt`'s operand: the formula it holds, in its place
/// (`yields.hpp`).
template <typename Bound>
    requires detail::AnyBound<Bound>
[[nodiscard]] constexpr auto cbrt(Bound boundFormula) noexcept
{
    return cbrt(boundFormula.expression);
}

/// The `Degree`-th root of `operand`: `root<5>(var<Volume>)`.
template <int Degree, Node Operand>
[[nodiscard]] constexpr auto root(Operand operand) noexcept
{
    return RootNode<Degree, Operand> { {}, operand };
}

/// A bound formula as `root`'s operand: the formula it holds, in its place
/// (`yields.hpp`).
template <int Degree, typename Bound>
    requires detail::AnyBound<Bound>
[[nodiscard]] constexpr auto root(Bound boundFormula) noexcept
{
    return root<Degree>(boundFormula.expression);
}

/// The spelling of pi in a formula.
inline constexpr PiNode pi {};

/// The natural logarithm of `operand`, a dimensionless expression:
/// `ln(var<Count> / var<InitialCount>)`. Takes a formula node only -- a bare `Rational` is not one.
template <Node Operand>
[[nodiscard]] constexpr auto ln(Operand operand) noexcept
{
    return TranscendentalNode<Transcendental::NaturalLogarithm, Operand> { {}, operand };
}

/// A bound formula as `ln`'s operand: the formula it holds, in its place
/// (`yields.hpp`).
template <typename Bound>
    requires detail::AnyBound<Bound>
[[nodiscard]] constexpr auto ln(Bound boundFormula) noexcept
{
    return ln(boundFormula.expression);
}

/// The decimal logarithm of `operand`, a dimensionless expression.
template <Node Operand>
[[nodiscard]] constexpr auto log10(Operand operand) noexcept
{
    return TranscendentalNode<Transcendental::DecimalLogarithm, Operand> { {}, operand };
}

/// A bound formula as `log10`'s operand: the formula it holds, in its place
/// (`yields.hpp`).
template <typename Bound>
    requires detail::AnyBound<Bound>
[[nodiscard]] constexpr auto log10(Bound boundFormula) noexcept
{
    return log10(boundFormula.expression);
}

/// The exponential of `operand`, a dimensionless expression: e raised to it.
template <Node Operand>
[[nodiscard]] constexpr auto exp(Operand operand) noexcept
{
    return TranscendentalNode<Transcendental::Exponential, Operand> { {}, operand };
}

/// A bound formula as `exp`'s operand: the formula it holds, in its place
/// (`yields.hpp`).
template <typename Bound>
    requires detail::AnyBound<Bound>
[[nodiscard]] constexpr auto exp(Bound boundFormula) noexcept
{
    return exp(boundFormula.expression);
}

// ---------------------------------------------------------------- evaluation

/// Powers and roots per representation. Kept here rather than in `RepTraits`
/// itself so that `evaluate.hpp` stays free of `<cmath>`: a consumer who never
/// writes a root never compiles it.
///
/// Like `RepTraits`, this is a **public extension point** and belongs outside
/// `detail` for the same reason: a consumer teaching the evaluator a new
/// representation specialises both, and a specialisation of one without the
/// other reaches only as far as the first power or root in a formula.
template <typename Rep>
struct RepFunctions;

/// Exact rational powers, roots and pi -- every failure (an inexact root, an
/// overflow, a reciprocal of zero, a root with no real answer) is reported as an
/// `ArithmeticError` rather than silently approximated.
template <>
struct RepFunctions<Rational>
{
    /// `base` raised to `exponent`, exactly. Fails with `Overflow` if the exact
    /// result, or an intermediate on the way to it, exceeds `Rational`'s range,
    /// and with `DivisionByZero` for a negative exponent of zero -- a negative
    /// exponent inverts, and zero has no reciprocal.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> raise(Rational base, int exponent) noexcept
    {
        return checked_pow(base, exponent);
    }

    /// The `degree`-th root of `radicand`, exactly. Fails with `Inexact` if that
    /// root is not itself a rational number, with `DomainError` for a degree
    /// below 1 or an even root of a negative value, and with `Overflow` for the
    /// one numerator whose magnitude `Rational::Int` cannot hold.
    ///
    /// `RootNode` rejects a degree below 1 at compile time, so that case is
    /// reachable only by calling this directly.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> root(Rational radicand, int degree) noexcept
    {
        return checked_exact_nth_root(radicand, degree);
    }

    /// Pi, as the library's own rational approximation -- see `Pi`.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> pi_value() noexcept
    {
        return Pi;
    }

    /// The natural logarithm of `argument`, exactly: 0 at 1. Every other positive rational has an
    /// irrational logarithm, which is `Inexact`; zero and below have none, which is `DomainError`.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> natural_log(Rational argument) noexcept
    {
        if (argument.sign() <= 0)
            return std::unexpected { ArithmeticError::DomainError };
        if (argument == Rational { 1 })
            return Rational {};
        return std::unexpected { ArithmeticError::Inexact };
    }

    /// The decimal logarithm of `argument`, exactly: k at 10^k, for k from -38 to 38. Every other positive
    /// rational has an irrational one, which is `Inexact`; zero and below have none, which is
    /// `DomainError`.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> decimal_log(Rational argument) noexcept
    {
        if (argument.sign() <= 0)
            return std::unexpected { ArithmeticError::DomainError };
        std::optional<int> const tens = detail::power_of_ten_exponent(argument);
        if (tens.has_value())
            return Rational { *tens };
        return std::unexpected { ArithmeticError::Inexact };
    }

    /// e raised to `argument`, exactly: 1 at 0. At every other rational it is irrational, however large,
    /// which is `Inexact`.
    [[nodiscard]] static constexpr std::expected<Rational, ArithmeticError> exponential(Rational argument) noexcept
    {
        if (argument.is_zero())
            return Rational { 1 };
        return std::unexpected { ArithmeticError::Inexact };
    }
};

/// Binary floating-point powers, roots and pi.
template <>
struct RepFunctions<double>
{
    /// `base` raised to `exponent`, via `std::pow`.
    [[nodiscard]] static std::expected<double, ArithmeticError> raise(double base, int exponent) noexcept
    {
        return std::pow(base, static_cast<double>(exponent));
    }

    /// The `degree`-th root of `radicand`. An even-degree root of a negative value
    /// has no real result and is reported as `ArithmeticError::DomainError`; an
    /// odd-degree root of a negative value returns the negative real root.
    [[nodiscard]] static std::expected<double, ArithmeticError> root(double radicand, int degree) noexcept
    {
        if (radicand < 0.0 && degree % 2 == 0)
            return std::unexpected { ArithmeticError::DomainError };
        if (radicand < 0.0)
            return -std::pow(-radicand, 1.0 / static_cast<double>(degree));
        return std::pow(radicand, 1.0 / static_cast<double>(degree));
    }

    /// Pi, to `double` precision.
    [[nodiscard]] static std::expected<double, ArithmeticError> pi_value() noexcept
    {
        return 3.141592653589793238462643383279502884;
    }

    /// The natural logarithm, via `std::log`. `DomainError` for zero, a negative value and NaN, all three
    /// caught by `!(argument > 0.0)` before the standard library is called.
    [[nodiscard]] static std::expected<double, ArithmeticError> natural_log(double argument) noexcept
    {
        if (!(argument > 0.0))
            return std::unexpected { ArithmeticError::DomainError };
        return std::log(argument);
    }

    /// The decimal logarithm, via `std::log10` -- qualified, as every call here is: inside namespace
    /// `formula` an unqualified `log10` or `exp` finds `formula::log10` or `formula::exp`, which take a
    /// formula node, and looks no further. `DomainError` as for `natural_log`.
    [[nodiscard]] static std::expected<double, ArithmeticError> decimal_log(double argument) noexcept
    {
        if (!(argument > 0.0))
            return std::unexpected { ArithmeticError::DomainError };
        return std::log10(argument);
    }

    /// The exponential, via `std::exp`. A result too large for `double` is `+inf`, and passes, as `raise`'s
    /// does: the caller asked for `double`.
    [[nodiscard]] static std::expected<double, ArithmeticError> exponential(double argument) noexcept
    {
        return std::exp(argument);
    }
};

/// Evaluates the operand, then raises it to `Exponent` via `RepFunctions<Rep>`.
template <typename Rep = Rational, int Exponent, Node Operand, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(PowerNode<Exponent, Operand> const& node,
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

    std::expected<Rep, ArithmeticError> const raised = RepFunctions<Rep>::raise(**evaluatedOperand, Exponent);
    Evaluated<Rep> const evaluated =
        raised.has_value() ? detail::present<Rep>(*raised) : Evaluated<Rep> { std::unexpected { raised.error() } };
    sink.produced(node, evaluated);
    return evaluated;
}

/// Evaluates the operand, then takes its `Degree`-th root via `RepFunctions<Rep>`.
template <typename Rep = Rational, int Degree, Node Operand, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(RootNode<Degree, Operand> const& node,
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

    std::expected<Rep, ArithmeticError> const rooted = RepFunctions<Rep>::root(**evaluatedOperand, Degree);
    Evaluated<Rep> const evaluated =
        rooted.has_value() ? detail::present<Rep>(*rooted) : Evaluated<Rep> { std::unexpected { rooted.error() } };
    sink.produced(node, evaluated);
    return evaluated;
}

/// Pi is always present; produces it in `Rep` via `RepFunctions<Rep>::pi_value`.
template <typename Rep = Rational, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(PiNode const& node, Env const&, Sink sink = {}) noexcept
{
    sink.entered(node);
    std::expected<Rep, ArithmeticError> const piValue = RepFunctions<Rep>::pi_value();
    Evaluated<Rep> const evaluated =
        piValue.has_value() ? detail::present<Rep>(*piValue) : Evaluated<Rep> { std::unexpected { piValue.error() } };
    sink.produced(node, evaluated);
    return evaluated;
}

namespace detail
{
    /// @p F of @p argument through `RepFunctions<Rep>`: the one place the three functions are told
    /// apart, for the plain node and, under a representation other than `Rational`, the rounded one.
    template <Transcendental F, typename Rep>
    [[nodiscard]] constexpr std::expected<Rep, ArithmeticError> transcendental_of(Rep argument) noexcept
    {
        if constexpr (F == Transcendental::NaturalLogarithm)
            return RepFunctions<Rep>::natural_log(argument);
        else if constexpr (F == Transcendental::DecimalLogarithm)
            return RepFunctions<Rep>::decimal_log(argument);
        else
            return RepFunctions<Rep>::exponential(argument);
    }
} // namespace detail

/// Evaluates the argument, then takes `F` of it via `RepFunctions<Rep>`. An absent argument leaves the
/// node absent and a failed one fails it, before the function is asked anything.
template <typename Rep = Rational, Transcendental F, Node Operand, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(TranscendentalNode<F, Operand> const& node,
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

    std::expected<Rep, ArithmeticError> const taken = detail::transcendental_of<F, Rep>(**evaluatedOperand);
    Evaluated<Rep> const evaluated =
        taken.has_value() ? detail::present<Rep>(*taken) : Evaluated<Rep> { std::unexpected { taken.error() } };
    sink.produced(node, evaluated);
    return evaluated;
}

} // namespace formula
