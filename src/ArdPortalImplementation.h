// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
// Compiled with the sketch so its feature defines apply to every module.
#include "ArdPortalImpl.h"
#if ARDPORTAL_ENABLE_DYNAMIC_PAGES
#include "ArdAppControlsImpl.h"
#include "DynamicPortalImpl.h"
#endif
#if ARDPORTAL_ENABLE_MQTT
#include "ArdMqttImpl.h"
#endif
#if ARDPORTAL_ENABLE_HA
#include "ArdHomeAssistantImpl.h"
#endif
#if ARDPORTAL_ENABLE_OTA
#include "ArdOtaImpl.h"
#endif
