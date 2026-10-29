// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#include "ArdPortalFeatures.h"
#if ARDPORTAL_USE_LITTLEFS
#include "ArdLittleFSVolume.h"
#include "ArdStorage.h"
using ArdPortalStorage = ArdStorage<ArdLittleFSVolume, ArdLittleFSFile>;
#else
#include "ArdFS.h"
using ArdPortalStorage = ArdFS;
#endif
