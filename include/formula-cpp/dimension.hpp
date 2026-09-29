// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Dimensional analysis: a structural exponent vector over the seven SI base
/// dimensions and up to four named ones, usable as a non-type template
/// parameter so that a dimension is part of a type rather than a runtime tag.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace formula
{

/// A rational exponent on one base dimension.
///
/// Deliberately a separate type from `Rational`, and deliberately all-public. A
/// class used as a non-type template parameter must be *structural*: every
/// non-static data member public, recursively. `Rational` keeps its members
/// private -- which is exactly what lets its `operator==` compare componentwise,
/// since no code path can build a non-canonical value -- so it cannot be used
/// here. This type pays for its public members by canonicalising in its factory.
///
/// Always in lowest terms with a positive denominator. Build one with
/// `exponent(...)`. Aggregate initialisation bypasses that and is a mistake:
/// `Exponent { 2, 4 }` and `Exponent { 1, 2 }` are the same number but
/// different objects, and as template arguments they name different types.
struct Exponent
{
    /// The numerator, in lowest terms as produced by `exponent()`. A public
    /// field on a public aggregate: nothing stops a caller writing a
    /// non-canonical value directly, as the file comment above warns.
    std::int32_t numerator = 0;
    /// The denominator: in lowest terms and positive, as produced by
    /// `exponent()` -- again not enforced for a hand-built `Exponent`.
    std::int32_t denominator = 1;

    /// Memberwise equality -- meaningful when both fields are canonical, which
    /// is true of everything `exponent()` produces but not guaranteed for a
    /// value built by aggregate initialisation instead.
    [[nodiscard]] constexpr bool operator==(Exponent const&) const noexcept = default;
};

namespace detail
{
    /// Deliberately NOT `constexpr`. Calling it makes the enclosing expression a
    /// non-constant one, so an invalid exponent is a compile error at the point of
    /// use rather than a silently wrong value -- and the diagnostic names this
    /// function, which is why the name is a sentence. Dimensions are built in
    /// constant expressions, so this is where the error belongs. It is defined,
    /// not merely declared, because a runtime call must still link; reaching it at
    /// runtime is a programming error with no recovery.
    [[noreturn]] inline void formula_exponent_denominator_must_not_be_zero()
    {
        std::abort();
    }

    /// Same mechanism, for an exponent whose reduced form does not fit 32 bits.
    [[noreturn]] inline void formula_exponent_out_of_range()
    {
        std::abort();
    }

    [[nodiscard]] constexpr std::int64_t exponent_gcd(std::int64_t leftOperand, std::int64_t rightOperand) noexcept
    {
        if (leftOperand < 0)
            leftOperand = -leftOperand;
        if (rightOperand < 0)
            rightOperand = -rightOperand;
        while (rightOperand != 0)
        {
            std::int64_t const remainder = leftOperand % rightOperand;
            leftOperand = rightOperand;
            rightOperand = remainder;
        }
        return leftOperand == 0 ? 1 : leftOperand;
    }

    /// Reduces to lowest terms with a positive denominator, then narrows back to
    /// the stored width. Intermediates are 64-bit, which is wide enough that no
    /// combination of two stored exponents can overflow on the way. The widest
    /// product is 2^31 * (2^31 - 1), not (2^31 - 1) squared: a stored numerator
    /// can be `INT32_MIN`, whose magnitude is one MORE than `INT32_MAX`. The
    /// widest expression, the numerator of `operator+`, is twice that product:
    ///
    ///     9223372032559808512  worst case
    ///     9223372036854775807  INT64_MAX
    ///     ---------------------------------
    ///            4294967295    headroom, exactly 2^32 - 1
    ///
    /// and that worst case is reachable rather than hypothetical, since
    /// `exponent(INT32_MIN, INT32_MAX)` is already canonical -- 2^31 and 2^31 - 1
    /// are coprime, the latter being prime. Changing the stored width or these
    /// formulas means redoing this arithmetic.
    ///
    /// The two outcomes that would break the type are rejected here rather than
    /// stored. A zero denominator names no number, and would give `exponent(0, 0)`
    /// a different representation from `exponent(0, 1)`: two distinct types for
    /// one physical dimension, the exact failure this type exists to prevent. A
    /// reduced result outside `std::int32_t` is the other. Neither is caught by
    /// the narrowing itself -- a `static_cast` from a wider integer type is
    /// well-defined modular arithmetic and therefore perfectly legal in a constant
    /// expression, which is precisely why it would wrap in silence. Rejecting
    /// means calling a non-`constexpr` sentinel, making each one a compile error
    /// wherever a dimension is built, that being a constant expression by
    /// construction. Every `Exponent` in the library is made here, so this one
    /// place covers `exponent()` and every operator.
    [[nodiscard]] constexpr Exponent reduced(std::int64_t exponentNumerator, std::int64_t exponentDenominator) noexcept
    {
        if (exponentDenominator == 0)
            formula_exponent_denominator_must_not_be_zero();
        if (exponentDenominator < 0)
        {
            exponentNumerator = -exponentNumerator;
            exponentDenominator = -exponentDenominator;
        }
        std::int64_t const common = exponent_gcd(exponentNumerator, exponentDenominator);
        exponentNumerator /= common;
        exponentDenominator /= common;
        // On the 64-bit values, before the narrowing that would hide it. The
        // denominator needs no lower bound: it is positive by this point.
        if (exponentNumerator < std::numeric_limits<std::int32_t>::min()
            || exponentNumerator > std::numeric_limits<std::int32_t>::max()
            || exponentDenominator > std::numeric_limits<std::int32_t>::max())
            formula_exponent_out_of_range();
        return { static_cast<std::int32_t>(exponentNumerator), static_cast<std::int32_t>(exponentDenominator) };
    }
} // namespace detail

/// The only sanctioned way to make an `Exponent`: canonicalises.
[[nodiscard]] constexpr Exponent exponent(std::int32_t exponentNumerator, std::int32_t exponentDenominator = 1) noexcept
{
    return detail::reduced(exponentNumerator, exponentDenominator);
}

/// Adding exponents is what multiplying the dimensions they belong to does.
[[nodiscard]] constexpr Exponent operator+(Exponent leftOperand, Exponent rightOperand) noexcept
{
    return detail::reduced(static_cast<std::int64_t>(leftOperand.numerator) * rightOperand.denominator
                               + static_cast<std::int64_t>(rightOperand.numerator) * leftOperand.denominator,
                           static_cast<std::int64_t>(leftOperand.denominator) * rightOperand.denominator);
}

/// Subtracting exponents is what dividing the dimensions they belong to does.
[[nodiscard]] constexpr Exponent operator-(Exponent leftOperand, Exponent rightOperand) noexcept
{
    return detail::reduced(static_cast<std::int64_t>(leftOperand.numerator) * rightOperand.denominator
                               - static_cast<std::int64_t>(rightOperand.numerator) * leftOperand.denominator,
                           static_cast<std::int64_t>(leftOperand.denominator) * rightOperand.denominator);
}

/// Negation.
[[nodiscard]] constexpr Exponent operator-(Exponent exponentValue) noexcept
{
    return detail::reduced(-static_cast<std::int64_t>(exponentValue.numerator), exponentValue.denominator);
}

/// Raising a dimension to an integer power scales its exponents.
[[nodiscard]] constexpr Exponent operator*(Exponent exponentValue, std::int32_t multiplier) noexcept
{
    return detail::reduced(static_cast<std::int64_t>(exponentValue.numerator) * multiplier, exponentValue.denominator);
}

/// Taking an nth root divides them -- the operation integer exponents cannot express.
[[nodiscard]] constexpr Exponent operator/(Exponent exponentValue, std::int32_t divisor) noexcept
{
    return detail::reduced(exponentValue.numerator, static_cast<std::int64_t>(exponentValue.denominator) * divisor);
}

/// True for the exponent of a dimension a quantity does not depend on at all.
[[nodiscard]] constexpr bool is_zero(Exponent exponentValue) noexcept
{
    return exponentValue.numerator == 0;
}

/// True when the exponent is a whole number, not a genuine fraction such as one half.
[[nodiscard]] constexpr bool is_integer(Exponent exponentValue) noexcept
{
    return exponentValue.denominator == 1;
}

/// Bytes available for a unit symbol, including the terminator. Enough for the
/// UTF-8 spellings that occur in practice: `m3`, `°C` (3 bytes), `µm` (3). A
/// symbol that does not fit is a compile error (see `symbol()`), never a
/// silent truncation; bump this deliberately if a real symbol ever needs more.
inline constexpr std::size_t SymbolCapacity = 16;

/// A fixed-capacity symbol. An array of a structural type is structural, which a
/// `std::string_view` is not -- and unlike a `FixedString<N>` template this keeps
/// `Unit` a single non-template type, so every unit has the same type.
struct Symbol
{
    /// The symbol's UTF-8 bytes, zero-terminated as produced by `symbol()`;
    /// read with `view()`, which does not assume that and scans instead of
    /// trusting a terminator -- `Symbol` is a public aggregate, so a caller
    /// can fill `characters` directly and leave no room for one.
    char characters[SymbolCapacity] {};

    /// Memberwise equality -- the full `SymbolCapacity` bytes, terminator
    /// included when the value is one `symbol()` produced.
    [[nodiscard]] constexpr bool operator==(Symbol const&) const noexcept = default;
};

namespace detail
{
    /// Deliberately NOT `constexpr`, for the same reason as
    /// `formula_exponent_out_of_range` in dimension.hpp: `symbol()` runs in
    /// exactly the same context -- a constant expression building a constant
    /// that determines a `Unit`'s type -- and has exactly the same consequence
    /// when it goes wrong. Truncating instead of refusing would let two
    /// distinct symbols collapse into the same `Symbol` object and therefore
    /// the same NTTP type, and could split a multi-byte UTF-8 character in
    /// half. Calling this makes the enclosing expression a non-constant one,
    /// so the mistake is a compile error at the point of use. Defined, not
    /// merely declared, because a runtime call must still link; reaching it at
    /// runtime is a programming error with no recovery.
    [[noreturn]] inline void formula_unit_symbol_too_long()
    {
        std::abort();
    }
} // namespace detail

/// Builds a Symbol from a byte string. Refuses -- see
/// `formula_unit_symbol_too_long` -- rather than truncating when the text does
/// not fit in `SymbolCapacity` bytes including the terminator; every symbol
/// shipped by this library is well within the limit.
[[nodiscard]] constexpr Symbol symbol(char const* spelling) noexcept
{
    Symbol built {};
    std::size_t characterIndex = 0;
    while (spelling[characterIndex] != '\0')
    {
        if (characterIndex + 1 >= SymbolCapacity)
            detail::formula_unit_symbol_too_long();
        built.characters[characterIndex] = spelling[characterIndex];
        ++characterIndex;
    }
    return built;
}

/// Reads a Symbol back as a view. The storage has to be structural; this does not.
///
/// The scan is bounded by `SymbolCapacity` rather than left to the terminator,
/// and that is not belt-and-braces. `Symbol` is a public aggregate -- it has to
/// be, or `Unit` is not structural and cannot be a template argument -- so a
/// caller can fill `characters` directly, and exactly `SymbolCapacity` bytes of
/// text is a legal initialiser that leaves no room for a terminator. Handing
/// that to `std::string_view { value.characters }` reads until it happens to
/// find a zero somewhere after the array. Measured on a `Symbol` followed by
/// seven bytes of padding: 23 characters returned from a 16-byte array, the
/// neighbours included. A symbol built by `symbol()` is always terminated, but
/// this function cannot assume its argument came from there.
[[nodiscard]] constexpr std::string_view view(Symbol const& unitSymbol) noexcept
{
    std::size_t symbolLength = 0;
    while (symbolLength < SymbolCapacity && unitSymbol.characters[symbolLength] != '\0')
        ++symbolLength;
    return std::string_view { unitSymbol.characters, symbolLength };
}

/// Deleted: binding a temporary here would return a view into a `Symbol` that
/// is already destroyed by the time the caller reads through it -- e.g.
/// `view(symbol("mm"))`. Measured silent on cl /W4, clang-cl /W4 and
/// `clang++ -Wall -Wextra -Wdangling`. Bind the `Symbol` to a named local
/// first, then call `view()` on that.
std::string_view view(Symbol&&) = delete;

/// How many named base dimensions one `Dimension` can hold at once: a tariff in
/// euros per kilowatt-hour needs one, an exchange rate between two currencies
/// two. A product that would need a fifth is a compile error -- see
/// `formula_dimension_has_too_many_named_bases` -- never a silently dropped base.
inline constexpr std::size_t NamedBaseCapacity = 4;

/// A base dimension the SI does not have, declared by the application -- money
/// in one currency is the usual one -- together with its exponent in the
/// `Dimension` that holds it.
///
/// Make one with `base_dimension()`, never by hand; see `Dimension` for the
/// canonical form a hand-built value would bypass.
struct NamedBase
{
    /// The base's name, compared byte for byte: two libraries that both declare
    /// `base_dimension("EUR")` mean the same dimension. It is also the symbol of
    /// the base's coherent unit -- the unit of magnitude one is written `EUR`.
    Symbol name {};
    /// The exponent of this base. Never zero in a slot that is in use.
    Exponent exponent {};

    /// Memberwise equality: the name, all `SymbolCapacity` bytes, and the exponent.
    [[nodiscard]] constexpr bool operator==(NamedBase const&) const noexcept = default;
};

/// An exponent vector over the seven SI base dimensions and up to
/// `NamedBaseCapacity` base dimensions the application names itself.
///
/// Structural, so it can be a non-type template parameter -- which is the point:
/// a dimension belongs to a *type*, checked when the program is compiled, not to
/// a value checked when it runs. Verified on MSVC, clang-cl, clang++ and g++,
/// including that two translation units agree on the mangling.
///
/// The named bases are kept in one canonical form, which is what lets
/// memberwise equality stand for equality of dimensions: the slots in use come
/// first, each with a non-zero exponent; their names are strictly ascending,
/// compared byte by byte as `unsigned char` over all `SymbolCapacity` bytes; and
/// every slot after the last one in use equals `NamedBase {}`. `base_dimension()`
/// and every operator below produce that form. Filling `namedBases` by hand
/// bypasses it and is a mistake, as aggregate initialisation of an `Exponent`
/// is: euros times yen with the two names in the other order is the same
/// dimension but a different object, and as template arguments the two name
/// different types.
struct Dimension
{
    /// Exponent on length (SI base unit: metre).
    Exponent length {};
    /// Exponent on mass (SI base unit: kilogram).
    Exponent mass {};
    /// Exponent on time (SI base unit: second).
    Exponent time {};
    /// Exponent on electric current (SI base unit: ampere).
    Exponent current {};
    /// Exponent on thermodynamic temperature (SI base unit: kelvin).
    Exponent temperature {};
    /// Exponent on amount of substance (SI base unit: mole).
    Exponent amount {};
    /// Exponent on luminous intensity (SI base unit: candela).
    Exponent luminosity {};
    /// The named base dimensions, in the canonical form the class comment
    /// describes. Last, so that a designated initialiser of SI exponents alone,
    /// `Dimension { .length = exponent(1) }`, still compiles.
    ///
    /// The four elements are spelled out rather than written `{}`, and that is a
    /// workaround, not style. With `{}`, g++ 13.3 and 14.2 miscompile a constant
    /// evaluation that copies a dimension holding a named base and then writes
    /// the copy's last slot: the ORIGINAL's last slot changes as well, its
    /// exponent becoming 0/0 in the emitted object, and the original stops being
    /// the same template argument as an equal dimension spelled another way.
    /// Measured; cl, clang-cl and clang++ were unaffected, and this spelling
    /// removes it on both g++ versions. `dimension_tests.cpp` keeps the case,
    /// and only g++ can fail it.
    NamedBase namedBases[NamedBaseCapacity] { NamedBase {}, NamedBase {}, NamedBase {}, NamedBase {} };
    static_assert(NamedBaseCapacity == 4,
                  "formula: namedBases spells out one NamedBase {} per slot; list exactly NamedBaseCapacity of them");

    /// Memberwise equality: the seven SI exponents and every named-base slot.
    [[nodiscard]] constexpr bool operator==(Dimension const&) const noexcept = default;
};

namespace detail
{
    /// Deliberately NOT `constexpr`, like `formula_exponent_out_of_range`: a
    /// product or quotient that would need more than `NamedBaseCapacity` named
    /// bases calls this, so building such a dimension -- always a constant
    /// expression -- fails to compile and the diagnostic names this function.
    /// Defined because a runtime call must still link; reaching it at runtime
    /// is a programming error with no recovery.
    [[noreturn]] inline void formula_dimension_has_too_many_named_bases()
    {
        std::abort();
    }

    /// `base_dimension("")`: a base needs a name. Same mechanism as above.
    [[noreturn]] inline void formula_base_dimension_name_must_not_be_empty()
    {
        std::abort();
    }

    /// A base name of `SymbolCapacity` bytes or more: it would not fit in a
    /// `Symbol` with its terminator. Same mechanism as above.
    [[noreturn]] inline void formula_base_dimension_name_too_long()
    {
        std::abort();
    }

    /// A base name that is not an ASCII letter followed by ASCII letters or
    /// digits. The name is also its coherent unit's symbol, and that symbol is
    /// joined into compound unit text with spaces, `/`, `^` and parentheses and
    /// printed into Markdown, where `_`, `*` and `[` are markup; a name made of
    /// letters and digits cannot collide with any of them. Same mechanism as
    /// above.
    [[noreturn]] inline void formula_base_dimension_name_must_be_a_letter_then_letters_or_digits()
    {
        std::abort();
    }

    /// A base name that is the symbol of an SI base unit -- `m`, `kg`, `s`, `A`,
    /// `K`, `mol` or `cd` -- so that a named base would read as metres,
    /// kilograms and the rest. Same mechanism as above.
    [[noreturn]] inline void formula_base_dimension_name_is_an_si_base_unit_symbol()
    {
        std::abort();
    }

    /// The result of `merged_dimension`: the merged dimension, and whether its
    /// named bases fit in `NamedBaseCapacity`. When `fits` is false,
    /// `dimension` is incomplete and must not be used.
    struct MergedDimension
    {
        /// The combined dimension; complete only when `fits` is true.
        Dimension dimension {};
        /// False when the result would need more than `NamedBaseCapacity` named bases.
        bool fits = true;
    };

    /// Orders two base names byte by byte as `unsigned char` over all
    /// `SymbolCapacity` bytes: negative, zero or positive as the left name
    /// sorts before, equal to or after the right one. Bytes after a terminator
    /// are zero in every `Symbol` that `symbol()` built, so a name sorts before
    /// every longer name it is a prefix of.
    [[nodiscard]] constexpr int compare_base_names(Symbol const& leftName, Symbol const& rightName) noexcept
    {
        for (std::size_t byteAt = 0; byteAt < SymbolCapacity; ++byteAt)
        {
            auto const leftByte = static_cast<unsigned char>(leftName.characters[byteAt]);
            auto const rightByte = static_cast<unsigned char>(rightName.characters[byteAt]);
            if (leftByte != rightByte)
                return leftByte < rightByte ? -1 : 1;
        }
        return 0;
    }

    /// Whether `slot` of `dimensionValue` holds a named base. The slots in use
    /// come first, so the first slot that does not ends the list.
    [[nodiscard]] constexpr bool named_base_in_use(Dimension const& dimensionValue, std::size_t slot) noexcept
    {
        return slot < NamedBaseCapacity && !is_zero(dimensionValue.namedBases[slot].exponent);
    }

    /// The product of two dimensions, or their quotient when `dividing`: the SI
    /// exponents added or subtracted as ever, and the two named-base lists
    /// merged in one pass. Both lists are sorted, so two cursors walk them
    /// together: the smaller name is emitted -- the right operand's negated
    /// when dividing -- and equal names are emitted once with the two
    /// exponents combined, or not at all when they cancel. Cancelling happens
    /// before counting, so `(EUR / USD) * (USD / JPY)` fits even though four
    /// names go in. The result is built in a fresh value, never by editing a
    /// copy of an operand (see `Dimension::namedBases` for why that matters on
    /// g++). Exponent overflow goes through `reduced`'s guard as everywhere else.
    [[nodiscard]] constexpr MergedDimension merged_dimension(Dimension const& leftOperand,
                                                             Dimension const& rightOperand,
                                                             bool dividing) noexcept
    {
        auto const combined = [dividing](Exponent leftExponent, Exponent rightExponent) noexcept {
            return dividing ? leftExponent - rightExponent : leftExponent + rightExponent;
        };

        MergedDimension merged {};
        merged.dimension.length = combined(leftOperand.length, rightOperand.length);
        merged.dimension.mass = combined(leftOperand.mass, rightOperand.mass);
        merged.dimension.time = combined(leftOperand.time, rightOperand.time);
        merged.dimension.current = combined(leftOperand.current, rightOperand.current);
        merged.dimension.temperature = combined(leftOperand.temperature, rightOperand.temperature);
        merged.dimension.amount = combined(leftOperand.amount, rightOperand.amount);
        merged.dimension.luminosity = combined(leftOperand.luminosity, rightOperand.luminosity);

        std::size_t leftSlot = 0;
        std::size_t rightSlot = 0;
        std::size_t filledSlots = 0;
        for (;;)
        {
            bool const leftInUse = named_base_in_use(leftOperand, leftSlot);
            bool const rightInUse = named_base_in_use(rightOperand, rightSlot);
            if (!leftInUse && !rightInUse)
                return merged;
            // Negative: take the left name next; positive: the right; zero: both, one name.
            int const order = !rightInUse  ? -1
                              : !leftInUse ? 1
                                           : compare_base_names(leftOperand.namedBases[leftSlot].name,
                                                                rightOperand.namedBases[rightSlot].name);
            NamedBase const& taken = order > 0 ? rightOperand.namedBases[rightSlot] : leftOperand.namedBases[leftSlot];
            // Not `const`, deliberately. g++ 13.3 and 14.2 keep a local's `const`
            // in the value they copy out of it, so a slot built from a `const
            // Exponent` local becomes a different template argument from the
            // equal slot `base_dimension()` builds -- measured: `EUR * Energy /
            // Energy` stopped being the same type as `EUR`.
            Exponent takenExponent =
                order < 0   ? taken.exponent
                : order > 0 ? (dividing ? -taken.exponent : taken.exponent)
                            : combined(taken.exponent, rightOperand.namedBases[rightSlot].exponent);
            if (order <= 0)
                ++leftSlot;
            if (order >= 0)
                ++rightSlot;
            // Only one name on both sides can reach zero here: a slot in use never
            // holds a zero exponent, and negating one does not make it zero.
            if (is_zero(takenExponent))
                continue;
            if (filledSlots == NamedBaseCapacity)
            {
                merged.fits = false;
                return merged;
            }
            merged.dimension.namedBases[filledSlots] = NamedBase { taken.name, takenExponent };
            ++filledSlots;
        }
    }
} // namespace detail

/// Declares a base dimension the SI does not have -- `base_dimension("EUR")` --
/// with exponent one.
///
/// **Identity is the name, byte for byte.** Two parts of a program, or two
/// libraries, that both write `base_dimension("EUR")` get the same dimension,
/// which for a three-letter currency code is what is wanted. For a generic
/// word, pick a distinctive name ("AcmeCredit" rather than "credit"). Each
/// base is its own dimension: euros and yen never convert into each other --
/// an exchange rate is data, a quantity in yen per euro.
///
/// The name is also the symbol of the base's coherent unit, the unit of
/// magnitude one: the unit named after the base is one of it, and a cent is
/// a hundredth of it. It must be an ASCII letter followed by ASCII letters or
/// digits, shorter than `SymbolCapacity`, and not the symbol of an SI base
/// unit; each refusal is a compile error naming the rule -- see the
/// `formula_base_dimension_name_...` functions. `consteval`, so a bad name can
/// never reach run time; a helper that forwards a name here must be
/// `consteval` too.
[[nodiscard]] consteval Dimension base_dimension(char const* baseName) noexcept
{
    if (baseName[0] == '\0')
        detail::formula_base_dimension_name_must_not_be_empty();
    for (std::size_t nameLength = 0; baseName[nameLength] != '\0'; ++nameLength)
    {
        if (nameLength + 1 >= SymbolCapacity)
            detail::formula_base_dimension_name_too_long();
        char const spelt = baseName[nameLength];
        bool const isLetter = (spelt >= 'A' && spelt <= 'Z') || (spelt >= 'a' && spelt <= 'z');
        bool const isLaterDigit = nameLength > 0 && spelt >= '0' && spelt <= '9';
        if (!isLetter && !isLaterDigit)
            detail::formula_base_dimension_name_must_be_a_letter_then_letters_or_digits();
    }
    char const* const siBaseUnitSymbols[] = { "m", "kg", "s", "A", "K", "mol", "cd" };
    for (char const* const siBaseUnitSymbol: siBaseUnitSymbols)
        if (symbol(baseName) == symbol(siBaseUnitSymbol))
            detail::formula_base_dimension_name_is_an_si_base_unit_symbol();

    Dimension based {};
    based.namedBases[0] = NamedBase { symbol(baseName), exponent(1) };
    return based;
}

/// Multiplying quantities adds their dimensions' exponents, the named bases'
/// included. A product needing more than `NamedBaseCapacity` named bases fails
/// to compile, naming `formula_dimension_has_too_many_named_bases`.
[[nodiscard]] constexpr Dimension operator*(Dimension leftOperand, Dimension rightOperand) noexcept
{
    detail::MergedDimension const merged = detail::merged_dimension(leftOperand, rightOperand, false);
    if (!merged.fits)
        detail::formula_dimension_has_too_many_named_bases();
    return merged.dimension;
}

/// Dividing subtracts them, with the same limit on named bases.
[[nodiscard]] constexpr Dimension operator/(Dimension leftOperand, Dimension rightOperand) noexcept
{
    detail::MergedDimension const merged = detail::merged_dimension(leftOperand, rightOperand, true);
    if (!merged.fits)
        detail::formula_dimension_has_too_many_named_bases();
    return merged.dimension;
}

/// Raising a quantity to an integer power scales every exponent of its
/// dimension, the named bases' included. The zeroth power is the scalar
/// dimension outright: scaling a named base's exponent to zero would leave a
/// slot in use with a zero exponent, which is not canonical.
[[nodiscard]] constexpr Dimension power(Dimension dimensionValue, std::int32_t exponentOfPower) noexcept
{
    if (exponentOfPower == 0)
        return Dimension {};
    Dimension powered {};
    powered.length = dimensionValue.length * exponentOfPower;
    powered.mass = dimensionValue.mass * exponentOfPower;
    powered.time = dimensionValue.time * exponentOfPower;
    powered.current = dimensionValue.current * exponentOfPower;
    powered.temperature = dimensionValue.temperature * exponentOfPower;
    powered.amount = dimensionValue.amount * exponentOfPower;
    powered.luminosity = dimensionValue.luminosity * exponentOfPower;
    for (std::size_t slot = 0; detail::named_base_in_use(dimensionValue, slot); ++slot)
        powered.namedBases[slot] = NamedBase { dimensionValue.namedBases[slot].name,
                                               dimensionValue.namedBases[slot].exponent * exponentOfPower };
    return powered;
}

/// The nth root. Integer exponents cannot express the result at all -- the
/// square root of an area is a length, but the square root of a length is
/// length to the one half, and norm formulas do take such roots. A named
/// base's exponent is divided the same way; degree zero is refused by the
/// exponent guard before any named base is looked at.
[[nodiscard]] constexpr Dimension nth_root(Dimension dimensionValue, std::int32_t degree) noexcept
{
    Dimension rooted {};
    rooted.length = dimensionValue.length / degree;
    rooted.mass = dimensionValue.mass / degree;
    rooted.time = dimensionValue.time / degree;
    rooted.current = dimensionValue.current / degree;
    rooted.temperature = dimensionValue.temperature / degree;
    rooted.amount = dimensionValue.amount / degree;
    rooted.luminosity = dimensionValue.luminosity / degree;
    for (std::size_t slot = 0; detail::named_base_in_use(dimensionValue, slot); ++slot)
        rooted.namedBases[slot] = NamedBase { dimensionValue.namedBases[slot].name,
                                              dimensionValue.namedBases[slot].exponent / degree };
    return rooted;
}

/// True for a quantity with no dependence on any base dimension -- a pure ratio.
[[nodiscard]] constexpr bool is_dimensionless(Dimension dimensionValue) noexcept
{
    return dimensionValue == Dimension {};
}

/// Dimension constants. Users compose these rather than spelling exponents, which
/// keeps the representation swappable.
namespace dim
{
    /// Dimensionless -- every exponent zero.
    inline constexpr Dimension Scalar {};
    /// The base dimension of length.
    inline constexpr Dimension Length { .length = exponent(1) };
    /// The base dimension of mass.
    inline constexpr Dimension Mass { .mass = exponent(1) };
    /// The base dimension of time.
    inline constexpr Dimension Time { .time = exponent(1) };
    /// The base dimension of electric current.
    inline constexpr Dimension Current { .current = exponent(1) };
    /// The base dimension of thermodynamic temperature.
    inline constexpr Dimension Temperature { .temperature = exponent(1) };
    /// The base dimension of amount of substance.
    inline constexpr Dimension Amount { .amount = exponent(1) };
    /// The base dimension of luminous intensity.
    inline constexpr Dimension Luminosity { .luminosity = exponent(1) };

    /// Length squared.
    inline constexpr Dimension Area = Length * Length;
    /// Length cubed.
    inline constexpr Dimension Volume = Area * Length;
    /// Mass per volume.
    inline constexpr Dimension Density = Mass / Volume;
    /// Length per time.
    inline constexpr Dimension Velocity = Length / Time;
    /// Velocity per time.
    inline constexpr Dimension Acceleration = Velocity / Time;
    /// Mass times acceleration.
    inline constexpr Dimension Force = Mass * Acceleration;
    /// Force per area.
    inline constexpr Dimension Pressure = Force / Area;
    /// Force times length.
    inline constexpr Dimension Energy = Force * Length;
    /// Energy per time -- the rate at which energy is delivered or used. The
    /// name has nothing to do with the function `power()` above, which raises a
    /// dimension to an integer exponent.
    inline constexpr Dimension Power = Energy / Time;
    /// The reciprocal of time.
    inline constexpr Dimension Frequency = Scalar / Time;
    /// Mass per area -- what a sheet or a membrane is specified by, and NOT a
    /// density: one length short of it, so the two are different dimensions and
    /// compare unequal.
    inline constexpr Dimension MassPerArea = Mass / Area;
    /// Force per length -- a force carried per unit of width, which is not a
    /// stress: dividing a force by a *length* rather than by an area leaves a
    /// dimension of its own.
    inline constexpr Dimension ForcePerLength = Force / Length;
    /// Pressure times time: the resistance of a fluid to shear, measured in
    /// pascal seconds. Distinct from `KinematicViscosity` below by a factor of
    /// density, which is why the two are separate dimensions rather than two
    /// spellings of one.
    inline constexpr Dimension DynamicViscosity = Pressure * Time;
    /// Area per time: dynamic viscosity divided by density, and the other of
    /// the two quantities called "viscosity". A value in one is not a value in
    /// the other, and the type system says so.
    inline constexpr Dimension KinematicViscosity = Area / Time;
} // namespace dim

/// True when two dimensions are identical.
template <Dimension Left, Dimension Right>
inline constexpr bool SameDimension = (Left == Right);

/// Fails to compile, loudly and legibly, when two dimensions differ.
///
/// The indirection through a named template is deliberate and was measured: an
/// inline `static_assert(Left == Right, ...)` at the point of use prints only
/// the operand type names, while instantiating a template *on the values* makes
/// every supported compiler print the exponent vectors themselves --
///
///     RequireSameDimension<Dimension{...length 3...}, Dimension{...mass 1...}>
///
/// -- so the reader sees volume against mass rather than two opaque template
/// ids. The wording below is ours, which is what allows the negative-compile
/// test harness to assert why a compile failed rather than only that it did.
///
/// **It fires only when the type is completed.** The assertion lives in the class
/// body, so it runs when the class template is instantiated -- and naming the
/// specialisation is not instantiating it. Measured, one form per translation
/// unit, on cl and clang-cl:
///
///     using Checked = RequireSameDimension<dim::Volume, dim::Mass>;  // SILENT
///     void f(RequireSameDimension<dim::Volume, dim::Mass>);          // SILENT
///     RequireSameDimension<dim::Volume, dim::Mass>::value            // fires
///     sizeof(RequireSameDimension<dim::Volume, dim::Mass>)           // fires
///     RequireSameDimension<dim::Volume, dim::Mass> checked {};       // fires
///
/// The first two compile clean with mismatched dimensions. Write `::value`:
///
///     static_assert(RequireSameDimension<Left, Right>::value);
///
/// An alias that is never touched is a guard that never guards, and it looks
/// exactly like one that does. Where a plain bool is wanted, use
/// `SameDimension<Left, Right>` -- a variable template, so always evaluated; it
/// simply cannot print the vectors, which is what this one is for.
template <Dimension Left, Dimension Right>
struct RequireSameDimension
{
    // "in this diagnostic", not "above": clang puts the vectors inside this very
    // error line, in its `due to requirement` clause, and again in a note below;
    // cl puts them only in a note below. Measured on all three. Nothing prints
    // them above the message, so do not send the reader to look there. A named
    // base's name is printed as text by g++ (`Symbol{"EUR"}`) but as character
    // codes by clang (`{69, 85, 82, 0, ...}`) and cl (`char69,85,82,0,...`).
    static_assert(Left == Right,
                  "formula: these two dimensions are not the same; the offending exponent vectors "
                  "appear in this diagnostic as the template arguments of RequireSameDimension, in "
                  "the order length, mass, time, current, temperature, amount, luminosity, then the "
                  "named base dimensions by name");

    /// Always `true` once reached -- the `static_assert` above already failed
    /// compilation otherwise. Present so `::value` is the spelling that instantiates
    /// the class template; see the class comment for why that spelling matters.
    static constexpr bool value = true;
};

} // namespace formula
