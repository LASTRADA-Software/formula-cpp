// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Umbrella header. Deliberately pulls in no `<string>`, `<vector>` or `<format>`:
/// a consumer that only evaluates numbers must not compile them in every
/// translation unit. `render.hpp` and `document.hpp` do require these headers
/// for documentation generation; they are deliberately not included here, so
/// that arithmetic-only consumers stay lean and pay nothing for text a
/// consumer who wants it asks for by name.

#include <formula-cpp/citation.hpp>
#include <formula-cpp/detail/checked_int.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/environment.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluate.hpp>
#include <formula-cpp/expression.hpp>
#include <formula-cpp/function.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/outcome.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/rounding_node.hpp>
#include <formula-cpp/unit.hpp>
#include <formula-cpp/version.hpp>
