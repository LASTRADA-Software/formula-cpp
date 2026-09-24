// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// What an evaluation produces.
///
/// A calculation defined by a test method does not always produce a number. It
/// may reject the specimen, discard the whole result, or call for the test to
/// be repeated -- and for a substantial minority of methods that verdict *is*
/// the primary output rather than an exception path. A `double`-returning
/// evaluator has nowhere to put any of it, and a library that models them as
/// exceptions or sentinel values makes them invisible to the audit trail, which
/// is precisely where they have to be visible.
///
/// So evaluation returns `Outcome<Q>`: a value, an empty, a verdict or an
/// invalidation. `Verdict` and `InvalidReason` carry a bare label here; the
/// vocabulary that produces them arrives with constraints, and nothing in this
/// header needs to know it.

#include <formula-cpp/measured.hpp>
#include <formula-cpp/quantity.hpp>

#include <cstdint>
#include <string_view>

namespace formula
{

/// How a value came to be. A result that was typed in by an operator must never
/// be presented as though the library had derived it, so provenance travels
/// with the number rather than beside it.
enum class ValueSource : std::uint8_t
{
    /// Computed by this library from other values.
    Derived,
    /// Read from an instrument or entered as an observation of the specimen.
    Measured,
    /// Typed in by a person, replacing whatever would have been computed.
    ManuallyEntered,
};

/// Which alternative an `Outcome` holds.
///
/// There is deliberately no `Overridden` alternative: an override is the `Value`
/// alternative whose source is `ManuallyEntered`, so provenance has exactly one
/// home and a kind cannot contradict a source. `Outcome::is_overridden()` asks
/// the question directly.
enum class OutcomeKind : std::uint8_t
{
    Value,
    Empty,
    Verdict,
    Invalid,
};

/// A decision rather than a number: "reject the specimen", "repeat the test".
struct Verdict
{
    /// What the decision is, in words: "reject the specimen".
    std::string_view label {};

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(Verdict const&) const noexcept = default;
};

/// Why a result was discarded entirely.
struct InvalidReason
{
    /// Why, in words.
    std::string_view label {};

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(InvalidReason const&) const noexcept = default;
};

/// A number together with where it came from.
template <Described Q>
struct Value
{
    /// The number itself, possibly absent.
    Measured<Q> measurement {};
    /// Where it came from.
    ValueSource source = ValueSource::Derived;

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(Value const&) const noexcept = default;
};

/// The result of evaluating an expression for quantity @p Q.
template <Described Q>
class Outcome
{
  public:
    /// A computed or measured number. An **absent** measurement yields `Empty`,
    /// not a `Value` holding nothing: the two would otherwise be two ways of
    /// saying the same thing, and callers would have to check both.
    [[nodiscard]] static constexpr Outcome value(Measured<Q> measurement, ValueSource source) noexcept
    {
        Outcome result {};
        result._kind = measurement.has_value() ? OutcomeKind::Value : OutcomeKind::Empty;
        result._value = Value<Q> { measurement, source };
        return result;
    }

    /// No value, because an input was never measured.
    [[nodiscard]] static constexpr Outcome empty() noexcept
    {
        return value(Measured<Q>::absent(), ValueSource::Derived);
    }

    /// A decision in place of a number: "reject the specimen".
    [[nodiscard]] static constexpr Outcome verdict(Verdict decision) noexcept
    {
        Outcome result {};
        result._kind = OutcomeKind::Verdict;
        result._verdict = decision;
        return result;
    }

    /// The result is discarded entirely, for `reason`.
    [[nodiscard]] static constexpr Outcome invalid(InvalidReason reason) noexcept
    {
        Outcome result {};
        result._kind = OutcomeKind::Invalid;
        result._reason = reason;
        return result;
    }

    /// Which alternative this outcome holds.
    [[nodiscard]] constexpr OutcomeKind kind() const noexcept
    {
        return _kind;
    }

    /// True when `kind()` is `OutcomeKind::Value`.
    [[nodiscard]] constexpr bool is_value() const noexcept
    {
        return _kind == OutcomeKind::Value;
    }
    /// True when `kind()` is `OutcomeKind::Empty`.
    [[nodiscard]] constexpr bool is_empty() const noexcept
    {
        return _kind == OutcomeKind::Empty;
    }
    /// True when `kind()` is `OutcomeKind::Verdict`.
    [[nodiscard]] constexpr bool is_verdict() const noexcept
    {
        return _kind == OutcomeKind::Verdict;
    }
    /// True when `kind()` is `OutcomeKind::Invalid`.
    [[nodiscard]] constexpr bool is_invalid() const noexcept
    {
        return _kind == OutcomeKind::Invalid;
    }

    /// True when a person typed this result in place of a computed one.
    [[nodiscard]] constexpr bool is_overridden() const noexcept
    {
        return is_value() && _value.source == ValueSource::ManuallyEntered;
    }

    /// The measurement, absent for every kind but `Value`.
    [[nodiscard]] constexpr Measured<Q> measurement() const noexcept
    {
        return _value.measurement;
    }

    /// Where the number came from. Meaningful only when `is_value()`.
    [[nodiscard]] constexpr ValueSource source() const noexcept
    {
        return _value.source;
    }

    /// Empty for every kind but `Verdict` -- never a stale label from a
    /// different alternative.
    [[nodiscard]] constexpr std::string_view verdict_label() const noexcept
    {
        return _verdict.label;
    }

    /// Empty for every kind but `Invalid`.
    [[nodiscard]] constexpr std::string_view reason_label() const noexcept
    {
        return _reason.label;
    }

    /// Memberwise equality across every alternative.
    [[nodiscard]] constexpr bool operator==(Outcome const&) const noexcept = default;

  private:
    OutcomeKind _kind = OutcomeKind::Empty;
    Value<Q> _value {};
    Verdict _verdict {};
    InvalidReason _reason {};
};

} // namespace formula
