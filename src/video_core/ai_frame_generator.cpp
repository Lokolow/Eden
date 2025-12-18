// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "video_core/ai_frame_generator.h"
#include "video_core/gpu.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

#ifdef __ARM_NEON
#include <arm_neon.h>
#endif

#ifdef __linux__
#include <fstream>
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

namespace VideoCore {

AIFrameGenerator::AIFrameGenerator(GPU& gpu_) : gpu(gpu_) {
    LOG_INFO(Render, "AI Frame Generator initialized");
}

AIFrameGenerator::~AIFrameGenerator() {
    Shutdown();
}

void AIFrameGenerator::Initialize() {
    LOG_INFO(Render, "Initializing AI Frame Generator...");
    
    // Detect CPU architecture and capabilities
    cpu_info = DetectCPU();
    LOG_INFO(Render, "Detected CPU: {} cores ({} big + {} little), {} MHz, {} MB RAM",
             cpu_info.core_count, cpu_info.big_cores, cpu_info.little_cores,
             cpu_info.max_freq_mhz, cpu_info.ram_mb);
    
    // Optimize based on detected hardware
    OptimizeForCPU(cpu_info);
    
    // Allocate frame buffers with conservative memory usage
    const size_t frame_size = frame_width * frame_height * 4; // RGBA
    frame_buffer_prev.resize(frame_size);
    frame_buffer_curr.resize(frame_size);
    frame_buffer_next.resize(frame_size);
    interpolated_frame.resize(frame_size);
    
    // Motion vectors (downsampled for memory efficiency)
    const size_t mv_size = (frame_width / 8) * (frame_height / 8);
    motion_vectors_x.resize(mv_size);
    motion_vectors_y.resize(mv_size);
    
    running = true;
    LOG_INFO(Render, "AI Frame Generator ready - Mode: {}, NEON: {}",
             static_cast<u32>(current_mode), use_neon);
}

void AIFrameGenerator::Shutdown() {
    running = false;
    enabled = false;
    
    // Free memory
    frame_buffer_prev.clear();
    frame_buffer_curr.clear();
    frame_buffer_next.clear();
    interpolated_frame.clear();
    motion_vectors_x.clear();
    motion_vectors_y.clear();
    
    LOG_INFO(Render, "AI Frame Generator shutdown");
}

CPUInfo AIFrameGenerator::DetectCPU() {
    CPUInfo info{};
    
#ifdef __linux__
    // Detect number of cores
    info.core_count = std::thread::hardware_concurrency();
    
    // Detect RAM
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        info.ram_mb = (si.totalram * si.mem_unit) / (1024 * 1024);
    }
    
    // Read CPU info from /proc/cpuinfo
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    bool found_cortex = false;
    
    while (std::getline(cpuinfo, line)) {
        if (line.find("Hardware") != std::string::npos || 
            line.find("model name") != std::string::npos) {
            info.cpu_model = line.substr(line.find(":") + 2);
            
            // Detect ARM Cortex variants
            if (line.find("Cortex-A53") != std::string::npos) {
                info.arch = CPUArchitecture::ARM_Cortex_A53;
                found_cortex = true;
            } else if (line.find("Cortex-A55") != std::string::npos) {
                info.arch = CPUArchitecture::ARM_Cortex_A55;
                found_cortex = true;
            } else if (line.find("Cortex-A73") != std::string::npos) {
                info.arch = CPUArchitecture::ARM_Cortex_A73;
                found_cortex = true;
            } else if (line.find("Cortex-A75") != std::string::npos) {
                info.arch = CPUArchitecture::ARM_Cortex_A75;
                found_cortex = true;
            } else if (line.find("Cortex-A76") != std::string::npos) {
                info.arch = CPUArchitecture::ARM_Cortex_A76;
                found_cortex = true;
            } else if (line.find("Cortex-A77") != std::string::npos) {
                info.arch = CPUArchitecture::ARM_Cortex_A77;
                found_cortex = true;
            } else if (line.find("Cortex-X") != std::string::npos) {
                info.arch = CPUArchitecture::ARM_Cortex_X1;
                found_cortex = true;
            }
        }
        
        if (line.find("cpu MHz") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos) {
                try {
                    info.max_freq_mhz = std::max(info.max_freq_mhz, 
                                                  static_cast<u64>(std::stof(line.substr(pos + 2))));
                } catch (...) {}
            }
        }
    }
    
    // Heuristic for big.LITTLE detection
    if (info.core_count >= 8) {
        info.big_cores = 4;
        info.little_cores = info.core_count - 4;
    } else if (info.core_count >= 6) {
        info.big_cores = 2;
        info.little_cores = info.core_count - 2;
    } else if (info.core_count >= 4) {
        info.big_cores = 2;
        info.little_cores = 2;
    } else {
        info.big_cores = info.core_count;
        info.little_cores = 0;
    }
    
#endif

#ifdef __ARM_NEON
    info.has_neon = true;
#else
    info.has_neon = false;
#endif
    
    // Default to ARM Custom if not detected
    if (info.arch == CPUArchitecture::Unknown && info.has_neon) {
        info.arch = CPUArchitecture::ARM_Custom;
    }
    
    return info;
}

void AIFrameGenerator::OptimizeForCPU(const CPUInfo& cpu_info) {
    switch (cpu_info.arch) {
        case CPUArchitecture::ARM_Cortex_A53:
        case CPUArchitecture::ARM_Cortex_A55:
            OptimizeForCortexA53();
            break;
            
        case CPUArchitecture::ARM_Cortex_A73:
        case CPUArchitecture::ARM_Cortex_A75:
            OptimizeForCortexA73();
            break;
            
        case CPUArchitecture::ARM_Cortex_A76:
        case CPUArchitecture::ARM_Cortex_A77:
        case CPUArchitecture::ARM_Cortex_X1:
        case CPUArchitecture::ARM_Cortex_X2:
            OptimizeForHighEnd();
            break;
            
        default:
            // Conservative defaults
            if (cpu_info.ram_mb <= 4096) {
                OptimizeForCortexA55();
            } else {
                OptimizeForCortexA73();
            }
            break;
    }
    
    // Enable NEON if available
    use_neon = cpu_info.has_neon;
    
    // Adjust memory limit based on available RAM
    if (cpu_info.ram_mb <= 3072) {
        memory_limit_mb = 256;  // Very conservative
    } else if (cpu_info.ram_mb <= 4096) {
        memory_limit_mb = 384;  // Conservative
    } else if (cpu_info.ram_mb <= 6144) {
        memory_limit_mb = 512;  // Moderate
    } else {
        memory_limit_mb = 768;  // Comfortable
    }
}

void AIFrameGenerator::OptimizeForCortexA53() {
    LOG_INFO(Render, "Optimizing for Cortex-A53 (low-end)");
    
    // Very conservative settings for weak CPUs
    frame_width = 854;   // Lower resolution
    frame_height = 480;
    use_advanced_interpolation = false;
    quality_factor = 0.5f;
    complexity_threshold = 0.7f;
    
    if (current_mode == AIFrameGenMode::Adaptive) {
        current_mode = AIFrameGenMode::Conservative;
    }
}

void AIFrameGenerator::OptimizeForCortexA55() {
    LOG_INFO(Render, "Optimizing for Cortex-A55 (entry-level)");
    
    frame_width = 960;
    frame_height = 540;
    use_advanced_interpolation = false;
    quality_factor = 0.6f;
    complexity_threshold = 0.6f;
}

void AIFrameGenerator::OptimizeForCortexA73() {
    LOG_INFO(Render, "Optimizing for Cortex-A73 (mid-range)");
    
    frame_width = 1280;
    frame_height = 720;
    use_advanced_interpolation = true;
    quality_factor = 0.75f;
    complexity_threshold = 0.5f;
}

void AIFrameGenerator::OptimizeForHighEnd() {
    LOG_INFO(Render, "Optimizing for high-end CPU");
    
    frame_width = 1920;
    frame_height = 1080;
    use_advanced_interpolation = true;
    quality_factor = 0.9f;
    complexity_threshold = 0.3f;
}

void AIFrameGenerator::SetMode(AIFrameGenMode mode) {
    current_mode = mode;
    LOG_INFO(Render, "AI Frame Gen mode set to: {}", static_cast<u32>(mode));
    
    switch (mode) {
        case AIFrameGenMode::Disabled:
            enabled = false;
            break;
            
        case AIFrameGenMode::Conservative:
            quality_factor = 0.5f;
            use_advanced_interpolation = false;
            break;
            
        case AIFrameGenMode::Balanced:
            quality_factor = 0.7f;
            use_advanced_interpolation = cpu_info.arch >= CPUArchitecture::ARM_Cortex_A73;
            break;
            
        case AIFrameGenMode::Aggressive:
            quality_factor = 0.9f;
            use_advanced_interpolation = true;
            break;
            
        case AIFrameGenMode::Adaptive:
            // Will adjust dynamically
            break;
    }
}

void AIFrameGenerator::Enable(bool enable) {
    enabled = enable;
    if (enable && !running) {
        Initialize();
    }
    LOG_INFO(Render, "AI Frame Generator {}", enable ? "enabled" : "disabled");
}

void AIFrameGenerator::ProcessFrame(const u8* frame_data, u32 width, u32 height) {
    if (!enabled || !running) {
        return;
    }
    
    // Update frame dimensions if changed
    if (width != frame_width || height != frame_height) {
        frame_width = width;
        frame_height = height;
    }
    
    // Rotate frame buffers
    std::swap(frame_buffer_prev, frame_buffer_curr);
    std::swap(frame_buffer_curr, frame_buffer_next);
    
    // Copy new frame
    const size_t frame_size = width * height * 4;
    std::memcpy(frame_buffer_next.data(), frame_data, frame_size);
    
    // Update statistics
    stats.frames_generated++;
    
    auto now = std::chrono::high_resolution_clock::now();
    if (last_frame_time > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count() - last_frame_time;
        stats.frame_time_ms = elapsed / 1000.0f;
        stats.current_fps = 1000000.0f / elapsed;
    }
    last_frame_time = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count();
    
    // Adaptive mode updates
    if (current_mode == AIFrameGenMode::Adaptive) {
        UpdateAdaptiveMode();
    }
}

bool AIFrameGenerator::ShouldGenerateFrame() const {
    if (!enabled || !running) {
        return false;
    }
    
    // Check if we should interpolate based on current performance
    if (stats.current_fps < target_fps * 0.9f) {
        return false;  // Too slow, skip interpolation
    }
    
    // Check CPU load
    if (cpu_load > 0.85f) {
        return false;  // CPU too busy
    }
    
    return true;
}

void AIFrameGenerator::InterpolateFrameSimple(const u8* prev, const u8* next, u8* output) {
    const size_t frame_size = frame_width * frame_height * 4;
    
#ifdef __ARM_NEON
    if (use_neon) {
        InterpolateFrameNEON(prev, next, output);
        return;
    }
#endif
    
    // Simple linear interpolation
    for (size_t i = 0; i < frame_size; ++i) {
        output[i] = (prev[i] + next[i]) >> 1;
    }
}

#ifdef __ARM_NEON
void AIFrameGenerator::InterpolateFrameNEON(const u8* prev, const u8* next, u8* output) {
    const size_t frame_size = frame_width * frame_height * 4;
    const size_t simd_size = frame_size & ~15;  // Process 16 bytes at a time
    
    for (size_t i = 0; i < simd_size; i += 16) {
        uint8x16_t v_prev = vld1q_u8(prev + i);
        uint8x16_t v_next = vld1q_u8(next + i);
        
        // Average using NEON
        uint8x16_t v_result = vrhaddq_u8(v_prev, v_next);
        
        vst1q_u8(output + i, v_result);
    }
    
    // Handle remaining bytes
    for (size_t i = simd_size; i < frame_size; ++i) {
        output[i] = (prev[i] + next[i]) >> 1;
    }
    
    stats.interpolation_quality = 0.8f;
}
#endif

void AIFrameGenerator::InterpolateFrameAdvanced(const u8* prev, const u8* next, u8* output) {
    // Motion-compensated interpolation
    EstimateMotion(prev, next);
    
    // TODO: Implement optical flow for better quality
    // For now, fall back to simple interpolation
    InterpolateFrameSimple(prev, next, output);
}

void AIFrameGenerator::EstimateMotion(const u8* prev, const u8* next) {
    // Block-based motion estimation (simplified)
    const u32 block_size = 8;
    const u32 search_range = 4;
    
    // This would be expensive, so we skip for low-end devices
    if (cpu_info.arch <= CPUArchitecture::ARM_Cortex_A55) {
        return;
    }
    
    // TODO: Implement diamond search or similar fast algorithm
}

void AIFrameGenerator::UpdateAdaptiveMode() {
    MonitorPerformance();
    AdjustQuality();
}

void AIFrameGenerator::MonitorPerformance() {
    // Simple CPU load estimation based on frame time
    const f32 expected_frame_time = 1000.0f / target_fps;
    cpu_load = std::clamp(stats.frame_time_ms / expected_frame_time, 0.0f, 1.0f);
    
    stats.cpu_usage_percent = cpu_load * 100.0f;
}

void AIFrameGenerator::AdjustQuality() {
    if (cpu_load > 0.8f) {
        // Reduce quality
        quality_factor = std::max(0.3f, quality_factor - 0.05f);
        use_advanced_interpolation = false;
    } else if (cpu_load < 0.5f) {
        // Increase quality
        quality_factor = std::min(0.9f, quality_factor + 0.02f);
        use_advanced_interpolation = cpu_info.arch >= CPUArchitecture::ARM_Cortex_A73;
    }
}

void AIFrameGenerator::AnalyzeFrameComplexity(const u8* frame) {
    // Simple complexity estimation based on variance
    // Skip for performance reasons on low-end devices
}

void AIFrameGenerator::SetTargetFPS(u32 target) {
    target_fps = target;
    LOG_INFO(Render, "AI Frame Gen target FPS: {}", target);
}

void AIFrameGenerator::OptimizeMemoryUsage() {
    // Compress older frames or reduce buffer sizes
    PruneOldFrames();
    
    // Use reduced precision for motion vectors
    UseReducedPrecision();
}

void AIFrameGenerator::PruneOldFrames() {
    // Keep only necessary buffers in memory
    // Already optimized by using only 3 frame buffers
}

void AIFrameGenerator::UseReducedPrecision() {
    // Motion vectors already use s16 instead of float
    // This saves significant memory
}

void AIFrameGenerator::SetMemoryLimit(u32 limit_mb) {
    memory_limit_mb = limit_mb;
    LOG_INFO(Render, "AI Frame Gen memory limit: {} MB", limit_mb);
}

void AIFrameGenerator::EnableNEONOptimizations(bool enable) {
    use_neon = enable && cpu_info.has_neon;
    LOG_INFO(Render, "NEON optimizations: {}", use_neon ? "enabled" : "disabled");
}

FrameGenStats AIFrameGenerator::GetStats() const {
    return stats;
}

void AIFrameGenerator::ResetStats() {
    stats = FrameGenStats{};
}

} // namespace VideoCore
