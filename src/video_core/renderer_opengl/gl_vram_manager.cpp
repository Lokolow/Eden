// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <fstream>
#include "common/logging/log.h"
#include "video_core/renderer_opengl/gl_vram_manager.h"

namespace OpenGL {

VRAMManager::VRAMManager(Config config) : config_(config) {
    LOG_INFO(Render_OpenGL, "VRAMManager initialized:");
    LOG_INFO(Render_OpenGL, "  VRAM Cap: {}MB", config_.vram_cap_bytes / 1024 / 1024);
    LOG_INFO(Render_OpenGL, "  Device Tier: {}", static_cast<int>(config_.device_tier));
    LOG_INFO(Render_OpenGL, "  Auto Cleanup: {}", config_.enable_auto_cleanup);
    LOG_INFO(Render_OpenGL, "  Emergency Purge: {}", config_.enable_emergency_purge);
}

VRAMManager::~VRAMManager() {
    const auto stats = GetStats();
    LOG_INFO(Render_OpenGL, 
             "VRAMManager destroyed - Peak: {}MB, Cleanups: {}, Emergency: {}, Freed: {}MB",
             peak_usage_ / 1024 / 1024, stats.cleanup_count, 
             stats.emergency_purge_count, stats.total_bytes_freed / 1024 / 1024);
}

void VRAMManager::UpdateUsage(u64 current_vram_bytes) {
    current_usage_ = current_vram_bytes;
    
    if (current_usage_ > peak_usage_) {
        peak_usage_ = current_usage_;
    }
    
    // Check for pressure changes
    const auto new_pressure = CalculatePressure();
    if (new_pressure != last_pressure_) {
        HandlePressureChange(new_pressure);
        last_pressure_ = new_pressure;
    }
    
    // Check if we need cleanup
    if (ShouldCleanup()) {
        ExecuteCleanup();
    }
    
    // Check if we need emergency purge
    if (ShouldEmergencyPurge()) {
        ExecuteEmergencyPurge();
    }
}

void VRAMManager::RegisterCleanupCallback(CleanupCallback callback) {
    cleanup_callbacks_.push_back(callback);
}

void VRAMManager::RegisterEmergencyCallback(EmergencyCallback callback) {
    emergency_callbacks_.push_back(callback);
}

void VRAMManager::RequestCleanup() {
    ExecuteCleanup();
}

void VRAMManager::ForceEmergencyPurge() {
    ExecuteEmergencyPurge();
}

float VRAMManager::GetUsagePercentage() const {
    return static_cast<float>(current_usage_) / static_cast<float>(config_.vram_cap_bytes);
}

VRAMManager::MemoryPressure VRAMManager::GetMemoryPressure() const {
    return CalculatePressure();
}

bool VRAMManager::IsOverLimit() const {
    return current_usage_ > config_.vram_cap_bytes;
}

u64 VRAMManager::GetAvailableVRAM() const {
    if (current_usage_ >= config_.vram_cap_bytes) {
        return 0;
    }
    return config_.vram_cap_bytes - current_usage_;
}

bool VRAMManager::CanAllocate(u64 size_bytes) const {
    return (current_usage_ + size_bytes) <= config_.vram_cap_bytes;
}

void VRAMManager::TickFrame() {
    current_frame_++;
    
    // Log statistics periodically
    if (current_frame_ % config_.log_interval_frames == 0) {
        const auto stats = GetStats();
        const auto pressure_names = std::array{
            "None", "Low", "Medium", "High", "Critical"
        };
        
        LOG_DEBUG(Render_OpenGL,
                  "VRAM Manager - Usage: {}MB / {}MB ({:.1f}%), Pressure: {}, Available: {}MB",
                  stats.current_usage_mb, stats.vram_cap_mb, 
                  stats.usage_percentage * 100.0f,
                  pressure_names[static_cast<int>(stats.pressure_level)],
                  GetAvailableVRAM() / 1024 / 1024);
    }
}

VRAMManager::Stats VRAMManager::GetStats() const {
    Stats stats;
    stats.current_usage_mb = current_usage_ / 1024 / 1024;
    stats.vram_cap_mb = config_.vram_cap_bytes / 1024 / 1024;
    stats.usage_percentage = GetUsagePercentage();
    stats.pressure_level = GetMemoryPressure();
    stats.cleanup_count = cleanup_count_;
    stats.emergency_purge_count = emergency_purge_count_;
    stats.total_bytes_freed = total_bytes_freed_;
    stats.current_frame = current_frame_;
    return stats;
}

VRAMManager::DeviceTier VRAMManager::DetectDeviceTier() {
    // Try to read system memory from /proc/meminfo
    std::ifstream meminfo("/proc/meminfo");
    if (!meminfo.is_open()) {
        LOG_WARNING(Render_OpenGL, "Could not open /proc/meminfo, defaulting to MidRange tier");
        return DeviceTier::MidRange;
    }
    
    std::string line;
    u64 mem_total_kb = 0;
    
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            sscanf(line.c_str(), "MemTotal: %lu kB", &mem_total_kb);
            break;
        }
    }
    
    const u64 mem_total_mb = mem_total_kb / 1024;
    
    LOG_INFO(Render_OpenGL, "Detected system RAM: {}MB", mem_total_mb);
    
    // Classify device tier
    if (mem_total_mb <= 3072) {  // <= 3GB
        return DeviceTier::LowEnd;
    } else if (mem_total_mb <= 4608) {  // <= 4.5GB
        return DeviceTier::MidRange;
    } else if (mem_total_mb <= 6656) {  // <= 6.5GB
        return DeviceTier::HighEnd;
    } else {  // > 6.5GB
        return DeviceTier::Flagship;
    }
}

VRAMManager::Config VRAMManager::GetRecommendedConfig(DeviceTier tier) {
    Config config;
    
    switch (tier) {
        case DeviceTier::LowEnd:
            // 3GB devices - Very conservative
            config.vram_cap_bytes = 1024 * 1024 * 1024;  // 1GB
            config.cleanup_threshold_bytes = 870 * 1024 * 1024;  // 870MB
            config.emergency_threshold_bytes = 970 * 1024 * 1024; // 970MB
            config.low_pressure_threshold = 0.50f;
            config.medium_pressure_threshold = 0.65f;
            config.high_pressure_threshold = 0.80f;
            config.critical_pressure_threshold = 0.90f;
            LOG_INFO(Render_OpenGL, "VRAM Config: LowEnd (1GB cap)");
            break;
            
        case DeviceTier::MidRange:
            // 4GB devices - Balanced
            config.vram_cap_bytes = 1536 * 1024 * 1024;  // 1.5GB
            config.cleanup_threshold_bytes = 1280 * 1024 * 1024;  // 1.25GB
            config.emergency_threshold_bytes = 1460 * 1024 * 1024; // 1.43GB
            config.low_pressure_threshold = 0.60f;
            config.medium_pressure_threshold = 0.75f;
            config.high_pressure_threshold = 0.85f;
            config.critical_pressure_threshold = 0.95f;
            LOG_INFO(Render_OpenGL, "VRAM Config: MidRange (1.5GB cap)");
            break;
            
        case DeviceTier::HighEnd:
            // 6GB devices - Generous
            config.vram_cap_bytes = 2048 * 1024 * 1024;  // 2GB
            config.cleanup_threshold_bytes = 1740 * 1024 * 1024;  // 1.7GB
            config.emergency_threshold_bytes = 1940 * 1024 * 1024; // 1.9GB
            config.low_pressure_threshold = 0.65f;
            config.medium_pressure_threshold = 0.80f;
            config.high_pressure_threshold = 0.90f;
            config.critical_pressure_threshold = 0.95f;
            LOG_INFO(Render_OpenGL, "VRAM Config: HighEnd (2GB cap)");
            break;
            
        case DeviceTier::Flagship:
            // 8GB+ devices - Maximum
            config.vram_cap_bytes = 3072 * 1024 * 1024;  // 3GB
            config.cleanup_threshold_bytes = 2600 * 1024 * 1024;  // 2.5GB
            config.emergency_threshold_bytes = 2900 * 1024 * 1024; // 2.8GB
            config.low_pressure_threshold = 0.70f;
            config.medium_pressure_threshold = 0.85f;
            config.high_pressure_threshold = 0.92f;
            config.critical_pressure_threshold = 0.95f;
            LOG_INFO(Render_OpenGL, "VRAM Config: Flagship (3GB cap)");
            break;
    }
    
    config.device_tier = tier;
    return config;
}

VRAMManager::MemoryPressure VRAMManager::CalculatePressure() const {
    const float usage = GetUsagePercentage();
    
    if (usage >= config_.critical_pressure_threshold) {
        return MemoryPressure::Critical;
    } else if (usage >= config_.high_pressure_threshold) {
        return MemoryPressure::High;
    } else if (usage >= config_.medium_pressure_threshold) {
        return MemoryPressure::Medium;
    } else if (usage >= config_.low_pressure_threshold) {
        return MemoryPressure::Low;
    }
    
    return MemoryPressure::None;
}

void VRAMManager::HandlePressureChange(MemoryPressure new_pressure) {
    const auto pressure_names = std::array{
        "None", "Low", "Medium", "High", "Critical"
    };
    
    LOG_INFO(Render_OpenGL, "Memory pressure changed: {} -> {} ({:.1f}%)",
             pressure_names[static_cast<int>(last_pressure_)],
             pressure_names[static_cast<int>(new_pressure)],
             GetUsagePercentage() * 100.0f);
    
    // Take action based on new pressure level
    if (new_pressure >= MemoryPressure::High) {
        LOG_WARNING(Render_OpenGL, "High memory pressure detected! Requesting cleanup...");
        ExecuteCleanup();
    }
    
    if (new_pressure == MemoryPressure::Critical) {
        LOG_ERROR(Render_OpenGL, "CRITICAL memory pressure! Executing emergency purge!");
        ExecuteEmergencyPurge();
    }
}

void VRAMManager::ExecuteCleanup() {
    if (!config_.enable_auto_cleanup) {
        return;
    }
    
    // Don't cleanup too frequently (minimum 60 frames = 1 second)
    if (current_frame_ - last_cleanup_frame_ < 60) {
        return;
    }
    
    LOG_INFO(Render_OpenGL, "Executing VRAM cleanup - Current: {}MB / {}MB",
             current_usage_ / 1024 / 1024, config_.vram_cap_bytes / 1024 / 1024);
    
    u64 total_freed = 0;
    
    // Execute all cleanup callbacks
    for (const auto& callback : cleanup_callbacks_) {
        const u64 freed = callback();
        total_freed += freed;
        LOG_DEBUG(Render_OpenGL, "Cleanup callback freed: {}MB", freed / 1024 / 1024);
    }
    
    cleanup_count_++;
    total_bytes_freed_ += total_freed;
    last_cleanup_frame_ = current_frame_;
    
    LOG_INFO(Render_OpenGL, "Cleanup completed - Freed: {}MB, New usage: {}MB",
             total_freed / 1024 / 1024, current_usage_ / 1024 / 1024);
}

void VRAMManager::ExecuteEmergencyPurge() {
    if (!config_.enable_emergency_purge) {
        return;
    }
    
    // Don't purge too frequently (minimum 120 frames = 2 seconds)
    if (current_frame_ - last_emergency_frame_ < 120) {
        return;
    }
    
    LOG_ERROR(Render_OpenGL, "EMERGENCY PURGE! VRAM usage: {}MB / {}MB ({}%)",
              current_usage_ / 1024 / 1024, 
              config_.vram_cap_bytes / 1024 / 1024,
              GetUsagePercentage() * 100.0f);
    
    // Execute emergency callbacks
    for (const auto& callback : emergency_callbacks_) {
        callback();
    }
    
    // Also run regular cleanup
    ExecuteCleanup();
    
    emergency_purge_count_++;
    last_emergency_frame_ = current_frame_;
    
    LOG_WARNING(Render_OpenGL, "Emergency purge completed");
}

bool VRAMManager::ShouldCleanup() const {
    return config_.enable_auto_cleanup && 
           current_usage_ >= config_.cleanup_threshold_bytes &&
           (current_frame_ - last_cleanup_frame_) >= 60;
}

bool VRAMManager::ShouldEmergencyPurge() const {
    return config_.enable_emergency_purge && 
           current_usage_ >= config_.emergency_threshold_bytes &&
           (current_frame_ - last_emergency_frame_) >= 120;
}

} // namespace OpenGL
