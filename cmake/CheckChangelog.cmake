# SPDX-License-Identifier: Apache-2.0
# `CHANGELOG.md` is where a user reads what changed, in the form Keep a
# Changelog describes. A fix recorded only in a commit message is recorded
# nowhere a user reads, and a changelog with no `## [Unreleased]` section has
# nowhere for the next entry to go. This check fails when either is missing.

set(changelog "${SOURCE_DIR}/CHANGELOG.md")

if(NOT EXISTS "${changelog}")
    message(FATAL_ERROR
        "changelog check: ${changelog} does not exist. SOURCE_DIR=\"${SOURCE_DIR}\" is wrong, or the "
        "changelog was removed.")
endif()

file(STRINGS "${changelog}" unreleased REGEX "^## \\[Unreleased\\]$")
if(NOT unreleased)
    message(FATAL_ERROR
        "changelog check: CHANGELOG.md has no \"## [Unreleased]\" section, so there is nowhere to record "
        "the next change.")
endif()

message(STATUS "changelog check: CHANGELOG.md exists and has an Unreleased section")
