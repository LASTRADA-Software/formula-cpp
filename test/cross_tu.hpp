// SPDX-License-Identifier: Apache-2.0
// Named tags identify these Quantity specializations, not `decltype([]{})`.
// This header is a reproducer of the retired internal-linkage-lambda idiom,
// not a regression guard for include/: no change under include/ can make the
// cross_tu test fail, since formula-cpp no longer identifies types that way.
// It fails again only if that idiom is reintroduced here.
#pragma once

#include <formula-cpp/formula.hpp>

template <typename T, typename Tag>
struct Quantity
{
    T value {};
};

struct FirstTag;
struct SecondTag;
struct ThirdTag;

using First = Quantity<int, FirstTag>;
using Second = Quantity<int, SecondTag>;
using Third = Quantity<int, ThirdTag>;

int consumeFirst(First value);
