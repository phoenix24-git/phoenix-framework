// Copyright 2026 The Phoenix Authors. SPDX-License-Identifier: Apache-2.0
#pragma once

#include "phoenix/script_abi.h"

namespace phoenix {

/// Installs the live service table (engine boot) or a fake (tests).
void set_engine_api(const PhxEngineApi* api);
PhxFrameInfo& mutable_frame_info();

}  // namespace phoenix
