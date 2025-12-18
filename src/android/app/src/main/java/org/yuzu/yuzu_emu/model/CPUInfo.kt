// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.model

data class CPUInfo(
    val architecture: Int,
    val coreCount: Int,
    val bigCores: Int,
    val maxFreqMHz: Long,
    val ramMB: Int,
    val hasNEON: Boolean,
    val cpuModel: String
) {
    fun getArchitectureName(): String {
        return when (architecture) {
            0 -> "Unknown"
            1 -> "ARM Cortex-A53"
            2 -> "ARM Cortex-A55"
            3 -> "ARM Cortex-A73"
            4 -> "ARM Cortex-A75"
            5 -> "ARM Cortex-A76"
            6 -> "ARM Cortex-A77"
            7 -> "ARM Cortex-X1"
            8 -> "ARM Cortex-X2"
            9 -> "ARM Custom"
            10 -> "x86_64"
            else -> "Unknown"
        }
    }

    fun isLowEnd(): Boolean {
        return architecture <= 2 || ramMB <= 3072
    }

    fun isMidRange(): Boolean {
        return architecture in 3..4 && ramMB > 3072
    }

    fun isHighEnd(): Boolean {
        return architecture >= 5 && ramMB >= 6144
    }

    fun getRecommendedMode(): Int {
        return when {
            isLowEnd() -> 1  // Conservative
            isMidRange() -> 2 // Balanced
            isHighEnd() -> 3  // Aggressive
            else -> 4  // Adaptive
        }
    }
}

data class FrameGenStats(
    val framesGenerated: Long,
    val framesSkipped: Long,
    val framesInterpolated: Long,
    val currentFps: Float,
    val targetFps: Float,
    val cpuUsagePercent: Float,
    val gpuUsagePercent: Float,
    val ramUsageMB: Int,
    val frameTimeMs: Float
)
