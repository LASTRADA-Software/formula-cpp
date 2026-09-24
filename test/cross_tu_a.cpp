// SPDX-License-Identifier: Apache-2.0
// Defines consumeFirst() in a second translation unit from the declaration in
// cross_tu.hpp. This is a reproducer of the retired internal-linkage-lambda
// idiom, not a regression guard for include/: no change under include/ can
// make it fail, since formula-cpp no longer uses that idiom there. It fails
// again only if the idiom is reintroduced in cross_tu.hpp.
#include "cross_tu.hpp"

int consumeFirst(First value)
{
    return value.value;
}
