// SPDX-License-Identifier: Apache-2.0
#pragma once

/// @file
/// Umbrella header. Deliberately pulls in no <string>, <vector> or <format>:
/// a consumer that only evaluates numbers must not compile them in every
/// translation unit.

#include <formula-cpp/detail/checked_int.hpp>
#include <formula-cpp/detail/type_list.hpp>
#include <formula-cpp/dimension.hpp>
#include <formula-cpp/error.hpp>
#include <formula-cpp/evaluation.hpp>
#include <formula-cpp/measured.hpp>
#include <formula-cpp/quantity.hpp>
#include <formula-cpp/rational.hpp>
#include <formula-cpp/rounding.hpp>
#include <formula-cpp/unit.hpp>
#include <formula-cpp/version.hpp>
