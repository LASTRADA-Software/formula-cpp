// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Quantities: a variable's identity, carrying its own documentation.

#include <formula-cpp/detail/fixed_string.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/unit.hpp>

#include <concepts>
#include <string_view>
#include <type_traits>
#include <utility>

namespace formula
{

/// A quantity: a variable's identity, carrying its own metadata.
///
/// Declared in either of two spellings, and both are supported wherever a
/// quantity is named, mixed in one formula if need be. The alias is the
/// shorter, and the one the guides and examples lead with:
///
///     using Rise = formula::Quantity<struct RiseTag,  // a tag of its own
///                                    "h",
///                                    "height gained",
///                                    formula::unit::Millimetre>;
///
/// The struct derives a type of its own, and gives that type's own name back
/// to it as the tag:
///
///     struct Rise:
///         formula::Quantity<Rise, "h", "height gained", formula::unit::Millimetre>
///     {
///     };
///
/// **The tag is what makes a quantity distinct.** Two quantities whose symbol,
/// description and unit coincide are two types as long as their tags differ,
/// and neither is accepted where the other is expected. In the alias form,
/// `struct RiseTag` in the argument list declares the tag, an
/// incomplete class that is never defined and need not be, in the nearest
/// enclosing namespace or block -- inside a class, that is the namespace
/// around the class, not the class. `using Rise =
/// Quantity<Rise, ...>` does not compile, since an alias cannot name
/// itself: an alias needs a second name for its tag. In the struct form the
/// type is its own tag, which also keeps the bases of two quantities distinct,
/// so a function taking one quantity's base cannot accept another's.
///
/// **What to know about the alias form:**
///
/// - **An alias cannot be forward-declared; a struct can.** A header that only
///   names a quantity -- a function declaration taking `Measured<Q>` -- can
///   say `struct Rise;` for a struct quantity, and must include the
///   alias's declaration.
/// - **Two aliases with all their arguments equal are one type.** `using A =
///   Quantity<ATag, "L", "a length", unit::Millimetre>;` and a `using B` with the
///   same arguments declare one quantity under two names, and nothing
///   can object: there is only one type, and naming it twice is not an error
///   anywhere in C++. Give every alias its own tag. Two structs never
///   collapse this way, whatever their bases.
/// - **A tag shared by two quantities is harmless while any other argument
///   differs.** The two are still two distinct types, and nothing in the
///   library reads the tag on its own. That is what happens in an alias
///   template that declares its tag inside itself: every instantiation names
///   the same tag, and each is its own quantity as long as the arguments
///   differ, as they do here by unit:
///
///       template <Unit U>
///       using LengthIn = formula::Quantity<struct LengthInTag, "L", "a length", U>;
///
///   To give each instantiation a tag of its own, make the tag depend on what
///   the other arguments depend on:
///
///       template <Unit U>
///       struct LengthInTag;
///       template <Unit U>
///       using LengthIn = formula::Quantity<LengthInTag<U>, "L", "a length", U>;
///
/// - **A diagnostic may name the specialisation rather than the alias.** g++
///   and clang print the whole specialisation, tag first --
///   `Quantity<RiseTag, FixedString<2>{"h"}, ..., Unit{...}>`. cl
///   usually keeps the alias's name where the alias was written, in the
///   library's own messages among them, but not always. All three print a
///   struct quantity's own name. Naming a tag after its quantity --
///   `RiseTag` -- is what keeps such a diagnostic readable.
///
/// Distinctness across translation units is verified by linking, for both
/// spellings.
///
/// **There is no dimension parameter.** A `Unit` already carries its dimension,
/// so passing both would state it twice and let the two contradict each other.
/// That spelling, with `dim::Mass` against `unit::Litre`, compiled without a
/// diagnostic on every compiler it was tried on. `dimension` below is derived,
/// so the contradiction cannot be written.
///
/// **A quantity may declare its own decimal places.** The fifth parameter,
/// `Places`, defaults to the places `U` declares. Given, it replaces them for
/// this quantity alone, so two quantities in one unit can be read to different
/// precision without a second unit:
///
///     struct FineCurrent:
///         formula::Quantity<FineCurrent, "I_f", "a current read to a tenth of a milliampere", Milliampere,
///                           formula::DecimalPlaces { 1 }>
///     {
///     };
///
/// with `Milliampere` declared to whole milliamperes. Everything that reads a
/// quantity's declared places -- `checked_round_to_declared`, `number_text`,
/// `std::format`, a trace -- reads them off `unit`, which carries `Places`.
/// `FineCurrent::unit == Milliampere` is then false, since `==` compares every
/// member; `same_unit(FineCurrent::unit, Milliampere)` is true.
template <typename Tag,
          detail::FixedString Symbol,
          detail::FixedString Description,
          Unit U,
          DecimalPlaces Places = declared_decimals(U)>
struct Quantity
{
    /// The quantity's own type, for anything that needs to name it.
    using QuantityTag = Tag;

    /// How the quantity is written in a formula.
    static constexpr std::string_view symbol = Symbol.view();

    /// What the quantity means, in words, for generated documentation.
    static constexpr std::string_view description = Description.view();

    /// The unit its values are expressed in: `U`, with its declared places
    /// replaced when the quantity declares its own.
    static constexpr Unit unit = detail::with_decimals(U, Places);

    /// Derived from the unit -- see the note above about why it is not a
    /// parameter of its own.
    static constexpr Dimension dimension = U.dimension;
};

namespace detail
{
    /// Never defined, never called. Its only job is to let template argument
    /// deduction find a type's `Quantity` specialisation -- the type itself,
    /// for an alias, or its base, through the derived-to-base conversion, for
    /// a struct -- which is how `Describe` learns a type's metadata without the
    /// type having to repeat it. Measured on cl, clang-cl and clang++.
    template <typename Tag, FixedString Symbol, FixedString Description, Unit U, DecimalPlaces Places>
    auto quantity_base_of(Quantity<Tag, Symbol, Description, U, Places> const&)
        -> Quantity<Tag, Symbol, Description, U, Places>;

    template <typename T>
    concept DeclaresQuantity = requires(T const& value) { quantity_base_of(value); };
} // namespace detail

/// The one place anything reads a quantity's metadata.
///
/// Nothing above this layer knows how the metadata was declared. A type of ours
/// gets it from its `Quantity` specialisation, which an alias names and a struct
/// derives from; a type nobody owns -- a `double`, something from a vendor SDK
/// -- gets it from an explicit specialisation of this template. Both are read
/// the same way, which is what lets a foreign type be used in a formula without
/// owning its source.
///
/// Specialise it like this:
///
///     template <>
///     struct formula::Describe<TheirType>
///     {
///         static constexpr std::string_view symbol = "theta";
///         static constexpr std::string_view description = "...";
///         static constexpr Unit unit = formula::unit::Celsius;
///         static constexpr Dimension dimension = formula::unit::Celsius.dimension;
///     };
/// **Empty on purpose.** It would be nicer for this to `static_assert` with a
/// helpful message, and that was the first design; it is wrong. A
/// `static_assert` failure is not in the immediate context, so it is a hard
/// error rather than a substitution failure -- which means the `Described`
/// concept below, whose whole job is to answer "is this type described?",
/// would fail to COMPILE for every type that is not, instead of answering no.
/// Measured on clang: `static_assert(!Described<int>)` did not compile.
///
/// So the primary stays empty, `Described` works by member detection, and the
/// helpful diagnostic lives in `RequireDescribed` below, where it can be asked
/// for deliberately.
template <typename T>
struct Describe
{
};

/// The specialisation used automatically for a type declared through
/// `Quantity`, as an alias of it or a struct derived from it -- reads the
/// metadata straight off that specialisation rather than asking the type to
/// repeat it.
template <detail::DeclaresQuantity T>
struct Describe<T>
{
  private:
    using Base = decltype(detail::quantity_base_of(std::declval<T const&>()));

  public:
    /// How the quantity is written in a formula.
    static constexpr std::string_view symbol = Base::symbol;
    /// What the quantity means, in words.
    static constexpr std::string_view description = Base::description;
    /// The unit its values are expressed in.
    static constexpr Unit unit = Base::unit;
    /// What the quantity measures.
    static constexpr Dimension dimension = Base::dimension;
};

/// True when `T` has metadata, however it was declared.
///
/// Answers rather than explodes for a type that has none -- see the note on the
/// primary template above for why that took a redesign.
///
/// All four members `Measured` and `checked_convert_to` actually read, not
/// merely two of them. Were the concept to ask only for `symbol` and `unit`, a
/// type specialising those two would pass it, pass `RequireDescribed`, and
/// only then fail deep inside `checked_convert_to` with the compiler's own "no
/// member named 'dimension'" -- exactly the raw diagnostic `RequireDescribed`
/// exists to replace, delivered from the one place it is supposed to be caught
/// first. Measured: a `Describe` specialisation naming only `symbol` and
/// `unit` satisfies a two-member concept and breaks only three calls deeper.
template <typename T>
concept Described = requires {
    { Describe<T>::symbol } -> std::convertible_to<std::string_view>;
    { Describe<T>::description } -> std::convertible_to<std::string_view>;
    { Describe<T>::unit } -> std::convertible_to<Unit>;
    { Describe<T>::dimension } -> std::convertible_to<Dimension>;
};

/// True when a described type's own `dimension` agrees with its `unit`'s.
///
/// A type declared through `Quantity` can never fail this: `Quantity`
/// derives `dimension` from `unit`, so there is no second place for it to
/// disagree with (see `Quantity`'s note on the dimension). A `Describe`
/// specialisation for a foreign type states both independently, so nothing
/// stops them from disagreeing --
/// measured, a specialisation naming `unit::Litre` (a volume) alongside
/// `dim::Mass` compiles in silence and mislabels every value read through it.
/// `Described<T> &&` short-circuits the second clause for an undescribed `T`,
/// so this never triggers the raw "no member" error `Described` itself is
/// built to avoid.
template <typename T>
concept DescribesConsistentDimension = Described<T> && Describe<T>::dimension == Describe<T>::unit.dimension;

namespace detail
{
    /// `RequireNamedScaledScalar` of a described type's unit, asked only once the type is described: an
    /// undescribed type has no unit to ask about, and is already refused, in full, by `RequireDescribed`.
    template <typename T, bool IsDescribed = Described<T>>
    struct RequireDescribedUnitNamesItsScale: std::true_type
    {
    };

    template <typename T>
    struct RequireDescribedUnitNamesItsScale<T, true>: RequireNamedScaledScalar<Describe<T>::unit>
    {
    };

    /// `RequireAsciiKey` of a described type's unit, asked only once the type is described, as above.
    template <typename T, bool IsDescribed = Described<T>>
    struct RequireDescribedUnitHasAsciiKey: std::true_type
    {
    };

    template <typename T>
    struct RequireDescribedUnitHasAsciiKey<T, true>: RequireAsciiKey<Describe<T>::unit>
    {
    };
} // namespace detail

/// Fails to compile, in our own words, when `T` declares no metadata,
/// declares metadata whose `dimension` contradicts its own `unit`, declares
/// it in a dimensionless unit with a scale and no symbol, or declares it in a
/// unit whose symbol is not ASCII and that has no ASCII key.
///
/// The counterpart to `Describe` being silent: somewhere has to say what to do
/// about it, and a bare "no member named 'symbol'" does not. Same shape as
/// `RequireSameDimension` in `dimension.hpp`, and the same caveat applies --
/// the assertion is in the class body, so it fires when the type is COMPLETED.
/// Write `RequireDescribed<T>::value`; a bare alias instantiates nothing and
/// checks nothing.
template <typename T>
struct RequireDescribed
{
    static_assert(Described<T>,
                  "formula: this type does not declare any quantity metadata. Either declare it as "
                  "formula::Quantity<struct TheTypeTag, symbol, description, unit>, or derive it from "
                  "formula::Quantity<TheType, symbol, description, unit>, or -- if it is a type you do "
                  "not own -- specialise formula::Describe for it");

    // Only meaningful once Described<T> holds; `!Described<T> ||` keeps this
    // from ever adding a second, misleading message to a type that is simply
    // undescribed -- that case is already reported, in full, above.
    static_assert(!Described<T> || DescribesConsistentDimension<T>,
                  "formula: this type's declared dimension does not match its unit's dimension. "
                  "A quantity's unit already carries a dimension; declaring a second one that "
                  "disagrees mislabels every value read through it -- derive Describe<T>::dimension "
                  "from Describe<T>::unit.dimension instead of stating it independently");
    static_assert(detail::RequireDescribedUnitNamesItsScale<T>::value);
    static_assert(detail::RequireDescribedUnitHasAsciiKey<T>::value);

    /// Always `true` once reached -- the `static_assert`s above already failed
    /// compilation otherwise. Present so `::value` is the spelling that
    /// instantiates the class template; see the class comment for why that
    /// spelling matters.
    static constexpr bool value = true;
};

} // namespace formula
