// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Phoenix's platform_config.h for Godot core/ (each Godot platform ships one).
// Based on Godot's platform/{linuxbsd,web,windows,macos}/platform_config.h (MIT).
#pragma once

#if defined(_WIN32)
#include <malloc.h>
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <cstdlib>  // alloca
#ifdef __NetBSD__
#define PTHREAD_NETBSD_SET_NAME
#else
#define PTHREAD_BSD_SET_NAME
#endif
#else
#include <alloca.h>
#endif

#if defined(__APPLE__)
#define PTHREAD_RENAME_SELF
#endif
