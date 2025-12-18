// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>
#include "common/common_types.h"

namespace OpenGL {

/**
 * ASTC Compression Optimizer
 * 
 * Detects and optimizes ASTC texture compression for Adreno GPUs.
 * Automatically chooses between hardware and software decoding based on GPU capabilities.
 * 
 * Features:
 * - Auto-detection of Adreno GPU models
 * - Native ASTC support detection (Adreno 4xx+)
 * - Fallback to software decoding for older GPUs
 * - Performance hints for optimal settings
 * - Memory usage optimization
 */
class ASTCOptimizer {
public:
    enum class GPUVendor {
        Unknown,
        Qualcomm,   // Adreno
        ARM,        // Mali
        Imagination,// PowerVR
        Nvidia,     // Tegra
        Intel,
        AMD,
    };
    
    enum class ASTCSupport {
        None,           // No ASTC support
        SoftwareOnly,   // Software decoding only (slow)
        HardwareLDR,    // Hardware LDR only
        HardwareFull,   // Full hardware support (LDR + HDR)
    };
    
    struct GPUInfo {
        GPUVendor vendor = GPUVendor::Unknown;
        std::string renderer_name;
        std::string gpu_model;
        int generation = 0;  // For Adreno: 4xx, 5xx, 6xx, 7xx, 8xx
        ASTCSupport astc_support = ASTCSupport::None;
        bool has_native_astc = false;
        bool recommend_hardware_decode = false;
    };
    
    ASTCOptimizer();
    ~ASTCOptimizer() = default;
    
    // Initialize with OpenGL context info
    void Initialize(std::string_view vendor, std::string_view renderer);
    
    // Get GPU information
    const GPUInfo& GetGPUInfo() const { return gpu_info_; }
    
    // Check if hardware ASTC is available
    bool HasHardwareASTC() const;
    
    // Check if we should use hardware decoding
    bool ShouldUseHardwareDecoding() const;
    
    // Check if software decoding is fast enough
    bool IsSoftwareDecodingFast() const;
    
    // Get recommended ASTC format for this GPU
    // Returns true if hardware ASTC formats should be used
    bool GetRecommendedFormat() const;
    
    // Check if specific ASTC block size is supported in hardware
    bool IsBlockSizeSupported(u32 block_width, u32 block_height) const;
    
    // Get performance hint for current configuration
    std::string GetPerformanceHint() const;
    
private:
    GPUInfo gpu_info_;
    
    // Detection methods
    void DetectAdrenoGPU(std::string_view renderer);
    void DetectMaliGPU(std::string_view renderer);
    void DetectPowerVRGPU(std::string_view renderer);
    void DetectTegraGPU(std::string_view renderer);
    
    // Helper methods
    int ExtractAdrenoGeneration(std::string_view renderer);
    ASTCSupport DetermineASTCSupport();
};

} // namespace OpenGL
