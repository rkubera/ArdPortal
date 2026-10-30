// Author: Radoslaw Kubera (rkubera on GitHub).
// SPDX-License-Identifier: MIT
#pragma once
#ifndef ARDPORTAL_USE_ARDUINOJSON
#define ARDPORTAL_USE_ARDUINOJSON 0
#endif
#if ARDPORTAL_USE_ARDUINOJSON != 0 && ARDPORTAL_USE_ARDUINOJSON != 1
#error ARDPORTAL_USE_ARDUINOJSON must be 0 or 1
#endif
