// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// The failure vocabulary of exact arithmetic.
///
/// Two layers, deliberately: every fallible operation has a `checked_` form
/// returning `std::expected`, and the operators are thin wrappers that throw.
/// Nothing saturates and nothing truncates -- a norm calculation that quietly
/// produces a near miss is worse than one that stops.
///
/// A throw inside a constant expression is a compile error, so misuse in a
/// constexpr context is caught at build time with no extra machinery.

#include <cstdint>
#include <exception>
#include <expected>
#include <string_view>

namespace formula
{

/// Why an exact arithmetic operation could not produce a result.
enum class ArithmeticError : std::uint8_t
{
    /// The divisor was zero.
    DivisionByZero,
    /// The exact result, or an intermediate term, is outside the representable range.
    Overflow,
    /// A floating-point input was NaN or infinite.
    NotFinite,
    /// An argument was outside the domain of the operation -- a non-positive
    /// step, say, or fewer than one significant digit.
    DomainError,
};

/// A lowercase noun phrase with no trailing punctuation, so callers can embed it
/// in a longer sentence. Spec phase 5 renders this into the `invalid` arm of the
/// evaluation result.
[[nodiscard]] constexpr std::string_view describe(ArithmeticError error) noexcept
{
    switch (error)
    {
        case ArithmeticError::DivisionByZero:
            return "division by zero";
        case ArithmeticError::Overflow:
            return "overflow in exact arithmetic";
        case ArithmeticError::NotFinite:
            return "value is not finite";
        case ArithmeticError::DomainError:
            return "argument outside the domain of the operation";
    }
    return "unknown arithmetic error";
}

/// Thrown by the operator layer when the corresponding `checked_` operation fails.
class ArithmeticException: public std::exception
{
  public:
    /// Not `constexpr`: `std::exception`'s own constructor is not constexpr in
    /// libstdc++, so a `constexpr` constructor here can never produce a constant
    /// expression and clang rejects it under `-Winvalid-constexpr`. Nothing is
    /// lost -- an exception object is never built during constant evaluation.
    /// `or_throw` stays `constexpr` regardless: a `throw` is allowed in a
    /// constexpr function as long as it is not reached at compile time, and when
    /// it is reached the result is the compile error we want.
    explicit ArithmeticException(ArithmeticError error) noexcept:
        _error { error }
    {
    }

    [[nodiscard]] ArithmeticError code() const noexcept
    {
        return _error;
    }

    [[nodiscard]] char const* what() const noexcept override
    {
        return describe(_error).data();
    }

  private:
    ArithmeticError _error;
};

namespace detail
{

    /// Unwraps a checked result, throwing on failure. The operator layer is
    /// written entirely in terms of this.
    template <typename T>
    [[nodiscard]] constexpr T or_throw(std::expected<T, ArithmeticError> const& result)
    {
        if (!result)
            throw ArithmeticException { result.error() };
        return *result;
    }

} // namespace detail

} // namespace formula
