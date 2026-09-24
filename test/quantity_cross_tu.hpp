// SPDX-License-Identifier: Apache-2.0
#pragma once

/// Two quantity types shared by two translation units.
///
/// A quantity type is only useful if it means the same thing everywhere. That is
/// a statement about NAME MANGLING, and a mangling bug shows at link time, not
/// at compile time -- so the functions below are DEFINED in
/// quantity_cross_tu_b.cpp and CALLED from quantity_tests.cpp. If the two
/// translation units disagreed about what `WaterVolume` is, this would fail to
/// link rather than fail a check.

#include <formula-cpp/quantity.hpp>
#include <formula-cpp/unit.hpp>

namespace cross
{

struct WaterVolume: formula::Quantity<WaterVolume, "V_w", "volume of water added", formula::unit::Litre>
{
};

/// Same symbol, same description, same unit -- different tag. The tag is the
/// whole reason these do not collapse into one type.
struct CementVolume: formula::Quantity<CementVolume, "V_w", "volume of water added", formula::unit::Litre>
{
};

/// Defined in quantity_cross_tu_b.cpp.
[[nodiscard]] std::string_view symbol_of_water_volume();
[[nodiscard]] bool water_and_cement_are_distinct();

/// The address of `WaterVolume::dimension`, as THIS translation unit sees it.
///
/// The link test above proves a function signature carries the quantity type. It
/// does not, on its own, prove that both translation units named the SAME
/// specialisation: `Quantity<WaterVolume, "V_w", ...>` is instantiated
/// independently on each side, and two instantiations that mangled differently
/// would quietly become two distinct objects rather than failing to link.
/// Comparing a per-specialisation member's address is what notices that.
///
/// `dimension`, not `symbol`, and the difference is not cosmetic. Measured:
///
///     WaterVolume::symbol.data() == CementVolume::symbol.data()   -> TRUE
///     &WaterVolume::dimension    == &CementVolume::dimension      -> false
///
/// `symbol` is a view onto the `FixedString` TEMPLATE PARAMETER OBJECT, and two
/// specialisations given the same string share that object however their tags
/// differ. So an address comparison on `symbol` is satisfied by two entirely
/// different specialisations and proves nothing about identity -- the first
/// version of this test did exactly that, and passed when deliberately pointed
/// at the wrong quantity. `dimension` is computed from the unit, so each
/// specialisation owns one.
[[nodiscard]] void const* address_of_water_volume_dimension();

} // namespace cross
