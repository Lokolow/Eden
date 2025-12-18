// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <unordered_map>
#include <vector>

#include "common/common_types.h"
#include "video_core/texture_cache/types.h"

namespace OpenGL {

/**
 * Aggressive Texture Garbage Collector
 * 
 * Automatically frees textures from VRAM that haven't been used recently.
 * Optimized for devices with limited memory (4GB RAM).
 * 
 * Features:
 * - Frame-based tracking of texture usage
 * - Configurable cleanup threshold (default: 60 frames = ~1 second at 60fps)
 * - Priority-based cleanup (keeps render targets, purges effects first)
 * - Memory pressure detection
 * - Forced cleanup when memory is critical
 */
class TextureGarbageCollector {
public:
    struct Config {
        // Number of frames before a texture is considered unused
        u32 unused_frame_threshold = 60;
        
        // Enable aggressive mode (lower threshold under memory pressure)
        bool aggressive_mode = true;
        
        // Aggressive mode threshold (in frames)
        u32 aggressive_threshold = 30;
        
        // Memory pressure threshold (MB) - trigger aggressive cleanup
        u64 memory_pressure_mb = 512;
        
        // Maximum VRAM usage target (MB)
        u64 max_vram_target_mb = 1024;
    };
    
    explicit TextureGarbageCollector(Config config = {});
    ~TextureGarbageCollector() = default;

    // Called every frame to track and cleanup
    void TickFrame();
    
    // Register texture usage
    void MarkTextureUsed(VideoCommon::ImageId image_id);
    
    // Register new texture
    void RegisterTexture(VideoCommon::ImageId image_id, u64 size_bytes, bool is_render_target);
    
    // Unregister texture (manual deletion)
    void UnregisterTexture(VideoCommon::ImageId image_id);
    
    // Get list of textures to purge this frame
    std::vector<VideoCommon::ImageId> GetTexturesToPurge();
    
    // Force cleanup regardless of frame count
    void ForceCleanup(u32 target_free_mb = 256);
    
    // Check if we're under memory pressure
    bool IsMemoryPressureHigh() const;
    
    // Get statistics
    struct Stats {
        u64 total_textures = 0;
        u64 total_vram_mb = 0;
        u64 textures_purged = 0;
        u64 vram_freed_mb = 0;
        u32 current_frame = 0;
    };
    Stats GetStats() const;
    
    // Update memory usage info
    void UpdateMemoryUsage(u64 current_vram_bytes);
    
private:
    struct TextureInfo {
        u64 size_bytes = 0;
        u32 last_used_frame = 0;
        bool is_render_target = false;
        u32 usage_count = 0;
    };
    
    Config config_;
    u32 current_frame_ = 0;
    u64 current_vram_usage_ = 0;
    
    std::unordered_map<VideoCommon::ImageId, TextureInfo> tracked_textures_;
    
    // Statistics
    u64 total_textures_purged_ = 0;
    u64 total_vram_freed_ = 0;
    
    // Helper methods
    u32 GetEffectiveThreshold() const;
    bool ShouldPurgeTexture(const TextureInfo& info, u32 frames_unused) const;
    void SortTexturesByPriority(std::vector<VideoCommon::ImageId>& textures);
};

} // namespace OpenGL
