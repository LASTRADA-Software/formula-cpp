// SPDX-License-Identifier: Apache-2.0
// symbol() is consteval: text that only exists at run time cannot reach it, so a symbol too long for
// SymbolCapacity can never abort a running program. Run-time text goes through checked_symbol. This must not compile.
#include <formula-cpp/unit.hpp>

int main(int argumentCount, char** arguments)
{
    formula::Symbol const fromRunTime = formula::symbol(arguments[argumentCount - 1]);
    return fromRunTime.characters[0];
}
