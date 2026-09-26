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
/// **A formula reads from another record through a scope**,
/// `from_record<Reference>(expression)`: the expression is evaluated against
/// that record's environment, and the rest of the formula against this
/// record's. A scope evaluated against anything but a context, over a role
/// the context does not bind, or naming `ThisRecord` is refused at compile
/// time, each with one message. A scope over a role bound to an unbound
/// record -- a test not done yet -- is absent, never zero.
///
/// **Traced, but not yet rendered or documented.** A trace records a scope
/// -- through `explain` or a `RecordingSink` -- and every step inside it
/// carries a `RecordOrigin` saying which record it was read from
/// (`trace.hpp`). Until the page learns the scope, `render` and `document`
/// over a formula holding one are refused with the compiler's own errors,
/// not with a message of this library's. That is a refusal, not a silent
/// gap: no page omits a scope it was given.
///
/// **What the library prevents, and what it does not.** Every field a trace
/// reads for an origin comes from one of these:
/// - the role's name, from the role's type, through `tag_name<Role>()`;
/// - the record's key, from the `Record`, which only `record<Role>(...)` and
///   `unbound()` build -- its members are private and it has no aggregate
///   spelling. The key is the caller's statement about its own data, and is
///   recorded as stated;
/// - the `RecordOrigin` itself, which a trace step holds: built only by
///   `detail::RecordOriginAccess::of(record)`, from the one `Record` whose
///   environment the scope's operand is evaluated against. Its public
///   constructor is refused in this library's words
///   (`record_origin_by_hand.cpp`), and its other one is private. The public
///   one is not a template, so it cannot be explicitly specialised to build
///   an origin by other means (`record_origin_specialised.cpp`): the refusal
///   lives in the converting constructor of its first parameter's type, a
///   `detail::` class template, and specialising that means writing
///   `detail::`. That the value and its origin come from the same record is
///   pinned by the trace tests, and by building the origin from this record
///   instead, which they catch.
///
/// Not prevented, and documented here plainly:
/// - `Record::lineage_of<Attr>` is a public member template, and explicitly
///   specialising it for a record type can make a lineage check compare any
///   key at all: outside the contract (see below), and not prevented;
/// - `Trace::steps` is a public arena any code may append to or edit, as it
///   has been since the trace was introduced;
/// - a `RecordOrigin` the library built can be copied, and handed to a
///   sink's `record_entered` by hand;
/// - a `RecordOrigin` is trivially copyable, so `std::bit_cast` from a
///   struct of the same layout -- a `std::string_view` and a
///   `std::optional<RecordKey>` -- produces one holding whatever that struct
///   held. No access check applies to `bit_cast`, and nothing in the
///   language lets a class refuse it while staying trivially copyable;
/// - a caller can wrap a typed-in value as `Measured<Q>`, and the trace will
///   then call it measured;
/// - a role whose `TagName` spells "this record" still renders as `from
///   record this record (sample ...)`: the framing and the key show the
///   value is foreign, but the name misleads.
///
/// The guarantee is about the recording path: the evaluator never attributes
/// a value to a record it did not read it from.
///
/// **Only the documented customization points are supported** -- `TagName`,
/// `EnumeratorName`, `Describe` through `Quantity`, and the others each
/// header names as one. Explicitly specialising any other library template
/// or member is outside the contract: it can make a trace say anything, and
/// no library can prevent it. `RecordOrigin`'s refusing constructor is not a
/// template, so that there is nothing there to specialise, but that is a
/// courtesy for the obvious spelling, not a guarantee against the rest.
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

#include <formula-cpp/constraint.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/lineage.hpp>
#include <formula-cpp/method.hpp>
#include <formula-cpp/overlay.hpp>
#include <formula-cpp/sink.hpp>
#include <formula-cpp/tag.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
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

    /// Whether @p displayed is identifier-like: ASCII letters, digits and
    /// underscores, with single spaces between words and none at either end.
    /// Empty is accepted: `tag_name` answers empty only when the compiler's
    /// signature is not in the shape the library reads, a fault that is the
    /// library's and not the author's (see `RequireReadableTagName`).
    [[nodiscard]] constexpr bool is_identifier_like_role_name(std::string_view displayed) noexcept
    {
        if (displayed.empty())
            return true;
        if (displayed.front() == ' ' || displayed.back() == ' ')
            return false;
        char previous = '\0';
        for (char const nameCharacter: displayed)
        {
            bool const allowed = (nameCharacter >= 'A' && nameCharacter <= 'Z')
                                 || (nameCharacter >= 'a' && nameCharacter <= 'z')
                                 || (nameCharacter >= '0' && nameCharacter <= '9') || nameCharacter == '_'
                                 || nameCharacter == ' ';
            if (!allowed || (nameCharacter == ' ' && previous == ' '))
                return false;
            previous = nameCharacter;
        }
        return true;
    }

    /// Whether @p Role's displayed name is identifier-like; true for a role
    /// that is not a plain class type, which `RequirePlainRole` refuses on its
    /// own, so that its name is not read and refused a second time.
    template <typename Role>
    [[nodiscard]] consteval bool role_name_is_identifier_like() noexcept
    {
        if constexpr (isPlainClassTag<Role>)
            return is_identifier_like_role_name(tag_name<Role>());
        else
            return true;
    }

    /// Fails to compile when a record role's displayed name -- its `TagName`,
    /// or its own name when it has none -- is not identifier-like.
    ///
    /// The name is written into formulas in every dialect (`f_c of
    /// Reference`) and into every trace line read from the record. There, an
    /// operator character would read as arithmetic the method does not do
    /// (`Reference-B` typesets as a subtraction in LaTeX, and `Batch<2>` as
    /// two comparisons), a control character breaks a LaTeX document and a
    /// Markdown page's structure, and a non-ASCII character is silently
    /// dropped by a LaTeX engine whose math font lacks it. Refusing all three
    /// at once keeps every dialect true without escaping each one.
    ///
    /// One message per role, whichever of a record and a scope names it
    /// first: both ask this one class template.
    template <typename Role>
    struct RequireIdentifierLikeRoleName
    {
        static_assert(role_name_is_identifier_like<Role>(),
                      "formula: this record role's displayed name is not identifier-like; a role's name is written "
                      "into formulas and traces in every dialect, so it may hold only ASCII letters, digits, "
                      "underscores and single spaces between words -- an operator character such as - < ' * would "
                      "read as arithmetic, a control character breaks the page, and a non-ASCII character is dropped "
                      "by some LaTeX fonts; the role appears in this diagnostic as the template argument of "
                      "RequireIdentifierLikeRoleName -- specialise formula::TagName for it to spell its name so");
        static constexpr bool value = true;
    };

    /// Whether @p Env is a plain `Environment` -- the only thing a record
    /// holds its values in. A context is not one: a record has no records of
    /// its own.
    template <typename Env>
    inline constexpr bool isEnvironment = false;

    template <typename... Entries>
    inline constexpr bool isEnvironment<Environment<Entries...>> = true;

    /// What a refused `record()` holds when its values were neither an
    /// environment nor a context -- a bare `Measured<Q>` where
    /// `environment(...)` was meant, say. It answers every quantity, each as
    /// absent, and asserts nothing, so that the refusal of the values is the
    /// only message however many quantities a formula then reads through the
    /// record. No record holds one otherwise.
    struct AbsentEnvironment
    {
        /// Every quantity is answered -- absent.
        template <Described Q>
        static constexpr bool provides = true;

        /// Nothing was typed in.
        template <Described Q>
        static constexpr bool is_entered = false;

        /// Absent, for every quantity.
        template <Described Q>
        [[nodiscard]] constexpr Measured<Q> get() const noexcept
        {
            return Measured<Q>::absent();
        }

        /// What `Environment::source_of` answers for a value not typed in.
        template <Described Q>
        [[nodiscard]] constexpr ValueSource source_of() const noexcept
        {
            return ValueSource::Measured;
        }
    };

    /// What a record holds its values in: a plain `Environment`, or, after a
    /// refused `record()`, an `AbsentEnvironment`.
    template <typename Env>
    concept RecordEnvironment = isEnvironment<Env> || std::is_same_v<Env, AbsentEnvironment>;

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

    /// An `AbsentEnvironment` is absent throughout already.
    [[nodiscard]] constexpr AbsentEnvironment absent_environment(std::type_identity<AbsentEnvironment>) noexcept
    {
        return AbsentEnvironment {};
    }

    /// The environment a `RecordContext` inherits, as `type`; nothing for any
    /// other type. Specialized below `RecordContext`, and read only by
    /// `record()` after it has refused a context as a record's values.
    template <typename T>
    struct ContextOwnEnvironment
    {
    };

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
/// `unbound()`. The members are private, there is no aggregate spelling, and
/// no member sets the key or the environment alone; a record is replaced
/// only whole, by assigning another record. Which key goes with which
/// environment is the caller's statement about its own data, and is
/// recorded as stated: `record<Reference>(keyOfOne, environmentOfAnother)`
/// pairs the two exactly as it is told. The library has no way to know
/// better, and does not try.
///
/// **An unbound record** is ordinary lab data -- the reference test has not
/// been done yet -- and keeps the record's type, so that one formula needs one
/// context type whether or not the record exists. Only another role's record
/// may be unbound, never `ThisRecord`'s. It has no key. Its
/// environment holds every quantity of @p Env, each absent, so that nothing
/// read from it is ever a number; every library caller asks `is_bound()`
/// before reading either.
///
/// @p Lineage is the record's lineage keys, `lineage<Attr>(key)` or
/// `unknown_lineage<Attr>()` each (`lineage.hpp`); an unbound record holds
/// every one as unknown. A record that declares an attribute twice is
/// refused, as is anything in the pack that is not a lineage entry -- the
/// latter alone when it applies, since every attribute of such a pack would
/// look like a duplicate.
template <typename Role, detail::RecordEnvironment Env, typename... Lineage>
class Record
{
    static_assert(detail::RequirePlainRole<Role>::value);
    static_assert(detail::RequireIdentifierLikeRoleName<Role>::value);
    static_assert(detail::RequireLineageEntries<Lineage...>::value);
    static_assert(std::conditional_t<(detail::isLineageEntry<Lineage> && ...),
                                     detail::RequireAttributesDeclaredOnce<typename detail::AttributeOf<Lineage>::type...>,
                                     std::true_type>::value);

    /// How many of this record's entries are for @p Attr.
    template <typename Attr>
    static constexpr std::size_t entriesFor =
        (std::size_t { 0 } + ... + std::size_t { std::is_same_v<Attr, typename detail::AttributeOf<Lineage>::type> });

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

    /// Whether this record declares a key for lineage attribute @p Attr.
    template <typename Attr>
    static constexpr bool declares = entriesFor<Attr> != 0;

    /// This record's key for lineage attribute @p Attr; empty when it is not
    /// known, and for an unbound record. Asking for an attribute the record
    /// does not declare is refused, naming the attribute and the role.
    template <typename Attr>
    [[nodiscard]] constexpr std::optional<std::uint64_t> lineage_of() const noexcept
    {
        static_assert(detail::RequireDeclaredAttribute<Attr, Role, declares<Attr>>::value);
        if constexpr (entriesFor<Attr> == 1)
            return std::get<LineageEntry<Attr>>(_lineage).key();
        else
            return std::nullopt;
    }

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
        _lineage { detail::unknown_entry(std::type_identity<Lineage> {})... }
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
/// and the refused call still returns a record, so that the refusal is not
/// followed by errors about a result that does not exist:
/// - given a `record_context`, the record holds that context's own
///   record's environment, the values the context itself evaluates against,
///   so evaluating through it draws nothing more;
/// - given anything else, the record holds a `detail::AbsentEnvironment`,
///   which answers every quantity as absent without refusing it, so a
///   formula evaluated through the record is absent and draws nothing more.
template <typename Role, typename Env, typename... Lineage>
[[nodiscard]] constexpr auto record(RecordKey recordKey, Env recordEnvironment, Lineage... lineageKeys) noexcept
{
    static_assert(detail::RequireRecordEnvironment<Env>::value);
    if constexpr (detail::isEnvironment<Env>)
        return detail::RecordAccess::bound<Record<Role, Env, Lineage...>>(recordKey, recordEnvironment, lineageKeys...);
    else if constexpr (requires { typename detail::ContextOwnEnvironment<Env>::type; })
    {
        using OwnEnvironment = typename detail::ContextOwnEnvironment<Env>::type;
        return detail::RecordAccess::bound<Record<Role, OwnEnvironment, Lineage...>>(
            recordKey, static_cast<OwnEnvironment const&>(recordEnvironment), lineageKeys...);
    }
    else
        return detail::RecordAccess::bound<Record<Role, detail::AbsentEnvironment, Lineage...>>(
            recordKey, detail::AbsentEnvironment {}, lineageKeys...);
}

namespace detail
{
    /// Fails to compile when an author builds a `RecordOrigin`. An origin
    /// makes a trace say a value was read from a particular record; one built
    /// by hand could say it of a record nothing was read from.
    template <typename Given>
    struct RequireLibraryStatesOrigin
    {
        static_assert(!std::is_same_v<Given, Given>,
                      "formula: a record origin is the library's to state, not an author's; the trace takes it "
                      "from the record a from_record<Role> scope reads, and nothing else -- what was given for "
                      "the role appears in this diagnostic as the template argument of "
                      "RequireLibraryStatesOrigin");

        static constexpr bool value = true;
    };

    /// The first parameter of `RecordOrigin`'s refusing constructor: it
    /// converts from anything, and refuses whatever it is given. The refusal
    /// lives here rather than in a constructor template of `RecordOrigin`
    /// because a member template can be explicitly specialised by user code
    /// without naming anything in `detail::` -- its default template argument
    /// is deduced -- and a specialisation is a member, free to set the
    /// private fields. This one can be specialised only by writing
    /// `detail::`.
    template <typename Placeholder>
    struct OriginStatedByHand
    {
        /// Refused: see `RequireLibraryStatesOrigin`.
        template <typename Given>
        constexpr OriginStatedByHand(Given&&) noexcept
        {
            static_assert(RequireLibraryStatesOrigin<Given>::value);
        }
    };

    /// Marks the one constructor of `RecordOrigin` that states an origin.
    struct ReadFromRecord
    {
    };

    struct RecordOriginAccess;
} // namespace detail

/// Which record a value was read from, as a trace step records it: the role's
/// name, whether a record played it, and that record's key.
///
/// **Built only by the library**, from the record a scope reads -- see
/// `detail::RecordOriginAccess`. The public constructor below is refused, so
/// that an origin stated by hand is refused in this library's words. A copy
/// of one the library built travels freely; see the forgery paragraph in
/// this header's file comment for what that does and does not allow.
class RecordOrigin
{
  public:
    /// Refused, by its first parameter's conversion: see
    /// `detail::OriginStatedByHand`. Declared only so that
    /// `RecordOrigin { "Reference", record_key(...), true }` is refused in
    /// this library's words rather than the compiler's. Not a template, so
    /// there is nothing here to specialise; its body sets nothing, and runs
    /// only after the refusal.
    constexpr RecordOrigin(detail::OriginStatedByHand<void>, RecordKey, bool) noexcept {}

    /// The role's name, as `tag_name<Role>()` spells it. Points into static
    /// storage, so it outlives any trace.
    [[nodiscard]] constexpr std::string_view role() const noexcept { return _role; }

    /// Whether a record played the role.
    [[nodiscard]] constexpr bool is_bound() const noexcept { return _key.has_value(); }

    /// Which record played it; empty when none did.
    [[nodiscard]] constexpr std::optional<RecordKey> key() const noexcept { return _key; }

    /// Equal when the role's name and the key are.
    [[nodiscard]] constexpr bool operator==(RecordOrigin const&) const noexcept = default;

  private:
    friend struct detail::RecordOriginAccess;

    constexpr RecordOrigin(detail::ReadFromRecord, std::string_view roleName, std::optional<RecordKey> recordKey) noexcept:
        _role { roleName },
        _key { recordKey }
    {
    }

    std::string_view _role;
    std::optional<RecordKey> _key;
};

namespace detail
{
    /// The one way to build a `RecordOrigin`: from the record itself, never
    /// from its parts, so that its only input is the object a scope's values
    /// are read from. Called only by the scope's evaluator.
    struct RecordOriginAccess
    {
        /// The origin of every value read from @p readFrom.
        template <typename Role, typename Env, typename... Lineage>
        [[nodiscard]] static constexpr RecordOrigin of(Record<Role, Env, Lineage...> const& readFrom) noexcept
        {
            return RecordOrigin { ReadFromRecord {}, tag_name<Role>(), readFrom.key() };
        }
    };

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

    /// Fails to compile when a context is asked for a role it binds no record
    /// to -- by `record<Role>()`, or by a `from_record<Role>` scope evaluated
    /// against it.
    ///
    /// With the role in the type, a formula reading from a role its context
    /// does not bind is refused where it is evaluated, never a run-time miss.
    template <typename Role, typename Context>
    struct RequireBoundRole
    {
        static_assert(Context::template binds<Role>,
                      "formula: this record_context binds no record to this role; a formula that reads from a "
                      "role needs a context that binds one, record<Role>(...) or Record<Role, ...>::unbound() "
                      "when that test has not been done -- the role and the context appear in this diagnostic as "
                      "the template arguments of RequireBoundRole");

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
    ///
    /// A role this context does not bind is refused in this library's words,
    /// by `detail::RequireBoundRole`, and not by overload resolution. So
    /// `requires { context.template record<Cube>(); }` is true for every
    /// role, and the call itself then fails to compile: ask `binds<Role>` to
    /// learn whether a role is bound. After the refusal the call returns
    /// `this_record()`, so that code using the result draws nothing more.
    template <typename Role>
    [[nodiscard]] constexpr auto const& record() const noexcept
    {
        static_assert(detail::RequireBoundRole<Role, RecordContext>::value);
        if constexpr (!binds<Role> || std::is_same_v<Role, typename ThisRec::role>)
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

namespace detail
{
    template <typename ThisRec, typename... Others>
    struct ContextOwnEnvironment<RecordContext<ThisRec, Others...>>
    {
        using type = typename ThisRec::environment_type;
    };

    /// The `RecordContext` an environment is, as `type`, and the way to reach
    /// it, `of(environment)`; nothing for any other type.
    ///
    /// A scope's evaluator recognises a context only through this trait, and
    /// never by looking for a `record<Role>()` member: a type of the
    /// consumer's own that merely has one must not be able to supply the
    /// record a foreign value is read from. A type that wraps a context --
    /// the environment a retry evaluates against, say -- specializes this to
    /// reach the context it wraps; until one does, a scope evaluated against
    /// it is refused, which is safe.
    template <typename Env>
    struct RecordContextOf
    {
    };

    template <typename ThisRec, typename... Others>
    struct RecordContextOf<RecordContext<ThisRec, Others...>>
    {
        /// The context itself.
        using type = RecordContext<ThisRec, Others...>;

        /// @p recordContext, unchanged.
        [[nodiscard]] static constexpr type const& of(type const& recordContext) noexcept { return recordContext; }
    };

    /// Whether @p Env is, or reaches, a `RecordContext`.
    template <typename Env>
    inline constexpr bool reachesRecordContext = requires { typename RecordContextOf<Env>::type; };

    /// Fails to compile when a formula that reads from another record is
    /// evaluated against an environment that holds only one.
    template <typename Env>
    struct RequireRecordContext
    {
        static_assert(!std::is_same_v<Env, Env>,
                      "formula: this formula reads from another record, but was evaluated against an environment "
                      "that holds only one; evaluate it against a record_context(...) that binds the role -- the "
                      "environment appears in this diagnostic as the template argument of RequireRecordContext");

        static constexpr bool value = true;
    };

    /// Fails to compile when a scope names `ThisRecord`.
    ///
    /// Reading this record "from another record" is a mislabel waiting to
    /// appear in a trace: the value is this record's, and a plain `var<Q>`
    /// reads it. A cv-qualified `ThisRecord` is refused here too, since no
    /// context binds one and it can only mean this record.
    template <typename Role>
    struct RequireForeignRole
    {
        static_assert(!std::is_same_v<std::remove_cv_t<Role>, ThisRecord>,
                      "formula: from_record<ThisRecord> reads this record as though it were another; read this "
                      "record's values with var<Q> directly, and name another role in from_record -- the role "
                      "appears in this diagnostic as the template argument of RequireForeignRole");

        static constexpr bool value = true;
    };

    /// Whether @p Role names this record, cv-qualified or not.
    template <typename Role>
    inline constexpr bool namesThisRecord = std::is_same_v<std::remove_cv_t<Role>, ThisRecord>;

    /// The requirement of a scope that checks no lineage.
    struct NoLineageRequirement
    {
    };

    /// The role a scope's lineage requirement compares with; `void` when it
    /// has none.
    template <typename Requirement>
    struct ComparandOf
    {
        using type = void;
    };

    template <typename Comparand, typename... Attrs>
    struct ComparandOf<LineageRequirement<Comparand, Attrs...>>
    {
        using type = Comparand;
    };

    /// Fails to compile when a scope's requirement is neither a lineage
    /// requirement nor none -- a type spelt into `RecordScopeNode` by hand.
    template <typename Requirement>
    struct RequireLineageRequirement
    {
        static_assert(isLineageRequirement<Requirement> || std::is_same_v<Requirement, NoLineageRequirement>,
                      "formula: a record scope's requirement is not a lineage requirement; write same_lineage<...>() "
                      "or leave it out -- the type appears in this diagnostic as the template argument of "
                      "RequireLineageRequirement");

        static constexpr bool value = true;
    };

    /// Fails to compile when a lineage requirement compares the record a
    /// scope reads with itself: `against<Role>` naming the scope's own role
    /// is always satisfied, so it checks nothing while reading as a check.
    template <typename Role, typename Comparand>
    struct RequireComparandNotSubject
    {
        static_assert(!std::is_same_v<Role, Comparand>,
                      "formula: this lineage requirement compares the record it reads with itself, which always "
                      "agrees; name another role in against<Role>, or leave it out to compare with this record -- "
                      "the role appears in this diagnostic as the template argument of RequireComparandNotSubject");

        static constexpr bool value = true;
    };

    /// Refuses one attribute a requirement compares if either record does not
    /// declare it -- the read record first, and the compared one only when
    /// the read record does declare it, so that an attribute neither declares
    /// draws one message, not one per record.
    template <typename Attr, typename Subject, typename Comparand>
    struct RequireDeclaredOnBoth
    {
        using Checked = std::conditional_t<Subject::template declares<Attr>,
                                           RequireDeclaredAttribute<Attr, typename Comparand::role,
                                                                    Comparand::template declares<Attr>>,
                                           RequireDeclaredAttribute<Attr, typename Subject::role, false>>;

        static constexpr bool value = Checked::value;
    };

    /// Whether both records declare every attribute a requirement compares.
    /// Asking refuses nothing: the refusal is `RefuseUndeclaredLineage`, a
    /// template of its own, since a static member beside `all` would be
    /// instantiated -- and refuse -- whenever `all` is merely read.
    template <typename Requirement, typename Subject, typename Comparand>
    struct LineageDeclared
    {
        static constexpr bool all = true;
    };

    template <typename Comparand, typename... Attrs, typename Subject, typename ComparandRecord>
    struct LineageDeclared<LineageRequirement<Comparand, Attrs...>, Subject, ComparandRecord>
    {
        static constexpr bool all =
            (... && (Subject::template declares<Attrs> && ComparandRecord::template declares<Attrs>));
    };

    /// Refuses every attribute a requirement compares that either record
    /// does not declare -- see `RequireDeclaredOnBoth`.
    template <typename Requirement, typename Subject, typename Comparand>
    struct RefuseUndeclaredLineage
    {
        static constexpr bool value = true;
    };

    template <typename Comparand, typename... Attrs, typename Subject, typename ComparandRecord>
    struct RefuseUndeclaredLineage<LineageRequirement<Comparand, Attrs...>, Subject, ComparandRecord>
    {
        static constexpr bool value = (... && RequireDeclaredOnBoth<Attrs, Subject, ComparandRecord>::value);
    };

    /// Fails to compile when a sink defines one of `record_entered` and
    /// `lineage_checked` without the other. Told of neither, such a sink's
    /// trace would show a refused read with no origin and no attribute that
    /// refused it; told of one, it would show half of that. A sink defines
    /// both or neither.
    template <typename Sink, bool Together>
    struct RequireScopeHooksTogether
    {
        static_assert(Together,
                      "formula: this sink defines only one of record_entered and lineage_checked; a scope tells a "
                      "sink of the record it reads and of every lineage attribute it compares together, so define "
                      "both or neither -- the sink appears in this diagnostic as the template argument of "
                      "RequireScopeHooksTogether");

        static constexpr bool value = true;
    };

    /// What comparing a requirement's attributes decided.
    enum class LineageVerdict : std::uint8_t
    {
        /// Every attribute known on both sides, and equal.
        Agreed,
        /// None violated, but at least one unknown on a side.
        NotChecked,
        /// At least one known on both sides and different.
        Violated,
    };

    /// Compares every attribute @p Attrs of @p subject with @p comparandRecord,
    /// in declared order, and tells @p sink of each when @p Reports. Every
    /// attribute is compared and reported, not only those up to the first
    /// mismatch, so that a trace shows every attribute that disagrees.
    template <bool Reports, typename Comparand, typename... Attrs, typename Subject, typename ComparandRecord,
              typename Sink>
    [[nodiscard]] constexpr LineageVerdict check_lineage(LineageRequirement<Comparand, Attrs...>, Subject const& subject,
                                                         ComparandRecord const& comparandRecord, Sink& sink) noexcept
    {
        bool anyViolated = false;
        bool anyUnknown = false;
        (
            [&] {
                LineageCheck const attributeCheck = LineageCheckAccess::of<Attrs>(subject, comparandRecord);
                std::optional<std::uint64_t> const subjectKey = attributeCheck.subject_key();
                std::optional<std::uint64_t> const comparandKey = attributeCheck.comparand_key();
                ConstraintOutcome attributeOutcome = ConstraintOutcome::not_checked();
                if (subjectKey.has_value() && comparandKey.has_value())
                    attributeOutcome = *subjectKey == *comparandKey ? ConstraintOutcome::satisfied()
                                                                    : ConstraintOutcome::violated(Verdict { "violated" });
                anyViolated = anyViolated || attributeOutcome.is_violated();
                anyUnknown = anyUnknown || attributeOutcome.is_not_checked();
                if constexpr (Reports)
                    sink.lineage_checked(attributeCheck, attributeOutcome);
            }(),
            ...);
        if (anyViolated)
            return LineageVerdict::Violated;
        return anyUnknown ? LineageVerdict::NotChecked : LineageVerdict::Agreed;
    }
} // namespace detail

/// A read from another record: @p Operand, evaluated against the environment
/// of the record the context binds to @p Role.
///
/// Its dimension is its operand's: reading a value from another record does
/// not change what it measures.
///
/// `from_record<ThisRecord>` is refused in the class body, so that an
/// aggregate spelling is refused as well as the factory -- see
/// `detail::RequireForeignRole`.
template <typename Role, typename Requirement, Node Operand>
struct RecordScopeNode: NodeBase
{
    static_assert(detail::RequireForeignRole<Role>::value);
    static_assert(detail::RequireIdentifierLikeRoleName<Role>::value);
    static_assert(detail::RequireLineageRequirement<Requirement>::value);
    static_assert(detail::RequireComparandNotSubject<Role, typename detail::ComparandOf<Requirement>::type>::value);

    /// The role of the record the operand is read from.
    using role = Role;
    /// What the scope requires of the two records' lineage before it reads.
    using requirement = Requirement;

    /// Forwarded from `Operand` unchanged.
    static constexpr Dimension dimension = Operand::dimension;

    /// The expression evaluated against the other record's environment.
    /// Deliberately no `{}` default member initialiser: see `Corrections`
    /// (`lookup.hpp`).
    Operand operand;
};

/// Reads @p operand from the record the context binds to @p Role:
/// `from_record<Reference>(var<Strength>)`, or a whole computation,
/// `from_record<Reference>(var<Force> / (var<EdgeX> * var<EdgeY>))`.
template <typename Role, Node Operand>
[[nodiscard]] constexpr RecordScopeNode<Role, detail::NoLineageRequirement, Operand> from_record(Operand operand) noexcept
{
    return RecordScopeNode<Role, detail::NoLineageRequirement, Operand> { {}, operand };
}

/// Reads @p operand from the record the context binds to @p Role, only if
/// that record agrees with the compared one on every attribute the
/// requirement names:
/// `from_record<Reference>(var<Strength>, same_lineage<MaterialBatch, TestMethod>())`.
/// See `lineage.hpp` for what agreement, disagreement and an unknown key
/// each give.
template <typename Role, Node Operand, typename Comparand, typename... Attrs>
[[nodiscard]] constexpr RecordScopeNode<Role, LineageRequirement<Comparand, Attrs...>, Operand>
from_record(Operand operand, LineageRequirement<Comparand, Attrs...>) noexcept
{
    return RecordScopeNode<Role, LineageRequirement<Comparand, Attrs...>, Operand> { {}, operand };
}

/// Evaluates a scope: its operand against the environment of the record
/// @p environment binds to the scope's role.
///
/// - The role's record unbound -- a test not done yet: absent, never zero.
/// - Otherwise the operand's value, computed wholly from that record.
///
/// The value and the record it is read from are the one `foreignRecord`
/// below, so a value can never be attributed to a record it was not read
/// from.
///
/// With a lineage requirement, every attribute is compared first, before the
/// operand is read, and reported to a sink that asks: any violated gives
/// `DomainError`, otherwise any unknown gives absent, and only agreement
/// reads the operand. An unbound record compares nothing.
///
/// A sink is told of the origin and of each attribute through
/// `record_entered` and `lineage_checked` together: a sink defines both or
/// neither, and one defining only one of them is told of neither.
///
/// Refused, each with one message and without evaluating the operand, so
/// that no second message follows from it:
/// - @p environment is not a `record_context` (`detail::RequireRecordContext`).
///   A scope nested in a scope meets this too, since the inner one is
///   evaluated against a record's plain environment;
/// - the context binds no record to the role, or none to the role a
///   lineage requirement compares with (`detail::RequireBoundRole`);
/// - the role is `ThisRecord`, refused by the node itself
///   (`detail::RequireForeignRole`);
/// - a compared attribute either record does not declare
///   (`detail::RequireDeclaredAttribute`).
template <typename Rep = Rational, typename Role, typename Requirement, Node Operand, typename Env, typename Sink = NullSink>
[[nodiscard]] constexpr Evaluated<Rep> checked_evaluate_si(RecordScopeNode<Role, Requirement, Operand> const& node,
                                                           Env const& environment,
                                                           Sink sink = {}) noexcept
{
    if constexpr (!detail::reachesRecordContext<Env>)
    {
        static_assert(detail::RequireRecordContext<Env>::value);
        return detail::nothing<Rep>();
    }
    else if constexpr (detail::namesThisRecord<Role>)
        // Already refused by the node's own class body.
        return detail::nothing<Rep>();
    else
    {
        using Context = typename detail::RecordContextOf<Env>::type;
        using ComparandRole = typename detail::ComparandOf<Requirement>::type;
        constexpr bool requiresLineage = detail::isLineageRequirement<Requirement>;
        if constexpr (!Context::template binds<Role>)
        {
            static_assert(detail::RequireBoundRole<Role, Context>::value);
            return detail::nothing<Rep>();
        }
        else if constexpr (requiresLineage && !Context::template binds<ComparandRole>)
        {
            static_assert(detail::RequireBoundRole<ComparandRole, Context>::value);
            return detail::nothing<Rep>();
        }
        else
        {
            using Subject = std::remove_cvref_t<decltype(std::declval<Context const&>().template record<Role>())>;
            using ComparandRecord =
                std::remove_cvref_t<decltype(std::declval<Context const&>().template record<
                                             std::conditional_t<requiresLineage, ComparandRole, Role>>())>;
            if constexpr (!detail::LineageDeclared<Requirement, Subject, ComparandRecord>::all)
            {
                static_assert(detail::RefuseUndeclaredLineage<Requirement, Subject, ComparandRecord>::value);
                return detail::nothing<Rep>();
            }
            else
            {
                Context const& recordContext = detail::RecordContextOf<Env>::of(environment);
                sink.entered(node);
                auto const& foreignRecord = recordContext.template record<Role>();
                // The origin, from the same record the operand is read from
                // below, before anything inside the scope is recorded.
                constexpr bool knowsOrigin = requires(RecordOrigin const& openedFrom) { sink.record_entered(openedFrom); };
                constexpr bool knowsLineage =
                    requires(LineageCheck const& attributeCheck, ConstraintOutcome const& attributeOutcome) {
                        sink.lineage_checked(attributeCheck, attributeOutcome);
                    };
                static_assert(detail::RequireScopeHooksTogether<Sink, knowsOrigin == knowsLineage>::value);
                // After a refusal, neither: the refusal is the only message.
                constexpr bool reports = knowsOrigin && knowsLineage;
                if constexpr (reports)
                    sink.record_entered(detail::RecordOriginAccess::of(foreignRecord));

                detail::LineageVerdict lineageVerdict = detail::LineageVerdict::Agreed;
                if constexpr (requiresLineage)
                    if (foreignRecord.is_bound())
                        lineageVerdict = detail::check_lineage<reports>(
                            Requirement {}, foreignRecord, recordContext.template record<ComparandRole>(), sink);

                Evaluated<Rep> scopeValue = detail::nothing<Rep>();
                if (foreignRecord.is_bound())
                {
                    if (lineageVerdict == detail::LineageVerdict::Violated)
                        scopeValue = std::unexpected { ArithmeticError::DomainError };
                    else if (lineageVerdict == detail::LineageVerdict::Agreed)
                        scopeValue = detail::dispatch<Rep>(node.operand, foreignRecord.environment(), sink);
                }
                sink.produced(node, scopeValue);
                return scopeValue;
            }
        }
    }
}

namespace detail
{
    /// An overlay's constant or derived quantity rewrites through a read from
    /// another record exactly as through any other node with one operand:
    /// the operand is rewritten, and the scope rebuilt around it with the
    /// same role and the same lineage requirement.
    ///
    /// A jurisdiction's constant is the method's constant, and a scope
    /// evaluates the method's algebra over another record's measurements, so
    /// a shape factor in that algebra is the same shape factor (phase 14's
    /// X10 ruling). Without this, the primary template would refuse every
    /// overlay over a method holding a scope, with `RequireOverlaySeesNode`.
    /// A use of the quantity inside the scope counts as a use for the
    /// overlay's own checks, and an `OverriddenConstant` step recorded there
    /// is stamped with the scope's origin, so the trace says the overlay
    /// fixed a value inside the computation over that record.
    ///
    /// Declared here, after `overlay.hpp` is included, and found by `apply()`
    /// all the same -- measured by phase 14's spike on cl, clang-cl,
    /// clang++ and g++.
    template <typename Sub, typename Role, typename Requirement, Node Operand>
    struct ConstantRewrite<Sub, RecordScopeNode<Role, Requirement, Operand>>:
        ConstantRewriteOperand<Sub, Operand,
                               RecordScopeNode<Role, Requirement, typename ConstantRewriteOf<Sub, Operand>::type>>
    {
    };
} // namespace detail

/// The context of @p own, reading from @p others by their roles:
/// `record_context(record<ThisRecord>(...), record<Reference>(...))`.
template <detail::RecordType ThisRec, detail::RecordType... Others>
[[nodiscard]] constexpr RecordContext<ThisRec, Others...> record_context(ThisRec own, Others... others) noexcept
{
    return RecordContext<ThisRec, Others...> { own, others... };
}

} // namespace formula
