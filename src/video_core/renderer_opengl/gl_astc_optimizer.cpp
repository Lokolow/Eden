// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cctype>
#include <sstream>
#include "common/logging/log.h"
#include "video_core/renderer_opengl/gl_astc_optimizer.h"

namespace OpenGL {

ASTCOptimizer::ASTCOptimizer() = default;

void ASTCOptimizer::Initialize(std::string_view vendor, std::string_view renderer) {
    gpu_info_.renderer_name = std::string(renderer);
    
    // Convert vendor to lowercase for comparison
    std::string vendor_lower(vendor);
    std::transform(vendor_lower.begin(), vendor_lower.end(), vendor_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    
    // Detect GPU vendor and model
    if (vendor_lower.find("qualcomm") != std::string::npos || 
        renderer.find("Adreno") != std::string::npos) {
        gpu_info_.vendor = GPUVendor::Qualcomm;
        DetectAdrenoGPU(renderer);
    } else if (vendor_lower.find("arm") != std::string::npos || 
               renderer.find("Mali") != std::string::npos) {
        gpu_info_.vendor = GPUVendor::ARM;
        DetectMaliGPU(renderer);
    } else if (renderer.find("PowerVR") != std::string::npos) {
        gpu_info_.vendor = GPUVendor::Imagination;
        DetectPowerVRGPU(renderer);
    } else if (vendor_lower.find("nvidia") != std::string::npos) {
        gpu_info_.vendor = GPUVendor::Nvidia;
        DetectTegraGPU(renderer);
    } else if (vendor_lower.find("intel") != std::string::npos) {
        gpu_info_.vendor = GPUVendor::Intel;
    } else if (vendor_lower.find("amd") != std::string::npos) {
        gpu_info_.vendor = GPUVendor::AMD;
    }
    
    // Determine ASTC support based on GPU
    gpu_info_.astc_support = DetermineASTCSupport();
    gpu_info_.has_native_astc = (gpu_info_.astc_support == ASTCSupport::HardwareLDR ||
                                  gpu_info_.astc_support == ASTCSupport::HardwareFull);
    gpu_info_.recommend_hardware_decode = gpu_info_.has_native_astc;
    
    // Log GPU information
    LOG_INFO(Render_OpenGL, "ASTC Optimizer initialized:");
    LOG_INFO(Render_OpenGL, "  Vendor: {}", static_cast<int>(gpu_info_.vendor));
    LOG_INFO(Render_OpenGL, "  Renderer: {}", gpu_info_.renderer_name);
    LOG_INFO(Render_OpenGL, "  GPU Model: {}", gpu_info_.gpu_model);
    LOG_INFO(Render_OpenGL, "  Generation: {}", gpu_info_.generation);
    LOG_INFO(Render_OpenGL, "  ASTC Support: {}", static_cast<int>(gpu_info_.astc_support));
    LOG_INFO(Render_OpenGL, "  Hardware ASTC: {}", gpu_info_.has_native_astc);
    LOG_INFO(Render_OpenGL, "  Recommendation: {}", 
             gpu_info_.recommend_hardware_decode ? "Hardware" : "Software");
}

void ASTCOptimizer::DetectAdrenoGPU(std::string_view renderer) {
    // Examples:
    // "Adreno (TM) 640"
    // "Adreno (TM) 730"
    // "Adreno 418"
    
    size_t pos = renderer.find("Adreno");
    if (pos == std::string::npos) {
        return;
    }
    
    // Extract model number
    std::string renderer_str(renderer);
    size_t num_start = renderer_str.find_first_of("0123456789", pos);
    if (num_start != std::string::npos) {
        size_t num_end = renderer_str.find_first_not_of("0123456789", num_start);
        std::string model_str = renderer_str.substr(num_start, num_end - num_start);
        
        if (!model_str.empty()) {
            int model = std::stoi(model_str);
            gpu_info_.gpu_model = "Adreno " + model_str;
            gpu_info_.generation = model / 100;  // 640 -> 6, 730 -> 7, etc
            
            LOG_INFO(Render_OpenGL, "Detected Adreno GPU: {}, Generation: {}", 
                     gpu_info_.gpu_model, gpu_info_.generation);
        }
    }
}

void ASTCOptimizer::DetectMaliGPU(std::string_view renderer) {
    // Examples:
    // "Mali-G76"
    // "Mali-G78"
    
    size_t pos = renderer.find("Mali");
    if (pos == std::string::npos) {
        return;
    }
    
    std::string renderer_str(renderer);
    size_t model_start = pos;
    size_t model_end = renderer_str.find_first_of(" \t\n\r", model_start);
    gpu_info_.gpu_model = renderer_str.substr(model_start, model_end - model_start);
    
    // Extract generation (G52, G76, etc -> 5, 7)
    size_t gen_pos = gpu_info_.gpu_model.find('G');
    if (gen_pos != std::string::npos && gen_pos + 1 < gpu_info_.gpu_model.length()) {
        char gen_char = gpu_info_.gpu_model[gen_pos + 1];
        if (std::isdigit(gen_char)) {
            gpu_info_.generation = gen_char - '0';
        }
    }
    
    LOG_INFO(Render_OpenGL, "Detected Mali GPU: {}, Generation: {}", 
             gpu_info_.gpu_model, gpu_info_.generation);
}

void ASTCOptimizer::DetectPowerVRGPU(std::string_view renderer) {
    gpu_info_.gpu_model = std::string(renderer);
    // PowerVR Series 6XT and later support ASTC
    if (renderer.find("Series") != std::string::npos) {
        size_t num_pos = renderer.find_first_of("0123456789");
        if (num_pos != std::string::npos) {
            gpu_info_.generation = renderer[num_pos] - '0';
        }
    }
}

void ASTCOptimizer::DetectTegraGPU(std::string_view renderer) {
    gpu_info_.gpu_model = std::string(renderer);
    // Tegra X1 (Gen 5) and later support ASTC
    if (renderer.find("X1") != std::string::npos) {
        gpu_info_.generation = 5;
    } else if (renderer.find("X2") != std::string::npos) {
        gpu_info_.generation = 6;
    }
}

ASTCOptimizer::ASTCSupport ASTCOptimizer::DetermineASTCSupport() {
    switch (gpu_info_.vendor) {
        case GPUVendor::Qualcomm:
            // Adreno support:
            // 4xx: Full hardware ASTC (LDR + HDR)
            // 5xx: Full hardware ASTC (LDR + HDR)
            // 6xx: Full hardware ASTC (LDR + HDR)
            // 7xx: Full hardware ASTC (LDR + HDR)
            // 8xx: Full hardware ASTC (LDR + HDR)
            // 3xx and below: No hardware support
            if (gpu_info_.generation >= 4) {
                return ASTCSupport::HardwareFull;
            } else if (gpu_info_.generation == 3) {
                return ASTCSupport::SoftwareOnly;
            }
            return ASTCSupport::None;
            
        case GPUVendor::ARM:
            // Mali support:
            // Midgard (T6xx, T7xx, T8xx): LDR only
            // Bifrost (G3x, G5x, G7x): Full support
            // Valhall (G77+): Full support
            if (gpu_info_.generation >= 7) {
                return ASTCSupport::HardwareFull;
            } else if (gpu_info_.generation >= 3) {
                return ASTCSupport::HardwareLDR;
            }
            return ASTCSupport::SoftwareOnly;
            
        case GPUVendor::Imagination:
            // PowerVR Series 6XT+ supports ASTC
            if (gpu_info_.generation >= 6) {
                return ASTCSupport::HardwareFull;
            }
            return ASTCSupport::SoftwareOnly;
            
        case GPUVendor::Nvidia:
            // Tegra X1+ supports ASTC
            if (gpu_info_.generation >= 5) {
                return ASTCSupport::HardwareFull;
            }
            return ASTCSupport::SoftwareOnly;
            
        default:
            // Desktop GPUs typically have software ASTC
            return ASTCSupport::SoftwareOnly;
    }
}

bool ASTCOptimizer::HasHardwareASTC() const {
    return gpu_info_.has_native_astc;
}

bool ASTCOptimizer::ShouldUseHardwareDecoding() const {
    // Always use hardware if available
    if (gpu_info_.has_native_astc) {
        return true;
    }
    
    // Check if software decoding is acceptable
    // For high-end devices (Adreno 6xx+, Mali G7x+), software decode is fast enough
    if (gpu_info_.vendor == GPUVendor::Qualcomm && gpu_info_.generation >= 6) {
        return false;  // Fast CPU can handle it
    }
    
    if (gpu_info_.vendor == GPUVendor::ARM && gpu_info_.generation >= 7) {
        return false;  // Modern Mali has fast CPU
    }
    
    // For older/weaker devices, prefer to avoid ASTC if no hardware support
    return false;
}

bool ASTCOptimizer::IsSoftwareDecodingFast() const {
    // Modern high-end SoCs can handle software ASTC decode reasonably well
    switch (gpu_info_.vendor) {
        case GPUVendor::Qualcomm:
            // Adreno 6xx+ (Snapdragon 8xx series) have powerful CPUs
            return gpu_info_.generation >= 6;
            
        case GPUVendor::ARM:
            // Mali G7x+ (high-end chips) have good CPU performance
            return gpu_info_.generation >= 7;
            
        case GPUVendor::Nvidia:
            // Tegra chips generally have good CPUs
            return gpu_info_.generation >= 5;
            
        default:
            return false;
    }
}

bool ASTCOptimizer::GetRecommendedFormat() const {
    return gpu_info_.recommend_hardware_decode;
}

bool ASTCOptimizer::IsBlockSizeSupported(u32 block_width, u32 block_height) const {
    if (!gpu_info_.has_native_astc) {
        return false;
    }
    
    // All hardware ASTC implementations support these common block sizes
    static constexpr std::array common_sizes = {
        std::pair{4u, 4u},   // 4x4 - most common
        std::pair{5u, 4u},   // 5x4
        std::pair{5u, 5u},   // 5x5
        std::pair{6u, 5u},   // 6x5
        std::pair{6u, 6u},   // 6x6
        std::pair{8u, 5u},   // 8x5
        std::pair{8u, 6u},   // 8x6
        std::pair{8u, 8u},   // 8x8
        std::pair{10u, 5u},  // 10x5
        std::pair{10u, 6u},  // 10x6
        std::pair{10u, 8u},  // 10x8
        std::pair{10u, 10u}, // 10x10
        std::pair{12u, 10u}, // 12x10
        std::pair{12u, 12u}, // 12x12
    };
    
    for (const auto& [w, h] : common_sizes) {
        if (w == block_width && h == block_height) {
            return true;
        }
    }
    
    return false;
}

std::string ASTCOptimizer::GetPerformanceHint() const {
    std::ostringstream hint;
    
    hint << "ASTC Performance Hint: ";
    
    if (gpu_info_.has_native_astc) {
        hint << "✓ Hardware ASTC available - Optimal performance! ";
        hint << "Use native ASTC formats for best speed and memory.";
    } else if (IsSoftwareDecodingFast()) {
        hint << "⚠ Software ASTC decoding (acceptable performance). ";
        hint << "Consider using hardware formats on newer devices.";
    } else {
        hint << "✗ No hardware ASTC - Performance impact expected. ";
        hint << "Recommend: Disable ASTC or upgrade device for better experience.";
    }
    
    // Add GPU-specific recommendations
    if (gpu_info_.vendor == GPUVendor::Qualcomm) {
        if (gpu_info_.generation >= 6) {
            hint << " (Adreno " << gpu_info_.generation << "xx: Excellent support)";
        } else if (gpu_info_.generation >= 4) {
            hint << " (Adreno " << gpu_info_.generation << "xx: Good support)";
        } else {
            hint << " (Adreno " << gpu_info_.generation << "xx: Consider disabling)";
        }
    }
    
    return hint.str();
}

} // namespace OpenGL
