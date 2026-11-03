// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
//
// Umbrella header for Phoenix C++ scripts (the equivalent of
// `import 'package:flutter/material.dart';`).
#pragma once

#include "phoenix/console_profile.h"
#include "phoenix/engine.h"
#include "phoenix/script_abi.h"
#include "phoenix/storage.h"
#include "phoenix/widgets.h"

/// Every Phoenix app defines this in lib/main.cpp (Flutter's `main()`), and
/// calls phoenix::runApp(...) from it.
void phoenix_main();
