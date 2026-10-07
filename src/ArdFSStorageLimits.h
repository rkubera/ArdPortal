// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include <stddef.h>
constexpr size_t ArdFSMaxDocumentBytes = 14 * 1024;
constexpr size_t ArdFSMaxJournalBytes = 2 * ArdFSMaxDocumentBytes + 128;
