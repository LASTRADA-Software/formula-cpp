// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Quantities: a variable's identity, carrying its own documentation.

#include <formula-cpp/detail/fixed_string.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/unit.hpp>

#include <concepts>
#include <string_view>
#include <utility>

namespace formula
{

/// The base a quantity type derives from, carrying that quantity's metadata.
///
/// Declared like this:
///
///     struct WaterVolume:
///         formula::Quantity<WaterVolume,                    // the type's own name
///                           "V_w",
///                           "volume of the effective mixing water",
///                           formula::unit::Litre>
///     {
///     };
///
/// **The tag is first, and it earns its place — but not for the reason it is
/// tempting to give.** It is the type's own name, given back to itself.
///
/// The tempting claim is that without it, two quantities whose symbol,
/// description and unit coincide would be the same type. For the spelling above
/// that is simply false, and a reviewer caught it being asserted here. Measured,
/// with the tag removed from a stand-in template:
///
///     struct WaterVolume: NoTag<"V", "a volume", unit::Litre> {};
///     struct CementVolume: NoTag<"V", "a volume", unit::Litre> {};
///     is_same_v<WaterVolume, CementVolume>            -> false
///
/// C++ types are nominal, so two separately declared structs are distinct
/// however identical their bases. What the tag actually buys is two narrower
/// things, both real:
///
///     using WaterVolume = NoTag<"V", "a volume", unit::Litre>;
///     using CementVolume = NoTag<"V", "a volume", unit::Litre>;
///     is_same_v<WaterVolume, CementVolume>            -> TRUE
///
/// First, it makes that alias spelling impossible to write by accident: you
/// cannot name two distinct quantities without giving them distinct tags.
/// Second, it keeps the BASES distinct. Without it both derive from one base,
/// so a function taking that base accepts either — measured, a base-taking
/// function accepted both of the derived types above.
///
/// Distinctness across translation units is separately verified by linking.
///
/// **There is no dimension parameter.** A `Unit` already carries its dimension,
/// so passing both would state it twice and let the two contradict each other.
/// A spike compiled that spelling with `dim::Mass` against `unit::Litre` and all
/// three compilers accepted it in silence. `dimension` below is derived, so the
/// contradiction cannot be written.
template <typename Tag, detail::FixedString Symbol, detail::FixedString Description, Unit U>
struct Quantity
{
    /// The quantity's own type, for anything that needs to name it.
    using QuantityTag = Tag;

    /// How the quantity is written in a formula.
    static constexpr std::string_view symbol = Symbol.view();

    /// What the quantity means, in words, for generated documentation.
    static constexpr std::string_view description = Description.view();

    /// The unit its values are expressed in.
    static constexpr Unit unit = U;

    /// Derived from the unit -- see the note above about why it is not a
    /// parameter of its own.
    static constexpr Dimension dimension = U.dimension;
};

namespace detail
{
    /// Never defined, never called. Its only job is to let template argument
    /// deduction find a `Quantity` base through the derived-to-base conversion,
    /// which is how `Describe` learns a type's metadata without the type having
    /// to repeat it. Measured on cl, clang-cl and clang++.
    template <typename Tag, FixedString Symbol, FixedString Description, Unit U>
    auto quantity_base_of(Quantity<Tag, Symbol, Description, U> const&)
        -> Quantity<Tag, Symbol, Description, U>;

    template <typename T>
    concept DeclaresQuantity = requires(T const& value) { quantity_base_of(value); };
} // namespace detail

/// The one place anything reads a quantity's metadata.
///
/// Nothing above this layer knows how the metadata was declared. A type of ours
/// gets it from its `Quantity` base; a type nobody owns -- a `double`, something
/// from a vendor SDK -- gets it from an explicit specialisation of this
/// template. Both are read the same way, which is what lets a foreign type be
/// used in a formula without owning its source.
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

template <detail::DeclaresQuantity T>
struct Describe<T>
{
  private:
    using Base = decltype(detail::quantity_base_of(std::declval<T const&>()));

  public:
    static constexpr std::string_view symbol = Base::symbol;
    static constexpr std::string_view description = Base::description;
    static constexpr Unit unit = Base::unit;
    static constexpr Dimension dimension = Base::dimension;
};

/// True when `T` has metadata, however it was declared.
///
/// Answers rather than explodes for a type that has none -- see the note on the
/// primary template above for why that took a redesign.
///
/// All four members `Measured` and `checked_convert_to` actually read, not
/// merely the two that were enough to satisfy the concept's own author. A type
/// specialising only `symbol` and `unit` used to pass this concept, pass
/// `RequireDescribed`, and only then fail deep inside `checked_convert_to` with
/// the compiler's own "no member named 'dimension'" -- exactly the raw
/// diagnostic `RequireDescribed` exists to replace, delivered from the one
/// place it was supposed to be caught first. Measured: a `Describe`
/// specialisation naming only `symbol` and `unit` satisfied the two-member
/// concept and only broke three calls deeper.
template <typename T>
concept Described = requires {
    { Describe<T>::symbol } -> std::convertible_to<std::string_view>;
    { Describe<T>::description } -> std::convertible_to<std::string_view>;
    { Describe<T>::unit } -> std::convertible_to<Unit>;
    { Describe<T>::dimension } -> std::convertible_to<Dimension>;
};

/// True when a described type's own `dimension` agrees with its `unit`'s.
///
/// A type declared through the CRTP base can never fail this: `Quantity`
/// derives `dimension` from `unit`, so there is no second place for it to
/// disagree with (Ruling A, above). A `Describe` specialisation for a foreign
/// type states both independently, so nothing stops them from disagreeing --
/// measured, a specialisation naming `unit::Litre` (a volume) alongside
/// `dim::Mass` compiles in silence and mislabels every value read through it.
/// `Described<T> &&` short-circuits the second clause for an undescribed `T`,
/// so this never triggers the raw "no member" error `Described` itself is
/// built to avoid.
template <typename T>
concept DescribesConsistentDimension = Described<T> && Describe<T>::dimension == Describe<T>::unit.dimension;

/// Fails to compile, in our own words, when `T` declares no metadata, or
/// declares metadata whose `dimension` contradicts its own `unit`.
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
                  "formula: this type does not declare any quantity metadata. Either derive it "
                  "from formula::Quantity<TheType, symbol, description, unit>, or -- if it is a "
                  "type you do not own -- specialise formula::Describe for it");

    // Only meaningful once Described<T> holds; `!Described<T> ||` keeps this
    // from ever adding a second, misleading message to a type that is simply
    // undescribed -- that case is already reported, in full, above.
    static_assert(!Described<T> || DescribesConsistentDimension<T>,
                  "formula: this type's declared dimension does not match its unit's dimension. "
                  "A quantity's unit already carries a dimension; declaring a second one that "
                  "disagrees mislabels every value read through it -- derive Describe<T>::dimension "
                  "from Describe<T>::unit.dimension instead of stating it independently");

    static constexpr bool value = true;
};

} // namespace formula
