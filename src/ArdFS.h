// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdFSStorageLimits.h"
#if defined(ARDFS_HOST_TEST)
#include <ArdFSHostVolume.h>
#else
#include "ArdFSVolume.h"
#endif
#include "ArdStorage.h"
using ArdFS = ArdStorage<ArdFSVolume, ArdFSFile>;
