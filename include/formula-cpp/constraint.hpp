// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Constraints: relationships stated purely to validate, not to compute.
///
/// A standard routinely states a relationship in the same symbols as its
/// computing formulas but existing only to check a result -- "the two
/// replicates shall agree within 5%", "reject the specimen below 30 MPa".
/// `PredicateNode` (`predicate.hpp`) already compares two expressions;
/// `Constraint` is the small addition that pairs one with what to do when it
/// does not hold.
///
/// A constraint is deliberately **not** a `Node`, for the same reason a
/// predicate is not one: it produces a verdict, which has no dimension, and
/// giving it one would mean inventing a dimension to lie about. A constraint
/// cannot ride the `Node`-constrained machinery that renders, traces and
/// documents expressions -- see `citation.hpp`'s `DocumentedNode`, which
/// requires `Node Inner` and so cannot wrap a constraint either. That is why
/// `Constraint` carries its own `Citation` directly instead of being wrapped
/// by `documented()`.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/predicate.hpp>
#include <formula-cpp/sink.hpp>

#include <cstdint>
#include <expected>
#include <optional>

namespace formula
{

/// Which of the four states checking a constraint reached.
///
/// A predicate has three outcomes, not two: it holds, it does not hold, or it
/// never resolved -- because an input was never measured, or because
/// evaluating it raised an arithmetic error. `NotChecked` and `Invalid` stay
/// distinct for the same reason `OutcomeKind::Empty` and `OutcomeKind::Invalid`
/// are distinct on `Outcome`: "nobody measured the input" and "we tried to
/// check and the arithmetic broke" call for different action from whoever
/// reads the report.
enum class ConstraintOutcomeKind : std::uint8_t
{
    /// The predicate held.
    Satisfied,
    /// The predicate did not hold; the constraint's `Verdict` applies.
    Violated,
    /// The predicate never resolved, because an input was never measured.
    NotChecked,
    /// Evaluating the predicate raised an arithmetic error.
    Invalid,
};

/// What checking a `Constraint` produced.
///
/// **A constraint whose predicate could not be evaluated must never report
/// satisfied.** A check that silently passes when its input is missing is
/// worse than no check at all: it produces a record saying the specimen was
/// verified when nothing verified it. That is the entire reason this type
/// has four states instead of a `bool`.
class ConstraintOutcome
{
  public:
    /// The predicate held.
    [[nodiscard]] static constexpr ConstraintOutcome satisfied() noexcept
    {
        ConstraintOutcome result {};
        result._kind = ConstraintOutcomeKind::Satisfied;
        return result;
    }

    /// The predicate did not hold; carries the verdict the constraint was
    /// declared with.
    [[nodiscard]] static constexpr ConstraintOutcome violated(Verdict verdict) noexcept
    {
        ConstraintOutcome result {};
        result._kind = ConstraintOutcomeKind::Violated;
        result._verdict = verdict;
        return result;
    }

    /// The predicate never resolved -- an input was never measured.
    [[nodiscard]] static constexpr ConstraintOutcome not_checked() noexcept
    {
        ConstraintOutcome result {};
        result._kind = ConstraintOutcomeKind::NotChecked;
        return result;
    }

    /// Evaluating the predicate raised an arithmetic error; carries it.
    [[nodiscard]] static constexpr ConstraintOutcome invalid(ArithmeticError error) noexcept
    {
        ConstraintOutcome result {};
        result._kind = ConstraintOutcomeKind::Invalid;
        result._error = error;
        return result;
    }

    /// Which alternative this outcome holds.
    [[nodiscard]] constexpr ConstraintOutcomeKind kind() const noexcept
    {
        return _kind;
    }

    /// True when `kind()` is `ConstraintOutcomeKind::Satisfied`.
    [[nodiscard]] constexpr bool is_satisfied() const noexcept
    {
        return _kind == ConstraintOutcomeKind::Satisfied;
    }
    /// True when `kind()` is `ConstraintOutcomeKind::Violated`.
    [[nodiscard]] constexpr bool is_violated() const noexcept
    {
        return _kind == ConstraintOutcomeKind::Violated;
    }
    /// True when `kind()` is `ConstraintOutcomeKind::NotChecked`.
    [[nodiscard]] constexpr bool is_not_checked() const noexcept
    {
        return _kind == ConstraintOutcomeKind::NotChecked;
    }
    /// True when `kind()` is `ConstraintOutcomeKind::Invalid`.
    [[nodiscard]] constexpr bool is_invalid() const noexcept
    {
        return _kind == ConstraintOutcomeKind::Invalid;
    }

    // Deliberately no `bool satisfied()` and no `operator bool()`. Both would
    // have to answer *something* for `NotChecked` and `Invalid`, and every
    // answer is a lie: `true` collapses "held" together with "never checked"
    // or "broke while checking", which is exactly the safety property this
    // type exists to prevent someone from erasing by convenience-accessor.
    // `false` is not honest either -- it says the specimen failed the check,
    // when in fact no check ran at all. Callers who "just want to know if it
    // passed" are the exact callers who must instead look at `kind()`, or at
    // `is_satisfied()` *and* `is_violated()` *and* `is_not_checked()` *and*
    // `is_invalid()`, and decide what an unresolved check means for them. If
    // you are about to add one back: don't -- read this comment's title
    // first, then read D2 in phase-9-prep/design-decisions.md.

    /// The verdict, present only when `is_violated()`.
    [[nodiscard]] constexpr std::optional<Verdict> verdict() const noexcept
    {
        if (_kind == ConstraintOutcomeKind::Violated)
            return _verdict;
        return std::nullopt;
    }

    /// The arithmetic error, present only when `is_invalid()`.
    [[nodiscard]] constexpr std::optional<ArithmeticError> error() const noexcept
    {
        if (_kind == ConstraintOutcomeKind::Invalid)
            return _error;
        return std::nullopt;
    }

    /// Memberwise equality.
    [[nodiscard]] constexpr bool operator==(ConstraintOutcome const&) const noexcept = default;

  private:
    // Defaults to `NotChecked`, not `Satisfied` -- a default-constructed
    // outcome must never look like a passed check, the same reasoning that
    // makes `Outcome` default to `Empty` rather than to `Value`.
    ConstraintOutcomeKind _kind = ConstraintOutcomeKind::NotChecked;
    Verdict _verdict {};
    ArithmeticError _error { ArithmeticError::DivisionByZero };
};

/// A predicate paired with what to do when it does not hold.
///
/// The predicate states the condition that must **hold**, not the failure --
/// a standard says "the two replicates shall agree within 5%", so the
/// declaration reads as the standard reads, and `verdict` is what happens
/// when it does not. There is deliberately no separate verdict for success:
/// a satisfied constraint is silent by nature, and a slot for a pass verdict
/// only invites noise an auditor then has to read past.
///
/// Not a `Node` -- see the file comment -- so it carries its own `Citation`
/// rather than being wrapped by `documented()`.
template <Predicate P>
struct Constraint
{
    /// The condition that must hold for this constraint to be satisfied.
    P predicate {};
    /// What the outcome is when `predicate` does not hold.
    Verdict verdict {};
    /// Where this constraint comes from.
    Citation citation {};
};

/// `constraint(predicate, verdict, citation = {})`: a predicate paired with
/// the verdict that applies when it does not hold.
template <Predicate P>
[[nodiscard]] constexpr Constraint<P> constraint(P predicate, Verdict verdict, Citation citation = {}) noexcept
{
    return Constraint<P> { predicate, verdict, citation };
}

/// Checks @p constraint against @p environment, mapping `checked_evaluate_
/// predicate`'s three-state result onto the four `ConstraintOutcomeKind`
/// states: an arithmetic error becomes `Invalid`, an unresolved predicate
/// (an input was never measured) becomes `NotChecked`, a held predicate
/// becomes `Satisfied`, and a predicate that does not hold becomes `Violated`
/// carrying the constraint's own verdict. That mapping is the whole
/// implementation -- see the file comment and `ConstraintOutcome` for why
/// `NotChecked` must never collapse into `Satisfied`.
template <typename Rep = Rational, typename P, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr ConstraintOutcome check(Constraint<P> const& constraint, Env const& environment,
                                                Sink sink = {}) noexcept
{
    std::expected<std::optional<bool>, ArithmeticError> const result =
        checked_evaluate_predicate<Rep>(constraint.predicate, environment, sink);

    if (!result.has_value())
        return ConstraintOutcome::invalid(result.error());

    if (!result->has_value())
        return ConstraintOutcome::not_checked();

    if (**result)
        return ConstraintOutcome::satisfied();

    return ConstraintOutcome::violated(constraint.verdict);
}

} // namespace formula
