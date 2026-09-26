// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Records, the keys that name them, and the context a formula reads them
/// through.
///
/// A **record** is one sample's test as the lab holds it: a key saying which
/// sample and which test, and the environment of values measured or typed in
/// for it. A formula names a record by its **role**, a plain class tag the
/// author declares (`struct Reference {};`), and never by its key: which
/// actual record plays the role is data, decided at run time, while which
/// roles a formula reads from is code. `ThisRecord` is the library's own role,
/// for the specimen being evaluated.
///
/// A **context** binds each role to one record: `record_context(record<ThisRecord>(...),
/// record<Reference>(...))`. It *is* its own record's environment -- it
/// inherits that record's `Environment` type -- so every evaluator in the
/// library, which takes an environment, takes a context unchanged, and reads
/// this record's values from it exactly as it would from the plain
/// environment.
///
/// **Keys are integers, and two strong types.** `SampleId` and `TestId` each
/// wrap a `std::uint64_t`, which is how a lab information system keys its
/// records; turning one into a display name is a report's job. They are two
/// types, neither convertible to the other, so `record_key(test_id(3),
/// sample_id(23))`, with the pair swapped, does not compile. Both
/// constructors are `explicit`, so a bare integer is taken for neither, and
/// `record_key(17, 5)` does not compile either. There is no aggregate
/// `RecordKey { .sample = 17 }` either, because such an aggregate would
/// value-initialize the missing test key to 0, and 0 is a real key.

#include <formula-cpp/environment.hpp>
#include <formula-cpp/method.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>

namespace formula
{

/// The role of the record being evaluated: the first record of every
/// `record_context`, whose environment the context is.
///
/// Declared and never defined: a role is only ever named, never built.
struct ThisRecord;

/// Which sample a record belongs to, as the lab's own system keys it.
class SampleId
{
  public:
    /// The sample keyed @p sampleKey. Explicit, so that a bare integer is
    /// never taken for a sample key.
    constexpr explicit SampleId(std::uint64_t sampleKey) noexcept:
        _value { sampleKey }
    {
    }

    /// The key as the lab's system states it.
    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return _value; }

    /// Equal when the keys are.
    [[nodiscard]] constexpr bool operator==(SampleId const&) const noexcept = default;

  private:
    std::uint64_t _value;
};

/// Which test of a sample a record holds, as the lab's own system keys it.
class TestId
{
  public:
    /// The test keyed @p testKey. Explicit, so that a bare integer is never
    /// taken for a test key.
    constexpr explicit TestId(std::uint64_t testKey) noexcept:
        _value { testKey }
    {
    }

    /// The key as the lab's system states it.
    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return _value; }

    /// Equal when the keys are.
    [[nodiscard]] constexpr bool operator==(TestId const&) const noexcept = default;

  private:
    std::uint64_t _value;
};

/// Which record: one test of one sample.
///
/// Two keys, and both are needed. Two tests of one sample share its sample
/// key, so a record named by its sample alone could be either.
class RecordKey
{
  public:
    /// The record of test @p testKey on sample @p sampleKey. There is no
    /// constructor from one key alone, and none from integers.
    constexpr RecordKey(SampleId sampleKey, TestId testKey) noexcept:
        _sample { sampleKey },
        _test { testKey }
    {
    }

    /// The sample the record belongs to.
    [[nodiscard]] constexpr SampleId sample() const noexcept { return _sample; }

    /// Which of the sample's tests the record holds.
    [[nodiscard]] constexpr TestId test() const noexcept { return _test; }

    /// Equal when both keys are.
    [[nodiscard]] constexpr bool operator==(RecordKey const&) const noexcept = default;

  private:
    SampleId _sample;
    TestId _test;
};

/// The sample keyed @p sampleKey: `sample_id(17)`.
[[nodiscard]] constexpr SampleId sample_id(std::uint64_t sampleKey) noexcept
{
    return SampleId { sampleKey };
}

/// The test keyed @p testKey: `test_id(5)`.
[[nodiscard]] constexpr TestId test_id(std::uint64_t testKey) noexcept
{
    return TestId { testKey };
}

/// The record of test @p testKey on sample @p sampleKey:
/// `record_key(sample_id(17), test_id(5))`. The sample comes first, and a
/// swapped pair does not compile.
[[nodiscard]] constexpr RecordKey record_key(SampleId sampleKey, TestId testKey) noexcept
{
    return RecordKey { sampleKey, testKey };
}

namespace detail
{
    /// Fails to compile when a record's role is not a plain class type.
    ///
    /// A role is `method.hpp`'s tag rule again, for the reason that rule
    /// gives: `void` or `int` names nothing a record can be, and `Reference
    /// const` is a different type from `Reference`, so a context binding both
    /// would escape the duplicate-role check, which compares roles with
    /// `is_same_v`. The rule itself is `isPlainClassTag`, asserted here rather
    /// than restated.
    template <typename Role>
    struct RequirePlainRole
    {
        static_assert(detail::isPlainClassTag<Role>,
                      "formula: this record role is not a plain class type; a role names which record a formula "
                      "reads from, so it must be a class type without const or volatile -- the role appears in "
                      "this diagnostic as the template argument of RequirePlainRole");
        static constexpr bool value = true;
    };

    /// Whether @p Env is a plain `Environment` -- the only thing a record
    /// holds its values in. A context is not one: a record has no records of
    /// its own.
    template <typename Env>
    inline constexpr bool isEnvironment = false;

    template <typename... Entries>
    inline constexpr bool isEnvironment<Environment<Entries...>> = true;

    /// What a record holds its values in.
    template <typename Env>
    concept RecordEnvironment = isEnvironment<Env>;

    /// Fails to compile when `record<Role>(key, values)` is given values that
    /// are not a plain `Environment` -- a `record_context`, above all. A
    /// record has no records of its own: a formula reads another record
    /// through the context, never through a record nested in one.
    template <typename Env>
    struct RequireRecordEnvironment
    {
        static_assert(isEnvironment<Env>,
                      "formula: a record holds its values in an environment(...), and this is not one; a "
                      "record_context cannot be a record's values, since a record has no records of its own -- "
                      "the type appears in this diagnostic as the template argument of RequireRecordEnvironment");
        static constexpr bool value = true;
    };

    /// Fails to compile when the record being evaluated is made unbound.
    ///
    /// An unbound record is a test not done yet, which is ordinary for a
    /// reference and meaningless for the specimen being evaluated: a context
    /// is its own record's environment, and with no record every input would
    /// be absent, while a result held as typed in would still report itself
    /// as a person's entry.
    ///
    /// A cv-qualified `ThisRecord` passes here on purpose: `RequirePlainRole`
    /// already refuses it, and one mistake draws one message.
    template <typename Role>
    struct RequireThisRecordBound
    {
        static_assert(!std::is_same_v<Role, ThisRecord>,
                      "formula: the record being evaluated cannot be unbound; only a record another role plays "
                      "may be, when that test has not been done -- the role appears in this diagnostic as the "
                      "template argument of RequireThisRecordBound");
        static constexpr bool value = true;
    };

    /// @p Env with every entry absent: what an unbound record holds, so that
    /// it has an environment of its type without inventing a single value.
    template <typename... Entries>
    [[nodiscard]] constexpr Environment<Entries...> absent_environment(std::type_identity<Environment<Entries...>>) noexcept
    {
        return Environment<Entries...> { Entries {}... };
    }

    /// Marks the constructor of `Record` that builds an unbound one.
    struct UnboundRecord
    {
    };

    /// The one way to build a bound record: `record<Role>(...)` is its only
    /// caller. A record's key is what a trace will say a foreign value was
    /// read from, so its members are private and set here, never by
    /// aggregate initialization.
    struct RecordAccess
    {
        /// @p Rec, bound to @p recordKey and @p recordEnvironment.
        template <typename Rec, typename Env, typename... Lineage>
        [[nodiscard]] static constexpr Rec bound(RecordKey recordKey, Env recordEnvironment,
                                                 Lineage... lineageKeys) noexcept
        {
            return Rec { recordKey, recordEnvironment, lineageKeys... };
        }
    };
} // namespace detail

/// One record, playing the role @p Role: its key and its environment -- or,
/// when no record plays the role this time, neither.
///
/// **Built only by the library**, by `record<Role>(key, environment)` or
/// `unbound()`. The members are private, and there is no aggregate spelling,
/// so an existing record's key and environment cannot be reassigned apart
/// from each other. Which key goes with which environment is the caller's
/// statement about its own data, and is recorded as stated:
/// `record<Reference>(keyOfOne, environmentOfAnother)` pairs the two exactly
/// as it is told. The library cannot know better, and does not try.
///
/// **An unbound record** is ordinary lab data -- the reference test has not
/// been done yet -- and keeps the record's type, so that one formula needs one
/// context type whether or not the record exists. Only another role's record
/// may be unbound, never `ThisRecord`'s. It has no key. Its
/// environment holds every quantity of @p Env, each absent, so that nothing
/// read from it is ever a number; every library caller asks `is_bound()`
/// before reading either.
///
/// @p Lineage is the record's lineage keys. A record declares none yet.
template <typename Role, detail::RecordEnvironment Env, typename... Lineage>
class Record
{
    static_assert(detail::RequirePlainRole<Role>::value);

  public:
    /// Which record this is, as a formula names it.
    using role = Role;

    /// The environment type the record holds its values in.
    using environment_type = Env;

    /// A record of this type that no actual record plays: no key, and every
    /// value absent. Refused for `ThisRecord` -- see
    /// `detail::RequireThisRecordBound`.
    [[nodiscard]] static constexpr Record unbound() noexcept
    {
        static_assert(detail::RequireThisRecordBound<Role>::value);
        return Record { detail::UnboundRecord {} };
    }

    /// Whether an actual record plays the role.
    [[nodiscard]] constexpr bool is_bound() const noexcept { return _key.has_value(); }

    /// Which record plays the role; empty when none does.
    ///
    /// An unbound record has no key, and none is invented for it -- not a
    /// zero key, which would be a real one. So the answer is an optional,
    /// and asking an unbound record is an ordinary question with an empty
    /// answer, never a precondition to violate.
    [[nodiscard]] constexpr std::optional<RecordKey> key() const noexcept { return _key; }

    /// The record's values, keyed by quantity type. Every one is absent for
    /// an unbound record.
    [[nodiscard]] constexpr Env const& environment() const noexcept { return _environment; }

  private:
    friend struct detail::RecordAccess;

    constexpr Record(RecordKey recordKey, Env recordEnvironment, Lineage... lineageKeys) noexcept:
        _key { recordKey },
        _environment { recordEnvironment },
        _lineage { lineageKeys... }
    {
    }

    constexpr explicit Record(detail::UnboundRecord) noexcept:
        _key { std::nullopt },
        _environment { detail::absent_environment(std::type_identity<Env> {}) },
        _lineage {}
    {
    }

    std::optional<RecordKey> _key;
    // Deliberately no `{}` default member initialiser, the library's rule for
    // a member that holds an expression, a node or an environment -- see
    // `Corrections` (`lookup.hpp`) for what one does to a default-
    // constructibility probe there. Every constructor sets it, so a record
    // never holds an environment it was not given; `unbound()` gives it one
    // whose every value is absent.
    Env _environment;
    std::tuple<Lineage...> _lineage;
};

/// A record playing @p Role, bound to @p recordKey and holding
/// @p recordEnvironment:
/// `record<Reference>(record_key(sample_id(23), test_id(3)), environment(...))`.
///
/// @return a `Record<Role, Env, Lineage...>`. Values that are not a plain
/// `Environment` are refused -- see `detail::RequireRecordEnvironment` --
/// and the refused call then returns a record holding no values at all, so
/// that the refusal is the only message rather than the first of several.
template <typename Role, typename Env, typename... Lineage>
[[nodiscard]] constexpr auto record(RecordKey recordKey, Env recordEnvironment, Lineage... lineageKeys) noexcept
{
    static_assert(detail::RequireRecordEnvironment<Env>::value);
    if constexpr (detail::isEnvironment<Env>)
        return detail::RecordAccess::bound<Record<Role, Env, Lineage...>>(recordKey, recordEnvironment, lineageKeys...);
    else
        return detail::RecordAccess::bound<Record<Role, Environment<>, Lineage...>>(recordKey, Environment<> {},
                                                                                   lineageKeys...);
}

namespace detail
{
    /// Whether @p T is a `Record`.
    template <typename T>
    inline constexpr bool isRecord = false;

    template <typename Role, typename Env, typename... Lineage>
    inline constexpr bool isRecord<Record<Role, Env, Lineage...>> = true;

    /// What a context binds.
    template <typename T>
    concept RecordType = isRecord<T>;

    /// Fails to compile when a context's first record is not this record's.
    ///
    /// The context *is* the environment of its first record, so a context
    /// whose first record plays another role would evaluate every local
    /// input from that record while calling it this one.
    ///
    /// A cv-qualified `ThisRecord` passes here on purpose: `RequirePlainRole`
    /// already refuses it, and removing the qualifier fixes both, so only
    /// that one may report it.
    template <typename FirstRecord>
    struct RequireThisRecordFirst
    {
        static_assert(std::is_same_v<std::remove_cv_t<typename FirstRecord::role>, ThisRecord>,
                      "formula: the first record of a record_context must be record<ThisRecord>(...); the context "
                      "is the environment of the record being evaluated, so that record comes first and the "
                      "others follow by role -- the first record appears in this diagnostic as the template "
                      "argument of RequireThisRecordFirst");

        static constexpr bool value = true;
    };

    /// Fails to compile when a context binds one role to two records.
    ///
    /// A formula names the record it reads from by role, so with two records
    /// for one role it would read from one of them, and which one would be a
    /// guess. `ThisRecord` bound a second time is simply a duplicate role.
    template <typename... Roles>
    struct RequireDistinctRoles
    {
        template <typename Role>
        static constexpr std::size_t occurrences =
            (std::size_t { 0 } + ... + std::size_t { std::is_same_v<Role, Roles> });

        static_assert((... && (occurrences<Roles> == 1)),
                      "formula: this record_context binds the same role to more than one record; a formula names "
                      "the record it reads from by role, so each role must name exactly one -- the roles appear "
                      "in this diagnostic as the template arguments of RequireDistinctRoles");

        static constexpr bool value = true;
    };
} // namespace detail

/// The records a formula may read from: its own, which it *is*, and the
/// others, by role. Inheriting the own record's environment is the design:
/// every evaluator takes an environment, and a context passes for one
/// without any of them changing.
///
/// **A quantity this record does not hold** is refused exactly as it is for
/// a plain environment, by `RequireProvided`, and that message names the
/// environment the context inherits -- `Environment<Measured<Force>, ...>`
/// -- rather than the context, because that environment is the one asked.
///
/// **This record's environment is held twice**: once as the base the
/// evaluators read, and once inside `this_record()`. Both are copies of the
/// one record the context was built from. Assigning an environment to the
/// base through a reference to it -- `static_cast<Environment<...>&>(context)
/// = other` -- would make the two disagree; that is an explicit act on a
/// public base, as assigning `Method::rounding` is, and nothing in the
/// library does it.
template <detail::RecordType ThisRec, detail::RecordType... Others>
class RecordContext: public ThisRec::environment_type
{
    static_assert(detail::RequireThisRecordFirst<ThisRec>::value);
    static_assert(detail::RequireDistinctRoles<typename ThisRec::role, typename Others::role...>::value);

  public:
    /// The context of @p own, reading from @p others by their roles.
    constexpr explicit RecordContext(ThisRec own, Others... others) noexcept:
        ThisRec::environment_type { own.environment() },
        _own { own },
        _others { others... }
    {
    }

    /// Whether this context binds @p Role -- `ThisRecord` included.
    template <typename Role>
    static constexpr bool binds =
        std::is_same_v<Role, typename ThisRec::role> || (... || std::is_same_v<Role, typename Others::role>);

    /// The record being evaluated.
    [[nodiscard]] constexpr ThisRec const& this_record() const noexcept { return _own; }

    /// The record bound to @p Role. `record<ThisRecord>()` is `this_record()`.
    template <typename Role>
        requires binds<Role>
    [[nodiscard]] constexpr auto const& record() const noexcept
    {
        if constexpr (std::is_same_v<Role, typename ThisRec::role>)
            return _own;
        else
            return std::get<index_of<Role>()>(_others);
    }

  private:
    template <typename Role>
    [[nodiscard]] static constexpr std::size_t index_of() noexcept
    {
        std::size_t foundAt = sizeof...(Others);
        std::size_t scanned = 0;
        (((std::is_same_v<Role, typename Others::role> ? (foundAt = scanned) : foundAt), ++scanned), ...);
        return foundAt;
    }

    ThisRec _own;
    std::tuple<Others...> _others;
};

/// The context of @p own, reading from @p others by their roles:
/// `record_context(record<ThisRecord>(...), record<Reference>(...))`.
template <detail::RecordType ThisRec, detail::RecordType... Others>
[[nodiscard]] constexpr RecordContext<ThisRec, Others...> record_context(ThisRec own, Others... others) noexcept
{
    return RecordContext<ThisRec, Others...> { own, others... };
}

} // namespace formula
