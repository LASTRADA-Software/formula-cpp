// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// A value of a quantity, which may honestly be absent.

#include <formula-cpp/error.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>

#include <expected>
#include <optional>
#include <string_view>

namespace formula
{

/// A value of quantity `Q`, in `Q`'s declared unit, which may not have been
/// measured at all.
///
/// A default-constructed `Measured` is ABSENT, not zero. Zero is a measurement;
/// "not entered" is not a number, and a library that starts an unmeasured
/// quantity at zero produces a confident wrong answer -- which is the failure
/// this layer exists to prevent.
///
/// It carries no unit of its own: the unit is part of `Q`. Converting therefore
/// produces a value of a different quantity, or a bare `Rational`, and never a
/// silently re-scaled `Measured<Q>`.
///
/// Unlike `Unit` and `Dimension`, this is NOT a structural type and is not meant
/// to be a template argument -- `std::optional` is not structural in any of the
/// standard libraries we support. That costs nothing: the quantity TYPE is the
/// compile-time thing, and this is the runtime value. Measured on all three
/// compilers; the two are independent.
template <Described Q>
class Measured
{
    // Closes, for a foreign type, the door `Quantity` closes by deriving its
    // dimension from its unit: `Q`'s dimension must agree with its own unit's, or every reader below --
    // quantity_dimension() included -- reports a label the value does not
    // have. Fires when this class is COMPLETED (see RequireDescribed's own
    // note), which every constructed Measured<Q> is.
    //
    // A `static_assert` here rather than a constraint on `Q`, and that choice
    // has a cost worth stating. Measured:
    //
    //     requires { typename Measured<T>; }
    //       T undescribed                      -> false, gracefully
    //       T described but dimension-wrong    -> HARD ERROR
    //
    // because a `static_assert` failure is not in the immediate context -- the
    // same fact that forced `Describe`'s primary template to stay empty, one
    // layer up. Moving this into a `requires DescribesConsistentDimension<Q>`
    // clause would make both cases a graceful false, at the price of replacing
    // our own wording with "constraints not satisfied" at every direct misuse,
    // which is the common case and the one a person actually reads.
    //
    // Nothing performs SFINAE on `Measured<T>` today, so the cost is currently
    // zero. Phase 5's expression layer may well want to, and if it does, this
    // is the line to revisit -- the concept it would need already exists.
    static_assert(RequireDescribed<Q>::value);

  public:
    /// Not measured.
    constexpr Measured() noexcept = default;

    /// A present value, in `Q`'s declared unit.
    constexpr explicit Measured(Rational measuredValue) noexcept:
        _value { measuredValue }
    {
    }

    /// Not measured -- equivalent to the default constructor, but named for
    /// readability at the call site.
    [[nodiscard]] static constexpr Measured absent() noexcept { return Measured {}; }

    /// True when a value is present.
    [[nodiscard]] constexpr bool has_value() const noexcept { return _value.has_value(); }
    /// True when no value is present.
    [[nodiscard]] constexpr bool is_absent() const noexcept { return !_value.has_value(); }

    /// The stored payload, for code that wants to branch on it directly.
    ///
    /// Lvalues only. The rvalue overload below is deleted because binding a
    /// temporary here hands back a reference into an object that is already
    /// destroyed by the time the caller reads it -- confirmed with
    /// AddressSanitizer (`stack-use-after-scope`) and silent on cl /W4,
    /// clang-cl /W4 and `clang++ -Weverything`. Same hazard and same fix as
    /// `view(Symbol&&)` in `dimension.hpp`.
    [[nodiscard]] constexpr std::optional<Rational> const& stored() const& noexcept { return _value; }

    /// Deleted: see above. Bind the `Measured` to a named local first.
    std::optional<Rational> const& stored() const&& = delete;

    /// @throws ArithmeticException with `DomainError` when absent. There is no
    ///         number to return, and returning zero would be a lie.
    [[nodiscard]] constexpr Rational value() const
    {
        if (!_value.has_value())
            throw ArithmeticException { ArithmeticError::DomainError };
        return *_value;
    }

    /// The caller states what an absent reading counts as. Deliberately explicit:
    /// there is no default answer the library could give that is right for every
    /// caller.
    [[nodiscard]] constexpr Rational value_or(Rational fallback) const noexcept
    {
        return _value.value_or(fallback);
    }

    /// Equal when both are absent, or both present with the same value.
    [[nodiscard]] constexpr bool operator==(Measured const&) const noexcept = default;

    /// Convenience readers for the quantity's own metadata, so a caller holding
    /// a value does not have to name `Describe<Q>` to label it.
    [[nodiscard]] static constexpr Unit quantity_unit() noexcept { return Describe<Q>::unit; }
    /// `Q`'s symbol -- see `quantity_unit`.
    [[nodiscard]] static constexpr std::string_view quantity_symbol() noexcept { return Describe<Q>::symbol; }
    /// `Q`'s description -- see `quantity_unit`.
    [[nodiscard]] static constexpr std::string_view quantity_description() noexcept
    {
        return Describe<Q>::description;
    }
    /// `Q`'s dimension -- see `quantity_unit`.
    [[nodiscard]] static constexpr Dimension quantity_dimension() noexcept { return Describe<Q>::dimension; }

  private:
    std::optional<Rational> _value {};
};

/// Applies @p function to a present value; leaves an absent one absent.
///
/// This is the whole propagation rule in one place. Everything below is written
/// in terms of it or repeats it exactly.
template <Described Q, typename F>
[[nodiscard]] constexpr Measured<Q> transform(Measured<Q> value, F function)
{
    if (value.is_absent())
        return Measured<Q> {};
    return Measured<Q> { function(value.value()) };
}

/// Combines two measurements into a THIRD quantity the caller names. Absent
/// if EITHER is absent.
///
/// `Result` is not deduced from either argument, and cannot be: combining two
/// quantities generally yields a third (a mass and a volume combine into a
/// density, not into either operand's quantity), and there is no honest
/// default to fall back on. Naming it explicitly forces the caller to answer
/// the question this library cannot -- `combine<Density>(mass, volume,
/// [](Rational m, Rational v) { return m / v; })`.
///
/// An earlier signature deduced the result as the right-hand operand's
/// quantity, so `combine(mass, volume, divide)` was statically a `Measured`
/// of volume reporting a volume's symbol and unit for a value that was a
/// density. Measured: the branch's own test and example were both written
/// against that signature and read as if correct -- a wrong label on a right
/// number, and worse than a wrong number because it looks authoritative.
///
/// Not "absent if both": a formula with one missing input has no answer, and
/// producing one from the inputs that happen to be present is precisely the
/// wrong number this layer exists to prevent.
template <Described Result, Described Q, Described R, typename F>
[[nodiscard]] constexpr Measured<Result> combine(Measured<Q> lhs, Measured<R> rhs, F function)
{
    if (lhs.is_absent() || rhs.is_absent())
        return Measured<Result> {};
    return Measured<Result> { function(lhs.value(), rhs.value()) };
}

namespace detail
{

    /// Fails to compile when a measurement is converted into a quantity of
    /// another dimension: no such conversion exists, and both dimensions are
    /// known where the call is written.
    ///
    /// Only `::value`, `sizeof(...)` or a variable of this type runs the
    /// `static_assert`; see `RequireSameUnitDimension`.
    template <Described From, Described To>
    struct RequireConvertibleQuantities
    {
        static_assert(Describe<From>::dimension == Describe<To>::dimension,
                      "formula: these two quantities measure different dimensions, so no conversion "
                      "between them exists; the two quantities appear in this diagnostic as the "
                      "template arguments of RequireConvertibleQuantities");

        /// Always `true` once reached -- the `static_assert` above already failed
        /// compilation otherwise.
        static constexpr bool value = true;
    };

} // namespace detail

/// Converts a measurement of `Q` into one of `R`, exactly.
///
/// The dimensions are checked where the call is written, whether or not a value
/// is present: a conversion nobody could perform does not compile, and so cannot
/// look like it succeeded merely because there was no number to get wrong. A
/// refused conversion draws that one message: the unit conversion in the body is
/// an ordinary run-time call and adds none (measured with cl and clang-cl).
template <Described R, Described Q>
[[nodiscard]] constexpr std::expected<Measured<R>, ArithmeticError> checked_convert_to(Measured<Q> value) noexcept
{
    static_assert(detail::RequireConvertibleQuantities<Q, R>::value);
    if (value.is_absent())
        return Measured<R> {};

    std::expected<Rational, ArithmeticError> const inTargetUnit =
        checked_convert(value.value(), Describe<Q>::unit, Describe<R>::unit);
    if (!inTargetUnit)
        return std::unexpected { inTargetUnit.error() };
    return Measured<R> { *inTargetUnit };
}

/// Throwing spelling of `checked_convert_to`, for callers who would only rethrow.
///
/// Throws `ArithmeticException` where `checked_convert_to` returns an error; a
/// conversion across dimensions does not compile, as there.
template <Described R, Described Q>
[[nodiscard]] constexpr Measured<R> convert_to(Measured<Q> measured)
{
    return detail::or_throw(checked_convert_to<R>(measured));
}

/// Checks a measurement against its quantity's unit's declared bounds.
template <Described Q>
[[nodiscard]] constexpr std::expected<BoundsCheck, ArithmeticError> checked_within_bounds(
    Measured<Q> value) noexcept
{
    if (value.is_absent())
        return BoundsCheck::NotMeasured;
    return checked_within_bounds(value.value(), Describe<Q>::unit);
}

/// Rounds a measurement to the precision its quantity's unit declares.
template <Described Q>
[[nodiscard]] constexpr std::expected<Measured<Q>, ArithmeticError> checked_round_to_declared(
    Measured<Q> value, RoundingMode mode) noexcept
{
    if (value.is_absent())
        return Measured<Q> {};

    std::expected<Rational, ArithmeticError> const rounded =
        checked_round_to_declared(value.value(), Describe<Q>::unit, mode);
    if (!rounded)
        return std::unexpected { rounded.error() };
    return Measured<Q> { *rounded };
}

/// Throwing spelling of `checked_within_bounds`, for callers who would only rethrow.
///
/// An absent measurement is `BoundsCheck::NotMeasured`, as there. Throws
/// `ArithmeticException` where `checked_within_bounds` returns an error.
template <Described Q>
[[nodiscard]] constexpr BoundsCheck within_bounds(Measured<Q> measured)
{
    return detail::or_throw(checked_within_bounds(measured));
}

/// Throwing spelling of `checked_round_to_declared`, for callers who would only rethrow.
///
/// An absent measurement stays absent, as there. Throws `ArithmeticException`
/// where `checked_round_to_declared` returns an error.
template <Described Q>
[[nodiscard]] constexpr Measured<Q> round_to_declared(Measured<Q> measured, RoundingMode roundingMode)
{
    return detail::or_throw(checked_round_to_declared(measured, roundingMode));
}

} // namespace formula
