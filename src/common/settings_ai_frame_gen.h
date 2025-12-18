// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"
#include "common/settings_setting.h"
#include "common/settings_enums.h"

namespace Settings {

// AI Frame Generator settings additions
enum class AIFrameGenMode : u32 {
    Disabled = 0,
    Conservative = 1,
    Balanced = 2,
    Aggressive = 3,
    Adaptive = 4,
};

} // namespace Settings
