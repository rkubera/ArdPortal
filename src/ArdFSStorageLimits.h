// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>
#include <stddef.h>
// Compatibility constants: standalone ArdFS has no fixed document byte limit.
// Actual records are bounded by flash bank capacity; complete String APIs also
// require enough heap. Portal configuration budgets are defined separately.
constexpr size_t ArdFSMaxDocumentBytes = SIZE_MAX;
constexpr size_t ArdFSMaxJournalBytes = SIZE_MAX;
