// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
// Include this entry point once, in the main sketch. Feature defines placed
// before this include configure both declarations and implementation.
#define ARDPORTAL_DEFINE_CONTROL_DATA
#include "ArdPortalDeclarations.h"
#if ARDPORTAL_ENABLE_HA
#include "HaEntityData.h"
#endif
#undef ARDPORTAL_DEFINE_CONTROL_DATA
#include "ArdPortalImplementation.h"
