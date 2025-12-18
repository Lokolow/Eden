// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.AbstractIntSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting

class AIFrameGenModeSetting(
    setting: AbstractIntSetting,
    titleId: Int = R.string.ai_frame_gen_mode,
    descriptionId: Int = R.string.ai_frame_gen_mode_description
) : SettingsItem(setting, titleId, descriptionId) {
    override val type = TYPE_SINGLE_CHOICE

    val choices: Array<String>
        get() = arrayOf(
            "Disabled",
            "Conservative (Low Power)",
            "Balanced (Recommended)",
            "Aggressive (High Performance)",
            "Adaptive (Auto-adjust)"
        )

    val values: Array<Int>
        get() = arrayOf(0, 1, 2, 3, 4)

    fun getValueAt(index: Int): Int {
        return values[index]
    }

    val selectedValue: Int
        get() = setting.int

    fun setSelectedValue(selection: Int) {
        setting.setInt(selection)
    }
}

class AIFrameGenEnableSetting(
    setting: AbstractSetting,
    titleId: Int = R.string.ai_frame_gen_enable,
    descriptionId: Int = R.string.ai_frame_gen_enable_description
) : SwitchSetting(setting, titleId, descriptionId)

class AIFrameGenTargetFpsSetting(
    setting: AbstractIntSetting,
    titleId: Int = R.string.ai_frame_gen_target_fps,
    descriptionId: Int = R.string.ai_frame_gen_target_fps_description,
    min: Int = 30,
    max: Int = 120,
    units: String = " FPS"
) : SliderSetting(setting, titleId, descriptionId, min, max, units)

class AIFrameGenMemoryLimitSetting(
    setting: AbstractIntSetting,
    titleId: Int = R.string.ai_frame_gen_memory_limit,
    descriptionId: Int = R.string.ai_frame_gen_memory_limit_description,
    min: Int = 256,
    max: Int = 1024,
    units: String = " MB"
) : SliderSetting(setting, titleId, descriptionId, min, max, units)
