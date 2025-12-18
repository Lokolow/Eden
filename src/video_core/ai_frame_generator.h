// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include "common/common_types.h"

namespace VideoCore {

class GPU;

enum class AIFrameGenMode : u32 {
    Disabled = 0,
    Conservative = 1,  // Low power, safe for weak CPUs
    Balanced = 2,      // Good balance for mid-range devices
    Aggressive = 3,    // Maximum performance for powerful CPUs
    Adaptive = 4,      // Auto-adjust based on CPU load
};

enum class CPUArchitecture : u32 {
    Unknown = 0,
    ARM_Cortex_A53 = 1,  // Low-end (Android 9 era)
    ARM_Cortex_A55 = 2,  // Entry level
    ARM_Cortex_A73 = 3,  // Mid-range
    ARM_Cortex_A75 = 4,  // High-end
    ARM_Cortex_A76 = 5,  // Flagship
    ARM_Cortex_A77 = 6,  // Latest gen
    ARM_Cortex_X1 = 7,   // Ultra high-end
    ARM_Cortex_X2 = 8,
    ARM_Custom = 9,      // Snapdragon, Exynos, etc.
    x86_64 = 10,
};

struct CPUInfo {
    CPUArchitecture arch;
    u32 core_count;
    u32 big_cores;
    u32 little_cores;
    u64 max_freq_mhz;
    u32 ram_mb;
    bool has_neon;
    bool has_sve;
    std::string cpu_model;
};

struct FrameGenStats {
    u64 frames_generated;
    u64 frames_skipped;
    u64 frames_interpolated;
    f32 current_fps;
    f32 target_fps;
    f32 cpu_usage_percent;
    f32 gpu_usage_percent;
    u32 ram_usage_mb;
    f32 frame_time_ms;
    f32 interpolation_quality;
};

/**
 * AI Frame Generator - Intelligent frame interpolation system
 * Optimized for low-end Android devices (4GB RAM, Android 9+)
 * Uses CPU architecture detection and adaptive algorithms
 */
class AIFrameGenerator {
public:
    explicit AIFrameGenerator(GPU& gpu);
    ~AIFrameGenerator();

    // Initialization
    void Initialize();
    void Shutdown();

    // Configuration
    void SetMode(AIFrameGenMode mode);
    AIFrameGenMode GetMode() const { return current_mode; }
    
    void Enable(bool enable);
    bool IsEnabled() const { return enabled; }

    // Frame processing
    void ProcessFrame(const u8* frame_data, u32 width, u32 height);
    bool ShouldGenerateFrame() const;
    
    // CPU detection and optimization
    CPUInfo DetectCPU();
    void OptimizeForCPU(const CPUInfo& cpu_info);
    
    // Performance monitoring
    FrameGenStats GetStats() const;
    void ResetStats();
    
    // Adaptive mode
    void UpdateAdaptiveMode();
    void SetTargetFPS(u32 target_fps);

    // Memory management for 4GB RAM devices
    void OptimizeMemoryUsage();
    void SetMemoryLimit(u32 limit_mb);

    // ARM NEON optimizations
    bool HasNEONSupport() const { return cpu_info.has_neon; }
    void EnableNEONOptimizations(bool enable);

private:
    // Core interpolation algorithms
    void InterpolateFrameSimple(const u8* prev, const u8* next, u8* output);
    void InterpolateFrameAdvanced(const u8* prev, const u8* next, u8* output);
    void InterpolateFrameNEON(const u8* prev, const u8* next, u8* output);
    
    // Motion estimation
    void EstimateMotion(const u8* prev, const u8* next);
    void ApplyMotionCompensation(u8* output);
    
    // Adaptive algorithms
    void AnalyzeFrameComplexity(const u8* frame);
    void AdjustQuality();
    void MonitorPerformance();
    
    // CPU-specific optimizations
    void OptimizeForCortexA53();
    void OptimizeForCortexA55();
    void OptimizeForCortexA73();
    void OptimizeForHighEnd();
    
    // Memory optimization
    void CompressFrameBuffer();
    void PruneOldFrames();
    void UseReducedPrecision();

    // Threading
    void WorkerThread();
    void DistributeWorkload();

private:
    GPU& gpu;
    CPUInfo cpu_info;
    
    std::atomic<bool> enabled{false};
    std::atomic<bool> running{false};
    std::atomic<AIFrameGenMode> current_mode{AIFrameGenMode::Adaptive};
    
    // Frame buffers (optimized for 4GB RAM)
    std::vector<u8> frame_buffer_prev;
    std::vector<u8> frame_buffer_curr;
    std::vector<u8> frame_buffer_next;
    std::vector<u8> interpolated_frame;
    
    // Motion vectors (reduced precision for memory)
    std::vector<s16> motion_vectors_x;
    std::vector<s16> motion_vectors_y;
    
    // Statistics
    FrameGenStats stats{};
    
    // Configuration
    u32 target_fps{60};
    u32 memory_limit_mb{512};  // Conservative for 4GB devices
    u32 frame_width{1280};
    u32 frame_height{720};
    
    // Adaptive parameters
    f32 complexity_threshold{0.5f};
    f32 quality_factor{0.7f};
    u32 consecutive_frames{0};
    
    // NEON optimization
    bool use_neon{false};
    bool use_advanced_interpolation{true};
    
    // Performance monitoring
    std::atomic<f32> cpu_load{0.0f};
    std::atomic<f32> frame_time{16.6f};
    u64 last_frame_time{0};
};

} // namespace VideoCore
