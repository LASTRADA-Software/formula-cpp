// SPDX-License-Identifier: Apache-2.0
// A symbol that does not fit SymbolCapacity (16 bytes, including the
// terminator) must be rejected, not truncated: truncating could merge two
// distinct units into the same type, and could split a multi-byte UTF-8
// character in half. This must not compile.
#include <formula-cpp/unit.hpp>

// 16 bytes -- one more than the 15 usable characters -- so this does not fit.
constexpr formula::Symbol bad = formula::symbol("0123456789abcdef");

int main()
{
    return bad.characters[0];
}
