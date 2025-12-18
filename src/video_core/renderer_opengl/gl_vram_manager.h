// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <vector>
#include "common/common_types.h"

namespace OpenGL {

/**
 * VRAM Manager - Memory Cap System
 * 
 * Prevents Android from killing the app due to excessive VRAM usage.
 * Actively monitors and enforces VRAM usage limits based on device capabilities.
 * 
 * Features:
 * - Configurable VRAM cap (default: 1.5GB for 4GB devices)
 * - Real-time usage monitoring
 * - Automatic cleanup when approaching limit
 * - Emergency purge when exceeding limit
 * - Device-tier based configuration
 * - Callback system for memory pressure events
 */
class VRAMManager {
public:
    enum class MemoryPressure {
        None,       // <60% of cap
        Low,        // 60-75% of cap
        Medium,     // 75-85% of cap
        High,       // 85-95% of cap
        Critical,   // >95% of cap
    };
    
    enum class DeviceTier {
        LowEnd,     // 3GB RAM
        MidRange,   // 4GB RAM
        HighEnd,    // 6GB+ RAM
        Flagship,   // 8GB+ RAM
    };
    
    struct Config {
        // VRAM cap in bytes
        u64 vram_cap_bytes = 1536 * 1024 * 1024;  // 1.5GB default
        
        // Device tier (auto-detected or manual)
        DeviceTier device_tier = DeviceTier::MidRange;
        
        // Pressure thresholds (percentage of cap)
        float low_pressure_threshold = 0.60f;      // 60%
        float medium_pressure_threshold = 0.75f;   // 75%
        float high_pressure_threshold = 0.85f;     // 85%
        float critical_pressure_threshold = 0.95f; // 95%
        
        // Action thresholds
        u64 cleanup_threshold_bytes = 1280 * 1024 * 1024;  // 1.25GB
        u64 emergency_threshold_bytes = 1460 * 1024 * 1024; // 1.43GB
        
        // Enable automatic cleanup
        bool enable_auto_cleanup = true;
        
        // Enable emergency purge
        bool enable_emergency_purge = true;
        
        // Logging interval (frames)
        u32 log_interval_frames = 300;  // Every 5 seconds at 60fps
    };
    
    using CleanupCallback = std::function<u64()>;  // Returns bytes freed
    using EmergencyCallback = std::function<void()>;
    
    explicit VRAMManager(Config config = {});
    ~VRAMManager();
    
    // Update current VRAM usage
    void UpdateUsage(u64 current_vram_bytes);
    
    // Register callbacks
    void RegisterCleanupCallback(CleanupCallback callback);
    void RegisterEmergencyCallback(EmergencyCallback callback);
    
    // Manual operations
    void RequestCleanup();
    void ForceEmergencyPurge();
    
    // Query methods
    u64 GetCurrentUsage() const { return current_usage_; }
    u64 GetVRAMCap() const { return config_.vram_cap_bytes; }
    float GetUsagePercentage() const;
    MemoryPressure GetMemoryPressure() const;
    bool IsOverLimit() const;
    u64 GetAvailableVRAM() const;
    
    // Allocation check
    bool CanAllocate(u64 size_bytes) const;
    
    // Tick frame
    void TickFrame();
    
    // Statistics
    struct Stats {
        u64 current_usage_mb = 0;
        u64 vram_cap_mb = 0;
        float usage_percentage = 0.0f;
        MemoryPressure pressure_level = MemoryPressure::None;
        u32 cleanup_count = 0;
        u32 emergency_purge_count = 0;
        u64 total_bytes_freed = 0;
        u32 current_frame = 0;
    };
    Stats GetStats() const;
    
    // Auto-detect device tier based on system RAM
    static DeviceTier DetectDeviceTier();
    
    // Get recommended config for device tier
    static Config GetRecommendedConfig(DeviceTier tier);
    
private:
    Config config_;
    
    u64 current_usage_ = 0;
    u64 peak_usage_ = 0;
    u32 current_frame_ = 0;
    
    // Callbacks
    std::vector<CleanupCallback> cleanup_callbacks_;
    std::vector<EmergencyCallback> emergency_callbacks_;
    
    // Statistics
    u32 cleanup_count_ = 0;
    u32 emergency_purge_count_ = 0;
    u64 total_bytes_freed_ = 0;
    
    // State tracking
    MemoryPressure last_pressure_ = MemoryPressure::None;
    u32 last_cleanup_frame_ = 0;
    u32 last_emergency_frame_ = 0;
    
    // Helper methods
    MemoryPressure CalculatePressure() const;
    void HandlePressureChange(MemoryPressure new_pressure);
    void ExecuteCleanup();
    void ExecuteEmergencyPurge();
    bool ShouldCleanup() const;
    bool ShouldEmergencyPurge() const;
};

} // namespace OpenGL
