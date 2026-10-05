// SPDX-License-Identifier: Apache-2.0
#pragma once

/// Two quantities declared by alias, shared by two translation units -- the
/// alias form's counterpart of `quantity_cross_tu.hpp`, and for its reason: a
/// quantity type must mean the same thing everywhere, and a mangling bug shows
/// at link time. The functions below are DEFINED in
/// quantity_alias_cross_tu_b.cpp and CALLED from quantity_alias_tests.cpp.
///
/// An alias names `Quantity<Tag, ...>` itself, so what both translation units
/// must agree on is that specialisation, with the tag the elaborated type
/// specifier declares in this namespace.

#include <formula-cpp/quantity.hpp>
#include <formula-cpp/unit.hpp>

#include <string_view>

namespace cross_alias
{

using Rise = formula::Quantity<struct RiseTag, "h", "height gained", formula::unit::Millimetre>;

/// Same symbol, same description, same unit -- a different tag, and so a
/// different type.
using Run = formula::Quantity<struct RunTag, "h", "height gained", formula::unit::Millimetre>;

/// Defined in quantity_alias_cross_tu_b.cpp. Takes the quantity by value, so
/// the alias's specialisation is part of the mangled name the two translation
/// units must agree on.
[[nodiscard]] std::string_view symbol_of(Rise);
[[nodiscard]] bool rise_and_run_are_distinct();

/// The address of `Rise::dimension` as THIS translation unit sees it --
/// see `quantity_cross_tu.hpp` for why `dimension`, and not `symbol`.
[[nodiscard]] void const* address_of_rise_dimension();

} // namespace cross_alias
